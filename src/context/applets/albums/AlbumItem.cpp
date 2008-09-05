/*******************************************************************************
* copyright              : (C) 2008 Seb Ruiz <ruiz@kde.org>                    *
*                                                                              *
********************************************************************************/

/*******************************************************************************
*                                                                              *
*   This program is free software; you can redistribute it and/or modify       *
*   it under the terms of the GNU General Public License as published by       *
*   the Free Software Foundation; either version 2 of the License, or          *
*   (at your option) any later version.                                        *
*                                                                              *
********************************************************************************/

#include "AlbumItem.h"

#include <KLocale>

#include <QIcon>
#include <QPixmap>

void
AlbumItem::setAlbum( Meta::AlbumPtr albumPtr )
{
    if( m_album )
        unsubscribeFrom( m_album );
    m_album = albumPtr;
    subscribeTo( m_album );

    metadataChanged( m_album.data() );
}

void
AlbumItem::setIconSize( int iconSize )
{
    static const int padding = 10;

    m_iconSize = iconSize;

    QSize size = sizeHint();
    size.setHeight( iconSize + padding*2 );
    setSizeHint( size );
}

void
AlbumItem::metadataChanged( Meta::Album *album )
{
    QString albumName = album->name();
    albumName = albumName.isEmpty() ? i18n("Unknown") : albumName;

    QString displayText = albumName;
    Meta::TrackList tracks = album->tracks();

    QString year;
    if( !tracks.isEmpty() )
    {
        Meta::TrackPtr first = tracks.first();
        year = first->year()->name();
        // do some sanity checking
        if( year.length() != 4 )
            year = QString();
    }

    if( !year.isEmpty() )
        displayText += QString( " (%1)" ).arg( year );

    QString trackCount = i18np( "%1 track", "%1 tracks", album->tracks().size() );
    displayText += "\n" + trackCount;

    setText( displayText );

    QPixmap cover = album->image( m_iconSize );
    setIcon( QIcon( cover ) );
}

