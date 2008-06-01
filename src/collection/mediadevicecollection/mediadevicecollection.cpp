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

#include "mediadevicecollection.h"

#include "amarokconfig.h"
#include "mediadevicemeta.h"
#include "Debug.h"
#include "MemoryQueryMaker.h"


//#include <QStringList>


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
}

/*
void
MediaDeviceCollectionFactory::foundMediaDevice( DNSSD::RemoteService::Ptr service )
{
    DEBUG_BLOCK

    connect( service.data(), SIGNAL( resolved( bool ) ), this, SLOT( resolvedMediaDevice( bool ) ) );
    service->resolveAsync();
}
*/
/*
void
MediaDeviceCollectionFactory::resolvedMediaDevice( bool success )
{
    DEBUG_BLOCK
    const DNSSD::RemoteService* service =  dynamic_cast<const DNSSD::RemoteService*>(sender());
    if( !success || !service ) return;
    debug() << service->serviceName() << ' ' << service->hostName() << ' ' << service->domain() << ' ' << service->type();

    QString ip = resolve( service->hostName() );
    if( ip == "0" || m_collectionMap.contains(serverKey( service )) ) //same server from multiple interfaces
        return;

    MediaDeviceCollection *coll = new MediaDeviceCollection( service->hostName(), ip, service->port() );
    connect( coll, SIGNAL( collectionReady() ), SLOT( slotCollectionReady() ) );
    connect( coll, SIGNAL( remove() ), SLOT( slotCollectionDownloadFailed() ) );
    m_collectionMap.insert( serverKey( service ), coll );
}
*/

//MediaDeviceCollection

MediaDeviceCollection::MediaDeviceCollection()
    : Collection()
    , MemoryCollection()
{
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
  // TODO:
    // this will probably be taken care of by subclasses, due to nature of media devices
    return "filler";
}

QString
MediaDeviceCollection::prettyName() const
{
  // TODO:
    // this will probably be taken care of by subclasses, due to nature of media devices
    return "prettyfiller";
}

#include "mediadevicecollection.moc"
