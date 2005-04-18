/***************************************************************************
 *   Copyright (C) 2003-2005 by Mark Kretschmann <markey@web.de>           *
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#define DEBUG_PREFIX "Gst-Engine"

#include "config/gstconfig.h"
#include "equalizer/gstequalizer.h"
#include "enginebase.h"
#include "gstengine.h"
#include "streamsrc.h"

#include <math.h>
#include <unistd.h>

#include <qfile.h>
#include <qtimer.h>

#include <kapplication.h>
#include <kio/job.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kurl.h>

#include <gst/gst.h>

//HACK Needed until debug.h gets fixed
#define indent gst_indent
#include "debug.h"

#define RETURN_IF_PIPELINE_EMPTY if ( !m_pipelineFilled ) return;


using std::vector;

GstEngine* GstEngine::s_instance;


AMAROK_EXPORT_PLUGIN( GstEngine )


/////////////////////////////////////////////////////////////////////////////////////
// CALLBACKS
/////////////////////////////////////////////////////////////////////////////////////

void
GstEngine::eos_cb( GstElement* /*element*/, InputPipeline* input ) //static
{
    DEBUG_FUNC_INFO

    // Ignore eos when gst error was raised
    if ( !instance()->m_gst_error.isEmpty() ) return;

    input->m_eos = true;

    //this is the Qt equivalent to an idle function: delay the call until all events are finished,
    //otherwise gst will crash horribly
    QTimer::singleShot( 0, instance(), SLOT( endOfStreamReached() ) );
}


void
GstEngine::newPad_cb( GstElement*, GstPad* pad, gboolean, InputPipeline* input ) //static
{
    DEBUG_BLOCK

    GstPad* const audiopad = gst_element_get_pad( input->audioconvert, "sink" );

    if ( GST_PAD_IS_LINKED( audiopad ) ) {
        debug() << "audiopad is already linked. Unlinking old pad." << endl;
        gst_pad_unlink( audiopad, GST_PAD_PEER( audiopad ) );
    }

    gst_pad_link( pad, audiopad );

    gst_element_unlink( input->volume, instance()->m_gst_adder );
    gst_element_link( input->volume, instance()->m_gst_adder );

    gst_bin_sync_children_state( GST_BIN( input->bin ) );
}


void
GstEngine::handoff_cb( GstElement*, GstBuffer* buf, gpointer ) //static
{
    instance()->m_mutexScope.lock();

    // Check for buffer overflow
    const uint available = gst_adapter_available( instance()->m_gst_adapter );
    if ( available > SCOPEBUF_SIZE )
        gst_adapter_flush( instance()->m_gst_adapter, available - 30000 );

    // TODO On some systems buf is always 0. Why?
    if ( buf ) {
        gst_buffer_ref( buf );
        // Push buffer into adapter, where it's chopped into chunks
        gst_adapter_push( instance()->m_gst_adapter, buf );
    }

    instance()->m_mutexScope.unlock();
}


void
GstEngine::candecode_handoff_cb( GstElement*, GstBuffer*, gpointer ) //static
{
    DEBUG_FUNC_INFO

    instance()->m_canDecodeSuccess = true;
}


void
GstEngine::found_tag_cb( GstElement*, GstElement*, GstTagList* taglist, gpointer ) //static
{
    DEBUG_BLOCK

    char* string;
    Engine::SimpleMetaBundle bundle;
    bool success = false;

    if ( gst_tag_list_get_string( taglist, GST_TAG_TITLE, &string ) && string ) {
        debug() << "received tag 'Title': " << QString( string ) << endl;
        bundle.title = QString( string );
        success = true;
    }
    if ( gst_tag_list_get_string( taglist, GST_TAG_ARTIST, &string ) && string ) {
        debug() << "received tag 'Artist': " << QString( string ) << endl;
        bundle.artist = QString( string );
        success = true;
    }
    if ( gst_tag_list_get_string( taglist, GST_TAG_COMMENT, &string ) && string ) {
        debug() << "received tag 'Comment': " << QString( string ) << endl;
        bundle.comment = QString( string );
        success = true;
    }
    if ( gst_tag_list_get_string( taglist, GST_TAG_ALBUM, &string ) && string ) {
        debug() << "received tag 'Album': " << QString( string ) << endl;
        bundle.album = QString( string );
        success = true;
    }

    if ( success ) {
        instance()->m_metaBundle = bundle;
        QTimer::singleShot( 0, instance(), SLOT( newMetaData() ) );
    }
}


void
GstEngine::outputError_cb( GstElement* /*element*/, GstElement* /*domain*/, GError* error, gchar* debug, gpointer /*data*/ ) //static
{
    DEBUG_FUNC_INFO

    instance()->m_gst_error = QString::fromAscii( error->message );
    instance()->m_gst_debug = QString::fromAscii( debug );

    // Process error message in application thread
    QTimer::singleShot( 0, instance(), SLOT( handleOutputError() ) );
}


