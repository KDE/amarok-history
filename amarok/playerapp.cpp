/***************************************************************************
                      playerapp.cpp  -  description
                         -------------------
begin                : Mit Okt 23 14:35:18 CEST 2002
copyright            : (C) 2002 by Mark Kretschmann
email                : markey@web.de
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "amarokconfig.h"
#include "amarokconfigdialog.h"
#include "amarokdcophandler.h"
#include "amaroksystray.h"
#include "browserwin.h"
#include "effectwidget.h"
#include "enginebase.h"
#include "enginecontroller.h"
#include "metabundle.h"
#include "osd.h"
#include "playerapp.h"
#include "playerwidget.h"
#include "playlisttooltip.h"     //engineNewMetaData()
#include "plugin.h"
#include "pluginmanager.h"
#include "threadweaver.h"        //restoreSession()
#include "socketserver.h" 

#include <kaboutdata.h>          //initCliArgs()
#include <kaction.h>
#include <kcmdlineargs.h>
#include <kconfig.h>
#include <kconfigdialog.h>
#include <kdebug.h>
#include <kglobalaccel.h>
#include <kjsembed/jsconsolewidget.h>
#include <kjsembed/kjsembedpart.h>
#include <kkeydialog.h>          //slotConfigShortcuts()
#include <klocale.h>
#include <kmessagebox.h>         //applySettings()
#include <kshortcut.h>
#include <kstandarddirs.h>
#include <kstartupinfo.h>        //handleLoaderArgs()
#include <ktip.h>
#include <kurl.h>
#include <kwin.h>                //eventFilter()

#include <qcstring.h>            //initIpc()
#include <qfile.h>               //initEngine()
#include <qpixmap.h>             //QPixmap::setDefaultOptimization()
#include <qserversocket.h>       //initIpc()
#include <qsocketnotifier.h>     //initIpc()
#include <qtooltip.h>            //adding tooltip to systray

#include <unistd.h>              //initIpc()
#include <sys/socket.h>          //initIpc()
#include <sys/un.h>              //initIpc()

using namespace KJSEmbed;

PlayerApp::PlayerApp()
        : KApplication()
        , m_pActionCollection( 0 )
        , m_pGlobalAccel( new KGlobalAccel( this ) )
        , m_pDcopHandler( new amaroK::DcopHandler )
        , m_pTray( 0 )
        , m_pOSD( new amaroK::OSD() )
        , m_sockfd( -1 )
        , m_showBrowserWin( false )
{
    //TODO readConfig and applySettings first
    //     reason-> create Engine earlier, so we can restore session asap to get playlist loaded by
    //     the time amaroK is visible

    setName( "amarok" );
    pApp = this; //global

    QPixmap::setDefaultOptimization( QPixmap::MemoryOptim );

    new Vis::SocketServer( this );
    initBrowserWin(); //must go first as it creates the action collection
    initPlayerWidget();

    //we monitor for close, hide and show events
    m_pBrowserWin  ->installEventFilter( this );
    m_pPlayerWidget->installEventFilter( this );

    readConfig();
    initIpc();   //initializes Unix domain socket for loader communication, will also hide the splash

    //after this point only analyzer pixmaps will be created
    QPixmap::setDefaultOptimization( QPixmap::BestOptim );

    EngineController::instance()->attach( m_pPlayerWidget );
    EngineController::instance()->attach( this );
    m_pTray = new amaroK::TrayIcon( m_pPlayerWidget, actionCollection() ); //shown/hidden in applySettings()

    //KJSEmbed
    m_kjs = new KJSEmbedPart( this );
    m_kjs->addObject( this );
    m_kjs->addObject( m_pBrowserWin );
    m_kjs->addObject( m_pPlayerWidget );
    JSConsoleWidget* console = m_kjs->view();
    console->show();
    
    applySettings();  //will load the engine

    //restore session as long as the user isn't asking for stuff to be inserted into the playlist etc.
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if( args->count() == 0 || args->isSet( "enqueue" ) ) restoreSession(); //resume playback + load prev PLS

    //TODO remember if we were in tray last exit, if so don't show!
    m_pPlayerWidget->show(); //BrowserWin will sponaneously show if appropriate

    //process some events so that the UI appears and things feel more responsive
    kapp->processEvents();

    KTipDialog::showTip( "amarok/data/startupTip.txt", false );

    handleCliArgs();
}

PlayerApp::~PlayerApp()
{
    kdDebug() << k_funcinfo << endl;

    //close loader IPC server socket
    if ( m_sockfd != -1 )
        ::close( m_sockfd );

    //Save current item info in dtor rather than saveConfig() as it is only relevant on exit
    //and we may in the future start to use read and saveConfig() in other situations
    //    kapp->config()->setGroup( "Session" );

    EngineBase *engine = EngineController::engine();

    //TODO why are these configXT'd? We hardly need to accesss these globally.
    //     and it means they're a pain to extend
    if( AmarokConfig::resumePlayback() && !EngineController::instance()->playingURL().isEmpty() )
    {
        AmarokConfig::setResumeTrack( EngineController::instance()->playingURL().url() );

        if ( engine->state() != EngineBase::Empty )
            AmarokConfig::setResumeTime( engine->position() / 1000 );
        else
            AmarokConfig::setResumeTime( -1 );
    }
    else AmarokConfig::setResumeTrack( QString::null ); //otherwise it'll play previous resume next time!

    engine->stop(); //slotStop() is not necessary

    saveConfig();

    delete m_pPlayerWidget;
    delete m_pBrowserWin;
    delete m_pOSD;
    // delete EngineController
    PluginManager::unload( engine );
}


void PlayerApp::handleCliArgs()
{
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

    if ( args->count() > 0 )
    {
        KURL::List list;
        bool notEnqueue = !args->isSet( "enqueue" );

        for ( int i = 0; i < args->count(); i++ )
            list << args->url( i );

        kdDebug() << "List size: " << list.count() << endl;

        //add to the playlist with the correct arguments ( bool clear, bool play )
        m_pBrowserWin->insertMedia( list, notEnqueue, notEnqueue || args->isSet( "play" ) );
    }
    //we shouldn't let the user specify two of these since it is pointless!
    //so we prioritise, pause > stop > play > next > prev
    //thus pause is the least destructive, followed by stop as brakes are the most important bit of a car(!)
    //then the others seemed sensible. Feel free to modify this order, but please leave justification in the cvs log
    //I considered doing some sanity checks (eg only stop if paused or playing), but decided it wasn't worth it
    else if ( args->isSet( "pause" ) )
        EngineController::instance()->pause();
    else if ( args->isSet( "stop" ) )
        EngineController::instance()->stop();
    else if ( args->isSet( "play" ) ) //will restart if we are playing
        EngineController::instance()->play();
    else if ( args->isSet( "next" ) )
        EngineController::instance()->next();
    else if ( args->isSet( "previous" ) )
        EngineController::instance()->previous();

    args->clear();    //free up memory
}


//this method processes the cli arguments sent by the loader process
void PlayerApp::handleLoaderArgs( QCString args ) //SLOT
{
    //extract startup_env part
    int index = args.find( "|" );
    QCString startup_env = args.left( index );
    args.remove( 0, index + 1 );
    kdDebug() << k_funcinfo << "DESKTOP_STARTUP_ID: " << startup_env << endl;

    //stop startup cursor animation
    setStartupId( startup_env );
    KStartupInfo::appStarted();

    //divide argument line into single strings
    QStringList strlist = QStringList::split( "|", args );

    int argc = strlist.count();
    char **argv = new char*[argc]; // char *argv[argc] is not ISO c++

    for ( int i = 0; i < argc; i++ )
    {
        argv[i] = const_cast<char*>( strlist[i].latin1() );
        kdDebug() << k_funcinfo << " extracted string: " << argv[i] << endl;
    }

    //re-initialize KCmdLineArgs with the new arguments
    KCmdLineArgs::reset();
    initCliArgs( argc, argv );
    handleCliArgs();
    delete[] argv;
}

/////////////////////////////////////////////////////////////////////////////////////
// INIT
/////////////////////////////////////////////////////////////////////////////////////

void PlayerApp::initCliArgs( int argc, char *argv[] ) //static
{
    static const char *description = I18N_NOOP( "An audio player for KDE" );

    static KCmdLineOptions options[] =
        {
            { "+[URL(s)]", I18N_NOOP( "Files/URLs to Open" ), 0 },
            { "r", 0, 0 },
            { "previous", I18N_NOOP( "Skip backwards in playlist" ), 0 },
            { "p", 0, 0 },
            { "play", I18N_NOOP( "Start playing current playlist" ), 0 },
            { "s", 0, 0 },
            { "stop", I18N_NOOP( "Stop playback" ), 0 },
            { "pause", I18N_NOOP( "Pause playback" ), 0 },
            { "f", 0, 0 },
            { "next", I18N_NOOP( "Skip forwards in playlist" ), 0 },
            { ":", I18N_NOOP("Additional options:"), 0 },
            { "e", 0, 0 },
            { "enqueue", I18N_NOOP( "Enqueue Files/URLs" ), 0 },
            { 0, 0, 0 }
        };

    static KAboutData aboutData( "amarok", I18N_NOOP( "amaroK" ),
                                 APP_VERSION, description, KAboutData::License_GPL,
                                 I18N_NOOP( "(c) 2002-2003, Mark Kretschmann\n(c) 2003-2004, the amaroK developers" ),
                                 I18N_NOOP( "IRC:\nserver: irc.freenode.net / channel: #amarok\n\n"
                                            "Feedback:\namarok-devel@lists.sourceforge.net" ),
                                 I18N_NOOP( "http://amarok.sourceforge.net" ) );

    //TODO should we i18n this stuff?

    aboutData.addAuthor( "Christian \"babe-magnet\" Muehlhaeuser", "developer, stud", "chris@chris.de", "http://www.chris.de" );
    aboutData.addAuthor( "Frederik \"ich bin kein Deustcher!\" Holljen", "developer, 733t code, OSD improvement, patches", "fh@ez.no" );
    aboutData.addAuthor( "Mark \"it's good, but it's not irssi\" Kretschmann", "project founder, developer, maintainer", "markey@web.de" );
    aboutData.addAuthor( "Max \"sleep? there's no time!\" Howell", "developer, knight of the regression round-table",
                         "max.howell@methylblue.com", "http://www.methyblue.com" );
    aboutData.addAuthor( "Stanislav \"did someone say DCOP?\" Karchebny", "developer, DCOP, improvements, cleanups, i18n",
                         "berk@upnet.ru" );

    aboutData.addCredit( "Adam Pigg", "analyzers, patches", "adam@piggz.fsnet.co.uk" );
    aboutData.addCredit( "Alper Ayazoglu", "graphics: buttons", "cubon@cubon.de", "http://cubon.de" );
    aboutData.addCredit( "Enrico Ros", "analyzers, king of openGL", "eros.kde@email.it" );
    aboutData.addCredit( "Jarkko Lehti", "tester, IRC channel operator, whipping", "grue@iki.fi" );
    aboutData.addCredit( "Josef Spillner", "KDE RadioStation code", "spillner@kde.org" );
    aboutData.addCredit( "Markus A. Rykalski", "graphics", "exxult@exxult.de" );
    aboutData.addCredit( "Melchior Franz", "new FFT routine, bugfixes", "mfranz@kde.org" );
    aboutData.addCredit( "Roman Becker", "graphics: amaroK logo", "roman@formmorf.de", "http://www.formmorf.de" );
    aboutData.addCredit( "Scott Wheeler", "Taglib", "wheeler@kde.org" );
    aboutData.addCredit( "The Noatun Authors", "code and inspiration", 0, "http://noatun.kde.org" );
    aboutData.addCredit( "Whitehawk Stormchaser", "tester, patches", "zerokode@gmx.net" );

    KCmdLineArgs::init( argc, argv, &aboutData );
    KCmdLineArgs::addCmdLineOptions( options );   // Add our own options.
    PlayerApp::addCmdLineOptions();
}


void PlayerApp::initEngine()
{
    kdDebug() << "BEGIN " << k_funcinfo << endl;

    amaroK::Plugin* plugin = PluginManager::createFromQuery
                             ( "[X-KDE-amaroK-plugintype] == 'engine' and "
                               "Name                      == '" + AmarokConfig::soundSystem() + '\'' );

    if ( !plugin ) {
        kdWarning() << k_funcinfo << "Cannot load the specified engine. Trying with another engine..\n";
        
        //when the engine specified in our config does not exist/work, try to invoke _any_ engine plugin
        plugin = PluginManager::createFromQuery( "[X-KDE-amaroK-plugintype] == 'engine'" );

        if ( !plugin )
            kdFatal() << k_funcinfo << "No engine plugin found. Aborting.\n";
        
        AmarokConfig::setSoundSystem( PluginManager::getService( plugin )->name() );
        kdDebug() << k_funcinfo << "setting soundSystem to: " << AmarokConfig::soundSystem() << endl;
    }

    // feed engine to controller
    EngineController::setEngine( static_cast<EngineBase*>( plugin ) );
    EngineController::engine()->init( m_artsNeedsRestart, SCOPE_SIZE, AmarokConfig::rememberEffects() );

    kdDebug() << "END " << k_funcinfo << endl;
}


void PlayerApp::initIpc()
{
    int m_sockfd = ::socket( AF_UNIX, SOCK_STREAM, 0 );
    if ( m_sockfd == -1 )
    {
        kdWarning() << k_funcinfo << " socket() error\n";
        return;
    }
    sockaddr_un local;
    local.sun_family = AF_UNIX;
    QCString path = ::locateLocal( "socket", "amarok.loader_socket" ).local8Bit();
    ::strcpy( &local.sun_path[0], path );
    ::unlink( path );

    kdDebug() << "Opening control socket on " << path << endl;

    int len = sizeof( local );

    if ( ::bind( m_sockfd, (struct sockaddr*) &local, len ) == -1 )
    {
        kdWarning() << k_funcinfo << " bind() error\n";
        ::close ( m_sockfd );
        m_sockfd = -1;
        return;
    }
    if ( ::listen( m_sockfd, 1 ) == -1 )
    {
        kdWarning() << k_funcinfo << " listen() error\n";
        ::close ( m_sockfd );
        m_sockfd = -1;
        return;
    }

    LoaderServer* server = new LoaderServer( this );
    server->setSocket( m_sockfd );

    connect( server, SIGNAL( loaderArgs( QCString ) ),
             this,     SLOT( handleLoaderArgs( QCString ) ) );
}


void PlayerApp::initBrowserWin()
{
    m_pBrowserWin     = new BrowserWin( 0, "BrowserWin" );
    m_pPlaylistWidget = m_pBrowserWin->playlist();
}


void PlayerApp::initPlayerWidget()
{
    m_pPlayerWidget = new PlayerWidget( 0, "PlayerWidget" );

    connect( m_pPlayerWidget, SIGNAL( playlistToggled( bool ) ),
             this,              SLOT( slotPlaylistShowHide() ) );
    connect( m_pPlayerWidget, SIGNAL( effectsWindowActivated() ),
             this,              SLOT( showEffectWidget() ) );
}


void PlayerApp::restoreSession()
{
    //here we restore the session
    //however, do note, this is always done, KDE session management is not involved

    //load previous playlist
    if ( AmarokConfig::savePlaylist() )
        m_pBrowserWin->restoreSessionPlaylist();

    if ( AmarokConfig::resumePlayback() && !AmarokConfig::resumeTrack().isEmpty() )
    {
        MetaBundle *bundle = TagReader::readTags( KURL(AmarokConfig::resumeTrack()), true );

        if( bundle )
        {
            EngineController::instance()->play( *bundle );
            delete bundle;

            //see if we also saved the time
            int seconds = AmarokConfig::resumeTime();
            if ( seconds > 0 ) EngineController::instance()->engine()->seek( seconds * 1000 );
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// METHODS
/////////////////////////////////////////////////////////////////////////////////////

//SLOT
void PlayerApp::applySettings()
{
    kdDebug() << "BEGIN " << k_funcinfo << endl;

    if ( AmarokConfig::soundSystem() != PluginManager::getService( EngineController::engine() )->name() ) {
        PluginManager::unload( EngineController::engine() );
        initEngine();
        AmarokConfig::setHardwareMixer( EngineController::engine()->initMixer( AmarokConfig::hardwareMixer() ) );

        kdDebug() << k_funcinfo << " AmarokConfig::soundSystem() == " << AmarokConfig::soundSystem() << endl;
    }

    if ( AmarokConfig::hardwareMixer() != EngineController::engine()->isMixerHardware() )
        AmarokConfig::setHardwareMixer( EngineController::engine()->initMixer( AmarokConfig::hardwareMixer() ) );

    EngineController::instance()->setVolume( AmarokConfig::masterVolume() );
    EngineController::engine()->setRestoreEffects( AmarokConfig::rememberEffects() );
    EngineController::engine()->setXfadeLength( AmarokConfig::crossfade() ?
                                                AmarokConfig::crossfadeLength() : 0 );

    m_pOSD->setEnabled( AmarokConfig::osdEnabled() );
    m_pOSD->setFont( AmarokConfig::osdFont() );
    m_pOSD->setTextColor( AmarokConfig::osdTextColor() );
    m_pOSD->setBackgroundColor( AmarokConfig::osdBackgroundColor() );
    m_pOSD->setDuration( AmarokConfig::osdDuration() );
    m_pOSD->setPosition( (OSDWidget::Position)AmarokConfig::osdAlignment() );
    m_pOSD->setScreen( AmarokConfig::osdScreen() );
    m_pOSD->setOffset( AmarokConfig::osdXOffset(), AmarokConfig::osdYOffset() );

    m_pPlayerWidget->createAnalyzer( false );
    m_pBrowserWin->setFont( AmarokConfig::useCustomFonts() ?
                            AmarokConfig::browserWindowFont() : QApplication::font() );

    QFont font = m_pPlayerWidget->font();
    font.setFamily( AmarokConfig::useCustomFonts() ?
                    AmarokConfig::playerWidgetFont().family() : QApplication::font().family() );
    m_pPlayerWidget->setFont( font );
    m_pPlayerWidget->update(); //FIXME doesn't update the scroller, we require the metaBundle to do that, wait for my metaBundle modifications..

    //TODO delete when not in use
    m_pTray->setShown( AmarokConfig::showTrayIcon() );

    setupColors();

    kdDebug() << "END " << k_funcinfo << endl;
}


void PlayerApp::saveConfig()
{
    AmarokConfig::setBrowserWinPos     ( m_pBrowserWin->pos() );
    AmarokConfig::setBrowserWinSize    ( m_pBrowserWin->size() );
    AmarokConfig::setBrowserWinEnabled ( m_showBrowserWin );
    AmarokConfig::setMasterVolume      ( EngineController::instance()->engine()->volume() );
    AmarokConfig::setPlayerPos         ( m_pPlayerWidget->pos() );
    AmarokConfig::setVersion           ( APP_VERSION );
    m_pBrowserWin->saveConfig();

    AmarokConfig::writeConfig();
}


void PlayerApp::readConfig()
{
    kdDebug() << "BEGIN " << k_funcinfo << endl;

    //we must restart artsd after each version change, so that it picks up any plugin changes
    m_artsNeedsRestart = AmarokConfig::version() != APP_VERSION;

    initEngine();
    EngineBase *engine = EngineController::instance()->engine();
    AmarokConfig::setHardwareMixer( engine->initMixer( AmarokConfig::hardwareMixer() ) );
    EngineController::instance()->setVolume( AmarokConfig::masterVolume() );

    m_pPlayerWidget->move  ( AmarokConfig::playerPos() );
    m_pBrowserWin  ->move  ( AmarokConfig::browserWinPos() );
    m_pBrowserWin  ->resize( AmarokConfig::browserWinSize() );

    m_showBrowserWin = AmarokConfig::browserWinEnabled();
    m_pPlayerWidget->setPlaylistShown( m_showBrowserWin );

    // Actions ==========
    m_pGlobalAccel->insert( "add", i18n( "Add Location" ), 0, KKey("WIN+a"), 0,
                            this, SLOT( slotAddLocation() ), true, true );
    m_pGlobalAccel->insert( "show", i18n( "Show/Hide the Playlist" ), 0, KKey("WIN+p"), 0,
                            this, SLOT( slotPlaylistShowHide() ), true, true );
    m_pGlobalAccel->insert( "play", i18n( "Play" ), 0, KKey("WIN+x"), 0,
                            EngineController::instance(), SLOT( play() ), true, true );
    m_pGlobalAccel->insert( "pause", i18n( "Pause" ), 0, KKey("WIN+c"), 0,
                            EngineController::instance(), SLOT( pause() ), true, true );
    m_pGlobalAccel->insert( "stop", i18n( "Stop" ), 0, KKey("WIN+v"), 0,
                            EngineController::instance(), SLOT( stop() ), true, true );
    m_pGlobalAccel->insert( "next", i18n( "Next Track" ), 0, KKey("WIN+b"), 0,
                            EngineController::instance(), SLOT( next() ), true, true );
    m_pGlobalAccel->insert( "prev", i18n( "Previous Track" ), 0, KKey("WIN+z"), 0,
                            EngineController::instance(), SLOT( previous() ), true, true );
    m_pGlobalAccel->insert( "osd", i18n( "Show OSD" ), 0, KKey("WIN+o"), 0,
                            m_pOSD, SLOT( showTrack() ), true, true );
    m_pGlobalAccel->insert( "volup", i18n( "Increase Volume" ), 0, KKey("WIN+KP_Add"), 0,
                            this, SLOT( slotIncreaseVolume() ), true, true );
    m_pGlobalAccel->insert( "voldn", i18n( "Decrease Volume" ), 0, KKey("WIN+KP_Subtract"), 0,
                            this, SLOT( slotDecreaseVolume() ), true, true );

    m_pGlobalAccel->setConfigGroup( "Shortcuts" );
    m_pGlobalAccel->readSettings( kapp->config() );
    m_pGlobalAccel->updateConnections();

    //FIXME use a global actionCollection (perhaps even at global scope)
    actionCollection()->readShortcutSettings( QString::null, kapp->config() );
    m_pBrowserWin->actionCollection()->readShortcutSettings( QString::null, kapp->config() );

    kdDebug() << "END " << k_funcinfo << endl;
}


#include <qpalette.h>
#include <kglobalsettings.h>

void PlayerApp::setupColors()
{
    //FIXME you have to fix the XT stuff for this, we need an enum (and preferably, hard-coded amarok-defaults.. or maybe not)

    if( AmarokConfig::schemeKDE() )
    {
        //TODO this sucks a bit, perhaps just iterate over all children calling "unsetPalette"?
        QColorGroup group = QApplication::palette().active();
        group.setColor( QColorGroup::BrightText, group.highlight() ); //GlowColor
        group.setColor( QColorGroup::Midlight, group.mid() ); //column separator
        m_pBrowserWin->setColors( QPalette( group, group, group ), KGlobalSettings::alternateBackgroundColor() );

    }
    else if( AmarokConfig::schemeAmarok() )
    {

        QColorGroup group = QApplication::palette().active();
        const QColor bg( 32, 32, 80 );
        //const QColor bgAlt( 77, 80, 107 );
        const QColor bgAlt( 57, 64, 98 );
        //bgAlt.setRgb( 69, 68, 102 );
        //bgAlt.setRgb( 85, 84, 117 );
        //bgAlt.setRgb( 74, 81, 107 );
        //bgAlt.setRgb( 83, 86, 112 );

        /*PLEASE don't do this, it makes lots of widget ugly
         *instead customise BrowserWin::setColors();
         */
        //group.setColor( QColorGroup::Foreground, Qt::white );
        
        group.setColor( QColorGroup::Text, Qt::white );
        group.setColor( QColorGroup::Base, bg );
        group.setColor( QColorGroup::Background, bg.dark( 115 ) );

        group.setColor( QColorGroup::Highlight, Qt::white );
        group.setColor( QColorGroup::HighlightedText, bg );
        group.setColor( QColorGroup::BrightText, QColor( 0xff, 0x40, 0x40 ) ); //GlowColor
