/***************************************************************************
 *   Copyright (C) 2004,5 Max Howell <max.howell@methylblue.com>           *
 *             (C) 2003,4 J. Kofler <kaffeine@gmx.net>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "xine-config.h"
#include "xine-engine.h"
#include "xine-scope.h"

AMAROK_EXPORT_PLUGIN( XineEngine )

#define DEBUG_PREFIX "xine-engine"
#define indent xine_indent

#include <climits>
#include <cmath>
#include "debug.h"
#include <klocale.h>
#include <kmessagebox.h>
#include <qapplication.h>
#include <qdir.h>

extern "C"
{
    #include <unistd.h>
}

#ifndef LLONG_MAX
#define LLONG_MAX 9223372036854775807LL
#endif

//define this to use xine in a more standard way
//#define XINE_SAFE_MODE


///some logging static globals
namespace Log
{
    static uint bufferCount = 0;
    static uint scopeCallCount = 1; //prevent divideByZero
    static uint noSuitableBuffer = 0;
};

///returns the configuration we will use
static inline QCString configPath() { return QFile::encodeName( QDir::homeDirPath() + "/.xine/config" ); }


XineEngine::XineEngine()
        : EngineBase()
        , m_xine( 0 )
        , m_stream( 0 )
        , m_audioPort( 0 )
        , m_eventQueue( 0 )
        , m_post( 0 )
{
    addPluginProperty( "StreamingMode", "NoStreaming" );
    addPluginProperty( "HasConfigure", "true" );
    addPluginProperty( "HasEqualizer", "true" );
    //addPluginProperty( "HasCrossfade", "true" );
}

XineEngine::~XineEngine()
{
//     if( m_stream && xine_get_status( m_stream ) == XINE_STATUS_PLAY )
//     {
//         const int volume = xine_get_param( m_stream, XINE_PARAM_AUDIO_AMP_LEVEL );
//         const double D = 300000 * std::pow( (double)volume, -0.4951 );
//
//         debug() << "Sleeping: " << D << ", " << volume << endl;
//
//         for( int v = volume - 1; v >= 1; v-- ) {
//             xine_set_param( m_stream, XINE_PARAM_AUDIO_AMP_LEVEL, v );
//
//             const int sleep = int(D * (-log10( v + 1 ) + 2));
//
//             ::usleep( sleep );
//
//             debug() << v << ": " << sleep << "us\n";
//         }
//         xine_stop( m_stream );
//     }

    xine_config_save( m_xine, configPath() );

    if( m_stream )     xine_close( m_stream );
    if( m_eventQueue ) xine_event_dispose_queue( m_eventQueue );
    if( m_stream )     xine_dispose( m_stream );
    if( m_audioPort )  xine_close_audio_driver( m_xine, m_audioPort );
    if( m_post )       xine_post_dispose( m_xine, m_post );
    if( m_xine )       xine_exit( m_xine );

    debug() << "xine closed\n";

    debug() << "Scope statistics:\n"
            << "  Average list size: " << Log::bufferCount / Log::scopeCallCount << endl
            << "  Buffer failure:    " << double(Log::noSuitableBuffer*100) / Log::scopeCallCount << "%\n";
}

bool
XineEngine::init()
{
   debug() << "'Bringing joy to small mexican gerbils, a few weeks at a time.'\n";

   m_xine = xine_new();

   if( !m_xine ) {
      KMessageBox::error( 0, i18n("amaroK could not initialize xine.") );
      return false;
   }

   #ifdef XINE_SAFE_MODE
   xine_engine_set_param( m_xine, XINE_ENGINE_PARAM_VERBOSITY, 99 );
   #endif

   xine_config_load( m_xine, configPath() );

   xine_init( m_xine );

   if( !makeNewStream() )
      return false;

   #ifndef XINE_SAFE_MODE
   startTimer( 200 ); //prunes the scope
   #endif

   return true;
}

bool
XineEngine::makeNewStream()
{
   {
      ///this block is so we don't crash if we fail to allocate a thingy
      xine_stream_t     *stream;
      xine_audio_port_t *port;

      port = xine_open_audio_driver( m_xine, "auto", NULL );
      if( !port ) {
         //TODO make engine method that is the same but parents the dialog for us
         KMessageBox::error( 0, i18n("xine was unable to initialize any audio-drivers.") );
         return false;
      }

      stream = xine_stream_new( m_xine, port, NULL );
      if( !stream ) {
         KMessageBox::error( 0, i18n("amaroK could not create a new xine-stream.") );
         return false;
      }

      //assign temporaries to members, the important bits are now created;
      m_stream = stream;
      m_audioPort = port;
   }

   if( m_eventQueue )
      xine_event_dispose_queue( m_eventQueue );

   xine_event_create_listener_thread(
         m_eventQueue = xine_event_new_queue( m_stream ),
         &XineEngine::XineEventListener,
         (void*)this );

   #ifndef XINE_SAFE_MODE
   //implemented in xine-scope.h
   m_post = scope_plugin_new( m_xine, m_audioPort );

   xine_set_param( m_stream, XINE_PARAM_METRONOM_PREBUFFER, 6000 );
   xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 1 );
   #endif

   return true;
}


static Fader *s_fader = 0;

bool
XineEngine::load( const KURL &url, bool isStream )
{
   if( XINE_VERSION == "1-rc6a" && url.protocol() == "http" ) {
      emit infoMessage( i18n("Sorry xine 1-rc6a cannot play remote streams, please upgrade to 1-rc7") );
      return false;
   }

   Engine::Base::load( url, isStream || url.protocol() == "http" );

//     if( false && m_xfadeLength > 0 && xine_get_status( m_stream ) == XINE_STATUS_PLAY )
//     {
//        s_fader = new Fader( this );
//     }

   if( xine_open( m_stream, QFile::encodeName( url.url() ) ) )
   {
      #ifndef XINE_SAFE_MODE
      //we must ensure the scope is pruned of old buffers
      timerEvent( 0 );

      xine_post_out_t *source = xine_get_audio_source( m_stream );
      xine_post_in_t  *target = (xine_post_in_t*)xine_post_input( m_post, const_cast<char*>("audio in") );
      xine_post_wire( source, target );
      #endif

      return true;
   }

   //s_fader will delete itself

   return false;
}

bool
XineEngine::play( uint offset )
{
    if( xine_play( m_stream, 0, offset ) )
    {
        if( s_fader )
           s_fader->start();

        emit stateChanged( Engine::Playing );

        return true;
    }

    //we need to stop the track that is prepped for crossfade
    delete s_fader;

    emit stateChanged( Engine::Empty );

    switch( xine_get_error( m_stream ) )
    {
    case XINE_ERROR_NO_INPUT_PLUGIN:
        KMessageBox::error( 0, i18n("No input plugin available; check your installation.") );
        break;
    case XINE_ERROR_NO_DEMUX_PLUGIN:
        KMessageBox::error( 0, i18n("No demux plugin available; check your installation.") );
        break;
    case XINE_ERROR_DEMUX_FAILED:
        KMessageBox::error( 0, i18n("Demuxing failed; check your installation.") );
        break;
    default:
        KMessageBox::error( 0, i18n("Internal error; check your installation.") );
        break;
    }

    xine_close( m_stream );

    return false;
}

void
XineEngine::stop()
{
    m_url = KURL(); //to ensure we return Empty from state()

    std::fill( m_scope.begin(), m_scope.end(), 0 );

    xine_stop( m_stream );
    xine_set_param( m_stream, XINE_PARAM_AUDIO_CLOSE_DEVICE, 1);

    emit stateChanged( Engine::Empty );
}

void
XineEngine::pause()
{
    if( xine_get_param( m_stream, XINE_PARAM_SPEED ) )
    {
        xine_set_param( m_stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
        xine_set_param( m_stream, XINE_PARAM_AUDIO_CLOSE_DEVICE, 1);
        emit stateChanged( Engine::Paused );

    } else {

        xine_set_param( m_stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL );
        emit stateChanged( Engine::Playing );
    }
}

Engine::State
XineEngine::state() const
{
    switch( xine_get_status( m_stream ) )
    {
    case XINE_STATUS_PLAY: return xine_get_param( m_stream, XINE_PARAM_SPEED ) ? Engine::Playing : Engine::Paused;
    case XINE_STATUS_IDLE: return Engine::Empty;
    case XINE_STATUS_STOP:
    default:               return m_url.isEmpty() ? Engine::Empty : Engine::Idle;
    }
}

uint
XineEngine::position() const
{
    int pos;
    int time = 0;
    int length;

    xine_get_pos_length( m_stream, &pos, &time, &length );

    return time;
}

uint
XineEngine::length() const
{
    return 0;

// NOTE Deactivated because xine returns bogus values for ogg vorbis
#if 0
    int pos;
    int time;
    int length = 0;

    xine_get_pos_length( m_stream, &pos, &time, &length );

    return length;
#endif
}

void
XineEngine::seek( uint ms )
{
    if ( xine_get_param( m_stream, XINE_PARAM_SPEED ) )
        xine_play( m_stream, 0, (int)ms );
    else {
        // It was paused, then should keep paused
        xine_play( m_stream, 0, (int)ms );
        xine_set_param( m_stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
    }
}

void
XineEngine::setVolumeSW( uint vol )
{
    xine_set_param( m_stream, XINE_PARAM_AUDIO_AMP_LEVEL, vol );
}

void
XineEngine::setEqualizerEnabled( bool enable )
{
   if( !enable ) {
      QValueList<int> gains;
      for( uint x = 0; x < 10; x++ )
         gains += 0;
      setEqualizerParameters( 0, gains );
   }
}

void
XineEngine::setEqualizerParameters( int /*preamp*/, const QValueList<int> &gains )
{
   QValueList<int>::ConstIterator it = gains.begin();

   xine_set_param( m_stream, XINE_PARAM_EQ_30HZ, *it );
   xine_set_param( m_stream, XINE_PARAM_EQ_60HZ, *++it );
   xine_set_param( m_stream, XINE_PARAM_EQ_125HZ, *++it );
   xine_set_param( m_stream, XINE_PARAM_EQ_250HZ, *++it );
   xine_set_param( m_stream, XINE_PARAM_EQ_500HZ, *++it );
   xine_set_param( m_stream, XINE_PARAM_EQ_1000HZ, *++it );
   xine_set_param( m_stream, XINE_PARAM_EQ_2000HZ, *++it );
   xine_set_param( m_stream, XINE_PARAM_EQ_4000HZ, *++it );
   xine_set_param( m_stream, XINE_PARAM_EQ_8000HZ, *++it );
   xine_set_param( m_stream, XINE_PARAM_EQ_16000HZ, *++it );
}