void
GstEngine::inputError_cb( GstElement* /*element*/, GstElement* /*domain*/, GError* error, gchar* debug, gpointer /*inputPipeline*/ ) //static
{
    DEBUG_FUNC_INFO

    instance()->m_gst_error = QString::fromAscii( error->message );
    instance()->m_gst_debug = QString::fromAscii( debug );

    // Process error message in application thread
    QTimer::singleShot( 0, instance(), SLOT( handleInputError() ) );
}


void
GstEngine::kio_resume_cb() //static
{
    if ( instance()->m_transferJob && instance()->m_transferJob->isSuspended() ) {
        instance()->m_transferJob->resume();
        debug() << "RESUMING kio transfer.\n";
    }
}


void
GstEngine::shutdown_cb() //static
{
    instance()->m_shutdown = true;
    debug() << "Thread is shut down.\n";
}


/////////////////////////////////////////////////////////////////////////////////////
// CLASS GSTENGINE
/////////////////////////////////////////////////////////////////////////////////////

GstEngine::GstEngine()
        : Engine::Base()
        , m_currentInput( 0 )
        , m_gst_adapter( 0 )
        , m_streamBuf( new char[STREAMBUF_SIZE] )
        , m_streamBuffering( false )
        , m_transferJob( 0 )
        , m_pipelineFilled( false )
        , m_fadeValue( 0.0 )
        , m_eosReached( false )
        , m_shutdown( false )
{
    kdDebug() << k_funcinfo << endl;

    addPluginProperty( "StreamingMode", "Signal" );
    addPluginProperty( "HasConfigure",  "true" );
    addPluginProperty( "HasCrossfade",  "true" );
    addPluginProperty( "HasEqualizer",  "true" );

    m_inputs.setAutoDelete( true );
}


GstEngine::~GstEngine()
{
    DEBUG_BLOCK
    debug() << "bytes left in gst_adapter: " << gst_adapter_available( m_gst_adapter ) << endl;

    if ( m_pipelineFilled ) {
        g_signal_connect( G_OBJECT( m_gst_outputThread ), "shutdown", G_CALLBACK( shutdown_cb ), m_gst_outputThread );
        destroyPipeline();
        // Wait for pipeline to shut down properly
        while ( !m_shutdown ) ::usleep( 20000 ); // 20 msec
    }
    else
        destroyPipeline();

    delete[] m_streamBuf;

    // Destroy scope adapter
    g_object_unref( G_OBJECT( m_gst_adapter ) );

    // Save configuration
    GstConfig::writeConfig();
}


/////////////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
/////////////////////////////////////////////////////////////////////////////////////

bool
GstEngine::init()
{
    DEBUG_BLOCK

    s_instance = this;

    // GStreamer initilization
    if ( !gst_init_check( NULL, NULL ) ) {
        KMessageBox::error( 0,
            i18n( "<h3>GStreamer could not be initialized.</h3> "
                  "<p>Please make sure that you have installed all necessary GStreamer plugins (e.g. OGG and MP3), and run <i>'gst-register'</i> afterwards.</p>"
                  "<p>For further assistance consult the GStreamer manual, and join #gstreamer on irc.freenode.net.</p>" ) );
        return false;
    }

    m_gst_adapter = gst_adapter_new();

    // Check if registry exists
    GstElement* dummy = gst_element_factory_make ( "fakesink", "fakesink" );
    if ( !dummy || !gst_scheduler_factory_make( NULL, GST_ELEMENT ( dummy ) ) ) {
        KMessageBox::error( 0,
            i18n( "<h3>GStreamer is missing a registry.</h3> "
                  "<p>Please make sure that you have installed all necessary GStreamer plugins (e.g. OGG and MP3), and run <i>'gst-register'</i> afterwards.</p>"
                  "<p>For further assistance consult the GStreamer manual, and join #gstreamer on irc.freenode.net.</p>" ) );
        return false;
    }

    if ( !createPipeline() )
        error() << "createPipeline() failed.\n";

    startTimer( TIMER_INTERVAL );

    return true;
}


bool
GstEngine::canDecode( const KURL &url ) const
{
    // TODO Consider using decodebin here as well

    // We had some bug reports claiming that .mov files cause crashes in canDecode(),
    // so don't try to decode them
    if ( url.fileName().lower().endsWith( ".mov" ) ) return false;

    int count = 0;
    m_canDecodeSuccess = false;
    GstElement *pipeline, *filesrc, *spider, *fakesink;

    if ( !( pipeline = createElement( "pipeline" ) ) ) return false;
    if ( !( filesrc = createElement( "filesrc", pipeline ) ) ) return false;
    if ( !( spider = createElement( "spider", pipeline ) ) ) return false;
    if ( !( fakesink = createElement( "fakesink", pipeline ) ) ) return false;

    GstCaps* filtercaps = gst_caps_new_simple( "audio/x-raw-int", NULL );

    gst_element_link( filesrc, spider );
    gst_element_link_filtered( spider, fakesink, filtercaps );

    gst_element_set( filesrc, "location", (const char*) QFile::encodeName( url.path() ), NULL );
    gst_element_set( fakesink, "signal_handoffs", true, NULL );
    g_signal_connect( G_OBJECT( fakesink ), "handoff", G_CALLBACK( candecode_handoff_cb ), pipeline );

    gst_element_set_state( pipeline, GST_STATE_PLAYING );

    // Try to iterate over the bin until signal "handoff" gets triggered
    while ( gst_bin_iterate( GST_BIN( pipeline ) ) && !m_canDecodeSuccess && count < 1000 )
        count++;

    gst_element_set_state( pipeline, GST_STATE_NULL );
    gst_object_unref( GST_OBJECT( pipeline ) );

    return m_canDecodeSuccess;
}