/*
        group.setColor( QColorGroup::Light,    Qt::red );
        group.setColor( QColorGroup::Midlight, Qt::red );
        group.setColor( QColorGroup::Mid,      Qt::red );
        group.setColor( QColorGroup::Dark,     Qt::red );
        group.setColor( QColorGroup::Shadow,   Qt::red );
*/
        int h,s,v;
        bgAlt.getHsv( &h, &s, &v );
        group.setColor( QColorGroup::Midlight, QColor( h, s/3, (int)(v * 1.2), QColor::Hsv ) ); //column separator in playlist

        //FIXME QColorGroup member "disabled" looks very bad (eg for buttons)
        m_pBrowserWin->setColors( QPalette( group, group, group ), bgAlt );

    }
    else
    {
        // we try to be smart: this code figures out contrasting colors for selection and alternate background rows
        QColorGroup group = QApplication::palette().active();
        const QColor fg( AmarokConfig::browserFgColor() );
        const QColor bg( AmarokConfig::browserBgColor() );
        QColor bgAlt, highlight;
        int h, s, v;

        bg.hsv( &h, &s, &v );
        if ( v < 128 )
            v += 50;
        else
            v -= 50;
        bgAlt.setHsv( h, s, v );

        fg.hsv( &h, &s, &v );
        if ( v < 128 )
            v += 150;
        else
            v -= 150;
        if ( v < 0 )
            v = 0;
        if ( v > 255 )
            v = 255;
        highlight.setHsv( h, s, v );

        group.setColor( QColorGroup::Base, bg );
        group.setColor( QColorGroup::Background, bg.dark( 115 ) );
        group.setColor( QColorGroup::Text, fg );
        group.setColor( QColorGroup::Highlight, highlight );
        group.setColor( QColorGroup::HighlightedText, Qt::white );
        group.setColor( QColorGroup::Dark, Qt::darkGray );
        group.setColor( QColorGroup::BrightText, QColor( 0xff, 0x40, 0x40 ) ); //GlowColor

        //FIXME QColorGroup member "disabled" looks very bad (eg for buttons)
        m_pBrowserWin->setColors( QPalette( group, group, group ), bgAlt );
    }
}


