/*
 *  Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>
 *  Copyright (c) 2007 Casey Link <unnamedrambler@gmail.com>
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

#include "SqlCollection.h"

#include "DatabaseUpdater.h"
#include "ScanManager.h"
#include "SqlQueryMaker.h"
#include "SqliteCollection.h"
//#include "mysqlcollection.h"
#include "SqlCollectionLocation.h"

#ifdef Q_OS_WIN32
class XesamCollectionBuilder
{
public:
    XesamCollectionBuilder(SqlCollection *collection) {}
};
#else
#include "XesamCollectionBuilder.h"
#endif

#include <klocale.h>

#include <QTimer>

AMAROK_EXPORT_PLUGIN( SqlCollectionFactory )

void
SqlCollectionFactory::init()
{
    Collection* collection;
/*    switch( CollectionDB::instance()->getDbConnectionType() )
    {
        case DbConnection::sqlite :
            collection = new SqliteCollection( "localCollection", i18n( "Local collection" ) );
            break;
//fix this later
        case DbConnection::mysql :
            collection = new MySqlCollection( "localCollection", i18n( "Local collection" ) );
            break;
        default :
            collection = new SqlCollection( "localCollection", i18n( "Local collection" ) );
            break;
    }*/
    collection = new SqliteCollection( "localCollection", i18n( "Local collection" ) );
    emit newCollection( collection );
}

SqlCollection::SqlCollection( const QString &id, const QString &prettyName )
    : Collection()
    , m_registry( new SqlRegistry( this ) )
    , m_updater( new DatabaseUpdater( this ) )
    , m_scanManager( new ScanManager( this ) )
    , m_collectionId( id )
    , m_prettyName( prettyName )
    , m_xesamBuilder( 0 )
{
}

SqlCollection::~SqlCollection()
{
    delete m_registry;
}

void
SqlCollection::init()
{
    QTimer::singleShot( 0, this, SLOT( initXesam() ) );
    if( m_updater->needsUpdate() )
        m_updater->update();
    QStringList result = query( "SELECT count(*) FROM tracks" );
    // If database version is updated, the collection needs to be rescanned.
    // Works also if the collection is empty for some other reason
    // (e.g. deleted collection.db)
    if( !result.isEmpty() && result.first().toInt() == 0 )
    {
        QTimer::singleShot( 0, m_scanManager, SLOT( startFullScan() ) );
    }
    else
    {
        //the collection already exists, so check if it is up to date

        //This causes amarok to often crash on startup, so disable for now
        //QTimer::singleShot( 0, m_scanManager, SLOT( startIncrementalScan() ) );
    }
}

void
SqlCollection::startFullScan()
{
    m_scanManager->startFullScan();
}

void
SqlCollection::startIncrementalScan()
{
    m_scanManager->startIncrementalScan();
}

QString
SqlCollection::collectionId() const
{
    return m_collectionId;
}

QString
SqlCollection::prettyName() const
{
    return m_prettyName;
}

QueryMaker*
SqlCollection::queryMaker()
{
    return new SqlQueryMaker( this );
}

SqlRegistry*
SqlCollection::registry() const
{
    return m_registry;
}

DatabaseUpdater*
SqlCollection::dbUpdater() const
{
    return m_updater;
}

ScanManager*
SqlCollection::scanManager() const
{
    return m_scanManager;
}

void
SqlCollection::removeCollection()
{
    emit remove();
}

bool
SqlCollection::isDirInCollection( QString path )
{
    return m_scanManager->isDirInCollection( path );
}

bool
SqlCollection::isFileInCollection( const QString &url )
{
    return m_scanManager->isFileInCollection( url );
}

bool
SqlCollection::possiblyContainsTrack( const KUrl &url ) const
{
    return url.protocol() == "file";
}

Meta::TrackPtr
SqlCollection::trackForUrl( const KUrl &url )
{
    return m_registry->getTrack( url.path() );
}

CollectionLocation*
SqlCollection::location() const
{
    return new SqlCollectionLocation( this );
}

bool
SqlCollection::isWritable() const
{
    return true;
}

bool
SqlCollection::isOrganizable() const
{
    return true;
}

void
SqlCollection::sendChangedSignal()
{
    emit updated();
}

QString
SqlCollection::escape( QString text ) const           //krazy:exclude=constref
{
    return text.replace( '\'', "''" );;
}


int
SqlCollection::sqlDatabasePriority() const
{
    return 1;
}

QString
SqlCollection::type() const
{
    return "sql";
}

QString
SqlCollection::boolTrue() const
{
    return "1";
}

QString
SqlCollection::boolFalse() const
{
    return "0";
}

QString
SqlCollection::idType() const
{
    return "INTEGER PRIMARY KEY AUTO_INCREMENT";
}

QString
SqlCollection::textColumnType( int length ) const
{
    return QString( "VARCHAR(%1)" ).arg( length );
}

QString
SqlCollection::exactTextColumnType( int length ) const
{
    return textColumnType( length );
}

QString
SqlCollection::longTextColumnType() const
{
    return "TEXT";
}

QString
SqlCollection::randomFunc() const
{
    return "RAND()";
}

void
SqlCollection::vacuum() const
{
    //implement in subclasses if necessary
}

void
SqlCollection::initXesam()
{
    m_xesamBuilder = new XesamCollectionBuilder( this );
}

#include "SqlCollection.moc"
