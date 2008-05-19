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
#include "App.h"

#include "Amarok.h"
#include "amarokconfig.h"
#include "amarokdbushandler.h"
#include "atomicstring.h"
#include "CollectionManager.h"
#include "ConfigDialog.h"
#include "context/ContextView.h"
#include "Debug.h"
#include "EngineController.h"
#include "equalizersetup.h"
#include "MainWindow.h"
#include "mediabrowser.h"
#include "Meta.h"
#include "meta/MetaConstants.h"
#include "mountpointmanager.h"
#include "Osd.h"
#include "playlist/PlaylistModel.h"
#include "PluginManager.h"
#include "refreshimages.h"
#include "scriptmanager.h"
#include "ContextStatusBar.h"
#include "systray.h"
#include "TrackTooltip.h"        //engineNewMetaData()
#include "TheInstances.h"
#include "metadata/tplugins.h"

#include <iostream>

#include <KAboutData>
#include <KAction>
#include <KCmdLineArgs>        //initCliArgs()
#include <KConfigDialogManager>
#include <KCursor>             //Amarok::OverrideCursor
#include <KEditToolBar>        //slotConfigToolbars()
#include <KGlobalSettings>     //applyColorScheme()
#include <KIO/CopyJob>
#include <KIconLoader>         //amarok Icon
#include <KJob>
#include <KJobUiDelegate>
#include <KLocale>
#include <KRun>                //Amarok::invokeBrowser()
#include <KShell>
#include <KShortcutsDialog>     //slotConfigShortcuts()
#include <KSplashScreen>
#include <KStandardDirs>

#include <QDBusInterface>
#include <QDBusReply>
#include <QEventLoop>          //applySettings()
#include <QFile>
#include <QPixmapCache>
#include <QTimer>              //showHyperThreadingWarning()
#include <QToolTip>            //default tooltip for trayicon
#include <QtDBus/QtDBus>

QMutex Debug::mutex;
QMutex Amarok::globalDirsMutex;

int App::mainThreadId = 0;

#ifdef Q_WS_MAC
#include <CoreFoundation/CoreFoundation.h>
extern void setupEventHandler_mac(long);
#endif

#define AMAROK_CAPTION "Amarok 2 alpha"

AMAROK_EXPORT KAboutData aboutData( "amarok", 0,
    ki18n( "Amarok" ), APP_VERSION,
    ki18n( "The audio player for KDE" ), KAboutData::License_GPL,
    ki18n( "(C) 2002-2003, Mark Kretschmann\n(C) 2003-2008, The Amarok Development Squad" ),
    ki18n( "IRC:\nirc.freenode.net - #amarok, #amarok.de, #amarok.es, #amarok.fr\n\nFeedback:\namarok@kde.org\n\n(Build Date: " __DATE__ ")" ),
             ( "http://amarok.kde.org" ) );

