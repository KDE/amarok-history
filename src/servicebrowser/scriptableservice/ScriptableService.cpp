/***************************************************************************
 *   Copyright (c) 2007  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "ScriptableService.h"

#include "ServiceBrowser.h"
#include "ScriptableServiceInfoParser.h"
#include "Amarok.h"
#include "debug.h"
#include "SearchWidget.h"

using namespace Meta;

ScriptableService::ScriptableService( const QString & name, AmarokProcIO * script )
    : ServiceBase( name )
    , m_polished( false )
    , m_name( name )
    , m_script ( script )
    , m_trackIdCounter( 0 )
    , m_albumIdCounter( 0 )
    , m_artistIdCounter( 0 )
    , m_genreIdCounter( 0 )
{
    m_collection = 0;
    m_bottomPanel->hide();

}

ScriptableService::~ ScriptableService()
{
    delete m_collection;
}

void ScriptableService::init( int levels, const QString & rootHtml, bool showSearchBar )
{
    m_levels = levels;
    m_rootHtml = rootHtml;
    setInfoParser( new ScriptableServiceInfoParser() );
    m_collection = new ScriptableServiceCollection( m_name, m_script );
    m_collection->setLevels( levels );

    if ( !showSearchBar )
        m_searchWidget->hide();
}

ServiceCollection * ScriptableService::collection()
{
    return m_collection;
}


int ScriptableService::insertItem( int level, int parentId, const QString & name, const QString & infoHtml, const QString & callbackData, const QString & playableUrl )
{
    DEBUG_BLOCK

    debug() << "level: " << level;
    debug() << "parentId: " << parentId;
    debug() << "name: " << name;
    debug() << "infoHtml: " << infoHtml;
    debug() << "callbackData: " << callbackData;
    debug() << "playableUrl: " << playableUrl;

    if ( ( level +1 > m_levels ) || level < 0 )
        return -1;

    switch ( level ) {

        case 0:
        {

            if ( !callbackData.isEmpty() ||  playableUrl.isEmpty() )
                return -1;
            
            ScriptableServiceTrack * track = new ScriptableServiceTrack( name );
            track->setAlbumId( parentId );
            track->setUrl( playableUrl );
            return addTrack( track );
            break;
            
        } case 1:
        {
            if ( callbackData.isEmpty() ||  !playableUrl.isEmpty() )
                return -1;

            ScriptableServiceAlbum * album = new ScriptableServiceAlbum( name );
            album->setCallbackString( callbackData );
            album->setArtistId( parentId );
            album->setDescription( infoHtml );
            return addAlbum( album );
            
        } case 2:
        {
            if ( callbackData.isEmpty() ||  !playableUrl.isEmpty() )
                return -1;

            ScriptableServiceArtist * artist = new ScriptableServiceArtist( name );
            artist->setCallbackString( callbackData );
            artist->setGenreId( parentId );
            artist->setDescription( infoHtml );
            return addArtist( artist );
            
        } case 3:
        {
            
            if ( callbackData.isEmpty() ||  !playableUrl.isEmpty() || parentId != -1 )
                return -1;

            ScriptableServiceGenre * genre = new ScriptableServiceGenre( name );
            genre->setCallbackString( callbackData );
            genre->setDescription( infoHtml );
            return addGenre( genre );
            
        } default:
            return -1;
    }
    
}


int ScriptableService::addTrack( ScriptableServiceTrack * track )
{

    int artistId = -1;
    int genreId = -1;

    TrackPtr trackPtr = TrackPtr( track );
    m_collection->addTrack( track->name(), trackPtr );

    m_trackIdCounter++;
    track->setId( m_trackIdCounter );

    
    int albumId = track->albumId();

    //handle albums
    if ( m_levels > 1 ) {

        if ( !m_ssAlbumIdMap.contains( albumId ) ){
            delete track;
            return -1;
        }

        ScriptableServiceAlbum * album = m_ssAlbumIdMap.value( albumId );
        track->setAlbum( album->prettyName() );
        track->setAlbum( AlbumPtr( album ) );
        album->addTrack( trackPtr );

        artistId = album->artistId();

     }

     if ( m_levels > 2 ) {

         if ( !m_ssArtistIdMap.contains( artistId ) ) {
             delete track;
             return -1;
         }

         ScriptableServiceArtist * artist = m_ssArtistIdMap.value( artistId );
         track->setArtist( artist->prettyName() );
         track->setArtist( ArtistPtr( artist ) );
         artist->addTrack( trackPtr );

         genreId = artist->genreId();
     }

     if ( m_levels == 4) {
         
         if ( !m_ssGenreIdMap.contains( genreId ) ) {
             delete track;
             return -1;
         }

         ScriptableServiceGenre * genre = m_ssGenreIdMap.value( genreId );
         track->setGenre( genre->prettyName() );
         track->setGenre( GenrePtr( genre ) );
         genre->addTrack( trackPtr );
         
     }

    m_ssTrackIdMap.insert( m_trackIdCounter, track );
    m_collection->acquireWriteLock();
    m_collection->addTrack( trackPtr->name(), trackPtr );
    m_collection->releaseLock();

    //m_collection->emitUpdated();

    return m_trackIdCounter;
}

int ScriptableService::addAlbum( ScriptableServiceAlbum * album )
{
    int artistId = album->artistId();
    if ( m_levels > 2 && !m_ssArtistIdMap.contains( artistId ) ) {
        delete album;
        return -1;
    }

    album->setAlbumArtist( ArtistPtr( m_ssArtistIdMap.value( artistId ) ) );

    AlbumPtr albumPtr = AlbumPtr( album );
    m_albumIdCounter++;
    album->setId( m_albumIdCounter );
    m_ssAlbumIdMap.insert( m_albumIdCounter, album );
    m_collection->acquireWriteLock();
    m_collection->addAlbum( album->name(), albumPtr );
    m_collection->releaseLock();
    //m_collection->emitUpdated();
    return m_albumIdCounter;
}

int ScriptableService::addArtist( Meta::ScriptableServiceArtist * artist )
{
    int genreId = artist->genreId();
    if (  m_levels > 3 && !m_ssGenreIdMap.contains( genreId ) ) {
        delete artist;
        return -1;
    }
    
    ArtistPtr artistPtr = ArtistPtr( artist );
    m_artistIdCounter++;
    artist->setId( m_artistIdCounter );
    m_ssArtistIdMap.insert( m_artistIdCounter, artist );
    m_collection->acquireWriteLock();
    m_collection->addArtist( artist->name(), artistPtr );
    m_collection->releaseLock();

    return m_artistIdCounter;

}

int ScriptableService::addGenre( Meta::ScriptableServiceGenre * genre )
{
    GenrePtr genrePtr = GenrePtr( genre );
    m_genreIdCounter++;

    debug() << "adding genre: " << genre->name() << ", with id: " << m_genreIdCounter;
    
    genre->setId( m_genreIdCounter );
    m_ssGenreIdMap.insert( m_genreIdCounter, genre );
    m_collection->acquireWriteLock();
    m_collection->addGenre( genre->name(), genrePtr );
    m_collection->releaseLock();

    return m_genreIdCounter;
}

void ScriptableService::donePopulating( int parentId )
{
    DEBUG_BLOCK
    m_collection->donePopulating( parentId );
}

void ScriptableService::polish()
{

    if ( !m_polished ) {
        QList<int> viewLevels;

        switch ( m_levels ) {
            case 1:
                break;
            case 2:
                viewLevels << CategoryId::Album;
                break;
            case 3:
                viewLevels << CategoryId::Artist << CategoryId::Album;
                break;
            case 4:
                viewLevels << CategoryId::Genre << CategoryId::Artist << CategoryId::Album;
                break;
            default:
                return;
        }

        m_contentView->setModel( new SingleCollectionTreeItemModel( m_collection, viewLevels ) );
        m_polished = true;

    }

    infoChanged( m_rootHtml );
}






#include "ScriptableService.moc"

