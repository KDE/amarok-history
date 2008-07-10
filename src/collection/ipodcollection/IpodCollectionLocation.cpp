/*
 *  Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "IpodCollectionLocation.h"

#include "Debug.h"
#include "Meta.h"
#include "IpodCollection.h"
#include "IpodHandler.h"
#include "IpodMeta.h"
#include "../../statusbar/StatusBar.h"
#include "MediaDeviceCache.h" // for collection refresh hack

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <kjob.h>
#include <KLocale>
#include <KSharedPtr>
#include <kio/job.h>
#include <kio/jobclasses.h>


using namespace Meta;

IpodCollectionLocation::IpodCollectionLocation( IpodCollection const *collection )
    : CollectionLocation()
    , m_collection( const_cast<IpodCollection*>( collection ) )
    , m_removeSources( false )
    , m_overwriteFiles( false )
{
    //nothing to do
}

IpodCollectionLocation::~IpodCollectionLocation()
{
    DEBUG_BLOCK
    //nothing to do
}

QString
IpodCollectionLocation::prettyLocation() const
{
    return collection()->prettyName();
}

bool
IpodCollectionLocation::isWritable() const
{
    return true;
}

// TODO: implement (use IpodHandler ported method for removing a track)
bool
IpodCollectionLocation::remove( const Meta::TrackPtr &track )
{
    DEBUG_BLOCK
    Q_UNUSED(track);
            /*
    KSharedPtr<IpodTrack> ipodTrack = KSharedPtr<IpodTrack>::dynamicCast( track );
    if( ipodTrack && ipodTrack->inCollection() && ipodTrack->collection()->collectionId() == m_collection->collectionId() )
    {
        bool removed;
        if( m_tracksRemovedByDestination.contains( track ) )
        {
            removed = true;
        }
        else
        {
            removed = QFile::remove( ipodTrack->playableUrl().path() );
        }
        if( removed && ( !m_tracksRemovedByDestination.contains( track ) || m_tracksRemovedByDestination[ track ] ) )
        {

            QString query = QString( "SELECT id FROM urls WHERE deviceid = %1 AND rpath = '%2';" )
                                .arg( QString::number( ipodTrack->deviceid() ), m_collection->escape( ipodTrack->rpath() ) );
            QStringList res = m_collection->query( query );
            if( res.isEmpty() )
            {
                warning() << "Tried to remove a track from IpodCollection which is not in the collection";
            }
            else
            {
                int id = res[0].toInt();
                QString query = QString( "DELETE FROM tracks where id = %1;" ).arg( id );
                m_collection->query( query );
            }
        }
        return removed;
    }
    else
    {
        return false;
    }
            */

            return false; // this goes away after this is implemented
}

void
IpodCollectionLocation::slotJobFinished( KJob *job )
{
    DEBUG_BLOCK
    Q_UNUSED(job);
    // TODO: NYI
            /*
    m_jobs.remove( job );
    if( job->error() )
    {
        //TODO: proper error handling
        warning() << "An error occurred when copying a file: " << job->errorString();
    }
    job->deleteLater();
    if( m_jobs.isEmpty() )
    {
        insertTracks( m_destinations );
        insertStatistics( m_destinations );
        //m_collection->scanManager()->setBlockScan( false );
        slotCopyOperationFinished();
    }
            */
}

void
IpodCollectionLocation::copyUrlsToCollection( const QMap<Meta::TrackPtr, KUrl> &sources )
{
    DEBUG_BLOCK

    // iterate through source tracks
    foreach( const Meta::TrackPtr &track, sources.keys() )
    {

        debug() << "copying from " << sources[ track ];
        
        m_collection->copyTrackToDevice( track );

        
    }

    // hack to refresh the collection with new tracks, will fix later
    QString udi = m_collection->udi();
    m_collection->deviceRemoved();
    connect( this, SIGNAL( addDevice( const QString& ) ), MediaDeviceCache::instance(), SIGNAL(  deviceAdded( const QString& ) ) );
    emit addDevice( udi );
    

    slotCopyOperationFinished();
}

