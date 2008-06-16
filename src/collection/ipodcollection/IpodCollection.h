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

#ifndef IPODCOLLECTION_H
#define IPODCOLLECTION_H

extern "C" {
  #include <gpod/itdb.h>
}

#include "Collection.h"
#include "MemoryCollection.h"

#include <QtGlobal>

class IpodCollection;

class IpodCollectionFactory : public CollectionFactory
{
    Q_OBJECT
    public:
        IpodCollectionFactory();
        virtual ~IpodCollectionFactory();

        virtual void init();

    private:

    private slots:
    
    private:

};

class IpodCollection : public Collection, public MemoryCollection
{
    Q_OBJECT
    public:

        IpodCollection( const QString &mountPoint);
        virtual ~IpodCollection();

        virtual void startFullScan();
        virtual QueryMaker* queryMaker();

        virtual QString collectionId() const;
        virtual QString prettyName() const;        

    signals:
        void collectionReady();

    public slots:

    private slots:

    private:
    /* test if libgpod works */
    // ipod database
    Itdb_iTunesDB    *m_itdb;
    QString           m_mountPoint;


};

#endif