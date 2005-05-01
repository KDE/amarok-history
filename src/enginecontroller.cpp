/***************************************************************************
 *   Copyright (C) 2004 Frederik Holljen <fh@ez.no>                        *
 *             (C) 2004,5 Max Howell <max.howell@methylblue.com>           *
 *             (C) 2004,5 Mark Kretschmann                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#define DEBUG_PREFIX "controller"

#include "amarok.h"
#include "amarokconfig.h"
#include "debug.h"
#include "enginebase.h"
#include "enginecontroller.h"
#include "pluginmanager.h"
#include "statusbar.h"
#include "streamprovider.h"

#include <qfile.h>
#include <qtimer.h>

#include <kapplication.h>
#include <kio/global.h>
#include <kio/job.h>
#include <kmessagebox.h>
#include <krun.h>

#include <cstdlib>


ExtensionCache EngineController::s_extensionCache;


EngineController*
EngineController::instance()
{
    //will only be instantiated the first time this function is called
    //will work with the inline directive
    static EngineController Instance;

    return &Instance;
}


EngineController::EngineController()
        : m_engine( 0 )
        , m_voidEngine( 0 )
        , m_delayTime( 0 )
        , m_muteVolume( 0 )
        , m_xFadeThisTrack( false )
        , m_timer( new QTimer( this ) )
        , m_stream( 0 )
{
    m_voidEngine = m_engine = (EngineBase*)loadEngine( "void-engine" );

    connect( m_timer, SIGNAL( timeout() ), SLOT( slotMainTimer() ) );
}

EngineController::~EngineController()
{
    DEBUG_FUNC_INFO //we like to know when singletons are destroyed
}


//////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC
//////////////////////////////////////////////////////////////////////////////////////////

EngineBase*
EngineController::loadEngine() //static
{
    /// always returns a valid pointer to EngineBase

    DEBUG_BLOCK
    //TODO remember song position, and resume playback

    if( m_engine != m_voidEngine ) {
        EngineBase *oldEngine = m_engine;

        // we assign this first for thread-safety,
        // EngineController::engine() must always return an engine!
        m_engine = m_voidEngine;

        // we unload the old engine first because there are a number of
        // bugs associated with keeping one engine loaded while loading
        // another, eg xine-engine can't init(), and aRts-engine crashes
        PluginManager::unload( oldEngine );

        // the engine is not required to do this when we unload it but
        // we need to do it to ensure amaroK looks correct and to delete
        // m_stream. We don't do this for the void-engine because that
        // means amaroK sets all components to empty on startup, which is
        // their responsibility.
        slotStateChanged( Engine::Empty );

        // new engine, new ext cache required
        extensionCache().clear();
    }

    m_engine = loadEngine( AmarokConfig::soundSystem() );

    const QString engineName = PluginManager::getService( m_engine )->property( "X-KDE-amaroK-name" ).toString();

    if( !AmarokConfig::soundSystem().isEmpty() && engineName != AmarokConfig::soundSystem() ) {
        //AmarokConfig::soundSystem() is empty on the first-ever-run

        amaroK::StatusBar::instance()->longMessage( i18n(
                "Sorry, the '%1' could not be loaded, instead we have loaded the '%2'." )
                        .arg( AmarokConfig::soundSystem() )
                        .arg( engineName ),
                KDE::StatusBar::Sorry );

        AmarokConfig::setSoundSystem( engineName );
    }

    // Important: Make sure soundSystem is not empty
    if( AmarokConfig::soundSystem().isEmpty() )
        AmarokConfig::setSoundSystem( engineName );

    return m_engine;
}

#include <qvaluevector.h>
EngineBase*
EngineController::loadEngine( const QString &engineName )
{
    /// always returns a valid plugin (exits if it can't get one)

    DEBUG_BLOCK

    QString query = "[X-KDE-amaroK-plugintype] == 'engine' and [X-KDE-amaroK-name] != '%1'";
    KTrader::OfferList offers = PluginManager::query( query.arg( engineName ) );

    // sort by rank, QValueList::operator[] is O(n), so this is quite inefficient
    #define rank( x ) (x)->property( "X-KDE-amaroK-rank" ).toInt()
    for( int n = offers.count()-1, i = 0; i < n; i++ )
        for( int j = n; j > i; j-- )
            if( rank( offers[j] ) > rank( offers[j-1] ) )
                qSwap( offers[j], offers[j-1] );
    #undef rank

    // this is the actual engine we want
    query = "[X-KDE-amaroK-plugintype] == 'engine' and [X-KDE-amaroK-name] == '%1'";
    offers = PluginManager::query( query.arg( engineName ) ) + offers;

    foreachType( KTrader::OfferList, offers ) {
        amaroK::Plugin *plugin = PluginManager::createFromService( *it );

        if( plugin ) {
            QObject *bar = amaroK::StatusBar::instance();
            EngineBase *engine = (EngineBase*)plugin;

            connect( engine, SIGNAL(stateChanged( Engine::State )),
                       this, SLOT(slotStateChanged( Engine::State )) );
            connect( engine, SIGNAL(trackEnded()),
                       this, SLOT(slotTrackEnded()) );
            connect( engine, SIGNAL(statusText( const QString& )),
                        bar, SLOT(shortMessage( const QString& )) );
            connect( engine, SIGNAL(infoMessage( const QString& )),
                        bar, SLOT(longMessage( const QString& )) );
            connect( engine, SIGNAL(metaData( const Engine::SimpleMetaBundle& )),
                       this, SLOT(slotEngineMetaData( const Engine::SimpleMetaBundle& )) );
            connect( engine, SIGNAL(showConfigDialog( const QCString& )),
                       kapp, SLOT(slotConfigAmarok( const QCString& )) );

            if( engine->init() )
                return engine;
            else
                warning() << "Could not init() an engine\n";
        }
    }

    KRun::runCommand( "kbuildsycoca" );

    KMessageBox::error( 0, i18n(
            "<p>amaroK could not find any sound-engine plugins. "
            "amaroK is now updating the KDE configuration database. Please wait a couple of minutes, then restart amaroK.</p>"
            "<p>If this does not help, "
            "it is likely that amaroK is installed under the wrong prefix, please fix your installation using:<pre>"
            "$ cd /path/to/amarok/source-code/<br>"
            "$ su -c \"make uninstall\"<br>"
            "$ ./configure --prefix=`kde-config --prefix` && su -c \"make install\"<br>"
            "$ kbuildsycoca<br>"
            "$ amarok</pre>"
            "More information can be found in the README file. For further assistance join us at #amarok on irc.freenode.net.</p>" ) );

    // don't use QApplication::exit, as the eventloop may not have started yet
    std::exit( EXIT_SUCCESS );

    // Not executed, just here to prevent compiler warning
    return 0;
}


bool EngineController::canDecode( const KURL &url ) //static
{
   //NOTE this function must be thread-safe
    //TODO a KFileItem version? <- presumably so we can mimetype check

    const QString fileName = url.fileName();
    const QString ext = amaroK::extension( fileName );

    //FIXME why do we do this? Please add comments to odd looking code!
    if ( ext == "m3u" || ext == "pls" ) return false;

    // Ignore protocols "fetchcover" and "musicbrainz", they're not local but we dont really want them in the playlist :)
    if ( url.protocol() == "fetchcover" || url.protocol() == "musicbrainz" ) return false;

    // Accept non-local files, since we can't test them for validity at this point
    // TODO actually, only accept unconditionally http stuff
    // TODO this actually makes things like "Blarrghgjhjh:!!!" automatically get inserted
    // into the playlist
    // TODO remove for amaroK 1.3 and above silly checks, instead check for http type servers
    if ( !url.isLocalFile() ) return true;

    // If extension is already in the cache, return cache result
    if ( extensionCache().contains( ext ) )
        return s_extensionCache[ext];

    const bool valid = engine()->canDecode( url );

    //we special case this as otherwise users hate us
    if ( !valid && ext == "mp3" )
        amaroK::StatusBar::instance()->longMessageThreadSafe(
           i18n( "<p>The %1 claims it <b>cannot</b> play MP3 files."
                 "<p>You may want to choose a different engine from the <i>Configure Dialog</i>, or examine "
                 "the installation of the multimedia-framework that the current engine uses. "
                 "<p>You may find useful information in the <i>FAQ</i> section of the <i>amaroK HandBook</i>." )
            .arg( AmarokConfig::soundSystem() ) );

    // Cache this result for the next lookup
    if ( !ext.isEmpty() )
        extensionCache().insert( ext, valid );

    return valid;
}


void EngineController::restoreSession()
{
    //here we restore the session
    //however, do note, this is always done, KDE session management is not involved

    if( !AmarokConfig::resumeTrack().isEmpty() )
    {
        const KURL url = AmarokConfig::resumeTrack();

        if ( m_engine->load( url ) && m_engine->play( AmarokConfig::resumeTime() ) )
            newMetaDataNotify( m_bundle = MetaBundle( url ), true );
    }
}


void EngineController::endSession()
{
    //only update song stats, when we're not going to resume it
    if ( m_bundle.length() > 0 && !AmarokConfig::resumePlayback() )
        trackEnded( m_engine->position(), m_bundle.length() * 1000 );

    PluginManager::unload( m_voidEngine );
}

//////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC SLOTS
//////////////////////////////////////////////////////////////////////////////////////////

void EngineController::previous() //SLOT
{
    emit orderPrevious();
}


void EngineController::next( bool forceNext ) //SLOT
{
    m_isTiming = false;
    emit orderNext(forceNext);
}


void EngineController::play() //SLOT
{
    if ( m_engine->state() == Engine::Paused )
    {
        m_engine->pause();
    }
    else emit orderCurrent();
}


void EngineController::play( const MetaBundle &bundle )
{
    const KURL &url = bundle.url();
    debug() << "Loading URL: " << url.url() << endl;
    // Destroy stale StreamProvider
    delete m_stream;
    m_lastMetadata.clear();

    //TODO bummer why'd I do it this way? it should _not_ be in play!
    //let amaroK know that the previous track is no longer playing
    if ( m_timer->isActive() && m_bundle.length() > 0 )
        trackEnded( m_engine->position(), m_bundle.length() * 1000 );

    if ( m_engine->pluginProperty( "StreamingMode") != "NoStreaming" && url.protocol() == "http" ) {
        m_bundle = bundle;
        m_xFadeThisTrack = false;
        // Detect mimetype of remote file
        KIO::MimetypeJob* job = KIO::mimetype( url, false );
        connect( job, SIGNAL(result( KIO::Job* )), SLOT(playRemote( KIO::Job* )) );
        amaroK::StatusBar::instance()->shortMessage( i18n("Connecting to stream source...") );
        return; //don't do notify
    }

    if ( url.isLocalFile() ) {
        // does the file really exist? the playlist entry might be old
        if ( ! QFile::exists( url.path()) ) {
            //debug() << "  file >" << url.path() << "< does not exist!" << endl;
            amaroK::StatusBar::instance()->shortMessage( i18n("Local file does not exist.") );
            goto some_kind_of_failure;
        }
    }

    if( m_engine->load( url ) )
    {
        //assign bundle now so that it is available when the engine
        //emits stateChanged( Playing )
        m_bundle = bundle;

        if( m_engine->play() )
        {
            // Ask engine for track length, if available. It's more reliable than TagLib.
            const uint trackLength = m_engine->length();
            if ( trackLength ) m_bundle.setLength( trackLength / 1000 );

            m_xFadeThisTrack = AmarokConfig::crossfade() &&
                               m_engine->hasPluginProperty( "HasCrossfade" ) &&
                              !m_engine->isStream() &&
                               m_bundle.length()*1000 - AmarokConfig::crossfadeLength()*2 > 0;

            newMetaDataNotify( m_bundle, true /* track change */ );
        }
        else goto some_kind_of_failure;
    }
    else
    some_kind_of_failure:
        //NOTE now we don't do next() at all
        // say the user has a 4000 item playlist with all URLs in that playlist
        // being bad, amaroK will appear to freeze. We need to either stat files in
        // the background or not call next() more than 5 times in a row
        //don't do for repeatPlaylist() as it can produce a freeze
        //FIXME -> mxcl
        //if ( !AmarokConfig::repeatPlaylist() )
        //    next()
                ;
}