App::App()
        : KUniqueApplication()
        , m_splash( 0 )
{
    DEBUG_BLOCK
    PERF_LOG( "Begin Application ctor" )

    if( AmarokConfig::showSplashscreen() )
    {
        PERF_LOG( "Init KStandardDirs cache" )
        KStandardDirs *stdDirs = KGlobal::dirs();
        PERF_LOG( "Finding image" )
        QString img = stdDirs->findResource( "data", "amarok/images/splash_screen.jpg" );
        PERF_LOG( "Creating pixmap" )
        QPixmap splashpix( img );
        PERF_LOG( "Creating splashscreen" )
        m_splash = new KSplashScreen( splashpix, Qt::WindowStaysOnTopHint );
        PERF_LOG( "showing splashscreen" )
        m_splash->show();
    }

    PERF_LOG( "Registering taglib plugins" )
    registerTaglibPlugins();
    PERF_LOG( "Done Registering taglib plugins" )

    qRegisterMetaType<Meta::DataPtr>();
    qRegisterMetaType<Meta::DataList>();
    qRegisterMetaType<Meta::TrackPtr>();
    qRegisterMetaType<Meta::TrackList>();
    qRegisterMetaType<Meta::AlbumPtr>();
    qRegisterMetaType<Meta::AlbumList>();
    qRegisterMetaType<Meta::ArtistPtr>();
    qRegisterMetaType<Meta::ArtistList>();
    qRegisterMetaType<Meta::GenrePtr>();
    qRegisterMetaType<Meta::GenreList>();
    qRegisterMetaType<Meta::ComposerPtr>();
    qRegisterMetaType<Meta::ComposerList>();
    qRegisterMetaType<Meta::YearPtr>();
    qRegisterMetaType<Meta::YearList>();


    //make sure we have enough cache space for all our crazy svg stuff
    QPixmapCache::setCacheLimit ( 20 * 1024 );

#ifdef Q_WS_MAC
    // this is inspired by OpenSceneGraph: osgDB/FilePath.cpp

    // Start with the the Bundle PlugIns directory.

    // Get the main bundle first. No need to retain or release it since
    //  we are not keeping a reference
    CFBundleRef myBundle = CFBundleGetMainBundle();
    if( myBundle )
    {
        // CFBundleGetMainBundle will return a bundle ref even if
        //  the application isn't part of a bundle, so we need to
        //  check
        //  if the path to the bundle ends in ".app" to see if it is
        //  a
        //  proper application bundle. If it is, the plugins path is
        //  added
        CFURLRef urlRef = CFBundleCopyBundleURL(myBundle);
        if(urlRef)
        {
            char bundlePath[1024];
            if( CFURLGetFileSystemRepresentation( urlRef, true, (UInt8 *)bundlePath, sizeof(bundlePath) ) )
            {
                QByteArray bp( bundlePath );
                size_t len = bp.length();
                if( len > 4 && bp.right( 4 ) == ".app" )
                {
                    bp.append( "/Contents/MacOS" );
                    QByteArray path = qgetenv( "PATH" );
                    if( path.length() > 0 )
                    {
                        path.prepend( ":" );
                    }
                    path.prepend( bp );
                    debug() << "setting PATH=" << path;
                    setenv("PATH", path, 1);
                }
            }
            // docs say we are responsible for releasing CFURLRef
            CFRelease(urlRef);
        }
    }
#endif

    PERF_LOG( "Creating DBus handlers" )
    //needs to be created before the wizard
     new Amarok::DbusPlayerHandler(); // Must be created first
     new Amarok::DbusPlaylistHandler();
     new Amarok::DbusPlaylistBrowserHandler();
     new Amarok::DbusContextHandler();
     new Amarok::DbusCollectionHandler();
//     new Amarok::DbusMediaBrowserHandler();
     new Amarok::DbusScriptHandler();
     PERF_LOG( "Done creating DBus handlers" )

    // tell AtomicString that this is the GUI thread
    if ( !AtomicString::isMainThread() )
        qWarning("AtomicString was initialized from a thread other than the GUI "
                 "thread. This could lead to memory leaks.");

#ifdef Q_WS_MAC
    setupEventHandler_mac((long)this);
#endif
    QDBusConnection::sessionBus().registerService("org.kde.amarok");
    QTimer::singleShot( 0, this, SLOT( continueInit() ) );
    PERF_LOG( "Done App ctor" )
}

App::~App()
{
    DEBUG_BLOCK

    delete m_splash;
    m_splash = 0;

    // Hiding the OSD before exit prevents crash
    Amarok::OSD::instance()->hide();

    if ( AmarokConfig::resumePlayback() ) {
        if ( The::engineController()->state() != Phonon::StoppedState ) {
            Meta::TrackPtr track = The::engineController()->currentTrack();
            if( track )
            {
                AmarokConfig::setResumeTrack( track->playableUrl().prettyUrl() );
                AmarokConfig::setResumeTime( The::engineController()->trackPosition() );
            }
        }
        else AmarokConfig::setResumeTrack( QString() ); //otherwise it'll play previous resume next time!
    }

    The::engineController()->endSession(); //records final statistics
    The::engineController()->detach( this );

    // do even if trayicon is not shown, it is safe
    Amarok::config().writeEntry( "HiddenOnExit", mainWindow()->isHidden() );

    // this must be deleted before the connection to the Xserver is
    // severed, or we risk a crash when the QApplication is exited,
    // I asked Trolltech! *smug*
    delete Amarok::OSD::instance();

    AmarokConfig::setVersion( APP_VERSION );
    AmarokConfig::self()->writeConfig();

    mainWindow()->deleteBrowsers();
    delete mainWindow();

#ifdef Q_WS_WIN
    // work around for KUniqueApplication being not completely implemented on windows
    QDBusConnectionInterface* dbusService;
    if (QDBusConnection::sessionBus().isConnected() && (dbusService = QDBusConnection::sessionBus().interface()))
        dbusService->unregisterService("org.kde.amarok");
#endif
}


#include <QStringList>

