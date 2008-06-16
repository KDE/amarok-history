/*
   Mostly taken from Daap code:
   Copyright (C) 2006 Ian Monroe <ian@monroe.nu>
   Copyright (C) 2006 Seb Ruiz <ruiz@kde.org>
   Copyright (C) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#define DEBUG_PREFIX "MediaDeviceCollection"

#include "MediaDeviceCollection.h"
#include "MediaDeviceMeta.h"

#include "amarokconfig.h"
#include "Debug.h"
#include "MediaDeviceCache.h"
#include "MemoryQueryMaker.h"

//solid specific includes
#include <solid/devicenotifier.h>
#include <solid/device.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>

//#include <QDir>
#include <QStringList>


AMAROK_EXPORT_PLUGIN( MediaDeviceCollectionFactory )

MediaDeviceCollectionFactory::MediaDeviceCollectionFactory()
    : CollectionFactory()
{
    //nothing to do
}

MediaDeviceCollectionFactory::~MediaDeviceCollectionFactory()
{
  //    delete m_browser;
}

void
MediaDeviceCollectionFactory::init()
{
  DEBUG_BLOCK

    return;
}

//MediaDeviceCollection

MediaDeviceCollection::MediaDeviceCollection( const QString &mountPoint )
    : Collection()
    , MemoryCollection()
    , m_mountPoint(mountPoint)
{
    DEBUG_BLOCK
}

MediaDeviceCollection::~MediaDeviceCollection()
{

}

void
MediaDeviceCollection::startFullScan()
{
    //ignore
}

QueryMaker*
MediaDeviceCollection::queryMaker()
{
    return new MemoryQueryMaker( this, collectionId() );
}

QString
MediaDeviceCollection::collectionId() const
{
     return "filler";
}

QString
MediaDeviceCollection::prettyName() const
{
    return "prettyfiller";
}

#include "MediaDeviceCollection.moc"
