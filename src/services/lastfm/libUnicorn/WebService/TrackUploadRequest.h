/***************************************************************************
 *   Copyright (C) 2005 - 2007 by                                          *
 *      Last.fm Ltd <client@last.fm>                                       *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.          *
 ***************************************************************************/

#ifndef TRACKUPLOADREQUEST_H
#define TRACKUPLOADREQUEST_H

#include "Request.h"

/** @author <petgru@last.fm> */

class TrackUploadRequest : public Request
{
    Q_OBJECT
    
    PROP_GET_SET( TrackInfo, track, Track )
    PROP_GET_SET( QString, label, Label )
    
    public:
        TrackUploadRequest();
        
        virtual void start();
        //virtual void success( QByteArray data );
};

#endif //TRACKUPLOADREQUEST_H