namespace
{
    // grabbed from KsCD source, kompatctdisk.cpp
    QString urlToDevice(const QString& device)
    {
        KUrl deviceUrl(device);
        if (deviceUrl.protocol() == "media" || deviceUrl.protocol() == "system")
        {
            debug() << "WARNING: urlToDevice needs to be reimplemented with KDE4 technology, it is just a stub at the moment";
           QDBusInterface mediamanager( "org.kde.kded", "/modules/mediamanager", "org.kde.MediaManager" );
           QDBusReply<QStringList> reply = mediamanager.call( "properties",deviceUrl.fileName() );
           if (!reply.isValid()) {
        debug() << "Invalid reply from mediamanager";
               return QString();
           }
       QStringList properties = reply;
       if( properties.count()< 6 )
        return QString();
      debug() << "Reply from mediamanager " << properties[5];
          return properties[5];
        }

        return device;
    }

}


void App::handleCliArgs() //static
{
    DEBUG_BLOCK

    KCmdLineArgs* const args = KCmdLineArgs::parsedArgs();

    if ( args->isSet( "cwd" ) )
    {
        KCmdLineArgs::setCwd( args->getOption( "cwd" ).toLocal8Bit() );
    }

    bool haveArgs = false;
    if ( args->count() > 0 )
    {
        haveArgs = true;

        KUrl::List list;
        for( int i = 0; i < args->count(); i++ )
        {
            KUrl url = args->url( i );
            //TODO:PORTME
//             if( url.protocol() == "itpc" || url.protocol() == "pcast" )
//                 PlaylistBrowserNS::instance()->addPodcast( url );
//             else
                list << url;
                debug() << "here!!!!!!!!";
        }

        int options = Playlist::AppendAndPlay;
        if( args->isSet( "queue" ) )
           options = Playlist::Queue;
        else if( args->isSet( "append" ) )
           options = Playlist::Append;
        else if( args->isSet( "load" ) )
            options = Playlist::Replace;

        if( args->isSet( "play" ) )
            options |= Playlist::DirectPlay;

        Meta::TrackList tracks = CollectionManager::instance()->tracksForUrls( list );
        The::playlistModel()->insertOptioned( tracks, options );
    }

    //we shouldn't let the user specify two of these since it is pointless!
    //so we prioritise, pause > stop > play > next > prev
    //thus pause is the least destructive, followed by stop as brakes are the most important bit of a car(!)
    //then the others seemed sensible. Feel free to modify this order, but please leave justification in the cvs log
    //I considered doing some sanity checks (eg only stop if paused or playing), but decided it wasn't worth it
    else if ( args->isSet( "pause" ) )
    {
        haveArgs = true;
        The::engineController()->pause();
    }
    else if ( args->isSet( "stop" ) )
    {
        haveArgs = true;
        The::engineController()->stop();
    }
    else if ( args->isSet( "play-pause" ) )
    {
        haveArgs = true;
        The::engineController()->playPause();
    }
    else if ( args->isSet( "play" ) ) //will restart if we are playing
    {
        haveArgs = true;
        The::engineController()->play();
    }
    else if ( args->isSet( "next" ) )
    {
        haveArgs = true;
        The::playlistModel()->next();
    }
    else if ( args->isSet( "previous" ) )
    {
        haveArgs = true;
        The::playlistModel()->back();
    }
    else if (args->isSet("cdplay"))
    {
        haveArgs = true;
        QString device = args->getOption("cdplay");
        KUrl::List urls;
        if (The::engineController()->getAudioCDContents(device, urls)) {
            Meta::TrackList tracks = CollectionManager::instance()->tracksForUrls( urls );
            The::playlistModel()->insertOptioned(
                tracks, Playlist::Replace|Playlist::DirectPlay);
        } else { // Default behaviour
            debug() <<
                "Sorry, the engine does not support direct play from AudioCD..."
                   ;
        }
    }

    if ( args->isSet( "toggle-playlist-window" ) )
    {
        haveArgs = true;
        pApp->mainWindow()->showHide();
    }

    //FIXME Debug output always enabled for now. MUST BE REVERTED BEFORE RELEASE.
    Amarok::config().writeEntry( "Debug Enabled", true );
    //Amarok::config().writeEntry( "Debug Enabled", args->isSet( "debug" ) );

    static bool firstTime = true;
    if( !firstTime && !haveArgs )
        pApp->mainWindow()->activate();
    firstTime = false;

    args->clear();    //free up memory

}


/////////////////////////////////////////////////////////////////////////////////////
// INIT
/////////////////////////////////////////////////////////////////////////////////////

