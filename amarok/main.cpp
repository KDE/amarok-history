/***************************************************************************
                         main.cpp  -  description
                            -------------------
   begin                : Mit Okt 23 14:35:18 CEST 2002
   copyright            : (C) 2002 by Mark Kretschmann
   email                :
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "playerapp.h"

#include <qcstring.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kuniqueapplication.h>
#include <kdebug.h>
#include <kurl.h>


PlayerApp *pApp;

static const char *description = I18N_NOOP( "A media player for KDE" );

static KCmdLineOptions options[] =
    {
        { "+[URL]", I18N_NOOP( "Files/URLs to Open" ), 0 },
        { "e", I18N_NOOP( "Enqueue Files/URLs" ), 0 },
        { "s", I18N_NOOP( "Stop current song" ), 0 },
        { "p", I18N_NOOP( "Start playing current playlist" ), 0 },
        { "r", I18N_NOOP( "Skip backwards in playlist" ), 0 },
        { "f", I18N_NOOP( "Skip forward in playlist" ), 0 },
        { "playlist <file>", I18N_NOOP( "Open a Playlist" ), 0 },
        { 0, 0, 0 }
    };

int main( int argc, char *argv[] )
{
    KAboutData aboutData( "amarok", I18N_NOOP( "amaroK" ),
                          APP_VERSION, description, KAboutData::License_GPL,
                          "(c) 2002-2003, Mark Kretschmann", 0, "http://amarok.sourceforge.net", "" );

    aboutData.addAuthor( "Mark Kretschmann", "Developer, Maintainer", "markey@web.de" );
    aboutData.addCredit( "Markus A. Rykalski", "Graphics", "exxult@exxult.de" );
    aboutData.addCredit( "Roman Becker", "Graphics", "roman@formmorf.de", "http://www.formmorf.de" );
    aboutData.addCredit( "Jarkko Lehti", "Tester, Irc channel operator", "grue@iki.fi" );
    aboutData.addCredit( "Whitehawk Stormchaser", "Tester, Patches", "zerokode@gmx.net" );
    aboutData.addCredit( "The Noatun Authors", "Code Inspiration", 0, "http://noatun.kde.org" );

    KCmdLineArgs::init( argc, argv, &aboutData );
    KCmdLineArgs::addCmdLineOptions( options );   // Add our own options.
    PlayerApp::addCmdLineOptions();

    PlayerApp app;

    //     if (app.isRestored())
    //     {
    //         RESTORE(PlayerApp);
    //     }
    //     else
    //     {
    //        PlayerApp *pPlayerApp = new PlayerApp();
    //        app.setMainWidget( pPlayerApp );
    //        pPlayerApp->show();
    //     }

    return app.exec();
}