void EngineController::pause() //SLOT
{
    if ( m_engine->loaded() )
        m_engine->pause();
}


void EngineController::stop() //SLOT
{
    //let amaroK know that the previous track is no longer playing
    if ( m_bundle.length() > 0 )
        trackEnded( m_engine->position(), m_bundle.length() * 1000 );

    if ( m_engine->loaded() )
        m_engine->stop();
}


void EngineController::playPause() //SLOT
{
    //this is used by the TrayIcon, PlayPauseAction and DCOP

    if( m_engine->state() == Engine::Playing )
    {
        pause();
    }
    else play();
}


void EngineController::seekRelative( int ms ) //SLOT
{
  if( m_engine->state() == Engine::Playing )
  {
    int newPos = m_engine->position() + ms;
    seek( newPos <= 0 ? 1 : newPos );
  }
}


void EngineController::seekForward( int ms )
{
    seekRelative( ms );
}


void EngineController::seekBackward( int ms )
{
    seekRelative( -ms );
}


int EngineController::increaseVolume( int ticks ) //SLOT
{
    return setVolume( m_engine->volume() + ticks );
}


int EngineController::decreaseVolume( int ticks ) //SLOT
{
    return setVolume( m_engine->volume() - ticks );
}


int EngineController::setVolume( int percent ) //SLOT
{
    if( percent < 0 ) percent = 0;
    if( percent > 100 ) percent = 100;

    if( (uint)percent != m_engine->volume() )
    {
        m_engine->setVolume( (uint)percent );

        percent = m_engine->volume();
        AmarokConfig::setMasterVolume( percent );
        volumeChangedNotify( percent );
        return percent;
    }

    return m_engine->volume();
}