bool
XineEngine::canDecode( const KURL &url ) const
{
    //TODO should free the file_extensions char*
    static QStringList list = QStringList::split( ' ', xine_get_file_extensions( m_xine ) );

    const QString path = url.path();
    const QString ext  = path.mid( path.findRev( '.' ) + 1 ).lower();

    //HACK we also check for m4a because xine plays them but
    //     for some reason doesn't return the extension

    return ext != "txt" && (list.contains( ext ) || ext =="m4a");
}

const Engine::Scope&
XineEngine::scope()
{
    if( !m_post || xine_get_status( m_stream ) != XINE_STATUS_PLAY )
       return m_scope;

    MyNode* const myList         = scope_plugin_list( m_post );
    metronom_t* const myMetronom = scope_plugin_metronom( m_post );
    const int myChannels         = scope_plugin_channels( m_post );

    //prune the buffer list and update m_currentVpts
    timerEvent( 0 );

    for( int n, frame = 0; frame < 512; )
    {
        MyNode *best_node = 0;

        for( MyNode *node = myList->next; node != myList; node = node->next, Log::bufferCount++ )
            if( node->vpts <= m_currentVpts && (!best_node || node->vpts > best_node->vpts) )
               best_node = node;

        if( !best_node || best_node->vpts_end < m_currentVpts ) {
           Log::noSuitableBuffer++; break; }

        int64_t
        diff  = m_currentVpts;
        diff -= best_node->vpts;
        diff *= 1<<16;
        diff /= myMetronom->pts_per_smpls;

        const int16_t*
        data16  = best_node->mem;
        data16 += diff;

        diff += diff % myChannels; //important correction to ensure we don't overflow the buffer
        diff /= myChannels;        //use units of frames, not samples

        //calculate the number of available samples in this buffer
        n  = best_node->num_frames;
        n -= diff;
        n += frame; //clipping for # of frames we need

        if( n > 512 )
           n = 512; //we don't want more than 512 frames

        for( int a, c; frame < n; ++frame, data16 += myChannels ) {
            for( a = c = 0; c < myChannels; ++c )
                a += data16[c];

            a /= myChannels;
            m_scope[frame] = a;
        }

        m_currentVpts = best_node->vpts_end;
        m_currentVpts++; //FIXME needs to be done for some reason, or you get situations where it uses same buffer again and again
    }

    Log::scopeCallCount++;

    return m_scope;
}

