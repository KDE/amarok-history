/***************************************************************************
 * copyright        : (C) 2007 Ian Monroe <ian@monroe.nu>
 *                  : (C) 2007 Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **************************************************************************/


#include "PlaylistModel.h"

#include "amarok.h"
#include "amarokconfig.h"
#include "AmarokMimeData.h"
#include "debug.h"
#include "enginecontroller.h"
#include "PlaylistItem.h"
#include "PlaylistGraphicsView.h"
#include "PlaylistFileSupport.h"
#include "RepeatTrackNavigator.h"
#include "StandardTrackNavigator.h"
#include "RepeatPlaylistNavigator.h"
#include "ContextStatusBar.h"
#include "TheInstances.h"
#include "UndoCommands.h"

#include "collection/BlockingQuery.h"
#include "collection/Collection.h"
#include "collection/CollectionManager.h"
#include "collection/QueryMaker.h"

#include <QAction>
#include <QStringList>
#include <QUndoStack>

#include <KFileItem>
#include <KIcon>
#include <KUrl>

using namespace Playlist;

namespace Amarok
{
    // Sorting of a tracklist.
    bool trackNumberLessThan( Meta::TrackPtr left, Meta::TrackPtr right )
    {
        if( left->album() == right->album() )
            return left->trackNumber() < right->trackNumber();

        else if( left->artist() == right->artist() )
            return QString::localeAwareCompare( left->album()->prettyName(), right->album()->prettyName() ) < 0;
        else // compare artists alphabetically
            return QString::localeAwareCompare( left->artist()->prettyName(), right->artist()->prettyName() ) < 0;
    }
}

Model *Model::s_instance = 0;

Model::Model( QObject* parent )
    : QAbstractListModel( parent )
    , m_activeRow( -1 )
    , m_advancer( new StandardTrackNavigator( this ) )
    , m_undoStack( new QUndoStack( this ) )
    , m_stopAfterMode( StopNever )
{
    connect( EngineController::instance(), SIGNAL( orderNext( bool ) ), this, SLOT( trackFinished() ), Qt::QueuedConnection );
    connect( EngineController::instance(), SIGNAL( orderCurrent() ), this, SLOT( playCurrentTrack() ), Qt::QueuedConnection );
    s_instance = this;
}

void
Model::init()
{
    KActionCollection* ac = Amarok::actionCollection();
    QAction* undoButton  = m_undoStack->createUndoAction( this, i18n("Undo") );
    undoButton->setIcon( KIcon( "edit-undo-amarok" ) );
    ac->addAction("playlist_undo", undoButton);
    QAction* redoButton  = m_undoStack->createRedoAction( this, i18n("Redo") );
    ac->addAction("playlist_redo", redoButton);
    redoButton->setIcon( KIcon( "edit-redo-amarok" ) );

    //FIXME: because the restoring in app.cpp:711 causes problems we do it here. This doesn't respect the command line flags though.
    /*if ( AmarokConfig::savePlaylist() )
    {
        The::playlistModel()->restoreSession();
    }*/
}

Model::~Model()
{
    if( AmarokConfig::savePlaylist() )
    {
        Meta::TrackList list;
        foreach( Item* item, itemList() )
        {
            list << item->track();
        }
        The::playlistManager()->save( list, defaultPlaylistPath() );
    }
    delete m_advancer;
}

int
Model::rowCount( const QModelIndex& ) const
{
    return m_items.size();
}

