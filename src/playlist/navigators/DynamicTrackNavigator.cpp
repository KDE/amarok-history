/***************************************************************************
 * copyright            : (C) 2008 Daniel Jones <danielcjones@gmail.com>
 *
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

#include "DynamicTrackNavigator.h"

#include "Debug.h"
#include "DynamicModel.h"
#include "DynamicPlaylist.h"
#include "Meta.h"
#include "playlist/PlaylistActions.h"
#include "playlist/PlaylistModel.h"

#include <QMutexLocker>


Playlist::DynamicTrackNavigator::DynamicTrackNavigator( Dynamic::DynamicPlaylistPtr p ) : m_playlist(p)
{
    connect( m_playlist.data(), SIGNAL(tracksReady( Meta::TrackList )), SLOT(receiveTracks(Meta::TrackList)) );
    connect( Model::instance(), SIGNAL(modelReset()), SLOT(repopulate()) );
    connect( PlaylistBrowserNS::DynamicModel::instance(), SIGNAL(activeChanged()), SLOT(activePlaylistChanged()) );
    connect( Model::instance(), SIGNAL(activeTrackChanged(quint64)), SLOT(trackChanged()) );
}


Playlist::DynamicTrackNavigator::~DynamicTrackNavigator()
{
    m_playlist->requestAbort();
}


void
Playlist::DynamicTrackNavigator::requestNextTrack()
{
    DEBUG_BLOCK

    if( m_waitingForUserNext || m_waitingForNext )
        return;

    int updateRow = Model::instance()->activeRow() + 1;
    int rowCount = Model::instance()->rowCount();
    int upcomingCountLag = m_playlist->upcomingCount() - (rowCount - updateRow);

    if( upcomingCountLag > 0 )
    {
        m_waitingForNext = true;
        m_playlist->requestTracks( upcomingCountLag );
    }

    The::playlistActions()->setNextRow( updateRow );
}

void
Playlist::DynamicTrackNavigator::requestUserNextTrack()
{
    DEBUG_BLOCK

    if( m_waitingForUserNext || m_waitingForNext )
        return;

    int updateRow = Model::instance()->activeRow() + 1;
    int rowCount = Model::instance()->rowCount();
    int upcomingCountLag = m_playlist->upcomingCount() - (rowCount - updateRow);

    if( upcomingCountLag > 0 )
    {
        m_waitingForUserNext = true;
        m_playlist->requestTracks( upcomingCountLag );
    }

    The::playlistActions()->setUserNextRow( updateRow );
}

void
Playlist::DynamicTrackNavigator::requestLastTrack()
{
    if( m_waitingForUserNext || m_waitingForNext )
        return;

    int updateRow = Model::instance()->activeRow() - 1;
    The::playlistActions()->setPrevRow( updateRow );
}

void
Playlist::DynamicTrackNavigator::receiveTracks( Meta::TrackList tracks )
{
    DEBUG_BLOCK

    Controller::instance()->insertOptioned( tracks, Append );

    if( m_waitingForNext )
    {
        m_waitingForNext = false;
        int newRow = Model::instance()->activeRow() + 1;
        The::playlistActions()->setNextRow( newRow );
    }
    
    if( m_waitingForUserNext )
    {
        m_waitingForUserNext = false;
        int newRow = Model::instance()->activeRow() + 1;
        The::playlistActions()->setUserNextRow( newRow );
    }
}


void
Playlist::DynamicTrackNavigator::appendUpcoming()
{
    DEBUG_BLOCK

    int updateRow = Model::instance()->activeRow() + 1;
    int rowCount = Model::instance()->rowCount();
    int upcomingCountLag = m_playlist->upcomingCount() - (rowCount - updateRow);

    if( upcomingCountLag > 0 )
        m_playlist->requestTracks( upcomingCountLag );
}

void
Playlist::DynamicTrackNavigator::removePlayed()
{
    int activeRow = Model::instance()->activeRow();
    if( activeRow > m_playlist->previousCount() )
    {
        Controller::instance()->removeRows( 0, activeRow - m_playlist->previousCount() );
    }
}

void
Playlist::DynamicTrackNavigator::activePlaylistChanged()
{
    DEBUG_BLOCK

    Dynamic::DynamicPlaylistPtr newPlaylist =
        PlaylistBrowserNS::DynamicModel::instance()->activePlaylist();

    if( newPlaylist == m_playlist )
        return;

    m_playlist->requestAbort();
    QMutexLocker locker( &m_mutex );

    m_playlist = newPlaylist;

    connect( m_playlist.data(), SIGNAL(tracksReady( Meta::TrackList )),
            SLOT(receiveTracks(Meta::TrackList)) );
}

void
Playlist::DynamicTrackNavigator::trackChanged() {
    appendUpcoming();
    removePlayed();
}


void
Playlist::DynamicTrackNavigator::repopulate()
{
    if( !m_mutex.tryLock() )
        return;

    int start = Model::instance()->activeRow() + 1;
    if( start < 0 )
        start = 0;
    int span = Model::instance()->rowCount() - start;

    if( span > 0 )
        Controller::instance()->removeRows( start, span );

    m_playlist->recalculate();
    appendUpcoming();

    m_mutex.unlock();
}