void
IpodCollectionLocation::insertTracks( const QMap<Meta::TrackPtr, QString> &trackMap )
{
    // NOTE: IpodHandler doing this right now
    Q_UNUSED(trackMap);
    return;
    DEBUG_BLOCK
            /*
    QList<QVariantMap > metadata;
    QStringList urls;
    foreach( const Meta::TrackPtr &track, trackMap.keys() )
    {
        if( m_ignoredDestinations.contains( trackMap[ track ], Qt::CaseSensitive ) )
        {
            continue;
        }
        QVariantMap trackData = Meta::Field::mapFromTrack( track.data() );
        trackData.insert( Meta::Field::URL, trackMap[ track ] );  //store the new url of the file
        metadata.append( trackData );
        urls.append( trackMap[ track ] );
    }
    ScanResultProcessor processor( m_collection );
    processor.setScanType( ScanResultProcessor::IncrementalScan );
    QMap<QString, uint> mtime = updatedMtime( urls );
    foreach( const QString &dir, mtime.keys() )
    {
        processor.addDirectory( dir, mtime[ dir ] );
    }
    if( !metadata.isEmpty() )
    {
        QFileInfo info( metadata.first().value( Meta::Field::URL ).toString() );
        QString currentDir = info.dir().absolutePath();
        QList<QVariantMap > currentMetadata;
        foreach( const QVariantMap &map, metadata )
        {
            debug() << "processing file " << map.value( Meta::Field::URL );
            QFileInfo info( map.value( Meta::Field::URL ).toString() );
            QString dir = info.dir().absolutePath();
            if( dir != currentDir )
            {
                processor.processDirectory( currentMetadata );
                currentDir = dir;
                currentMetadata.clear();
            }
            currentMetadata.append( map );
        }
        if( !currentMetadata.isEmpty() )
        {
            processor.processDirectory( currentMetadata );
        }
    }
    processor.commit();
            */
}

void
IpodCollectionLocation::insertStatistics( const QMap<Meta::TrackPtr, QString> &trackMap )
{
    DEBUG_BLOCK
    Q_UNUSED(trackMap);
    return;
    // NOTE: not sure if this is needed
            /*
    MountPointManager *mpm = MountPointManager::instance();
    foreach( const Meta::TrackPtr &track, trackMap.keys() )
    {
        if( m_ignoredDestinations.contains( trackMap[ track ], Qt::CaseSensitive ) )
        {
            continue;
        }
        QString url = trackMap[ track ];
        int deviceid = mpm->getIdForUrl( url );
        QString rpath = mpm->getRelativePath( deviceid, url );
        QString ipod = QString( "SELECT COUNT(*) FROM statistics LEFT JOIN urls ON statistics.url = urls.id "
                               "WHERE urls.deviceid = %1 AND urls.rpath = '%2';" ).arg( QString::number( deviceid ), m_collection->escape( rpath ) );
        QStringList count = m_collection->query( ipod );
        if( count.first().toInt() != 0 )    //crash if the ipod is bad
        {
            continue;   //a statistics row already exists for that url, and we cannot merge the statistics
        }
        //the row will exist because this method is called after insertTracks
        QString select = QString( "SELECT id FROM urls WHERE deviceid = %1 AND rpath = '%2';" ).arg( QString::number( deviceid ), m_collection->escape( rpath ) );
        QStringList result = m_collection->query( select );
        QString id = result.first();    //if result is empty something is going very wrong
        //the following ipod was copied from IpodMeta.cpp
        QString insert = "INSERT INTO statistics(url,rating,score,playcount,accessdate,createdate) VALUES ( %1 );";
        QString data = "%1,%2,%3,%4,%5,%6";
        data = data.arg( id, QString::number( track->rating() ), QString::number( track->score() ) );
        data = data.arg( QString::number( track->playCount() ), QString::number( track->lastPlayed() ), QString::number( track->firstPlayed() ) );
        m_collection->insert( insert.arg( data ), "statistics" );
    }
            */
}

// NOTE: probably unnecessary
/*
void
IpodCollectionLocation::movedByDestination( const Meta::TrackPtr &track, bool removeFromDatabase )
{
    m_tracksRemovedByDestination.insert( track, removeFromDatabase );
}
*/
#include "IpodCollectionLocation.moc"