QVariant
Model::data( const QModelIndex& index, int role ) const
{
    int row = index.row();
    /*if( ( role ==  Qt::FontRole) && ( row == m_activeRow ) )
    {
        QFont original;
        original.setBold( true );
        return original;
    }
    else*/
    if( role == ItemRole && ( row != -1 ) )
        return QVariant::fromValue( m_items.at( row ) );

    else if( role == ActiveTrackRole )
        return ( row == m_activeRow );

    else if( role == TrackRole && ( row != -1 ) && m_items.at( row )->track() )
        return QVariant::fromValue( m_items.at( row )->track() );

    else if ( role == GroupRole )
    {
        Meta::TrackPtr track = m_items.at( row )->track();

        if ( !track->album() )
            return None;  // no albm set
        else if ( !m_albumGroups.contains( track->album()->prettyName() ) )
            return None;  // no group for this album, should never happen...

        AlbumGroup * albumGroup = m_albumGroups.value( track->album()->prettyName() );
        return albumGroup->groupMode( row );

    }

    else if ( role == GroupedTracksRole )
    {
        Meta::TrackPtr track = m_items.at( row )->track();
        AlbumGroup * albumGroup = m_albumGroups.value( track->album()->prettyName() );
        return albumGroup->elementsInGroup( row );
    }

    else if ( role == GroupedAlternateRole )
    {
        Meta::TrackPtr track = m_items.at( row )->track();
        AlbumGroup * albumGroup = m_albumGroups.value( track->album()->prettyName() );
        if( albumGroup )
            return albumGroup->alternate( row );
        return true;
    }

    else if ( role == GroupedCollapsibleRole )
    {
        Meta::TrackPtr track = m_items.at( row )->track();
        AlbumGroup * albumGroup = m_albumGroups.value( track->album()->prettyName() );
        //we cannot collapse the group that contains the currently selected track.
        return ( albumGroup->firstInGroup( m_activeRow ) == -1 );

    }

    else if( role == Qt::DisplayRole && row != -1 )
    {
        switch( index.column() )
        {
            case 0:
                return m_items.at( row )->track()->name();
            case 1:
                if ( m_items.at( row )->track()->album() )
                    return m_items.at( row )->track()->album()->name();
                else
                    return "";
            case 2:
                if ( m_items.at( row )->track()->artist() )
                    return m_items.at( row )->track()->artist()->name();
                else
                    return "";
            case 3:
                return QString("%1 - %2 - %3")
                    .arg(m_items.at( row )->track()->artist()->name())
                    .arg(m_items.at( row )->track()->album()->name())
                    .arg(m_items.at( row )->track()->name());
        }
    }
    // else
    return QVariant();
/*        switch( role )
        {
            case AlbumArtist: return track->album()->albumArtist()->name();
            case Album: return track->album()->name();
            case Artist: return track->artist()->name();
            case Bitrate: return track->bitrate();
            case Composer: return track->composer()->name();
            case CoverImage: return track->album()->image( 50 );
            case Comment: return track->comment();
            case DiscNumber: return track->discNumber();
            case Filesize: return track->filesize();
            case Genre: return track->genre()->name();
            case Length: return track->length();
            case Rating: return track->rating();
            case Score: return track->score();
            case Title: return track->name();
            case TrackNumber: return track->trackNumber();
            case Year: return track->year()->name().toInt();
            default: return QVariant();
        } */
}

void
Model::insertTrack( int row, Meta::TrackPtr track )
{
    DEBUG_BLOCK
    Meta::TrackList list;
    list.append( track );
    insertTracks( row, list );

}

void
Model::insertTracks( int row, Meta::TrackList tracks )
{
    //check if any tracks in this list ha a url that is actuall a playlist
    bool containsPlaylists = false;
    QList<int> playlistIndices;
    int index = 0;
    foreach( Meta::TrackPtr track, tracks ) {

        if( The::playlistManager()->canExpand( track ) ) {
            containsPlaylists = true;
            playlistIndices.append( index );
            index++;
        }
    }


    if ( !containsPlaylists ) {
         //if its all plain tracks, do the easy thing
        m_undoStack->push( new AddTracksCmd( 0, row, tracks ) );
    } else {
        //split it up, but add as much as possible as groups to keep the undo stuff usable

        int lastIndex = 0;
        int offset = 0;

        foreach( int playlistIndex, playlistIndices ) {

            if( ( playlistIndex  - lastIndex ) > 0 ) {
                m_undoStack->push( new AddTracksCmd( 0, row + lastIndex + offset, tracks.mid( lastIndex, playlistIndex  - lastIndex ) ) );
                row = row + ( playlistIndex  - lastIndex );
            }

            Meta::PlaylistPtr playlist =  The::playlistManager()->expand( tracks.at( playlistIndex ) );
            m_undoStack->push( new AddTracksCmd( 0, row + playlistIndex + offset, playlist->tracks() ) );
            offset += playlist->tracks().size();

            lastIndex = playlistIndex + 1;
        }
        

    }



    
}

bool
Model::removeRows( int position, int rows, const QModelIndex& /*parent*/  )
{
    m_undoStack->push( new RemoveTracksCmd( 0, position, rows ) );
    return true;
}

void
Model::insertTracks( int row, QueryMaker *qm )
{
    qm->startTrackQuery();
    connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ) );
    connect( qm, SIGNAL( newResultReady( QString, Meta::TrackList ) ), SLOT( newResultReady( QString, Meta::TrackList ) ) );
    m_queryMap.insert( qm, row );
    qm->run();
}

Qt::DropActions
Model::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