uint
GstEngine::position() const
{
    if ( !m_currentInput ) return 0;

    GstFormat fmt = GST_FORMAT_TIME;
    // Value will hold the current time position in nanoseconds. Must be initialized!
    gint64 value = 0;
    gst_element_query( m_currentInput->decodebin, GST_QUERY_POSITION, &fmt, &value );

    return static_cast<uint>( ( value / GST_MSECOND ) ); // nanosec -> msec
}


uint
GstEngine::length() const
{
    DEBUG_BLOCK

    if ( !m_currentInput ) return 0;

    GstFormat fmt = GST_FORMAT_TIME;
    // Value will hold the current time position in nanoseconds. Must be initialized!
    gint64 value = 0;
    gst_element_query( m_currentInput->decodebin, GST_QUERY_TOTAL, &fmt, &value );

    return static_cast<uint>( ( value / GST_MSECOND ) ); // nanosec -> msec
}


Engine::State
GstEngine::state() const
{
    if ( !m_pipelineFilled )
        return Engine::Empty;

    // amaroK expects the engine to return Idle on EOS
    if ( m_eosReached )
        return Engine::Idle;

    if ( !m_currentInput )
        return Engine::Empty;

    switch ( gst_element_get_state( m_currentInput->bin ) )
    {
        case GST_STATE_NULL:
            return Engine::Empty;
        case GST_STATE_READY:
            return Engine::Idle;
        case GST_STATE_PLAYING:
            return Engine::Playing;
        case GST_STATE_PAUSED:
            return Engine::Paused;

        default:
            return Engine::Empty;
    }
}


const Engine::Scope&
GstEngine::scope()
{
    const int channels = 2;

    if ( gst_adapter_available( m_gst_adapter ) < SCOPE_VALUES*channels*sizeof( gint16 ) )
        return m_scope;

    m_mutexScope.lock();

    guint64 firstStamp, lastStamp;
    GstBuffer* buf;
    GSList* list = m_gst_adapter->buflist;

    // Get timestamp from first buffer
    buf = (GstBuffer*) g_slist_nth_data( list, 0 );
    firstStamp = GST_BUFFER_TIMESTAMP( buf );

    // Get timestamp from last buffer
    GSList* last = g_slist_last( list );
    buf = (GstBuffer*) last->data;
    lastStamp = GST_BUFFER_TIMESTAMP( buf );

    // Get current clock time from sink
    GstFormat fmt = GST_FORMAT_TIME;
    gint64 sinkStamp = 0; // Must be initalised to 0
    gst_element_query( m_gst_audiosink, GST_QUERY_POSITION, &fmt, &sinkStamp );

    guint available = gst_adapter_available( m_gst_adapter );
    gint16* data = (gint16*) gst_adapter_peek( m_gst_adapter, available );

    double factor = (double) ( lastStamp - sinkStamp ) / ( lastStamp - firstStamp );
    int offset = available - static_cast<int>( factor * (double) available );
    offset /= channels;
    offset *= channels;
    if ( offset < 0 ) offset = -offset; //FIXME Offset should never become < 0. Find out why this happens.
    offset = QMIN( (guint) offset, available - SCOPE_VALUES*channels*sizeof( gint16 ) );

    for ( long i = 0; i < SCOPE_VALUES; i++, data += channels ) {
        long temp = 0;

        for ( int chan = 0; chan < channels; chan++ ) {
            // Add all channels together so we effectively get a mono scope
            temp += data[offset / sizeof( gint16 ) + chan];
        }
        m_scope[i] = temp / channels;
    }

//     debug() << "Timestamp first: " << firstStamp << endl;
//     debug() << "Timestamp last:  " << lastStamp << endl;
//     debug() << "Timestamp sink:  " << sinkStamp << endl;
//     debug() << "factor: " << factor << endl;
//     debug() << "offset: " << offset << endl;
//     debug() << endl;

    m_mutexScope.unlock();
    return m_scope;
}


amaroK::PluginConfig*
GstEngine::configure() const
{
    DEBUG_FUNC_INFO

    GstConfigDialog* dialog = new GstConfigDialog( this );
    connect( dialog, SIGNAL( settingsSaved() ), SLOT( configChanged() ) );

    return dialog;
}

/////////////////////////////////////////////////////////////////////////////////////
// PUBLIC SLOTS
/////////////////////////////////////////////////////////////////////////////////////