void PlayerApp::insertMedia( const KURL::List &list )
{
    m_pBrowserWin->insertMedia( list );
}


bool PlayerApp::eventFilter( QObject *o, QEvent *e )
{
    //Hi! Welcome to one of amaroK's less clear functions!
    //Please don't change anything in here without talking to mxcl or Larson[H] on amaroK
    //as most of this stuff is cleverly crafted and has purpose! Comments aren't always thorough as
    //it tough explaining what is going on! Thanks.

    if( e->type() == QEvent::Close && o == m_pBrowserWin && m_pPlayerWidget->isShown() )
    {
        m_pPlayerWidget->setPlaylistShown( m_showBrowserWin = false );
    }
    else if( e->type() == QEvent::Hide && o == m_pPlayerWidget )
    {
        //if the event is not spontaneous then amaroK was responsible for the event
        //we should therefore hide the playlist as well
        //the only spontaneous hide events we care about are iconification and shading
        if( AmarokConfig::hidePlaylistWindow() && !e->spontaneous() ) m_pBrowserWin->hide();
        else if( AmarokConfig::hidePlaylistWindow() )
        {
            KWin::WindowInfo info = KWin::windowInfo( m_pPlayerWidget->winId() );

            if( !info.valid() ); //do nothing
            else if( info.isMinimized() )
                KWin::iconifyWindow( m_pBrowserWin->winId(), false );
            else if( info.state() & NET::Shaded )
                m_pBrowserWin->hide();
        }
    }
    else if( e->type() == QEvent::Show && o == m_pPlayerWidget )
    {
        //TODO this is broke again if playlist is minimized
        //when fixing you have to make sure that changing desktop doesn't un minimise the playlist

        if( AmarokConfig::hidePlaylistWindow() && m_showBrowserWin && e->spontaneous()/*)
        {
            //this is to battle a kwin bug that affects xinerama users
            //FIXME I commented this out for now because spontaneous show events are sent to widgets
            //when you switch desktops, so this would cause the playlist to deiconify when switching desktop!
            //KWin::deIconifyWindow( m_pBrowserWin->winId(), false );
            m_pBrowserWin->show();
            eatActivateEvent = true;
        }
        else if( */ || m_showBrowserWin )
        {
            //if minimized the taskbar entry for browserwin is shown
            m_pBrowserWin->show();
        }

        if( m_pBrowserWin->isShown() )
        {
            //slotPlaylistHideShow() can make it so the PL is shown but the button is off.
            //this is intentional behavior BTW
            //FIXME it would be nice not to have to set this as it us unclean(TM)
            m_pPlayerWidget->setPlaylistShown( true );
        }
    }
    /*
    //The idea here is to raise both windows when one raises so that both get shown. Unfortunately
    //there just isn't a simple solution that doesn't cause breakage in other areas. Anything more complex
    //than this would probably be too much effort to maintain. I'll leave it commented though in case
    //a future developer has more wisdom than me. IMO if we can get the systray to do it, that'll be enough
    else if( e->type() == QEvent::WindowActivate )
    {
        (o == m_pPlayerWidget ? (QWidget*)m_pBrowserWin : (QWidget*)m_pPlayerWidget)->raise();
    }
    */
    return FALSE;
}