void
Model::trackFinished()
{
    if( m_activeRow < 0 || m_activeRow >= m_items.size() )
        return;
    Meta::TrackPtr track = m_items.at( m_activeRow )->track();
    track->finishedPlaying( 1.0 ); //TODO: get correct value for parameter
    m_advancer->advanceTrack();
}

void
Model::play( const QModelIndex& index )
{
    play( index.row() );
}

void
Model::play( int row )
{

    setActiveRow( row );
    EngineController::instance()->play( m_items[ m_activeRow ]->track() );
}

void
Model::playlistRepeatMode( int item )
{
    // FIXME? couldn't this method go away, and just call playModeChanged() instead of this?
    if( item == 0 )
        playModeChanged( Playlist::Standard );
    else if (item == 3)
        playModeChanged( Playlist::RepeatPlaylist);
    else //for now just turn on repeat if anything but "off" is clicked
        playModeChanged( Playlist::RepeatTrack );
}

void
Model::next()
{
    if( m_activeRow < 0 || m_activeRow >= m_items.size() )
        return;
    Meta::TrackPtr track = m_items.at( m_activeRow )->track();
    track->finishedPlaying( 0.5 ); //TODO: get correct value for parameter
    m_advancer->userAdvanceTrack();
}

void
Model::back()
{
    if( m_activeRow < 0 || m_activeRow >= m_items.size() )
        return;
    Meta::TrackPtr track = m_items.at( m_activeRow )->track();
    track->finishedPlaying( 0.5 ); //TODO: get correct value for parameter
    m_advancer->recedeTrack();
}

void
Model::playCurrentTrack()
{
    int selected = m_activeRow;
    if( selected < 0 || selected >= m_items.size() )
    {
        //play first track if there are tracks in the playlist
        if( m_items.size() )
            selected = 0;
        else
            return;
    }
    play( selected );
}


QString
Model::prettyColumnName( Column index ) //static
{
    switch( index )
    {
        case Filename:   return i18nc( "The name of the file this track is stored in", "Filename"    );
        case Title:      return i18n( "Title"       );
        case Artist:     return i18n( "Artist"      );
        case AlbumArtist:return i18n( "Album Artist");
        case Composer:   return i18n( "Composer"    );
        case Year:       return i18n( "Year"        );
        case Album:      return i18n( "Album"       );
        case DiscNumber: return i18n( "Disc Number" );
        case TrackNumber:return i18nc( "The Track number for this item", "Track"       );
        case Bpm:        return i18n( "BPM"         );
        case Genre:      return i18n( "Genre"       );
        case Comment:    return i18n( "Comment"     );
        case Directory:  return i18nc( "The location on disc of this track", "Directory"   );
        case Type:       return i18n( "Type"        );
        case Length:     return i18n( "Length"      );
        case Bitrate:    return i18n( "Bitrate"     );
        case SampleRate: return i18n( "Sample Rate" );
        case Score:      return i18n( "Score"       );
        case Rating:     return i18n( "Rating"      );
        case PlayCount:  return i18n( "Play Count"  );
        case LastPlayed: return i18nc( "Column name", "Last Played" );
        case Mood:       return i18n( "Mood"        );
        case Filesize:   return i18n( "File Size"   );
        default:         return "This is a bug.";
    }

}

void
Model::playModeChanged( int row )
{
    delete m_advancer;
    switch (row)
    {
        case Playlist::Standard:
            m_advancer = new StandardTrackNavigator(this);
            break;
        case Playlist::RepeatTrack:
            m_advancer = new RepeatTrackNavigator(this);
            break;
        case Playlist::RepeatPlaylist:
            m_advancer = new RepeatPlaylistNavigator(this);
            break;
    }
}

void
Model::setActiveRow( int row )
{
    DEBUG_BLOCK

    emit dataChanged( createIndex( m_activeRow, 0 ), createIndex( m_activeRow, 0 ) );
    emit dataChanged( createIndex( row, 0 ), createIndex( row, 0 ) );

    m_activeRow = row;

    //make sure that the group containg this track is expanded

    //not all tracks have a valid album ( radio stations for instance... )
    QString albumName;
    if ( !m_items[ row ]->track()->album().isNull() ) {
        albumName = m_items[ row ]->track()->album()->prettyName();
    }

    if( m_albumGroups.contains( albumName ) && !albumName.isEmpty() )
    {
        m_albumGroups[ albumName ]->setCollapsed( row,  false );
        debug() << "Here";
        emit( playlistGroupingChanged() );
    }

}

