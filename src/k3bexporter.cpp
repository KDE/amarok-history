/***************************************************************************
    begin                : Mon May 31 2004
    copyright            : (C) 2004 by Michael Pyne
                           (c) 2004 by Pierpaolo Di Panfilo
    email                : michael.pyne@kdemail.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "collectiondb.h"
#include "k3bexporter.h"
#include "playlist.h"
#include "playlistitem.h"

#include <kprocess.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kstandarddirs.h>

#include <qcstring.h>
#include <qstringlist.h>

#include <dcopref.h>
#include <dcopclient.h>


K3bExporter *K3bExporter::s_instance = 0;


K3bExporter::K3bExporter()
{
}

bool K3bExporter::isAvailable() //static
{
    return !KStandardDirs::findExe( "k3b" ).isNull();
}

void K3bExporter::exportTracks( const KURL::List &urls, int openmode )
{
    if( urls.empty() )
        return;

    DCOPClient *client = DCOPClient::mainClient();
    QCString appId, appObj;
    QByteArray data;

    if( openmode == -1 )
        //ask to open a data or an audio cd project
        openmode = openMode();

    if( !client->findObject( "k3b-*", "K3bInterface", "", data, appId, appObj) )
        exportViaCmdLine( urls, openmode );
    else {
        DCOPRef ref( appId, appObj );
        exportViaDCOP( urls, ref, openmode );
    }
}

void K3bExporter::exportCurrentPlaylist( int openmode )
{
    Playlist::instance()->burnPlaylist( openmode );
}

void K3bExporter::exportSelectedTracks( int openmode )
{
    Playlist::instance()->burnSelectedTracks( openmode );
}

void K3bExporter::exportAlbum( const QString &album, int openmode )
{
    CollectionDB db;
    QStringList values;

    QString albumId = QString::number( db.getValueID( "album", album, false ) );
    db.execSql( "SELECT url FROM tags "
                       "WHERE album = " + albumId + " "
                       "ORDER BY track;", &values );

    if( values.count() ) {
        KURL::List urls;

        for( uint i=0; i < values.count(); i++ )
            urls << KURL( values[i] );

        exportTracks( urls, openmode );
    }
}

void K3bExporter::exportArtist( const QString &artist, int openmode )
{
    CollectionDB db;
    QStringList values;

    QString artistId = QString::number( db.getValueID( "artist", artist, false ) );
    db.execSql( "SELECT url FROM tags "
                       "WHERE artist = " + artistId + " "
                       "ORDER BY title;", &values );

    if( values.count() ) {
        KURL::List urls;

        for( uint i=0; i < values.count(); i++ )
            urls << KURL( values[i] );

        exportTracks( urls, openmode );
    }
}

void K3bExporter::exportViaCmdLine( const KURL::List &urls, int openmode )
{
    QCString cmdOption;

    switch( openmode ) {
    case AudioCD:
        cmdOption = "--audiocd";
        break;

    case DataCD:
        cmdOption = "--datacd";
        break;

    case Abort:
        return;
    }

    KProcess *process = new KProcess;

    *process << "k3b";
    *process << cmdOption;

    KURL::List::ConstIterator it;
    for( it = urls.begin(); it != urls.end(); ++it )
        *process << (*it).path();

    if( !process->start( KProcess::DontCare ) )
        KMessageBox::error( 0, i18n("Unable to start K3b.") );
}

void K3bExporter::exportViaDCOP( const KURL::List &urls, DCOPRef &ref, int openmode )
{
    QValueList<DCOPRef> projectList;
    DCOPReply projectListReply = ref.call("projects()");

    if( !projectListReply.get<QValueList<DCOPRef> >(projectList, "QValueList<DCOPRef>") ) {
        DCOPErrorMessage();
        return;
    }

    if( projectList.count() == 0 && !startNewK3bProject(ref, openmode) )
        return;

    if( !ref.send( "addUrls(KURL::List)", DCOPArg(urls, "KURL::List") ) ) {
        DCOPErrorMessage();
        return;
    }
}

void K3bExporter::DCOPErrorMessage()
{
    KMessageBox::error( 0, i18n("There was a DCOP communication error with K3b."));
}

bool K3bExporter::startNewK3bProject( DCOPRef &ref, int openmode )
{
    QCString request;
    //K3bOpenMode mode = openMode();

    switch( openmode ) {
    case AudioCD:
        request = "createAudioCDProject()";
        break;

    case DataCD:
        request = "createDataCDProject()";
        break;

    case Abort:
        return false;
    }

    KMessageBox::sorry(0,request);
    if( !ref.send( request ) ) {
        DCOPErrorMessage();
        return false;
    }

    return true;
}

K3bExporter::K3bOpenMode K3bExporter::openMode()
{
    int reply = KMessageBox::questionYesNoCancel(
        0,
        i18n("Create an audio mode CD suitable for CD players, or a data "
             "mode CD suitable for computers and other digital music "
             "players?"),
        i18n("Create K3b Project"),
        i18n("Audio Mode"),
        i18n("Data Mode")
    );

    switch(reply) {
    case KMessageBox::Cancel:
        return Abort;

    case KMessageBox::No:
        return DataCD;

    case KMessageBox::Yes:
        return AudioCD;
    }

    return Abort;
}