void PlayerApp::engineStateChanged( EngineBase::EngineState state )
{
    switch( state )
    {
        case EngineBase::Empty:
        case EngineBase::Idle:
            m_pDcopHandler->setNowPlaying( QString::null );
            //QToolTip::remove( m_pTray );
            QToolTip::add( m_pTray, i18n( "amaroK - Audio Player" ) );
            break;
        case EngineBase::Paused: // shut up GCC
        case EngineBase::Playing:
            break;
    }
}


void PlayerApp::engineNewMetaData( const MetaBundle &bundle, bool /*trackChanged*/ )
{
    QString prettyTitle = bundle.prettyTitle();

    m_pOSD->showTrack( bundle );
    m_pDcopHandler->setNowPlaying( prettyTitle );
    //QToolTip::remove( m_pTray );
    //QToolTip::add( m_pTray, prettyTitle );
    PlaylistToolTip::add( m_pTray, bundle );
}


void PlayerApp::slotPlaylistShowHide()
{
    //show/hide the playlist global shortcut slot
    //bahavior depends on state of the PlayerWidget and various minimization states

    KWin::WindowInfo info = KWin::windowInfo( m_pBrowserWin->winId() );
    bool isMinimized = info.valid() && info.isMinimized();

    if( !m_pBrowserWin->isShown() )
    {
        if( isMinimized ) KWin::deIconifyWindow( info.win() );
        m_pBrowserWin->setShown( m_showBrowserWin = true );
    }
    else if( isMinimized ) KWin::deIconifyWindow( info.win() );
    else
    {
        KWin::WindowInfo info2 = KWin::windowInfo( m_pPlayerWidget->winId() );
        if( info2.valid() && info2.isMinimized() ) KWin::iconifyWindow( info.win() );
        else
        {
            m_pBrowserWin->setShown( m_showBrowserWin = false );
        }
    }

    // make sure playerwidget button is in sync
    m_pPlayerWidget->setPlaylistShown( m_showBrowserWin );
}