bool
GstEngine::load( const KURL& url, bool stream )  //SLOT
{
    DEBUG_BLOCK

    Engine::Base::load( url, stream );
    debug() << "Loading url: " << url.url() << endl;

    // Make sure we have a functional output pipeline
    if ( !m_pipelineFilled && !createPipeline() )
        return false;

    InputPipeline* input = new InputPipeline();
    if ( input->m_error ) {
        delete input;
        return false;
    }

    if ( url.isLocalFile() ) {
        // Use gst's filesrc element for local files, cause it's less overhead than KIO
        if ( !( input->src = createElement( "filesrc", input->bin ) ) ) { delete input; return false; }
        // Set file path
        gst_element_set( input->src, "location", static_cast<const char*>( QFile::encodeName( url.path() ) ), NULL );
    }
    else {
        // Create our custom streamsrc element, which transports data into the pipeline
        input->src = GST_ELEMENT( gst_streamsrc_new( m_streamBuf, &m_streamBufIndex, &m_streamBufStop, &m_streamBuffering ) );
        gst_element_set( input->src, "buffer_min", STREAMBUF_MIN, NULL );
        gst_bin_add ( GST_BIN ( input->bin ), input->src );
        g_signal_connect( G_OBJECT( input->src ), "kio_resume", G_CALLBACK( kio_resume_cb ), input->bin );

        m_streamBufIndex = 0;
        m_streamBufStop = false;
        m_streamBuffering = true;

        if ( !stream ) {
            // Use KIO for non-local files, except http, which is handled by StreamProvider
            m_transferJob = KIO::get( url, false, false );
            connect( m_transferJob, SIGNAL( data( KIO::Job*, const QByteArray& ) ), SLOT( newKioData( KIO::Job*, const QByteArray& ) ) );
            connect( m_transferJob, SIGNAL( result( KIO::Job* ) ), SLOT( kioFinished() ) );
        }
    }

    // Link all elements. The link from decodebin to audioconvert will be made in the newPad-callback
    gst_element_link( input->src, input->decodebin );
    gst_element_link_many( input->audioconvert, input->audioscale, input->volume, NULL );

    // Prepare bin for playing
    gst_element_set_state( input->bin, GST_STATE_READY );

    // Check if another track is already playing
    if ( m_currentInput ) {
        if ( m_xfadeLength ) {
            m_currentInput->setState( InputPipeline::XFADE_OUT );
            input->setState( InputPipeline::XFADE_IN );
            gst_element_set( input->volume, "volume", 0.0, NULL );
        }
        else
            destroyInput( m_currentInput );
    }
    else
        if ( GstConfig::fadeinDuration() ) {
            input->setState( InputPipeline::FADE_IN );
            gst_element_set( input->volume, "volume", 0.0, NULL );
    }

    m_currentInput = input;
    m_inputs.append( input );

    return true;
}


bool
GstEngine::play( uint offset )  //SLOT
{
    DEBUG_BLOCK

    m_eosReached = false;
    if ( !m_currentInput ) return false;

    // We must pause the queue before changing the state of the input thread, else the scheduler
    // freaks out. This is due to a bug in the queue element; will likely be fixed in gst-0.8.10.
    // @see http://bugzilla.gnome.org/show_bug.cgi?id=166250
    gst_element_set_state( m_gst_queue, GST_STATE_PAUSED );

    if ( !gst_element_set_state( m_gst_inputThread, GST_STATE_PAUSED ) )
        warning() << "Could not set input thread to PAUSED.\n";

    gst_element_set_state( m_gst_queue, GST_STATE_PLAYING );

    // Put input bin into input thread
    gst_bin_add( GST_BIN( m_gst_inputThread ), m_currentInput->bin );

    // Try to play input pipeline; if fails, destroy input bin
    if ( !gst_element_set_state( m_gst_inputThread, GST_STATE_PLAYING ) ) {
        warning() << "Could not set input thread to PLAYING.\n";
        destroyInput( m_currentInput );
        return false;
    }

    g_signal_connect( G_OBJECT( m_currentInput->bin ), "error", G_CALLBACK ( inputError_cb ), m_currentInput );

    // If "Resume playback on start" is enabled, we must seek to the last position
    if ( offset ) seek( offset );

    emit stateChanged( Engine::Playing );
    return true;
}


void
GstEngine::stop()  //SLOT
{
    DEBUG_BLOCK

    emit stateChanged( Engine::Empty );
    m_eosReached = false;

    RETURN_IF_PIPELINE_EMPTY
    if ( !m_currentInput ) return;

    // When engine is in pause mode, don't fade but destroy right away
    if ( state() == Engine::Paused )
        destroyInput( m_currentInput );
    else
        m_currentInput->setState( InputPipeline::FADE_OUT );
}


void
GstEngine::pause()  //SLOT
{
    DEBUG_BLOCK
    RETURN_IF_PIPELINE_EMPTY

    if ( GST_STATE( m_currentInput->bin ) == GST_STATE_PAUSED ) {
        gst_element_set_state( m_currentInput->bin, GST_STATE_PLAYING );
        emit stateChanged( Engine::Playing );
    }
    else {
        gst_element_set_state( m_currentInput->bin, GST_STATE_PAUSED );
        emit stateChanged( Engine::Paused );
    }
}