void EngineController::mute() //SLOT
{
    if( m_muteVolume == 0 )
    {
        m_muteVolume = m_engine->volume();
        setVolume( 0 );
    }
    else
    {
        setVolume( m_muteVolume );
        m_muteVolume = 0;
    }
}


const MetaBundle&
EngineController::bundle() const
{
    static MetaBundle null;
    return m_engine->state() == Engine::Empty ? null : m_bundle;
}


//////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE SLOTS
//////////////////////////////////////////////////////////////////////////////////////////

void EngineController::playRemote( KIO::Job* job ) //SLOT
{
    const QString mimetype = static_cast<KIO::MimetypeJob*>( job )->mimetype();
    debug() << "Detected mimetype: " << mimetype << endl;

    const KURL &url = m_bundle.url();

    const bool isStream = mimetype.isEmpty() || mimetype == "text/html" ||
                          url.host().endsWith( "last.fm" ); // HACK last.fm uses the mimetype audio/x-mp3

    if ( isStream && m_engine->pluginProperty( "StreamingMode" ) != "NoStreaming" )
    {
        delete m_stream;
        m_stream = new amaroK::StreamProvider( url, m_engine->pluginProperty( "StreamingMode" ) );

        if ( !m_stream->initSuccess() || !m_engine->play( m_stream->proxyUrl(), isStream ) ) {
            delete m_stream;
            if ( !AmarokConfig::repeatPlaylist() )
                next();
            return; //don't notify
        }

        connect( m_stream, SIGNAL(metaData( const MetaBundle& )),
                 this,       SLOT(slotStreamMetaData( const MetaBundle& )) );
        connect( m_stream, SIGNAL(streamData( char*, int )),
                 m_engine,   SLOT(newStreamData( char*, int )) );
        connect( m_stream, SIGNAL(sigError()),
                 this,     SIGNAL(orderNext()) );
    }
    else if( !m_engine->play( url, isStream ) && !AmarokConfig::repeatPlaylist() )
    {
        next();
        return; //don't notify
    }

    newMetaDataNotify( m_bundle, true /* track change */ );
}

