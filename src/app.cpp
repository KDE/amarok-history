/***************************************************************************
                      app.cpp  -  description
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
#include "amarokdcophandler.h"
#include "app.h"
#include "configdialog.h"
#include "effectwidget.h"
#include "enginebase.h"
#include "enginecontroller.h"
#include "metabundle.h"
#include "osd.h"
#include "playerwindow.h"
#include "playlist.h"
#include "playlistwindow.h"
#include "plugin.h"
#include "pluginmanager.h"
#include "socketserver.h"
#include "systray.h"
#include "tracktooltip.h"        //engineNewMetaData()

#include <kcmdlineargs.h>        //initCliArgs()
#include <kdebug.h>
#include <kedittoolbar.h>        //slotConfigToolbars()
#include <kglobalaccel.h>        //initGlobalShortcuts()
#include <kglobalsettings.h>     //setupColors()
#include <kkeydialog.h>          //slotConfigShortcuts()
#include <klocale.h>
#include <kmessagebox.h>         //applySettings(), genericEventHandler()
#include <kurldrag.h>            //genericEventHandler()

#include <qevent.h>              //genericEventHandler()
#include <qeventloop.h>          //applySettings()
#include <qpixmap.h>             //QPixmap::setDefaultOptimization()
#include <qpopupmenu.h>          //genericEventHandler
#include <qtooltip.h>            //default tooltip for trayicon
#include <qpalette.h>            //setupColors()
#include <qobjectlist.h>         //setupColors()

App::App()
        : KApplication()
        , m_pPlayerWindow( 0 ) //will be created in applySettings()
{
    const KCmdLineArgs* const args = KCmdLineArgs::parsedArgs();
    const bool bRestoreSession = args->count() == 0 || args->isSet( "enqueue" );

    QPixmap::setDefaultOptimization( QPixmap::MemoryOptim );

    m_pGlobalAccel    = new KGlobalAccel( this );
    m_pPlaylistWindow = new PlaylistWindow();
    m_pDcopHandler    = new amaroK::DcopHandler();
    m_pOSD            = amaroK::OSD::instance(); //creates the OSD
    m_pTray           = new amaroK::TrayIcon( m_pPlaylistWindow );
    (void)              new Vis::SocketServer( this );

    m_pPlaylistWindow->init(); //creates the playlist, browsers, etc.
    initGlobalShortcuts();

    //load previous playlist in separate thread
    if( bRestoreSession && AmarokConfig::savePlaylist() ) Playlist::instance()->restoreSession();

    //create engine, show PlayerWindow, show TrayIcon etc.
    applySettings( true );

    //initializes Unix domain socket for loader communication, and hides the splash
    //do here so splash is hidden just after amaroK's windows appear
    (void) new LoaderServer( this );

    //after this point only analyzer and temporary pixmaps will be created
    QPixmap::setDefaultOptimization( QPixmap::BestOptim );

    //do after applySettings(), or the OSD will flicker and other wierdness!
    //do before restoreSession()!
    EngineController::instance()->attach( this );

    //set a default interface
    engineStateChanged( EngineBase::Empty );

    if( AmarokConfig::resumePlayback() && bRestoreSession && !args->isSet( "stop" ) )
    {
        //restore session as long as the user didn't specify media to play etc.
        //do this after applySettings() so OSD displays correctly

        restoreSession();
    }

    handleCliArgs();
}

App::~App()
{
    kdDebug() << k_funcinfo << endl;

    EngineBase* const engine = EngineController::engine();

    if( AmarokConfig::resumePlayback() )
    {
        if( engine->state() != EngineBase::Empty )
        {
            AmarokConfig::setResumeTrack( EngineController::instance()->playingURL().url() );
            AmarokConfig::setResumeTime( engine->position() / 1000 );
        }
        else AmarokConfig::setResumeTrack( QString::null ); //otherwise it'll play previous resume next time!
    }

    engine->stop(); //don't call EngineController::stop() - it's slow

    if ( AmarokConfig::showTrayIcon() )
        amaroK::config()->writeEntry( "HiddenOnExit", mainWindow()->isHidden() );

    delete m_pPlayerWindow;   //sets some XT keys
    delete m_pPlaylistWindow; //sets some XT keys
    delete m_pDcopHandler;

    AmarokConfig::setVersion( APP_VERSION );
    AmarokConfig::writeConfig();

    //TODO move engine load and unload to controller so that it can handle this properly
    if( QCString( engine->name() ) != "Dummy" ) PluginManager::unload( engine );
}


void App::handleCliArgs()
{
    KCmdLineArgs* const args = KCmdLineArgs::parsedArgs();

    if ( args->count() > 0 )
    {
        KURL::List list;
        bool notEnqueue = !args->isSet( "enqueue" );

        for ( int i = 0; i < args->count(); i++ )
            list << args->url( i );

        if( notEnqueue ) Playlist::instance()->clear();
        Playlist::instance()->appendMedia( list, notEnqueue || args->isSet( "play" ) );
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


/////////////////////////////////////////////////////////////////////////////////////
// INIT
/////////////////////////////////////////////////////////////////////////////////////

void App::initCliArgs( int argc, char *argv[] ) //static
{
    extern class KAboutData aboutData; //defined in amarokcore/main.cpp

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

    KCmdLineArgs::reset();
    KCmdLineArgs::init( argc, argv, &aboutData ); //calls KApplication::addCmdLineOptions()
    KCmdLineArgs::addCmdLineOptions( options );   //add our own options
}


void App::initEngine()
{
    kdDebug() << "BEGIN " << k_funcinfo << endl;

    const QString query    = "[X-KDE-amaroK-plugintype] == 'engine' and Name == '%1'";
    amaroK::Plugin* plugin = PluginManager::createFromQuery( query.arg( AmarokConfig::soundSystem() ) );

    if ( !plugin )
    {
        kdWarning() << "Cannot load the: " << AmarokConfig::soundSystem() << " plugin. Trying another engine..\n";

        //try to invoke _any_ engine plugin
        plugin = PluginManager::createFromQuery( "[X-KDE-amaroK-plugintype] == 'engine'" );

        if ( !plugin )
        {
            KMessageBox::error( m_pPlaylistWindow, i18n(
                "<p>amaroK could not find any sound-engine plugins. "
                "It is likely that amaroK is installed under the wrong prefix, please fix your installation using:"
                "<pre>cd /path/to/amarok/source-code/<br>"
                "su -c \"make uninstall\"<br>"
                "./configure --prefix=`kde-config --prefix` && su -c \"make install\"</pre>"
                "More information can be found in the README file. For further assistance join us at #amarok on irc.freenode.net." ) );

            ::exit( EXIT_SUCCESS );
        }

        AmarokConfig::setSoundSystem( PluginManager::getService( plugin )->name() );
        kdDebug() << "Setting soundSystem to: " << AmarokConfig::soundSystem() << endl;
    }

    // feed engine to controller
    EngineBase* const engine = (EngineBase*)plugin;
    bool restartArts = AmarokConfig::version() != APP_VERSION;

    engine->init( restartArts, amaroK::SCOPE_SIZE, AmarokConfig::rememberEffects() );
    EngineController::setEngine( engine ); //will set engine's volume

    //NOTE applySettings() must be called now to ensure mixer settings are set

    kdDebug() << "END " << k_funcinfo << endl;
}


#include <kaction.h>
#include <kshortcutlist.h>
void App::initGlobalShortcuts()
{
    EngineController* const ec = EngineController::instance();


    m_pGlobalAccel->insert( "play", i18n( "Play" ), 0, KKey("WIN+x"), 0,
                            ec, SLOT( play() ), true, true );
    m_pGlobalAccel->insert( "pause", i18n( "Pause" ), 0, KKey("WIN+c"), 0,
                            ec, SLOT( pause() ), true, true );
    m_pGlobalAccel->insert( "play_pause", i18n( "Play/Pause" ), 0, 0, 0,
                            ec, SLOT( playPause() ), true, true );
    m_pGlobalAccel->insert( "stop", i18n( "Stop" ), 0, KKey("WIN+v"), 0,
                            ec, SLOT( stop() ), true, true );
    m_pGlobalAccel->insert( "next", i18n( "Next track" ), 0, KKey("WIN+b"), 0,
                            ec, SLOT( next() ), true, true );
    m_pGlobalAccel->insert( "prev", i18n( "Previous track" ), 0, KKey("WIN+z"), 0,
                            ec, SLOT( previous() ), true, true );
    m_pGlobalAccel->insert( "volup", i18n( "Increase volume" ), 0, KKey("WIN+KP_Add"), 0,
                            ec, SLOT( increaseVolume() ), true, true );
    m_pGlobalAccel->insert( "voldn", i18n( "Decrease volume" ), 0, KKey("WIN+KP_Subtract"), 0,
                            ec, SLOT( decreaseVolume() ), true, true );
    m_pGlobalAccel->insert( "playlist_add", i18n( "Add media" ), 0, KKey("WIN+a"), 0,
                            m_pPlaylistWindow, SLOT( slotAddLocation() ), true, true );
    m_pGlobalAccel->insert( "show", i18n( "Toggle the Playlist Window" ), 0, KKey("WIN+p"), 0,
                            m_pPlaylistWindow, SLOT( showHide() ), true, true );
    m_pGlobalAccel->insert( "osd", i18n( "Show the OSD" ), 0, KKey("WIN+o"), 0,
                            m_pOSD, SLOT( forceShowTrack() ), true, true );

    m_pGlobalAccel->setConfigGroup( "Shortcuts" );
    m_pGlobalAccel->readSettings( kapp->config() );
    m_pGlobalAccel->updateConnections();

    //TODO fix kde accel system so that kactions find appropriate global shortcuts
    //     and there is only one configure shortcuts dialog

    KActionCollection* const ac = actionCollection();
    KAccelShortcutList list( m_pGlobalAccel );

    for( uint i = 0; i < list.count(); ++i )
    {
        KAction *action = ac->action( list.name( i ).latin1() );

        if( action )
        {
            //this is a hack really, also it means there may be two calls to the slot for the shortcut
            action->setShortcutConfigurable( false );
            action->setShortcut( list.shortcut( i ) );
        }
    }
}


void App::restoreSession()
{
    //here we restore the session
    //however, do note, this is always done, KDE session management is not involved

    if( !AmarokConfig::resumeTrack().isEmpty() )
    {
        KURL track( AmarokConfig::resumeTrack() );
        MetaBundle bundle( track );

        EngineController::instance()->play( bundle );
        EngineController::engine()->seek( AmarokConfig::resumeTime() * 1000 );
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// METHODS
/////////////////////////////////////////////////////////////////////////////////////

//SLOT
void App::applySettings( bool firstTime )
{
    //FIXME it is possible to achieve a state where you have all windows hidden and no way to get them back
    //      eg hide systray while amarok is hidden (dumb, but possible)

    kdDebug() << "BEGIN " << k_funcinfo << endl;

    if( AmarokConfig::showPlayerWindow() )
    {
        if( !m_pPlayerWindow )
        {
            //the player Window becomes the main Window
            //it is the focus for hideWithMainWindow behaviour etc.
            //it gets the hailed "amaroK" caption
            m_pPlaylistWindow->setCaption( kapp->makeStdCaption( i18n("Playlist") ) );

            m_pPlayerWindow = new PlayerWidget( m_pPlaylistWindow, "PlayerWindow", firstTime && AmarokConfig::playlistWindowEnabled() );

            //don't show PlayerWindow on firstTime, that is done below
            //we need to explicately set the PL button if it's the first time
            if( !firstTime ) m_pPlayerWindow->show();

            connect( m_pPlayerWindow, SIGNAL(effectsWindowToggled( bool )), SLOT(slotConfigEffects( bool )) );
            connect( m_pPlayerWindow, SIGNAL(playlistToggled( bool )), m_pPlaylistWindow, SLOT(showHide()) );

            //TODO get this to work!
            //may work if you set no parent for the systray?
            //KWin::setSystemTrayWindowFor( m_pTray->winId(), m_pPlayerWindow->winId() );

            delete m_pTray; m_pTray = new amaroK::TrayIcon( m_pPlayerWindow );
        }

        QFont font = m_pPlayerWindow->font();
        font.setFamily( AmarokConfig::useCustomFonts() ? AmarokConfig::playerWidgetFont().family() : QApplication::font().family() );
        m_pPlayerWindow->setFont( font ); //NOTE dont use unsetFont(), we use custom font sizes (for now)
        m_pPlayerWindow->update(); //FIXME doesn't update the scroller

    } else if( m_pPlayerWindow ) {

        delete m_pTray; m_pTray = new amaroK::TrayIcon( m_pPlaylistWindow );
        delete m_pPlayerWindow; m_pPlayerWindow = 0;

        m_pPlaylistWindow->setCaption( "amaroK" );
        //m_pPlaylistWindow->show(); //must be shown //we do below now

        //ensure that at least one Menu is plugged into an accessible UI element
        if( !actionCollection()->action( "amarok_menu" )->isPlugged() )
        {
            playlistWindow()->reloadXML();
            playlistWindow()->createGUI();
        }
    }


    m_pOSD->setEnabled( AmarokConfig::osdEnabled() );
    m_pOSD->setFont( AmarokConfig::osdFont() );
    m_pOSD->setShadow( AmarokConfig::osdDrawShadow() );
    if( AmarokConfig::osdUseCustomColors() )
    {
        m_pOSD->setTextColor( AmarokConfig::osdTextColor() );
        m_pOSD->setBackgroundColor( AmarokConfig::osdBackgroundColor() );
    }
    else m_pOSD->unsetColors();
    m_pOSD->setDuration( AmarokConfig::osdDuration() );
    m_pOSD->setAlignment( (OSDWidget::Alignment)AmarokConfig::osdAlignment() );
    m_pOSD->setScreen( AmarokConfig::osdScreen() );
    m_pOSD->setOffset( AmarokConfig::osdXOffset(), AmarokConfig::osdYOffset() );


    playlistWindow()->setFont( AmarokConfig::useCustomFonts() ? AmarokConfig::playlistWindowFont() : QApplication::font() );
    reinterpret_cast<QWidget*>(playlistWindow()->statusBar())->setShown( AmarokConfig::showStatusBar() );

    m_pTray->setShown( AmarokConfig::showTrayIcon() );

    setupColors();


    if( firstTime && !amaroK::config()->readBoolEntry( "HiddenOnExit", false ) )
    {
        mainWindow()->show();

        //takes longer but feels shorter. Crazy eh? :)
        kapp->eventLoop()->processEvents( QEventLoop::ExcludeUserInput );
    }


    { //<Engine>
        //TODO move loading of engine and initEngine() to engineController; it can handle things better

        EngineBase *engine = EngineController::engine();
        const bool b = QCString( engine->name() ) == "Dummy";

        if( b || AmarokConfig::soundSystem() != PluginManager::getService( engine )->name() )
        {
            if( !b ) PluginManager::unload( engine );

            initEngine();
            engine = EngineController::engine();

            AmarokConfig::setHardwareMixer( engine->initMixer( AmarokConfig::hardwareMixer() ) );
        }
        else if( AmarokConfig::hardwareMixer() != engine->isMixerHardware() )
            AmarokConfig::setHardwareMixer( engine->initMixer( AmarokConfig::hardwareMixer() ) );

         engine->setSoundOutput( AmarokConfig::soundOutput() );
         engine->setSoundDevice( AmarokConfig::soundDevice() );
         engine->setDefaultSoundDevice( !AmarokConfig::customSoundDevice() );
         engine->setRestoreEffects( AmarokConfig::rememberEffects() );
         engine->setVolume( AmarokConfig::masterVolume() );
         //TODO deprecate/improve
         engine->setXfadeLength( AmarokConfig::crossfade() ? AmarokConfig::crossfadeLength() : 0 );
    } //</Engine>

    kdDebug() << "END " << k_funcinfo << endl;
}


void App::setupColors()
{
    //TODO move to PlaylistWindow?

    if( AmarokConfig::schemeKDE() )
    {
        QObject* const browserBar = m_pPlaylistWindow->child( "BrowserBar" );
        QObjectList* const list = browserBar->queryList( "QWidget" );
        list->prepend( browserBar );

        for( QObject *o = list->first(); o; o = list->next() )
        {
            //We have to unset the palette due to BrowserWin::setColors() setting
            //some widgets' palettes, and thus they won't propagate the changes

            static_cast<QWidget*>(o)->unsetPalette();

            if( o->inherits( "KListView" ) )
            {
                //TODO find out how KListView alternate colors are updated when a
                //     control center colour change is made

                static_cast<KListView*>(o)->setAlternateBackground( KGlobalSettings::alternateBackgroundColor() );
            }
        }

        delete list;

    } else if( AmarokConfig::schemeAmarok() ) {

        QColorGroup group = QApplication::palette().active();
        const QColor bg( 32, 32, 80 );
        const QColor bgAlt( 57, 64, 98 );

        /* PLEASE don't do this, it makes lots of widget ugly
         * instead customise BrowserWin::setColors();
         * OR figure out wicked colour scheme!
         */
        //group.setColor( QColorGroup::Foreground, Qt::white );

        group.setColor( QColorGroup::Text, Qt::white );
        group.setColor( QColorGroup::Base, bg );
        group.setColor( QColorGroup::Background, bg.light(120) );

        group.setColor( QColorGroup::Highlight, Qt::white );
        group.setColor( QColorGroup::HighlightedText, bg );
        group.setColor( QColorGroup::BrightText, QColor( 0xff, 0x40, 0x40 ) ); //GlowColor

        int h,s,v;
        bgAlt.getHsv( &h, &s, &v );
        group.setColor( QColorGroup::Midlight, QColor( h, s/3, (int)(v * 1.2), QColor::Hsv ) ); //column separator in playlist

        //TODO set all colours, even button colours, that way we can change the dark,
        //light, etc. colours and amaroK scheme will look much better

        m_pPlaylistWindow->setColors( QPalette( group, group, group ), bgAlt );

    } else {

        // we try to be smart: this code figures out contrasting colors for
        // selection and alternate background rows
        QColorGroup group = QApplication::palette().active();
        const QColor fg( AmarokConfig::playlistWindowFgColor() );
        const QColor bg( AmarokConfig::playlistWindowBgColor() );
        int h, s, v;

        //TODO use the ensureContrast function you devised in BlockAnalyzer

        bg.hsv( &h, &s, &v );
        v += (v < 128) ? +50 : -50;
        v &= 255; //ensures 0 <= v < 256
        QColor bgAlt( h, s, v, QColor::Hsv );

        fg.hsv( &h, &s, &v );
        v += (v < 128) ? +150 : -150;
        v &= 255; //ensures 0 <= v < 256
        QColor highlight( h, s, v, QColor::Hsv );

        group.setColor( QColorGroup::Base, bg );
        group.setColor( QColorGroup::Background, bg.dark( 115 ) );
        group.setColor( QColorGroup::Text, fg );
        group.setColor( QColorGroup::Highlight, highlight );
        group.setColor( QColorGroup::HighlightedText, Qt::white );
        group.setColor( QColorGroup::Dark, Qt::darkGray );
        group.setColor( QColorGroup::BrightText, QColor( 0xff, 0x40, 0x40 ) ); //GlowColor

        m_pPlaylistWindow->setColors( QPalette( group, group, group ), bgAlt );
    }
}