void
Model::metadataChanged( Meta::Track *track )
{
    const int size = m_items.size();
    const Meta::TrackPtr needle =  Meta::TrackPtr( track );
    for( int i = 0; i < size; i++ )
    {
        if( m_items.at( i )->track() == needle )
        {
            emit dataChanged( createIndex( i, 0 ), createIndex( i, 0 ) );
            break;
        }
    }

#if 0
    int index = m_tracks.indexOf( Meta::TrackPtr( track ), 0 );
    if( index != -1 )
        emit dataChanged( createIndex( index, 0 ), createIndex( index, 0 ) );
#endif
}

void
Model::metadataChanged(Meta::Album * album)
{
    //process each track
    Meta::TrackList tracks = album->tracks();
    foreach( Meta::TrackPtr track, tracks ) {
        metadataChanged( track.data() );
    }

}

void
Model::trackListChanged( Meta::Playlist * playlist )
{
    //So what if it changes, we don't care. We shouldn't even receive these events!
    if( m_observedPlaylist != playlist)
        return;
}

void
Model::clear()
{
    if( m_items.size() < 1 )
        return;
    removeRows( 0, m_items.size() );
    m_albumGroups.clear();
    m_lastAddedTrackAlbum = Meta::AlbumPtr();
    The::playlistView()->scene()->setSceneRect( The::playlistView()->scene()->itemsBoundingRect() );
//    m_activeRow = -1;
}

void
Model::insertOptioned( Meta::TrackList list, int options )
{

    DEBUG_BLOCK

//TODO: we call insertOptioned on resume before the statusbar is fully created... We need a better way to handle this
    if( list.isEmpty() ) {
//         Amarok::ContextStatusBar::instance()->shortMessage( i18n("Attempted to insert nothing into playlist.") );
        return; // don't add empty items
    }


    if( options & Unique )
    {
        int alreadyOnPlaylist = 0;
        for( int i = 0; i < list.size(); ++i )
        {

            Item* item;
            foreach( item, m_items )
            {
                if( item->track() == list.at( i ) )
                {
                    list.removeAt( i );
                    alreadyOnPlaylist++;
                    break;
                }
            }

        }
        if ( alreadyOnPlaylist )
            Amarok::ContextStatusBar::instance()->shortMessage( i18np("One track was already in the playlist, so it was not added.", "%1 tracks were already in the playlist, so they were not added.", alreadyOnPlaylist ) );
    }

    int firstItemAdded = -1;
    if( options & Replace )
    {
        clear();
        firstItemAdded = 0;
        insertTracks( 0, list );
    }
    else if( options & Append )
    {
        firstItemAdded = rowCount();
        insertTracks( firstItemAdded, list );
        if( EngineController::engine()->state() != Engine::Playing )
            play( firstItemAdded );
    }
    else if( options & Queue )
    {
        //TODO implement queue
    }

    if( options & DirectPlay )
    {
        if ( rowCount() > firstItemAdded )
            play( firstItemAdded );
    }
    else if( ( options & StartPlay ) && ( EngineController::engine()->state() != Engine::Playing ) && ( rowCount() != 0 ) )
    {
        play( firstItemAdded );
    }
    Amarok::actionCollection()->action( "playlist_clear" )->setEnabled( !m_items.isEmpty() );
}

void
Model::insertOptioned( Meta::TrackPtr track, int options )
{
    if( !track )
    {
        return;
    }
    Meta::TrackList list;
    list.append( track );
    insertOptioned( list, options );
}

void
Model::insertOptioned( Meta::PlaylistList list, int options )
{
    foreach( Meta::PlaylistPtr playlist, list )
    {
        insertOptioned( playlist, options );
    }
}

void
Model::insertOptioned( Meta::PlaylistPtr playlist, int options )
{
    if( !playlist )
    {
        return;
    }
    //TODO: Add this Meta::Playlist to the observed list
    insertOptioned( playlist->tracks(), options );
}

void
Model::insertPlaylist( int row, Meta::PlaylistPtr playlist )
{
    DEBUG_BLOCK
    m_undoStack->push( new AddTracksCmd( 0, row, playlist->tracks() ) );
}

void
Model::insertPlaylists( int row, Meta::PlaylistList playlists )
{
    int lastRow = row;
    foreach( Meta::PlaylistPtr playlist, playlists )
    {
        insertPlaylist( lastRow, playlist );
        lastRow += playlist->tracks().size();
    }
}