void EngineController::slotStreamMetaData( const MetaBundle &bundle ) //SLOT
{
    // Prevent spamming by ignoring repeated identical data (some servers repeat it every 10 seconds)
    if ( m_lastMetadata.contains( bundle ) )
        return;

    // We compare the new item with the last two items, because mth.house currently cycles
    // two messages alternating, which gets very annoying
    if ( m_lastMetadata.count() == 2 )
        m_lastMetadata.pop_front();

    m_lastMetadata << bundle;

    m_bundle = bundle;
    newMetaDataNotify( m_bundle, false /* not a new track */ );
}

void EngineController::slotEngineMetaData( const Engine::SimpleMetaBundle &simpleBundle ) //SLOT
{
    if( m_engine->isStream() )
    {
        MetaBundle bundle = m_bundle;
        bundle.setArtist( simpleBundle.artist );
        bundle.setTitle( simpleBundle.title );
        bundle.setComment( simpleBundle.comment );
        bundle.setAlbum( simpleBundle.album );

        slotStreamMetaData( bundle );
    }
}

void EngineController::slotMainTimer() //SLOT
{
    const uint position = m_engine->position();

    trackPositionChangedNotify( position );

    // Crossfading
    if ( m_engine->state() == Engine::Playing &&
         m_xFadeThisTrack &&
         ( m_bundle.length()*1000 - position < (uint) AmarokConfig::crossfadeLength() ) )
    {
        debug() << "Crossfading to next track...\n";
        next(false);
    }
}

void EngineController::slotTrackEnded() //SLOT
{
    if ( AmarokConfig::trackDelayLength() > 0 )
    {
        //FIXME not perfect
        if ( !m_isTiming )
        {
            QTimer::singleShot( AmarokConfig::trackDelayLength(), this, SLOT(next()) );
            m_isTiming = true;
        }

    }
    else next(false);
}

void EngineController::slotStateChanged( Engine::State newState ) //SLOT
{
    switch( newState )
    {
    case Engine::Empty:

        delete m_stream;

        //FALL THROUGH...

    case Engine::Paused:

        m_timer->stop();
        break;

    case Engine::Playing:

        m_timer->start( MAIN_TIMER );
        break;

    default:
        ;
    }

    stateChangedNotify( newState );
}


void EngineController::streamError() //SLOT
{
    delete m_stream;
    next();
}



#include "enginecontroller.moc"