void App::genericEventHandler( QWidget *source, QEvent *e )
{
    //this is used as a generic event handler for widgets that want to handle
    //typical events in an amaroK fashion

    //to use it just pass the event eg:
    //
    // void Foo::barEvent( QBarEvent *e )
    // {
    //     pApp->genericEventHandler( this, e );
    // }

    switch( e->type() )
    {
    case QEvent::DragEnter:
        #define e static_cast<QDropEvent*>(e)
        e->accept( KURLDrag::canDecode( e ) );
        break;

    case QEvent::Drop:
        if( KURLDrag::canDecode( e ) )
        {
            QPopupMenu popup;
            //FIXME this isn't a good way to determine if there is a currentTrack, need playlist() function
            const bool b = EngineController::engine()->loaded();

            popup.insertItem( i18n( "&Append to playlist" ), 101 );
            popup.insertItem( i18n( "Append and &play" ), 102 );
            if( b ) popup.insertItem( i18n( "&Queue after current track" ), 103 );
            popup.insertSeparator();
            popup.insertItem( i18n( "&Cancel" ), 0 );

            const int id = popup.exec( source->mapToGlobal( e->pos() ) );
            KURL::List list;
            KURLDrag::decode( e, list );

            switch( id )
            {
            case 101:
            case 102:
                Playlist::instance()->appendMedia( list, id == 102 );

            case 103:
                Playlist::instance()->queueMedia( list );
            }
        }
        #undef e

        break;

    case QEvent::Wheel:
    {
        #define e static_cast<QWheelEvent*>(e)
        const bool up = e->delta() > 0;

        switch( e->state() )
        {
        case ControlButton:
            if( up ) EngineController::instance()->previous();
            else     EngineController::instance()->next();
            break;

        default:
            EngineController::instance()->increaseVolume( e->delta() / 18 );
        }

        e->accept();
        #undef e

        break;
    }

    case QEvent::Close:

        //KDE policy states we should hide to tray and not quit() when the
        //close window button is pushed for the main widget

        static_cast<QCloseEvent*>(e)->accept(); //if we don't do this the info box appears on quit()!

        if( AmarokConfig::showTrayIcon() && !e->spontaneous() && !sessionSaving() )
        {
            KMessageBox::information( source,
                i18n( "<qt>Closing the main-window will keep amaroK running in the System Tray. "
                      "Use <B>Quit</B> from the menu, or the amaroK tray-icon to exit the application.</qt>" ),
                i18n( "Docking in System Tray" ), "hideOnCloseInfo" );
        }
        else quit();

        break;

    default:
        break;
    }
}


