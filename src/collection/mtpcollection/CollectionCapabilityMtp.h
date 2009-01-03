/*
   Copyright (C) 2008 Alejandro Wainzinger <aikawarazuni@gmail.com>

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

#ifndef COLLECTIONCAPABILITYMTP_H
#define COLLECTIONCAPABILITYMTP_H

#include "Meta.h"

#include "meta/CollectionCapability.h"

class MtpCollection;

namespace Meta
{

class CollectionCapabilityMtp : public CollectionCapability
{

    Q_OBJECT

    public:
        CollectionCapabilityMtp( MtpCollection *coll );

        virtual QList<PopupDropperAction *>  collectionActions( QueryMaker *qm );

        // NOTE: NYI
        virtual QList<PopupDropperAction *>  collectionActions( const TrackList tracklist );

    private:
        MtpCollection *m_coll;


};

}

#endif