void
Model::insertOptioned( QueryMaker *qm, int options )
{
    if( !qm )
    {
        return;
    }
    qm->startTrackQuery();
    connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ) );
    connect( qm, SIGNAL( newResultReady( QString, Meta::TrackList ) ), SLOT( newResultReady( QString, Meta::TrackList ) ) );
    m_optionedQueryMap.insert( qm, options );
    qm->run();
}

bool
Model::saveM3U( const QString &path ) const
{
    Meta::TrackList tl;
    foreach( Item* item, itemList() )
        tl << item->track();
    if( The::playlistManager()->save( tl, path ) )
        return true;
    return false;
}

Qt::ItemFlags
Model::flags(const QModelIndex &index) const
{
    if( index.isValid() )
    {
        return ( Qt::ItemIsEnabled     | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled |
                 Qt::ItemIsDragEnabled | Qt::ItemIsSelectable );
    }
    return Qt::ItemIsDropEnabled;
}

QStringList
Model::mimeTypes() const //reimplemented
{
    QStringList ret = QAbstractListModel::mimeTypes();
    ret << AmarokMimeData::TRACK_MIME;
    ret << "text/uri-list"; //we do accept urls
    return ret;
}

QMimeData*
Model::mimeData( const QModelIndexList &indexes ) const //reimplemented
{
    AmarokMimeData* mime = new AmarokMimeData();
    Meta::TrackList selectedTracks;

    foreach( const QModelIndex &it, indexes )
        selectedTracks << m_items.at( it.row() )->track();

    mime->setTracks( selectedTracks );
    return mime;
}

bool
Model::dropMimeData ( const QMimeData * data, Qt::DropAction action, int row, int column, const QModelIndex & parent ) //reimplemented
{
    Q_UNUSED( column ); Q_UNUSED( parent );
    DEBUG_BLOCK

    if( action == Qt::IgnoreAction )
        return true;

    if( data->hasFormat( AmarokMimeData::TRACK_MIME ) )
    {
        debug() << "Found track mime type";

        const AmarokMimeData* trackListDrag = dynamic_cast<const AmarokMimeData*>( data );
        if( trackListDrag )
        {
            if( row < 0 )
            {
                debug() << "Inserting at row: " << row << " so we are appending to the list.";
                insertOptioned( trackListDrag->tracks(), Playlist::Append );
            }
            else
            {
                debug() << "Inserting at row: " << row <<" so its inserted correctly.";
                insertTracks( row, trackListDrag->tracks() );
            }
            return true;
        }
    }
    else if( data->hasFormat( AmarokMimeData::PLAYLIST_MIME ) )
    {
        debug() << "Found playlist mime type";

        const AmarokMimeData* dragList = dynamic_cast<const AmarokMimeData*>( data );
        if( dragList )
        {
            if( row < 0 )
            {
                debug() << "Inserting at row: " << row << " so we are appending to the list.";
                insertOptioned( dragList->playlists(), Playlist::Append );
            }
            else
            {
                debug() << "Inserting at row: " << row <<" so its inserted correctly.";
                insertPlaylists( row, dragList->playlists() );
            }
            return true;
        }
    }
    else if( data->hasUrls() )
    {
        //drop from an external source or the file browser
        debug() << "Drop from external source or file browser";
        QList<QUrl> urls = data->urls();
        Meta::TrackList tracks;
        foreach( const QUrl &url, urls )
        {
            KUrl kurl = KUrl( url );
            KFileItem kitem( KFileItem::Unknown, KFileItem::Unknown, kurl, true );
            if( kitem.isDir() )
            {
                KIO::ListJob* lister = KIO::listRecursive( kurl ); //kjob's delete themselves
                connect( lister, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList&) ), SLOT( directoryListResults( KIO::Job*, const KIO::UDSEntryList& ) ) );
            }
            else if( EngineController::canDecode( kurl ) )
            {
                Meta::TrackPtr track = CollectionManager::instance()->trackForUrl( kurl );
                if( track )
                    tracks.append( track );
            }
            //else TODO: notify user if can't decode, see also MyDirLister::matchesFilter
        }
        if( !tracks.isEmpty() )
            insertOptioned( tracks, Playlist::Append );
        return true;
    }
    return false;
}

////////////
//Private Methods
///////////