void App::engineStateChanged( EngineBase::EngineState state )
{
    switch( state )
    {
        case EngineBase::Empty:
            QToolTip::add( m_pTray, i18n( "amaroK - Audio Player" ) );
            break;

        default:
            break;
    }
}


void App::engineNewMetaData( const MetaBundle &bundle, bool /*trackChanged*/ )
{
    m_pOSD->showTrack( bundle );

    TrackToolTip::add( m_pTray, bundle );
}


void App::engineVolumeChanged( int newVolume )
{
    m_pOSD->showOSD( i18n("Volume: %1%").arg( newVolume ), true );
}

void App::slotConfigEffects( bool show )
{
    if( show )
    {
        if( !EffectWidget::self )
        {
            //safe even if m_pPlayerWindow is 0
            connect( EffectWidget::self, SIGNAL(destroyed()), m_pPlayerWindow, SLOT(setEffectsWindowShown()) );
            EffectWidget::self = new EffectWidget( m_pPlaylistWindow );
        }
        else EffectWidget::self->raise();
    }
    else delete EffectWidget::self; //will set self = 0 in its dtor

    if( m_pPlayerWindow ) m_pPlayerWindow->setEffectsWindowShown( show );
}


void App::slotConfigAmarok()
{
    if( !KConfigDialog::showDialog( "settings" ) )
    {
        //KConfigDialog didn't find an instance of this dialog, so lets create it :
        KConfigDialog* dialog = new AmarokConfigDialog( m_pPlaylistWindow, "settings", AmarokConfig::self() );

        connect( dialog, SIGNAL(settingsChanged()), SLOT(applySettings()) );

        dialog->show();
    }
}

void App::slotConfigShortcuts()
{
    KKeyDialog::configure( actionCollection(), m_pPlaylistWindow );
}

void App::slotConfigGlobalShortcuts()
{
    KKeyDialog::configure( m_pGlobalAccel, true, m_pPlaylistWindow, true );
}

void App::slotConfigToolBars()
{
    PlaylistWindow* const pw = playlistWindow();
    KEditToolbar dialog( pw->actionCollection(), pw->xmlFile(), true, pw );

    dialog.showButtonApply( false );

    if( dialog.exec() )
    {
        playlistWindow()->reloadXML();
        playlistWindow()->createGUI();
    }
}


//globally available actionCollection and playlist retrieval functions

KActionCollection *App::actionCollection() const
{
    return m_pPlaylistWindow->actionCollection();
}

QWidget *App::mainWindow() const
{
    //mainWindow varies, honest!
    return AmarokConfig::showPlayerWindow() ? (QWidget*)m_pPlayerWindow : (QWidget*)m_pPlaylistWindow;
}

namespace amaroK
{
    KConfig *config( const QString &group )
    {
        //Slightly more useful config() that allows setting the group simultaneously

        kapp->config()->setGroup( group );
        return kapp->config();
    }
}

#include "app.moc"