void
GstEngine::seek( uint ms )  //SLOT
{
    RETURN_IF_PIPELINE_EMPTY

    if ( ms > 0 ) {
        const int seekType = GST_FORMAT_TIME | GST_SEEK_METHOD_SET | GST_SEEK_FLAG_FLUSH;
        GstEvent* event = gst_event_new_seek( (GstSeekType) seekType, ms * GST_MSECOND );

        gst_element_send_event( m_gst_audiosink, event );
    }
}


void
GstEngine::newStreamData( char* buf, int size )  //SLOT
{
    if ( m_streamBufIndex + size >= STREAMBUF_SIZE ) {
        m_streamBufIndex = 0;
        debug() << "Stream buffer overflow!" << endl;
    }

    sendBufferStatus();

    // Copy data into stream buffer
    memcpy( m_streamBuf + m_streamBufIndex, buf, size );
    // Adjust index
    m_streamBufIndex += size;
}


void
GstEngine::setEqualizerEnabled( bool enabled ) //SLOT
{
    RETURN_IF_PIPELINE_EMPTY

    gst_element_set( m_gst_equalizer, "active", enabled, NULL );
}


void
GstEngine::setEqualizerParameters( int preamp, const QValueList<int>& bandGains ) //SLOT
{
    RETURN_IF_PIPELINE_EMPTY

    // BEGIN Preamp
    gst_element_set( m_gst_equalizer, "preamp", ( preamp + 100 ) / 2 , NULL );
    // END

    // BEGIN Gains
    m_equalizerGains.resize( bandGains.count() );
    for ( uint i = 0; i < bandGains.count(); i++ )
        m_equalizerGains[i] = ( *bandGains.at( i ) + 100 ) / 2;

    gst_element_set( m_gst_equalizer, "gain", &m_equalizerGains, NULL );
    // END
}


/////////////////////////////////////////////////////////////////////////////////////
// PROTECTED
/////////////////////////////////////////////////////////////////////////////////////

void
GstEngine::setVolumeSW( uint percent )  //SLOT
{
    RETURN_IF_PIPELINE_EMPTY

    gst_element_set( m_gst_volume, "volume", (double) percent * 0.01, NULL );
}


void GstEngine::timerEvent( QTimerEvent* )
{
    // Display stream buffer status
    sendBufferStatus();

    // Fading transition management:
    QPtrList<InputPipeline> destroyList;
    InputPipeline* input;

    // Iterate over all input pipelines
    for ( uint i = 0; i < m_inputs.count(); i++ )
    {
        input = m_inputs.at( i );

        switch ( input->state() )
        {
            case InputPipeline::NO_FADE:
                break;

            case InputPipeline::FADE_IN:
                input->m_fade -= ( GstConfig::fadeinDuration() ) ?  1.0 / GstConfig::fadeinDuration() * TIMER_INTERVAL : 1.0;

                // Fade finished?
                if ( input->m_fade < 0.0 ) {
                    // Fade transition has finished, stop playback
                    debug() << "Fade-in finished.\n";
                    input->setState( InputPipeline::NO_FADE );
                }
                else {
                    // Set new value for fadeout volume element
                    const double value = 1.0 - log10( input->m_fade * 9.0 + 1.0 );
//                     kdDebug() << "FADE_IN: " << value << endl;
                    gst_element_set( input->volume, "volume", value, NULL );
                }
                break;

            case InputPipeline::FADE_OUT:
                input->m_fade -= ( GstConfig::fadeoutDuration() ) ?  1.0 / GstConfig::fadeoutDuration() * TIMER_INTERVAL : 1.0;

                // Fade finished?
                if ( input->m_fade < 0.0 )
                    destroyList.append( input );
                else {
                    // Set new value for fadeout volume element
                    const double value = 1.0 - log10( ( 1.0 - input->m_fade ) * 9.0 + 1.0 );
//                     kdDebug() << "FADE_OUT: " << value << endl;
                    gst_element_set( input->volume, "volume", value, NULL );
                }
                break;

            case InputPipeline::XFADE_IN:
                input->m_fade -= ( m_xfadeLength ) ?  1.0 / m_xfadeLength * TIMER_INTERVAL : 1.0;

                // Fade finished?
                if ( input->m_fade < 0.0 ) {
                    // Fade transition has finished, stop playback
                    debug() << "XFade-in finished.\n";
                    input->setState( InputPipeline::NO_FADE );
                }
                else {
                    // Set new value for fadeout volume element
                    const double value = 1.0 - input->m_fade;
//                     kdDebug() << "XFADE_IN: " << value << endl;
                    gst_element_set( input->volume, "volume", value, NULL );
                }
                break;

            case InputPipeline::XFADE_OUT:
                input->m_fade -= ( m_xfadeLength ) ?  1.0 / m_xfadeLength * TIMER_INTERVAL : 1.0;

                // Fade finished?
                if ( input->m_fade < 0.0 )
                    destroyList.append( input );
                else {
                    // Set new value for fadeout volume element
                    const double value = 1.0 - log10( ( 1.0 - input->m_fade ) * 9.0 + 1.0 );
//                     kdDebug() << "XFADE_OUT: " << value << endl;
                    gst_element_set( input->volume, "volume", value, NULL );
                }
                break;
        }
    }

    // Destroy pipelines which are finished
    for ( uint i = 0; i < destroyList.count(); i++ )
        destroyInput( destroyList.at( i ) );
}