void
XineEngine::timerEvent( QTimerEvent* )
{
   //here we prune the buffer list regularly

   MyNode *myList = scope_plugin_list( m_post );

   //we operate on a subset of the list for thread-safety
   MyNode * const first_node = myList->next;
   MyNode const * const list_end = myList;

   m_currentVpts = (xine_get_status( m_stream ) == XINE_STATUS_PLAY)
      ? xine_get_current_vpts( m_stream )
      : LLONG_MAX; //if state is not playing OR paused, empty the list
   //: std::numeric_limits<int64_t>::max(); //TODO don't support crappy gcc 2.95

   for( MyNode *prev = first_node, *node = first_node->next; node != list_end; node = node->next )
   {
      //we never delete first_node
      //this maintains thread-safety
      if( node->vpts_end < m_currentVpts ) {
         prev->next = node->next;

         free( node->mem );
         free( node );

         node = prev;
      }

      prev = node;
   }
}

amaroK::PluginConfig*
XineEngine::configure() const
{
    return new XineConfigDialog( m_xine );
}

void
XineEngine::customEvent( QCustomEvent *e )
{
    switch( e->type() )
    {
    case 3000:
        emit trackEnded();
        break;

    case 3001:
        #define message static_cast<QString*>(e->data())
        emit infoMessage( (*message).arg( m_url.prettyURL() ) );
        delete message;
        break;

    case 3002:
        emit statusText( *message );
        delete message;
        #undef message
        break;

    case 3003: { //meta info has changed

        Engine::SimpleMetaBundle bundle;

        bundle.title   = QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_TITLE ) );
        bundle.artist  = QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_ARTIST ) );
        bundle.album   = QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_ALBUM ) );
        bundle.comment = QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_COMMENT ) );

        emit metaData( bundle );
    }

    default:
        ;
    }
}

