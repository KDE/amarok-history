/*
 *  Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>
 *                2007 Nikolaj Hald Nielsen <nhnFreespirit@gamil.com>
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
 
#include "ServicePluginManager.h"
#include "pluginmanager.h"

#include <kservice.h>

ServicePluginManager * ServicePluginManager::m_instance = 0;

ServicePluginManager * ServicePluginManager::instance()
{
    if ( m_instance == 0 )
        m_instance = new ServicePluginManager();

    return m_instance;
}



ServicePluginManager::ServicePluginManager( )
    : QObject()
    , m_serviceBrowser( 0 )
{
    collect();
}


ServicePluginManager::~ServicePluginManager()
{
}

void ServicePluginManager::setBrowser( ServiceBrowser * browser ) {
    m_serviceBrowser = browser;
}

void ServicePluginManager::collect()
{
    DEBUG_BLOCK
    KService::List plugins = PluginManager::query( "[X-KDE-Amarok-plugintype] == 'service'" );
    debug() << "Received [" << QString::number( plugins.count() ) << "] collection plugin offers";
    foreach( KService::Ptr service, plugins )
    {
        Amarok::Plugin *plugin = PluginManager::createFromService( service );
        if ( plugin )
        {
            debug() << "Got hold of a valid plugin";
            ServiceFactory* factory = dynamic_cast<ServiceFactory*>( plugin );
            if ( factory )
            {
                debug() << "Got hold of a valid factory";
                m_factories.insert( factory->name(), factory );
                connect( factory, SIGNAL( newService( ServiceBase * ) ), this, SLOT( slotNewService( ServiceBase * ) ) );
            }
            else
            {
                debug() << "Plugin has wrong factory class";
                continue;
            }
        } else {
            debug() << "bad plugin";
            continue;
        }
    }
}

void ServicePluginManager::init()
{

    foreach( ServiceFactory* factory,  m_factories.values() ) {

        //check if this service is enabled
        QString pluginName = factory->info().pluginName();

        debug() << "PLUGIN CHECK: " << pluginName;
        if ( factory->config().readEntry( pluginName + "Enabled", true ) )
        {
            factory->init();
            m_loadedServices << pluginName;
        }
    }

}

void ServicePluginManager::slotNewService(ServiceBase * newService)
{
    DEBUG_BLOCK
    m_serviceBrowser->addService( newService );
}

QMap< QString, ServiceFactory * > ServicePluginManager::factories()
{
    return m_factories;
}

void ServicePluginManager::settingsChanged()
{

    //for now, just delete and reload everything....
    QMap<QString, ServiceBase *> activeServices =  m_serviceBrowser->services();
    QList<QString> names = activeServices.keys();

    foreach( ServiceFactory * factory,  m_factories ) {
        factory->clearActiveServices();
    }
    
    foreach( QString serviceName, names ) {
        m_serviceBrowser->removeService( serviceName );
        delete activeServices.value( serviceName );
    }

    foreach( QString serviceName, names ) {
        m_serviceBrowser->removeService( serviceName );
    }

    m_loadedServices.clear();
    
    init();


    //way too advanced for now and does not solve the issue of some services being loaded multiple times
    //based on their config
    /*QMap<QString, ServiceBase *> activeServices =  m_serviceBrowser->services();
    QList<QString> names = activeServices.keys();
    
    foreach( ServiceFactory* factory,  m_factories.values() ) {

        //check if this service is enabled in the config
        QString pluginName = factory->info().pluginName();

        debug() << "PLUGIN CHECK: " << pluginName;
        if ( factory->config().readEntry( pluginName + "Enabled", true ) ) {

            //check if it is enabled but not loaded
            if ( !names.contains( pluginName ) ) {
                //load it
                factory->init();
            } else {
                //loaded and enabled, so just reset it
                activeServices[pluginName]->reset();
            }
        } else {
            //check if it is loaded but needs to be disabled
            if ( names.contains( pluginName ) ) {
                //unload it
                m_serviceBrowser->removeService( pluginName );
            }
        }
    }*/

    
}

QStringList ServicePluginManager::loadedServices()
{
    return m_loadedServices;
}




#include "ServicePluginManager.moc"

