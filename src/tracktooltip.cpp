/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
*/

/*
  tracktooltip.cpp
  begin:     Tue 10 Feb 2004
  copyright: (C) 2004 by Christian Muehlhaeuser
  email:     chris@chris.de
*/

#include "tracktooltip.h"
#include "metabundle.h"
#include "collectiondb.h"
#include <qtooltip.h>


void TrackToolTip::add( QWidget * widget, const MetaBundle & tags )
{
    CollectionDB db;
    QString tipBuf;
    QStringList left, right;
    const QString tableRow = "<tr><td width=70 align=right>%1:</td><td align=left>%2</td></tr>";

    QString image = db.getImageForAlbum( tags.artist(), tags.album() );
    if ( !image )
        image = db.getImageForPath( tags.url().directory() );

    left  << i18n( "Title" ) << i18n( "Artist" ) << i18n( "Album" ) << i18n( "Length" ) << i18n( "Bitrate" ) << i18n( "Samplerate" );
    right << tags.title() << tags.artist() << tags.album() << tags.prettyLength() << tags.prettyBitrate() << tags.prettySampleRate();

    //NOTE it seems to be necessary to <center> each element indivdually
    tipBuf += "<center><b>amaroK</b></center><table cellpadding='2' cellspacing='2' align='center'><tr>";

    if ( !image.isEmpty() )
        tipBuf += QString( "<td><table cellpadding='0' cellspacing='0'><tr><td>"
                           "<img width='80' height='80' src='%1'>"
                           "</td></tr></table></td>" ).arg( image );

    tipBuf += "<td><table cellpadding='0' cellspacing='0'>";

    for( uint x = 0; x < left.count(); ++x )
        if ( !right[x].isEmpty() )
            tipBuf += tableRow.arg( left[x] ).arg( right[x] );

    tipBuf += "</table></td>";
    tipBuf += "</tr></table></center>";

    QToolTip::add( widget, tipBuf );
}