void PlayerApp::showEffectWidget()
{
    if ( !EffectWidget::self )
    {
        EffectWidget::self = new EffectWidget();

        connect( m_pPlayerWidget,    SIGNAL( destroyed() ),
                 EffectWidget::self,   SLOT( deleteLater() ) );
        connect( EffectWidget::self, SIGNAL( destroyed() ),
                 m_pPlayerWidget,      SLOT( setEffectsWindowShown() ) ); //defaults to false

        EffectWidget::self->show();

        if ( EffectWidget::save_geometry.isValid() )
            EffectWidget::self->setGeometry( EffectWidget::save_geometry );
    }
    else
    {
        m_pPlayerWidget->setEffectsWindowShown( false );
        delete EffectWidget::self;
    }
}

void PlayerApp::slotShowOptions()
{
    if( !KConfigDialog::showDialog( "settings" ) )
    {
        //KConfigDialog didn't find an instance of this dialog, so lets create it :
        KConfigDialog* dialog = new AmarokConfigDialog( m_pPlayerWidget, "settings", AmarokConfig::self() );

        connect( dialog, SIGNAL( settingsChanged() ), this, SLOT( applySettings() ) );

        dialog->show();
    }
}

void PlayerApp::setOsdEnabled( bool enabled ) //SLOT //FIXME required due to dcopHandler
{
    m_pOSD->setEnabled( enabled );
}