/////////////////////////////////////////////////////////////////////////////////////
// PRIVATE SLOTS
/////////////////////////////////////////////////////////////////////////////////////

void
GstEngine::handleOutputError()  //SLOT
{
    DEBUG_BLOCK

    QString text = "[GStreamer Error] ";
    text += m_gst_error;

    if ( !m_gst_debug.isEmpty() ) {
        text += " ** ";
        text += m_gst_debug;
    }

    m_gst_error = QString();
    emit statusText( text );
    error() << text << endl;

    // Destroy all pipelines
    destroyPipeline();

    emit trackEnded();
}


void
GstEngine::handleInputError()  //SLOT
{
    DEBUG_BLOCK

    QString text = "[GStreamer Error] ";
    text += m_gst_error;

    if ( !m_gst_debug.isEmpty() ) {
        text += " ** ";
        text += m_gst_debug;
    }
    m_gst_error = QString();
    error() << text << endl;
    emit statusText( text );
    error() << "Input-Pipeline has signaled an error. Destroying pipeline." << endl;

    m_inputError = true;
    destroyPipeline();
}


void
GstEngine::endOfStreamReached()  //SLOT
{
    DEBUG_BLOCK

    InputPipeline* input;

    // Iterate over all input pipelines
    for ( uint i = 0; i < m_inputs.count(); i++ ) {
        input = m_inputs.at( i );
        if ( input->m_eos ) {
            kdDebug() << "An input pipeline has reached EOS, destroying.\n";

            const bool fading = input->state() == InputPipeline::FADE_OUT ||
                                input->state() == InputPipeline::XFADE_OUT;

            destroyInput( input );

            // Simulate Idle state on EOS, if we are not fading
            if ( !m_currentInput ) m_eosReached = true;

            if ( !fading ) emit trackEnded();
        }
    }
}


void
GstEngine::newKioData( KIO::Job*, const QByteArray& array )  //SLOT
{
    const int size = array.size();

    if ( m_streamBufIndex >= STREAMBUF_MAX ) {
        debug() << "SUSPENDING kio transfer.\n";
        if ( m_transferJob ) m_transferJob->suspend();
    }

    if ( m_streamBufIndex + size >= STREAMBUF_SIZE ) {
        m_streamBufIndex = 0;
        debug() << "Stream buffer overflow!" << endl;
    }

    sendBufferStatus();

    // Copy data into stream buffer
    memcpy( m_streamBuf + m_streamBufIndex, array.data(), size );
    // Adjust index
    m_streamBufIndex += size;
}


void
GstEngine::newMetaData()  //SLOT
{
    emit metaData( m_metaBundle );
}


void
GstEngine::kioFinished()  //SLOT
{
    DEBUG_FUNC_INFO

    // KIO::Job deletes itself when finished, so we need to zero the pointer
    m_transferJob = 0;

    // Tell streamsrc: This is the end, my friend
    m_streamBufStop = true;
}


void
GstEngine::errorNoOutput() //SLOT
{
    KMessageBox::information( 0, i18n( "<p>Please select a GStreamer <u>output plugin</u> in the engine settings dialog.</p>" ) );

    // Show engine settings dialog
    showEngineConfigDialog();
}


void
GstEngine::configChanged() //SLOT
{
    debug() << "Rebuilding output pipeline with new settings.\n";

    // Stop playback and rebuild output pipeline, in order to apply new settings
    createPipeline();

    emit stateChanged( Engine::Empty );
}


/////////////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
/////////////////////////////////////////////////////////////////////////////////////

GstElement*
GstEngine::createElement( const QCString& factoryName, GstElement* bin, const QCString& name ) //static
{
    GstElement* element = gst_element_factory_make( factoryName, name );

    if ( element ) {
        if ( bin ) gst_bin_add( GST_BIN( bin ), element );
    }
    else {
        KMessageBox::error( 0,
            i18n( "<h3>GStreamer could not create the element: <i>%1</i></h3> "
                  "<p>Please make sure that you have installed all necessary GStreamer plugins (e.g. OGG and MP3), and run <i>'gst-register'</i> afterwards.</p>"
                  "<p>For further assistance consult the GStreamer manual, and join #gstreamer on irc.freenode.net.</p>" ).arg( factoryName ) );
        gst_object_unref( GST_OBJECT( bin ) );
    }

    return element;
}