void
XineEngine::XineEventListener( void *p, const xine_event_t* xineEvent )
{
    if( !p ) return;

    #define xe static_cast<XineEngine*>(p)

    switch( xineEvent->type )
    {
    case XINE_EVENT_UI_SET_TITLE:

        debug() << "XINE_EVENT_UI_SET_TITLE\n";

        QApplication::postEvent( xe, new QCustomEvent( 3003 ) );

        break;

    case XINE_EVENT_UI_PLAYBACK_FINISHED:

        //emit signal from GUI thread
        QApplication::postEvent( xe, new QCustomEvent(3000) );
        break;

    case XINE_EVENT_PROGRESS:
    {
        xine_progress_data_t* pd = (xine_progress_data_t*)xineEvent->data;

        QString
        msg = "%1 %2%";
        msg = msg.arg( QString( pd->description ) )
                 .arg( KGlobal::locale()->formatNumber( pd->percent, 0 ) );

        QCustomEvent *e = new QCustomEvent( 3002 );
        e->setData( new QString( msg ) );

        QApplication::postEvent( xe, e );

        break;
    }
    case XINE_EVENT_UI_MESSAGE:
    {
        debug() << "message received from xine\n";

        xine_ui_message_data_t *data = (xine_ui_message_data_t *)xineEvent->data;
        QString message;

        switch( data->type )
        {
        case XINE_MSG_NO_ERROR:
        {
            //series of \0 separated strings, terminated with a \0\0
            char str[2000];
            char *p = str;
            for( char *msg = data->messages; !(*msg == '\0' && *(msg+1) == '\0'); ++msg, ++p )
                *p = *msg == '\0' ? '\n' : *msg;
            *p = '\0';

            debug() << str << endl;

            break;
        }

        case XINE_MSG_ENCRYPTED_SOURCE:
            break;

        case XINE_MSG_UNKNOWN_HOST:
            message = i18n("The host is unknown for the URL: <i>%1</i>"); goto param;
        case XINE_MSG_UNKNOWN_DEVICE:
            message = i18n("The device name you specified seems invalid."); goto param;
        case XINE_MSG_NETWORK_UNREACHABLE:
            message = i18n("The network appears unreachable."); goto param;
        case XINE_MSG_AUDIO_OUT_UNAVAILABLE:
            message = i18n("Audio output unavailable; the device is busy."); goto param;
        case XINE_MSG_CONNECTION_REFUSED:
            message = i18n("The connection was refused for the URL: <i>%1</i>"); goto param;
        case XINE_MSG_FILE_NOT_FOUND:
            message = i18n("xine could not find the URL: <i>%1</i>"); goto param;
        case XINE_MSG_PERMISSION_ERROR:
            message = i18n("Access was denied for the URL: <i>%1</i>"); goto param;
        case XINE_MSG_READ_ERROR:
            message = i18n("The source cannot be read for the URL: <i>%1</i>"); goto param;
        case XINE_MSG_LIBRARY_LOAD_ERROR:
            message = i18n("A problem occurred while loading a library or decoder."); goto param;

        case XINE_MSG_GENERAL_WARNING:
            message = i18n("General Warning"); goto explain;
        case XINE_MSG_SECURITY:
            message = i18n("Security Warning"); goto explain;
        default:
            message = i18n("Unknown Error"); goto explain;


        explain:

            if(data->explanation)
            {
                message.prepend( "<b>" );
                message += "</b>:<p>";
                message += ((char *) data + data->explanation);
            }
            else break; //if no explanation then why bother!

            //FALL THROUGH

        param:

            message.prepend( "<p>" );
            message += "<p>";

            if(data->explanation)
            {
                message += "xine parameters: <i>";
                message += ((char *) data + data->parameters);
                message += "</i>";
            }
            else message += i18n("Sorry, no additional information is available.");

            QApplication::postEvent( xe, new QCustomEvent(QEvent::Type(3001), new QString(message)) );
        }

    } //case
    } //switch

    #undef xe
}