void App::initCliArgs( int argc, char *argv[] )
{
    KCmdLineArgs::reset();
    KCmdLineArgs::init( argc, argv, &::aboutData ); //calls KCmdLineArgs::addStdCmdLineOptions()
    initCliArgs();
}

void App::initCliArgs() //static
{

    KCmdLineOptions options;
    options.add("+[URL(s)]", ki18n( "Files/URLs to open" ));
    options.add("r");
    options.add("previous", ki18n( "Skip backwards in playlist" ));
    options.add("p");
    options.add("play", ki18n( "Start playing current playlist" ));
    options.add("t");
    options.add("play-pause", ki18n( "Play if stopped, pause if playing" ));
    options.add("pause", ki18n( "Pause playback" ));
    options.add("s");
    options.add("stop", ki18n( "Stop playback" ));
    options.add("f");
    options.add("next", ki18n( "Skip forwards in playlist" ));
    options.add(":", ki18n("Additional options:"));
    options.add("a");
    options.add("append", ki18n( "Append files/URLs to playlist" ));
    options.add("queue", ki18n("Queue URLs after the currently playing track"));
    options.add("l");
    options.add("load", ki18n("Load URLs, replacing current playlist"));
    options.add("d");
    options.add("debug", ki18n("Print verbose debugging information"));
    options.add("m");
    options.add("toggle-playlist-window", ki18n("Toggle the Playlist-window"));
    options.add("wizard", ki18n( "Run first-run wizard" ));
    options.add("cwd <directory>", ki18n( "Base for relative filenames/URLs" ));
    options.add("cdplay <device>", ki18n("Play an AudioCD from <device> or system:/media/<device>"));
    KCmdLineArgs::addCmdLineOptions( options );   //add our own options

}


/////////////////////////////////////////////////////////////////////////////////////
// METHODS
/////////////////////////////////////////////////////////////////////////////////////

#include <id3v1tag.h>
#include <tbytevector.h>
#include <QTextCodec>
#include <KGlobal>

//this class is only used in this module, so I figured I may as well define it
//here and save creating another header/source file combination

class ID3v1StringHandler : public TagLib::ID3v1::StringHandler
{
    QTextCodec *m_codec;

    virtual TagLib::String parse( const TagLib::ByteVector &data ) const
    {
        return QStringToTString( m_codec->toUnicode( data.data(), data.size() ) );
    }

    virtual TagLib::ByteVector render( const TagLib::String &ts ) const
    {
        const QByteArray qcs = m_codec->fromUnicode( TStringToQString(ts) );
        return TagLib::ByteVector( qcs, (uint) qcs.length() );
    }

public:
    ID3v1StringHandler( int codecIndex )
            : m_codec( QTextCodec::codecForName( QTextCodec::availableCodecs().at( codecIndex ) ) )
    {
        debug() << "codec: " << m_codec;
        debug() << "codec-name: " << m_codec->name();
    }

    ID3v1StringHandler( QTextCodec *codec )
            : m_codec( codec )
    {
        debug() << "codec: " << m_codec;
        debug() << "codec-name: " << m_codec->name();
    }

    virtual ~ID3v1StringHandler()
    {}
};

