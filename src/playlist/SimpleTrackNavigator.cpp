/***************************************************************************
 * copyright         : (C) 2008 Daniel Caleb Jones <danielcjones@gmail.com>
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

#include "PlaylistModel.h"
#include "SimpleTrackNavigator.h"

Playlist::SimpleTrackNavigator::SimpleTrackNavigator( Model* model )
    : TrackNavigator( model )
{
}
    
int
Playlist::SimpleTrackNavigator::lastRow()
{
    int updateRow = m_playlistModel->activeRow() - 1;
    return m_playlistModel->rowExists( updateRow ) ? updateRow : -1;
}

void
Playlist::SimpleTrackNavigator::requestNextRow()
{
    m_playlistModel->setNextRow( nextRow() );
}

void
Playlist::SimpleTrackNavigator::requestUserNextRow()
{
    m_playlistModel->setUserNextRow( userNextRow() );
}

void
Playlist::SimpleTrackNavigator::requestLastRow()
{
    m_playlistModel->setPrevRow( lastRow() );
}