//////////////////
/// class Fader
//////////////////

Fader::Fader( XineEngine *engine )
   : QObject( engine )
   , QThread()
   , m_xine( engine->m_xine )
   , m_decrease( engine->m_stream )
   , m_increase( 0 )
   , m_port( engine->m_audioPort )
   , m_post( engine->m_post )
{
    if( engine->makeNewStream() )
    {
        m_increase = engine->m_stream;

        xine_set_param( m_decrease, XINE_PARAM_AUDIO_AMP_LEVEL, 100 );
        xine_set_param( m_increase, XINE_PARAM_AUDIO_AMP_LEVEL, 0 );
    }
    else {
        s_fader = 0;
        deleteLater();
    }
}

Fader::~Fader()
{
     wait();

     debug() << k_funcinfo << endl;

     xine_close( m_decrease );
     xine_dispose( m_decrease );
     xine_close_audio_driver( m_xine, m_port );
     if( m_post ) xine_post_dispose( m_xine, m_post );
}

struct fade_s {
    int sleep;
    uint volume;
    xine_stream_t *stream;

    fade_s( uint s, uint v, xine_stream_t *st ) : sleep( s ), volume( v ), stream( st ) {}
};

#include <list>
void
Fader::run()
{
    using std::list;

    list<fade_s> data;

    int sleeps[100];
    for( uint v = 0; v < 100; ++v )
        //the usleep time for this volume
        sleeps[v] = int(120000.0 * (-log10( v+1 ) + 2));

    for( int v = 99; v >= 0; --v ) {
        data.push_back( fade_s( sleeps[v], v, m_decrease ) );
        kdDebug() << v << ": " << sleeps[v] << endl;
    }

    {
        /**
         * Here we try to make a list consisting of many small sleeps
         * inbetween each volume increase/decrease
         */

        int v = 0;
        int tu = 0;
        int td = sleeps[0];
        for( list<fade_s>::iterator it = data.begin(), end = data.end(); it != end; ++it ) {
            tu += (*it).sleep;

            while ( tu > td ) {
                kdDebug() << tu << ", " << td << " for v=" << v << endl;

                //this is the sleeptime for the structure we are about to insert
                const int newsleep = tu - td;

                //first we need to update the sleep for the previous structure
                list<fade_s>::iterator jt = it; --jt;
                (*jt).sleep -= newsleep;

                //insert the new structure for the increasing stream
                data.insert( it, fade_s( newsleep, v, m_increase ) );

                kdDebug() << "new: " << newsleep << endl;

                //decrease the contextual volume
                if ( ++v > 99 )
                    goto done;

                //update td
                td += sleeps[v];
            }

            kdDebug() << tu << ", " << td << " for v=" << v << endl;
        }

        done: ;
    }

    // perform the fading operations
    for( list<fade_s>::iterator it = data.begin(), end = data.end(); it != end; ++it )
    {
        debug() << "sleep: " << (*it).sleep << " volume: " << (*it).volume << endl;

        if( (*it).sleep > 0 ) //FIXME
           QThread::usleep( (*it).sleep );

        xine_set_param( (*it).stream, XINE_PARAM_AUDIO_AMP_LEVEL, (*it).volume );
    }

    //stop using cpu!
    xine_stop( m_decrease );

    QThread::sleep( 5 );

    deleteLater();
}


namespace Debug
{
    #undef xine_indent
    QCString xine_indent;
}

#include "xine-engine.moc"
