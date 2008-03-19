/***************************************************************************
 * copyright     : (C) 2007 Dan Meltzer <hydrogen@notyetimplemented.com    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2        *
 *   as published by the Free Software Foundation.                         *
 ***************************************************************************/

#include "RepeatTrackNavigator.h"


#include "enginecontroller.h"
#include "PlaylistModel.h"

using namespace Playlist;

void
RepeatTrackNavigator::advanceTrack()
{
    if( !m_previousTrack || (m_previousTrack != m_playlistModel->activeTrack() ) ) // we need to repeat
    {
        setCurrentTrack( m_playlistModel->activeRow() );
        m_previousTrack = m_playlistModel->activeTrack();
    }
    else {
        if( m_previousTrack == m_playlistModel->activeTrack() ) // We already repeated, advance
        {
            int updateRow = m_playlistModel->activeRow() + 1;
            if( updateRow < m_playlistModel->rowCount() )
            {
                setCurrentTrack( updateRow );
            }
        }
    }
}

void
RepeatTrackNavigator::userAdvanceTrack()
{
    int updateRow = m_playlistModel->activeRow() + 1;
    if( updateRow < m_playlistModel->rowCount() )
    {
        setCurrentTrack( updateRow );
    }
}