void
Model::directoryListResults( KIO::Job *job, const KIO::UDSEntryList &list )
{
    DEBUG_BLOCK
    Meta::TrackList tracks;
    //dfaure says that job->redirectionUrl().isValid() ? job->redirectionUrl() : job->url(); might be needed
    //but to wait until an issue is actually found, since it might take more work
    const KUrl dir = static_cast<KIO::SimpleJob*>( job )->url();
    foreach( KIO::UDSEntry entry, list )
    {
        KUrl currentUrl = dir;
        currentUrl.addPath( entry.stringValue( KIO::UDSEntry::UDS_NAME ) );
        debug() << "listing url: " << currentUrl;
        if( !entry.isDir() &&  EngineController::canDecode( currentUrl ) )
        {
            Meta::TrackPtr track = CollectionManager::instance()->trackForUrl( currentUrl );
            if( track )
                tracks.append( track );
        }
    }
    if( !tracks.isEmpty() )
        insertOptioned( tracks, Playlist::Append );
}


void
Model::insertTracksCommand( int row, Meta::TrackList list )
{
    DEBUG_BLOCK
    debug() << "inserting... row: " << row << ' ' << list.count() << " Tracks";
    if( !list.size() )
        return;

    beginInsertRows( QModelIndex(), row, row + list.size() - 1 );
    int i = 0;
    foreach( Meta::TrackPtr track , list )
    {
        if( track )
        {

            track->subscribe( this );
            if( track->album() )
                track->album()->subscribe( this );

            m_items.insert( row + i, new Item( track ) );
            i++;
        }
    }

    //we need to regroup everything below this point as all the index changes
    regroupAlbums( row, m_items.count() );

    endInsertRows();
    //push up the active row if needed
    if( m_activeRow > row )
    {
        int oldActiveRow = m_activeRow;
        m_activeRow += list.size();
        Q_UNUSED( oldActiveRow );
        //dataChanged( createIndex( oldActiveRow, 0 ), createIndex( oldActiveRow, columnCount() -1 ) );
        //dataChanged( createIndex( m_activeRow, 0 ), createIndex( m_activeRow, columnCount() -1 ) );
    }
    dataChanged( createIndex( row, 0 ), createIndex( rowCount() - 1, 0 ) );

    The::playlistView()->scene()->setSceneRect( The::playlistView()->scene()->itemsBoundingRect() );

    Amarok::actionCollection()->action( "playlist_clear" )->setEnabled( !m_items.isEmpty() );

    emit playlistCountChanged( rowCount() );
}


Meta::TrackList
Model::removeTracksCommand( int position, int rows )
{
    DEBUG_BLOCK

    beginRemoveRows( QModelIndex(), position, position + rows - 1 );
//     TrackList::iterator start = m_tracks.begin() + position;
//     TrackList::iterator end = start + rows;
//     m_tracks.erase( start, end );
    Meta::TrackList ret;
    for( int i = position; i < position + rows; i++ )
    {

        debug() << "delete index " << i;
        Item* item = m_items.takeAt( position ); //take at position, row times
        item->track()->unsubscribe( this );
        if( item->track()->album() )
            item->track()->album()->unsubscribe( this );
        ret.push_back( item->track() );
        delete item;
    }


    //update m_activeRow
    bool activeRowChanged = true;
    bool oldActiveRow = m_activeRow;
    if( m_activeRow >= position && m_activeRow < ( position + rows ) )
        m_activeRow = -1;
    else if( m_activeRow >= position )
        m_activeRow = m_activeRow - position;
    else
        activeRowChanged = false;
    if( activeRowChanged )
    {
        dataChanged( createIndex( oldActiveRow, 0 ), createIndex( oldActiveRow, columnCount() -1 ) );
        dataChanged( createIndex( m_activeRow, 0 ), createIndex( m_activeRow, columnCount() -1 ) );
    }
    dataChanged( createIndex( position, 0 ), createIndex( rowCount(), 0 ) );


    //we need to regroup everything below this point as all the index changes
    //also, use the count before the rows was removed to make sure all groups are deleted
    regroupAlbums( position, rows, OffsetAfter, 0 - rows );

    endRemoveRows();

    The::playlistView()->scene()->setSceneRect( The::playlistView()->scene()->itemsBoundingRect() );

    Amarok::actionCollection()->action( "playlist_clear" )->setEnabled( !m_items.isEmpty() );


    emit playlistCountChanged( rowCount() );

    emit rowsChanged( position );
    
    return ret;
}

void
Model::queryDone() //Slot
{
    QueryMaker *qm = dynamic_cast<QueryMaker*>( sender() );
    if( qm )
    {
        m_queryMap.remove( qm );
        qm->deleteLater();
    }
}