QStringList
GstEngine::getPluginList( const QCString& classname ) const
{
    GList * pool_registries = NULL;
    GList* registries = NULL;
    GList* plugins = NULL;
    GList* features = NULL;
    QString name;
    QStringList results;

    pool_registries = gst_registry_pool_list ();
    registries = pool_registries;

    while ( registries ) {
        GstRegistry * registry = GST_REGISTRY ( registries->data );
        plugins = registry->plugins;

        while ( plugins ) {
            GstPlugin * plugin = GST_PLUGIN ( plugins->data );
            features = gst_plugin_get_feature_list ( plugin );

            while ( features ) {
                GstPluginFeature * feature = GST_PLUGIN_FEATURE ( features->data );

                if ( GST_IS_ELEMENT_FACTORY ( feature ) ) {
                    GstElementFactory * factory = GST_ELEMENT_FACTORY ( feature );

                    if ( g_strrstr ( factory->details.klass, classname ) ) {
                        name = g_strdup ( GST_OBJECT_NAME ( factory ) );
                        /*if ( name != "artsdsink" )*/
                        results << name;
                    }
                }
                features = g_list_next ( features );
            }
            plugins = g_list_next ( plugins );
        }
        registries = g_list_next ( registries );
    }
    g_list_free ( pool_registries );
    pool_registries = NULL;

    return results;
}


bool
GstEngine::createPipeline()
{
    DEBUG_BLOCK

    if ( m_pipelineFilled )
        destroyPipeline();

    if ( GstConfig::soundOutput().isEmpty() ) {
        QTimer::singleShot( 0, this, SLOT( errorNoOutput() ) );
        return false;
    }
    debug() << "Thread scheduling priority: " << GstConfig::threadPriority() << endl;
    debug() << "Sound output method: " << GstConfig::soundOutput() << endl;
    debug() << "CustomSoundDevice: " << ( GstConfig::useCustomSoundDevice() ? "true" : "false" ) << endl;
    debug() << "Sound Device: " << GstConfig::soundDevice() << endl;
    debug() << "CustomOutputParams: " << ( GstConfig::useCustomOutputParams() ? "true" : "false" ) << endl;
    debug() << "Output Params: " << GstConfig::outputParams() << endl;

    m_gst_rootBin = gst_bin_new( "root_bin" );

    //<input>
    if ( !( m_gst_inputThread = createElement( "thread" ) ) ) { return false; }
    if ( !( m_gst_adder = createElement( "adder", m_gst_inputThread ) ) ) { return false; }
    //</input>

    /* create a new pipeline (thread) to hold the elements */
    if ( !( m_gst_outputThread = createElement( "thread" ) ) ) { return false; }
    gst_element_set( m_gst_outputThread, "priority", GstConfig::threadPriority(), NULL );

    // Let gst construct the output element from a string
    QCString output  = GstConfig::soundOutput().latin1();
    if ( GstConfig::useCustomOutputParams() ) {
        output += " ";
        output += GstConfig::outputParams().latin1();
    }
    GError* err;
    if ( !( m_gst_audiosink = gst_parse_launch( output, &err ) ) ) {
        QTimer::singleShot( 0, this, SLOT( errorNoOutput() ) );
        return false;
    }
    gst_bin_add( GST_BIN( m_gst_outputThread ), m_gst_audiosink );

    /* setting device property for AudioSink*/
    if ( GstConfig::useCustomSoundDevice() && !GstConfig::soundDevice().isEmpty() )
        gst_element_set( m_gst_audiosink, "device", GstConfig::soundDevice().latin1(), NULL );

    if ( !( m_gst_queue = createElement( "queue", m_gst_outputThread ) ) ) { return false; }
    m_gst_equalizer = GST_ELEMENT( gst_equalizer_new() );
    gst_bin_add( GST_BIN( m_gst_outputThread ), m_gst_equalizer );
    if ( !( m_gst_identity = createElement( "identity", m_gst_outputThread ) ) ) { return false; }
    if ( !( m_gst_volume = createElement( "volume", m_gst_outputThread ) ) ) { return false; }

    // Put everything into the root bin
    gst_bin_add_many( GST_BIN( m_gst_rootBin ), m_gst_inputThread, m_gst_outputThread, NULL );

    // More buffers means less dropouts and higher latency
    gst_element_set( m_gst_queue, "max-size-buffers", 60, NULL );

    g_signal_connect( G_OBJECT( m_gst_identity ), "handoff", G_CALLBACK( handoff_cb ), NULL );
    g_signal_connect ( G_OBJECT( m_gst_outputThread ), "error", G_CALLBACK ( outputError_cb ), NULL );

    /* link elements */
    gst_element_link_many( m_gst_adder, m_gst_queue, m_gst_equalizer,  m_gst_identity, m_gst_volume, m_gst_audiosink, NULL );

    setVolume( m_volume );

    if ( !gst_element_set_state( m_gst_inputThread, GST_STATE_READY ) ) {
        error() << "Could not set inputThread to state READY!\n";
        destroyPipeline();
        return false;
    }
    if ( !gst_element_set_state( m_gst_outputThread, GST_STATE_PLAYING ) ) {
        error() << "Could not set outputThread to state PLAYING!\n";
        destroyPipeline();
        return false;
    }

    m_pipelineFilled = true;
    m_inputError = false;
    return true;
}