//SLOT
void App::applySettings( bool firstTime )
{
    ///Called when the configDialog is closed with OK or Apply

    DEBUG_BLOCK

#ifndef Q_WS_MAC
    //probably needs to be done in TrayIcon when it receives a QEvent::ToolTip (see QSystemtrayIcon documentation)
//     TrackToolTip::instance()->removeFromWidget( m_tray );
#endif
    Amarok::OSD::instance()->applySettings();
    m_tray->setVisible( AmarokConfig::showTrayIcon() );
//     TrackToolTip::instance()->addToWidget( m_tray );


    //on startup we need to show the window, but only if it wasn't hidden on exit
    //and always if the trayicon isn't showing
    QWidget* main_window = mainWindow();


    if( ( main_window && firstTime && !Amarok::config().readEntry( "HiddenOnExit", false ) ) || ( main_window && !AmarokConfig::showTrayIcon() ) )
    {
        PERF_LOG( "showing main window again" )
        main_window->show();
        PERF_LOG( "after showing mainWindow" )
    }


    { //<Engine>
        The::engineController()->setVolume( AmarokConfig::masterVolume() );

#if 0
        EngineBase *engine = EngineController::engine();

        if( firstTime || AmarokConfig::soundSystem() !=
                         PluginManager::getService( engine )->property( "X-KDE-Amarok-name" ).toString() )
        {
            //will unload engine for us first if necessary
            engine = The::engineController()->loadEngine();
            PERF_LOG( "done loading engine" )
        }

        engine->setXfadeLength( AmarokConfig::crossfade() ? AmarokConfig::crossfadeLength() : 0 );
        engine->setEqualizerEnabled( AmarokConfig::equalizerEnabled() );
        if ( AmarokConfig::equalizerEnabled() )
            engine->setEqualizerParameters( AmarokConfig::equalizerPreamp(), AmarokConfig::equalizerGains() );
#endif
        Amarok::actionCollection()->action("play_audiocd")->setEnabled( false );

    } //</Engine>

    {   // delete unneeded cover images from cache
        PERF_LOG( "Begin cover handling" )
        const QString size = QString::number( AmarokConfig::coverPreviewSize() ) + '@';
        const QDir cacheDir = Amarok::saveLocation( "albumcovers/cache/" );
        const QStringList obsoleteCovers = cacheDir.entryList( QStringList("*") );
        foreach( const QString &it, obsoleteCovers )
            if ( !it.startsWith( size  ) && !it.startsWith( "50@" ) && !it.startsWith( "32@" ) )
                QFile( cacheDir.filePath( it ) ).remove();
        PERF_LOG( "done cover handling" )
    }

    //if ( !firstTime )
        // Bizarrely and ironically calling this causes crashes for
        // some people! FIXME
        //AmarokConfig::self()->writeConfig();

}

//SLOT
void
App::continueInit()
{
    DEBUG_BLOCK

    PERF_LOG( "Begin App::continueInit" )
    const KCmdLineArgs* const args = KCmdLineArgs::parsedArgs();
    bool restoreSession = args->count() == 0 || args->isSet( "append" ) || args->isSet( "queue" )
                                || Amarok::config().readEntry( "AppendAsDefault", false );

    PERF_LOG( "Starting moodserver" )
    // Make this instance so it can start receiving signals
            //FIXME: REENABLE When the moodserver is ported.
//     MoodServer::instance();
    PERF_LOG( "Done starting mood server" )

    // Remember old folder setup, so we can detect changes after the wizard was used
    //const QStringList oldCollectionFolders = MountPointManager::instance()->collectionFolders();


    // Is this needed in Amarok 2?
    if( Amarok::config().readEntry( "First Run", true ) || args->isSet( "wizard" ) )
    {
        std::cout << "STARTUP\n" << std::flush; //hide the splashscreen
        Amarok::config().writeEntry( "First Run", false );
        Amarok::config().sync();
    }

    PERF_LOG( "Creating MainWindow" )
    m_mainWindow = new MainWindow();
    PERF_LOG( "Done creating MainWindow" )

    m_tray           = new Amarok::TrayIcon( mainWindow() );

    PERF_LOG( "Start init of MainWindow" )
    mainWindow()->init(); //creates the playlist, browsers, etc.
    PERF_LOG( "Init of MainWindow done" )
    //DON'T DELETE THIS NEXT LINE or the app crashes when you click the X (unless we reimplement closeEvent)
    //Reason: in ~App we have to call the deleteBrowsers method or else we run afoul of refcount foobar in KHTMLPart
    //But if you click the X (not Action->Quit) it automatically kills MainWindow because KMainWindow sets this
    //for us as default (bad KMainWindow)
    mainWindow()->setAttribute( Qt::WA_DeleteOnClose, false );
    //init playlist window as soon as the database is guaranteed to be usable

    //create engine, show TrayIcon etc.
    applySettings( true );
    // Start ScriptManager. Must be created _after_ MainWindow.
    PERF_LOG( "Starting ScriptManager" )
    ScriptManager::instance();
    PERF_LOG( "ScriptManager started" )

    //load previous playlist in separate thread
    //FIXME: causes a lot of breakage due to the collection not being properly initialized at startup.
    //Reenable when fixed.

    //Fixed! I think....
            if ( restoreSession && AmarokConfig::savePlaylist() )
    {
        The::playlistModel()->restoreSession();
    }

    //do after applySettings(), or the OSD will flicker and other wierdness!
    //do before restoreSession()!
    The::engineController()->attach( this );
    //set a default interface
    engineStateChanged( Phonon::StoppedState );
    PERF_LOG( "Engine state changed" )
    if ( AmarokConfig::resumePlayback() && restoreSession && !args->isSet( "stop" ) ) {
        //restore session as long as the user didn't specify media to play etc.
        //do this after applySettings() so OSD displays correctly
        The::engineController()->restoreSession();
    }

    PERF_LOG( "before cover refresh" )
    // Refetch covers every 80 days to comply with Amazon license
    new RefreshImages();

   /* if ( AmarokConfig::monitorChanges() )
        CollectionManager::instance()->checkCollectionChanges();
   */

    handleCliArgs();

    delete m_splash;
    m_splash = 0;
    PERF_LOG( "App init done" )
}