void PlayerApp::slotShowVolumeOSD() //SLOT //FIXME required due to dcopHandler
{
    m_pOSD->showVolume();
}

void PlayerApp::slotIncreaseVolume()
{
    EngineController *controller = EngineController::instance();
    controller->setVolume( controller->engine()->volume() + 100 / 25 );
    m_pOSD->showVolume();
}

void PlayerApp::slotDecreaseVolume()
{
    EngineController *controller = EngineController::instance();
    controller->setVolume( controller->engine()->volume() - 100 / 25 );
    m_pOSD->showVolume();
}

void PlayerApp::slotConfigShortcuts()
{
    KKeyDialog::configure( actionCollection(), m_pBrowserWin );
}

void PlayerApp::slotConfigGlobalShortcuts()
{
    KKeyDialog::configure( m_pGlobalAccel, true, 0, true );
}

#include <kedittoolbar.h>
void PlayerApp::slotConfigToolBars()
{
    KEditToolbar dialog( m_pBrowserWin->actionCollection() );

    if( dialog.exec() )
    {
        m_pBrowserWin->reloadXML();
        m_pBrowserWin->createGUI();
    }
}


////////////////////////////////////////////////////////////////////////////////
// CLASS LoaderServer
////////////////////////////////////////////////////////////////////////////////

LoaderServer::LoaderServer( QObject* parent )
        : QServerSocket( parent )
{}


void LoaderServer::newConnection( int sockfd )
{
    kdDebug() << k_funcinfo << endl;

    char buf[2000];
    int nbytes = recv( sockfd, buf, sizeof(buf) - 1, 0 );

    if ( nbytes < 0 )
        kdDebug() << k_funcinfo << " recv error" << endl;
    else
    {
        buf[nbytes] = '\000';
        QCString result( buf );
        kdDebug() << k_funcinfo << " received: \n" << result << endl;

        if ( !result.contains( "STARTUP" ) )
            emit loaderArgs( result );
    }

    ::close( sockfd );
}


#include "playerapp.moc"