void
Model::newResultReady( const QString &collectionId, const Meta::TrackList &tracks ) //Slot
{
    Meta::TrackList ourTracks = tracks;
    qStableSort( ourTracks.begin(), ourTracks.end(), Amarok::trackNumberLessThan );
    Q_UNUSED( collectionId )
    QueryMaker *qm = dynamic_cast<QueryMaker*>( sender() );
    if( qm )
    {
        //requires better handling of queries which return multiple results
        if( m_queryMap.contains( qm ) )
            insertTracks( m_queryMap.value( qm ), ourTracks );
        else if( m_optionedQueryMap.contains( qm ) )
            insertOptioned( ourTracks, m_optionedQueryMap.value( qm ) );
    }
}

QVariant Model::headerData(int section, Qt::Orientation orientation, int role) const
{

    Q_UNUSED( orientation );

    if ( role != Qt::DisplayRole )
        return QVariant();

    switch ( section )
    {
        case 0:
            return "title";
        case 1:
            return "album";
        case 2:
            return "artist";
        case 3:
            return "custom";
        default:
            return QVariant();
     }


}

void Model::moveRow(int row, int to)
{

    m_items.move( row, to );

    int offset = -1;
    if ( to < row )
        offset = 1;

    regroupAlbums( qMin( row, to) , qMax( row, to ), OffsetBetween, offset );

}