void
GstEngine::destroyPipeline()
{
    DEBUG_BLOCK

    m_fadeValue = 0.0;

    // Destroy all input pipelines
    m_inputs.clear();

    // Clear the scope adapter
    m_mutexScope.lock();
    gst_adapter_clear( m_gst_adapter );
    m_mutexScope.unlock();

    if ( m_pipelineFilled ) {
        debug() << "Unreffing root bin." << endl;
        gst_object_unref( GST_OBJECT( m_gst_rootBin ) );

        m_pipelineFilled = false;
    }

    // Destroy KIO transmission job
    if ( m_transferJob ) {
        m_transferJob->kill();
        m_transferJob = 0;
    }
}


void
GstEngine::destroyInput( InputPipeline* input )
{
    DEBUG_BLOCK

    if ( input ) {
        debug() << "Destroying input pipeline.\n";
        // Destroy the pipeline
        m_inputs.remove( input );
    }

    // Destroy KIO transmission job
    if ( m_transferJob ) {
        m_transferJob->kill();
        m_transferJob = 0;
    }
}


void
GstEngine::sendBufferStatus()
{
    if ( m_streamBuffering ) {
        const int percent = (int) ( (float) m_streamBufIndex / STREAMBUF_MIN * 105.0 );
        emit statusText( i18n( "Buffering.. %1%" ).arg( MIN( percent, 100 ) ) );
    }
}


/////////////////////////////////////////////////////////////////////////////////////
// CLASS InputPipeline
/////////////////////////////////////////////////////////////////////////////////////

InputPipeline::InputPipeline()
    : m_state( NO_FADE )
    , m_fade( 0.0 )
    , m_error( false )
    , m_eos( false )
{
    DEBUG_BLOCK

    /* create a new pipeline (thread) to hold the elements */
    if ( !( bin = GstEngine::createElement( "bin" ) ) ) { goto error; }
    if ( !( decodebin = GstEngine::createElement( "decodebin", bin ) ) ) { goto error; }
    if ( !( audioconvert = GstEngine::createElement( "audioconvert", bin ) ) ) { goto error; }
    if ( !( audioscale = GstEngine::createElement( "audioscale", bin ) ) ) { goto error; }
    if ( !( volume = GstEngine::createElement( "volume", bin ) ) ) { goto error; }

    g_signal_connect( G_OBJECT( decodebin ), "eos", G_CALLBACK( GstEngine::eos_cb ), this );
    g_signal_connect( G_OBJECT( decodebin ), "new-decoded-pad", G_CALLBACK( GstEngine::newPad_cb ), this );
    g_signal_connect( G_OBJECT( decodebin ), "found-tag", G_CALLBACK( GstEngine::found_tag_cb ), 0 );

    return;

error:
    m_error = true;
}


InputPipeline::~InputPipeline()
{
    DEBUG_BLOCK

    if ( GstEngine::instance()->m_currentInput == this )
        GstEngine::instance()->m_currentInput = 0;

    if ( GstEngine::instance()->m_inputError )
        return;

    debug() << "Destroying input bin.\n";

    if ( gst_element_get_managing_bin( bin ) == GST_BIN( GstEngine::instance()->m_gst_inputThread ) )
    {
        // We must pause the queue before changing the state of the input thread, else the scheduler
        // freaks out. This is due to a bug in the queue element; will likely be fixed in gst-0.8.10.
        // @see http://bugzilla.gnome.org/show_bug.cgi?id=166250
        gst_element_set_state( GstEngine::instance()->m_gst_queue, GST_STATE_PAUSED );

        if ( !gst_element_set_state( GstEngine::instance()->m_gst_inputThread, GST_STATE_PAUSED ) )
            warning() << "Could not set input thread to PAUSED.\n";

        gst_element_set_state( GstEngine::instance()->m_gst_queue, GST_STATE_PLAYING );

        gst_element_unlink( volume, GstEngine::instance()->m_gst_adder );

        // Destroy bin
        gst_element_set_state( bin, GST_STATE_NULL );
        gst_bin_remove( GST_BIN( GstEngine::instance()->m_gst_inputThread ), bin );

        if ( !gst_element_set_state( GstEngine::instance()->m_gst_inputThread, GST_STATE_PLAYING ) )
            warning() << "Could not set input thread to PLAYING.\n";
    }
    else
    {
        debug() << "Bin is not in thread.\n";
        gst_element_set_state( bin, GST_STATE_NULL );
        gst_bin_remove( GST_BIN( GstEngine::instance()->m_gst_inputThread ), bin );
    }
}


void
InputPipeline::setState( State newState )
{
    if ( m_error ) return;

    switch ( newState )
    {
        case NO_FADE:
            m_fade = 0.0;
            break;

        default:
            if ( m_state == NO_FADE )
                m_fade = 1.0;
    }

    m_state = newState;
}


#undef gst_indent
namespace Debug { QCString gst_indent; }


#include "gstengine.moc"


