/*
 *  Copyright (c) 2008 Casey Link <unnamedrambler@gmail.com>
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
#include "Mp3tunesServiceCollectionLocation.h"
#include "Debug.h"
using namespace Meta;

Mp3tunesServiceCollectionLocation::Mp3tunesServiceCollectionLocation( Mp3tunesServiceCollection const *parentCollection )
    : ServiceCollectionLocation()
    , m_collection( const_cast<Mp3tunesServiceCollection*>(  parentCollection ) )
{}

Mp3tunesServiceCollectionLocation::~Mp3tunesServiceCollectionLocation()
{
    DEBUG_BLOCK
}

QString Mp3tunesServiceCollectionLocation::prettyLocation() const
{
    return "MP3Tunes Locker";
}

bool Mp3tunesServiceCollectionLocation::isWriteable() const
{
    return true;
}

bool Mp3tunesServiceCollectionLocation::isOrganizable() const
{
    return false;
}
bool Mp3tunesServiceCollectionLocation::remove( const Meta::TrackPtr &track ) 
{
    //TODO
    return false;
}
void Mp3tunesServiceCollectionLocation::copyUrlsToCollection ( 
        const QMap<Meta::TrackPtr, KUrl> &sources ) 
{
    //TODO 
    //do not forget to call slotCopyOperationFinished() when you are done copying
    //the files.
}