void Model::regroupAlbums( int firstRow, int lastRow, OffsetMode offsetMode, int offset )
{
    DEBUG_BLOCK

     //debug() << "first row: " << firstRow << ", last row: " << lastRow;


    int area1Start = -1;
    int area1End = -1;
    int area2Start = -1;
    int area2End = -1;

    int aboveFirst;
    int belowLast;

    aboveFirst = firstRow - 1;
    if ( aboveFirst < 0 ) aboveFirst = 0;

    belowLast = lastRow + 1;
    if ( belowLast > ( m_items.count() - 1 ) ) belowLast = m_items.count() - 1;

    debug() << "aboveFirst: " << aboveFirst << ", belowLast: " << belowLast;

    //delete affected groups

    QMapIterator< QString, AlbumGroup *> itt(m_albumGroups);
    while (itt.hasNext()) {

        itt.next();
        AlbumGroup * group = itt.value();

        bool removeGroupAboveFirstRow = false;
        bool removeGroupBelowFirstRow = false;
        bool removeGroupAboveLastRow = false;
        bool removeGroupBelowLastRow = false;

        int temp = group->firstInGroup( aboveFirst );
        if ( temp != -1 ) {
            debug() << "--3";
            area1Start = temp;
            removeGroupAboveFirstRow = true;
        }

        temp = group->lastInGroup( firstRow + 1 );
        if ( temp != -1 ) {
            debug() << "--4";
            area1End = temp;
            removeGroupBelowFirstRow = true;
        }

        temp = group->firstInGroup( lastRow - 1 );
        if ( temp != -1 ) {
            debug() << "--5";
            area2Start = temp;
            removeGroupAboveLastRow = true;
        }

        temp = group->lastInGroup( belowLast );
        if ( temp != -1 ) {
            debug() << "--6";
            area2End = temp;
            removeGroupBelowLastRow = true;
        }

        if ( removeGroupAboveFirstRow )
        { group->removeGroup( aboveFirst ); debug() << "removing group at row: " <<  aboveFirst; }

        if ( removeGroupBelowFirstRow )
            { group->removeGroup( firstRow + 1 ); debug() << "removing group at row: " <<  firstRow + 1; }

        if ( removeGroupAboveLastRow )
            { group->removeGroup( lastRow -1 ); debug() << "removing group at row: " <<  lastRow - 1; }
        if ( removeGroupBelowLastRow )
        { group->removeGroup( belowLast );  debug() << "removing group at row: " <<  belowLast; }

        group->removeGroup( firstRow );
        group->removeGroup( lastRow );

        //if there is nothing left in album group, discard it.

        if ( group->subgroupCount() == 0 ) {
            //debug() << "empty...";
            delete m_albumGroups.take( itt.key() );

        }



    }




    if ( m_albumGroups.count() == 0 ) { // start from scratch
        debug() << "--1";
        area1Start = 0;
        area1End = m_items.count();
        area2Start = area1Start; // just to skip second pass

    }


    if ( area1Start == -1 ) {
        //debug() << "--2";
        area1Start = aboveFirst;
        area1End = belowLast;
        area2Start = area1Start;
    }

    if ( area1End == -1 )
        area1End = belowLast;

    if ( area1Start < 0 )
        area1Start = 0;
    if ( area1End > ( m_items.count() - 1 ) )
        area1End = m_items.count() - 1;

    if ( area2Start < 0 )
        area2Start = 0;
    if ( area2End > ( m_items.count() - 1 ) )
        area2End = m_items.count() - 1;


    // regroup the two affected areas

    if ( area1Start == area2Start ) //merge areas
        area1End = qMax( area1End, area2End );
    else if ( area1End == area2End ) {//merge areas
        area1Start = qMin( area1Start, area2Start );
        area2Start = area1Start;
    }


    debug() << "area1Start: " << area1Start << ", area1End: " << area1End;
    debug() << "area2Start: " << area2Start << ", area2End: " << area2End;

    //debug stuff
    debug() << "Groups before:";
    foreach( AlbumGroup * ag, m_albumGroups)
       ag->printGroupRows();



    if ( offsetMode != OffsetNone ) {

        int offsetFrom;
        int offsetTo;

        if ( offsetMode == OffsetBetween ) {
            offsetFrom = area1End + 1;
            offsetTo = area2Start - 1;
            // last but not least, change area1end and area2start to match new offsets
             if ( area1End != area2End ) {
                area1End += offset;
                area2Start += offset;
                debug() << "area1Start: " << area1Start << ", area1End: " << area1End;
                debug() << "area2Start: " << area2Start << ", area2End: " << area2End;
             }
        } else {
            offsetFrom = lastRow;
            offsetTo = ( m_items.count() - offset ) + 1;
        }

        QMapIterator< QString, AlbumGroup *> itt(m_albumGroups);
        while (itt.hasNext()) {

           itt.next();
           AlbumGroup * group = itt.value();
           group->offsetBetween( offsetFrom, offsetTo, offset);

        }


    }

    //debug stuff
    debug() << "Groups after offsetting:";
    foreach( AlbumGroup * ag, m_albumGroups)
       ag->printGroupRows();


    int i;
    for (  i = area1Start; ( i <= area1End ) && ( i < m_items.count() ); i++ )
    {

       //debug() << "i: " << i;

       Meta::TrackPtr track = m_items.at( i )->track();

       if ( !track->album() )
           continue;

       QString albumName;

       //not all tracks have a valid album ( radio stations for instance... )
       if ( !track->album().isNull() ) {
           albumName = track->album()->prettyName();
       }

       if ( m_albumGroups.contains( albumName ) && !albumName.isEmpty() ) {
           m_albumGroups[ albumName ]->addRow( i );
       } else {
            //debug() << "Create new group for album " << track->album()->name() ;
            AlbumGroup * newGroup = new AlbumGroup();
            newGroup->addRow( i );
            m_albumGroups.insert( albumName, newGroup );
       }
    }

    if ( ( area1Start == area2Start ) || area2Start == -1 ) {

        debug() << "Groups after:";
        foreach( AlbumGroup * ag, m_albumGroups)
            ag->printGroupRows();
        return;
    }

    for (  i = area2Start; i <= area2End; i++ )
    {

       //debug() << "i: " << i;

       Meta::TrackPtr track = m_items.at( i )->track();

       if ( !track->album() )
           continue;

       if ( m_albumGroups.contains( track->album()->prettyName() ) ) {
            m_albumGroups[ track->album()->prettyName() ]->addRow( i );
       } else {
            AlbumGroup * newGroup = new AlbumGroup();
            newGroup->addRow( i );
            m_albumGroups.insert( track->album()->prettyName(), newGroup );
       }
    }

    debug() << "Groups after:";
    foreach( AlbumGroup *ag, m_albumGroups)
       ag->printGroupRows();



    //make sure that a group containg playing track is expanded
    if ( m_activeRow != -1 ){
        if ( m_albumGroups.contains( m_items[ m_activeRow ]->track()->album()->prettyName() ) ) {
            m_albumGroups[ m_items[ m_activeRow ]->track()->album()->prettyName() ]->setCollapsed( m_activeRow,  false );
            debug() << "Here";
            emit( playlistGroupingChanged() );
      }
   }

   //reset();

}

void Playlist::Model::setCollapsed(int row, bool collapsed)
{
    //DEBUG_BLOCK
    m_albumGroups[ m_items[ row ]->track()->album()->prettyName() ]->setCollapsed( row,  collapsed );
    emit( playlistGroupingChanged() );
}



namespace The {
    Playlist::Model* playlistModel() { return Playlist::Model::s_instance; }
}

#include "PlaylistModel.moc"