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
 
#ifndef SERVICEPLUGINMANAGER_H
#define SERVICEPLUGINMANAGER_H

#include "servicebase.h"
#include "servicebrowser.h"

#include <QObject>

/**
A class to keep track of available service plugins and load them as needed

	@author 
*/
class ServicePluginManager : public QObject {
    Q_OBJECT
public:

    static ServicePluginManager * instance();

    ~ServicePluginManager();

    void setBrowser( ServiceBrowser * browser );

    /**
     * Collects the factories of all services that can be loaded
     */
    void collect();

    /**
     * Load any services that are configured to be loaded
     */
    void init();

    /**
     * The service settings has been changed... add, remove or reset any affected services
     */
    void settingsChanged();

    QMap< QString, ServiceFactory* > factories();
    QStringList loadedServices();

private:
    ServicePluginManager();

    static ServicePluginManager * m_instance;
    ServiceBrowser * m_serviceBrowser;
    QMap< QString, ServiceFactory* > m_factories;
    QStringList m_loadedServices;
    
private slots:
    void slotNewService( ServiceBase *newService);

   

};

#endif