void App::engineStateChanged( Phonon::State state, Phonon::State oldState )
{
    Meta::TrackPtr track = The::engineController()->currentTrack();
    //track is 0 if the engien state is Empty. we check that in the switch
    switch( state )
    {
    case Phonon::StoppedState:
    case Phonon::LoadingState:
        mainWindow()->setPlainCaption( i18n( AMAROK_CAPTION ) );
        TrackToolTip::instance()->clear();
        Amarok::OSD::instance()->setImage( QImage( KIconLoader::global()->iconPath( "amarok", -KIconLoader::SizeHuge ) ) );
        break;

    case Phonon::PlayingState:
        if ( oldState == Phonon::PausedState )
            Amarok::OSD::instance()->OSDWidget::show( i18nc( "state, as in playing", "Play" ) );
        if ( !track->prettyName().isEmpty() )
//             //TODO: write a utility function somewhere
            mainWindow()->setPlainCaption( i18n( "%1 - %2 -  %3", track->artist() ? track->artist()->prettyName() : i18n( "Unknown"), track->prettyName(), AMAROK_CAPTION ) );
        break;

    case Phonon::PausedState:
        mainWindow()->setPlainCaption( i18n( "Paused  -  %1", QString( AMAROK_CAPTION ) ) );
        Amarok::OSD::instance()->OSDWidget::show( i18n("Paused") );
        break;

    case Phonon::ErrorState:
    case Phonon::BufferingState:
        break;

    default:
        ;
    }
}

void App::engineNewMetaData( const QHash<qint64, QString> &newMetaData, bool trackChanged )
{
    DEBUG_BLOCK

    Meta::TrackPtr currentTrack = The::engineController()->currentTrack();
    Amarok::OSD::instance()->show( currentTrack );
    if( !trackChanged )
    {
        if ( !currentTrack->prettyName().isEmpty() )
           mainWindow()->setPlainCaption( i18n( "%1 - %2 -  %3", newMetaData.value( Meta::valArtist ), newMetaData.value( Meta::valTitle ), AMAROK_CAPTION ) );
    }

    TrackToolTip::instance()->setTrack( currentTrack );
}

void App::engineTrackPositionChanged( long position, bool /*userSeek*/ )
{
    TrackToolTip::instance()->setPos( position );
}

void App::engineVolumeChanged( int newVolume )
{
    Amarok::OSD::instance()->OSDWidget::volChanged( newVolume );
}

void App::slotConfigEqualizer() //SLOT
{
    EqualizerSetup::instance()->show();
    EqualizerSetup::instance()->raise();
}


void App::slotConfigAmarok( const QByteArray& page )
{

    Amarok2ConfigDialog* dialog = static_cast<Amarok2ConfigDialog*>( KConfigDialog::exists( "settings" ) );

    if( !dialog )
    {
        //KConfigDialog didn't find an instance of this dialog, so lets create it :
        dialog = new Amarok2ConfigDialog( mainWindow(), "settings", AmarokConfig::self() );

        connect( dialog, SIGNAL(settingsChanged(const QString&)), SLOT(applySettings()) );
    }

    //FIXME it seems that if the dialog is on a different desktop it gets lost
    //      what do to? detect and move it?

//    if ( page.isNull() )
          // FIXME
//        dialog->showPage( AmarokConfigDialog::s_currentPage );
//    else
        dialog->showPageByName( page );

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void App::slotConfigShortcuts()
{
    KShortcutsDialog::configure( Amarok::actionCollection(), KShortcutsEditor::LetterShortcutsAllowed, mainWindow() );
}

void App::slotConfigToolBars()
{
    KEditToolBar dialog( Amarok::actionCollection(), mainWindow() );
    dialog.setResourceFile( mainWindow()->xmlFile() );

    dialog.showButton( KEditToolBar::Apply, false );

//     if( dialog.exec() )
//     {
//         mainWindow()->reloadXML();
//         mainWindow()->createGUI();
//     }
}

void App::setUseScores( bool use )
{
    AmarokConfig::setUseScores( use );
    emit useScores( use );
}

void App::setUseRatings( bool use )
{
    AmarokConfig::setUseRatings( use );
    emit useRatings( use );
}

void App::setMoodbarPrefs( bool show, bool moodier, int alter, bool withMusic )
{
    AmarokConfig::setShowMoodbar( show );
    AmarokConfig::setMakeMoodier( moodier );
    AmarokConfig::setAlterMood( alter );
    AmarokConfig::setMoodsWithMusic( withMusic );
    emit moodbarPrefs( show, moodier, alter, withMusic );
}

KIO::Job *App::trashFiles( const KUrl::List &files )
{
    KIO::Job *job = KIO::trash( files );
    Amarok::ContextStatusBar::instance()->newProgressOperation( job ).setDescription( i18n("Moving files to trash") );
    connect( job, SIGNAL( result( KJob* ) ), this, SLOT( slotTrashResult( KJob* ) ) );
    return job;
}

void App::slotTrashResult( KJob *job )
{
    if( job->error() )
        job->uiDelegate()->showErrorMessage();
}

void App::quit()
{
    emit prepareToQuit();
    if( MediaBrowser::instance() && MediaBrowser::instance()->blockQuit() )
    {
        // don't quit yet, as some media devices still have to finish transferring data
        QTimer::singleShot( 100, this, SLOT( quit() ) );
        return;
    }
    KApplication::quit();
}

namespace Amarok
{
    /// @see amarok.h

    QWidget *mainWindow()
    {
        return pApp->mainWindow();
    }

    KActionCollection *actionCollection()
    {
        return pApp->mainWindow()->actionCollection();
    }

    KConfigGroup config( const QString &group )
    {
        //Slightly more useful config() that allows setting the group simultaneously
        return KGlobal::config()->group( group );
    }

    bool invokeBrowser( const QString& url )
    {
        //URL can be in whatever forms KUrl understands - ie most.
        const QString cmd = KShell::quoteArg(AmarokConfig::externalBrowser())
            + ' ' + KShell::quoteArg(KUrl( url ).url());
        return KRun::runCommand( cmd, 0L ) > 0;
    }

    namespace ColorScheme
    {
        QColor Base;
        QColor Text;
        QColor Background;
        QColor Foreground;
        QColor AltBase;
    }

    OverrideCursor::OverrideCursor( Qt::CursorShape cursor )
    {
        QApplication::setOverrideCursor( cursor == Qt::WaitCursor ?
                                        Qt::WaitCursor :
                                        Qt::BusyCursor );
    }

    OverrideCursor::~OverrideCursor()
    {
        QApplication::restoreOverrideCursor();
    }

    QString saveLocation( const QString &directory )
    {
        globalDirsMutex.lock();
        QString result = KGlobal::dirs()->saveLocation( "data", QString("amarok/") + directory, true );
        globalDirsMutex.unlock();
        return result;
    }

    QString cleanPath( const QString &path )
    {
        QString result = path;
        // german umlauts
        result.replace( QChar(0x00e4), "ae" ).replace( QChar(0x00c4), "Ae" );
        result.replace( QChar(0x00f6), "oe" ).replace( QChar(0x00d6), "Oe" );
        result.replace( QChar(0x00fc), "ue" ).replace( QChar(0x00dc), "Ue" );
        result.replace( QChar(0x00df), "ss" );

        // some strange accents
        result.replace( QChar(0x00e7), "c" ).replace( QChar(0x00c7), "C" );
        result.replace( QChar(0x00fd), "y" ).replace( QChar(0x00dd), "Y" );
        result.replace( QChar(0x00f1), "n" ).replace( QChar(0x00d1), "N" );

        // czech letters with carons
        result.replace( QChar(0x0161), "s" ).replace( QChar(0x0160), "S" );
        result.replace( QChar(0x010d), "c" ).replace( QChar(0x010c), "C" );
        result.replace( QChar(0x0159), "r" ).replace( QChar(0x0158), "R" );
        result.replace( QChar(0x017e), "z" ).replace( QChar(0x017d), "Z" );
        result.replace( QChar(0x0165), "t" ).replace( QChar(0x0164), "T" );
        result.replace( QChar(0x0148), "n" ).replace( QChar(0x0147), "N" );
        result.replace( QChar(0x010f), "d" ).replace( QChar(0x010e), "D" );

        // accented vowels
        QChar a[] = { 'a', 0xe0,0xe1,0xe2,0xe3,0xe5, 0 };
        QChar A[] = { 'A', 0xc0,0xc1,0xc2,0xc3,0xc5, 0 };
        QChar E[] = { 'e', 0xe8,0xe9,0xea,0xeb,0x11a, 0 };
        QChar e[] = { 'E', 0xc8,0xc9,0xca,0xcb,0x11b, 0 };
        QChar i[] = { 'i', 0xec,0xed,0xee,0xef, 0 };
        QChar I[] = { 'I', 0xcc,0xcd,0xce,0xcf, 0 };
        QChar o[] = { 'o', 0xf2,0xf3,0xf4,0xf5,0xf8, 0 };
        QChar O[] = { 'O', 0xd2,0xd3,0xd4,0xd5,0xd8, 0 };
        QChar u[] = { 'u', 0xf9,0xfa,0xfb,0x16e, 0 };
        QChar U[] = { 'U', 0xd9,0xda,0xdb,0x16f, 0 };
        QChar nul[] = { 0 };
        QChar *replacements[] = { a, A, e, E, i, I, o, O, u, U, nul };

        for( int i = 0; i < result.length(); i++ )
        {
            QChar c = result[ i ];
            for( uint n = 0; replacements[n][0] != QChar(0); n++ )
            {
                for( uint k=0; replacements[n][k] != QChar(0); k++ )
                {
                    if( replacements[n][k] == c )
                    {
                        c = replacements[n][0];
                    }
                }
            }
            result[ i ] = c;
        }
        return result;
    }

    QString asciiPath( const QString &path )
    {
        QString result = path;
        for( int i = 0; i < result.length(); i++ )
        {
            QChar c = result[ i ];
            if( c > QChar(0x7f) || c == QChar(0) )
            {
                c = '_';
            }
            result[ i ] = c;
        }
        return result;
    }

    QString vfatPath( const QString &path )
    {
        QString s = path;

        for( int i = 0; i < s.length(); i++ )
        {
            QChar c = s[ i ];
            if( c < QChar(0x20)
                    || c=='*' || c=='?' || c=='<' || c=='>'
                    || c=='|' || c=='"' || c==':' || c=='/'
                    || c=='\\' )
                c = '_';
            s[ i ] = c;
        }

        uint len = s.length();
        if( len == 3 || (len > 3 && s[3] == '.') )
        {
            QString l = s.left(3).toLower();
            if( l=="aux" || l=="con" || l=="nul" || l=="prn" )
                s = '_' + s;
        }
        else if( len == 4 || (len > 4 && s[4] == '.') )
        {
            QString l = s.left(3).toLower();
            QString d = s.mid(3,1);
            if( (l=="com" || l=="lpt") &&
                    (d=="0" || d=="1" || d=="2" || d=="3" || d=="4" ||
                     d=="5" || d=="6" || d=="7" || d=="8" || d=="9") )
                s = '_' + s;
        }

        while( s.startsWith( '.' ) )
            s = s.mid(1);

        while( s.endsWith( '.' ) )
            s = s.left( s.length()-1 );

        s = s.left(255);
        len = s.length();
        if( s[len-1] == ' ' )
            s[len-1] = '_';

        return s;
    }

    QString decapitateString( const QString &input, const QString &ref )
    {
        QString t = ref.toUpper();
        int length = t.length();
        int commonLength = 0;
        while( length > 0 )
        {
            if ( input.toUpper().startsWith( t ) )
            {
                commonLength = t.length();
                t = ref.toUpper().left( t.length() + length/2 );
                length = length/2;
            }
            else
            {
                t = ref.toUpper().left( t.length() - length/2 );
                length = length/2;
            }
        }
        QString clean = input;
        if( t.endsWith( ' ' ) || !ref.at( t.length() ).isLetterOrNumber() ) // common part ends with a space or complete word
            clean = input.right( input.length() - commonLength ).trimmed();
        return clean;
    }

    void setUseScores( bool use ) { App::instance()->setUseScores( use ); }
    void setUseRatings( bool use ) { App::instance()->setUseRatings( use ); }
    void setMoodbarPrefs( bool show, bool moodier, int alter, bool withMusic )
    { App::instance()->setMoodbarPrefs( show, moodier, alter, withMusic ); }
    KIO::Job *trashFiles( const KUrl::List &files ) { return App::instance()->trashFiles( files ); }
}

int App::newInstance()
{
    static bool first = true;
    if ( isSessionRestored() && first)
    {
        first = false;
        return 0;
    }

    first = false;

    //initCliArgs();
    handleCliArgs();
    return 0;
}






#include "App.moc"