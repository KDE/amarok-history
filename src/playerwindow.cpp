/***************************************************************************
                     playerwidget.cpp  -  description
                        -------------------
begin                : Mit Nov 20 2002
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

#include "actionclasses.h"
#include "amarok.h"
#include "amarokconfig.h"
#include "analyzerbase.h"
#include "enginecontroller.h"
#include "metabundle.h"      //setScroll()
#include "playerwindow.h"
#include "sliderwidget.h"
#include "tracktooltip.h"    //setScroll()

#include <qaccel.h>          //our quit shortcut in the ctor
#include <qevent.h>          //various events
#include <qfont.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qstringlist.h>
#include <qtooltip.h>        //analyzer tooltip

#include <kapplication.h>
#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kurldrag.h>
#include <kwin.h>            //eventFilter()

#include <X11/Xlib.h>
#include <X11/Xutil.h>


//simple function for fetching amarok images
namespace amaroK
{
    QPixmap getPNG( const QString &filename )
    {
        return QPixmap( locate( "data", QString( "amarok/images/%1.png" ).arg( filename ) ), "PNG" );
    }

    QPixmap getJPG( const QString &filename )
    {
        return QPixmap( locate( "data", QString( "amarok/images/%1.jpg" ).arg( filename ) ), "JPEG" );
    }
}

using amaroK::getPNG;


//fairly pointless template which was designed to make the ctor clearer,
//but probably achieves the opposite. Still, the code is neater..
template<class W> static inline W*
createWidget( const QRect &r, QWidget *parent, const char *name = 0, Qt::WFlags f = 0 )
{
    W *w = new W( parent, name, f );
    w->setGeometry( r );
    return w;
}



PlayerWidget::PlayerWidget( QWidget *parent, const char *name, bool enablePlaylist )
    : QWidget( parent, name, Qt::WType_TopLevel )
    , m_pAnimTimer( new QTimer( this ) )
    , m_scrollBuffer( 291, 16 )
    , m_plusPixmap( getPNG( "time_plus" ) )
    , m_minusPixmap( getPNG( "time_minus" ) )
    , m_pAnalyzer( 0 )
{
    //the createWidget template function is used here
    //createWidget just creates a widget which has it's geometry set too

    kdDebug() << "BEGIN " << k_funcinfo << endl;

    EngineController* const ec = EngineController::instance();
    EngineBase* const engine   = EngineController::engine();

    ec->attach( this );
    parent->installEventFilter( this ); //for hidePLaylistWithMainWindow mode

    //if this is the first time we have ever been run we let KWin place us
    if ( AmarokConfig::playerPos() != QPoint(-1,-1) )
        move( AmarokConfig::playerPos() );

    setModifiedPalette();
    setFixedSize( 311, 140 );
    setCaption( "amaroK" );
    setAcceptDrops( true );

    //another quit shortcut because the other window has all the accels
    QAccel *accel = new QAccel( this );
    accel->insertItem( CTRL + Key_Q );
    connect( accel, SIGNAL(activated( int )), kapp, SLOT(quit()) );

    QFont font;
    font.setBold( true );
    font.setPixelSize( 10 );
    setFont( font );

    { //<NavButtons>
        //NOTE we use a layout for the buttons so resizing will be possible
        m_pFrameButtons = createWidget<QHBox>( QRect(0, 118, 311, 22), this );

        KActionCollection *ac =amaroK::actionCollection();

        //FIXME change the names of the icons to reflect kde names so we can fall back to them if necessary
                         new NavButton( m_pFrameButtons, "prev", ac->action( "prev" ) );
        m_pButtonPlay  = new NavButton( m_pFrameButtons, "play", ac->action( "play" ) );
        m_pButtonPause = new NavButton( m_pFrameButtons, "pause", ac->action( "pause" ) );
                         new NavButton( m_pFrameButtons, "stop", ac->action( "stop" ) );
                         new NavButton( m_pFrameButtons, "next", ac->action( "next" ) );

        m_pButtonPlay->setToggleButton( true );
        m_pButtonPause->setToggleButton( true );
    } //</NavButtons>

    { //<Sliders>
        m_pSlider    = new amaroK::PrettySlider( Qt::Horizontal, this );
        m_pVolSlider = new amaroK::PrettySlider( Qt::Vertical, this, amaroK::VOLUME_MAX );

        m_pSlider->setGeometry( 4,103, 303,12 );
        m_pVolSlider->setGeometry( 294,18, 12,79 );
        m_pVolSlider->setValue( AmarokConfig::masterVolume() );

        connect( m_pSlider, SIGNAL(sliderReleased( int )), ec, SLOT(seek( int )) );
        connect( m_pSlider, SIGNAL(valueChanged( int )), SLOT(timeDisplay( int )) );
        connect( m_pVolSlider, SIGNAL(sliderMoved( int )), ec, SLOT(setVolume( int )) );
        connect( m_pVolSlider, SIGNAL(sliderReleased( int )), ec, SLOT(setVolume( int )) );
    } //<Sliders>

    { //<Scroller>
        font.setPixelSize( 11 );
        const int fontHeight = QFontMetrics( font ).height(); //the real height is more like 13px

        m_pScrollFrame = createWidget<QFrame>( QRect(6,18, 285,fontHeight), this );
        m_pScrollFrame->setFont( font );
    { //</Scroller>

    } //<TimeLabel>
        font.setPixelSize( 18 );

        m_pTimeLabel = createWidget<QLabel>( QRect(16,36, 9*12+2,18), this, 0, Qt::WNoAutoErase );
        m_pTimeLabel->setFont( font );

        m_timeBuffer.resize( m_pTimeLabel->size() );
        m_timeBuffer.fill( backgroundColor() );
    } //<TimeLabel>


    m_pButtonFx = new IconButton( this, "eq", SIGNAL(effectsWindowToggled( bool )) );
    m_pButtonFx->setGeometry( 34,85, 28,13 );
    //TODO set isOn()

    m_pPlaylistButton = new IconButton( this, "pl", SIGNAL(playlistToggled( bool )) );
    m_pPlaylistButton->setGeometry( 5,85, 28,13 );
    m_pPlaylistButton->setOn( parent->isShown() || enablePlaylist );


    m_pDescription = createWidget<QLabel>( QRect(4,6, 130,10), this );
    m_pTimeSign    = createWidget<QLabel>( QRect(6,40, 10,10), this, 0, Qt::WRepaintNoErase );
    m_pVolSign     = createWidget<QLabel>( QRect(295,7, 9,8),  this );

    m_pDescription->setPixmap( getPNG( "description" ) );
    m_pVolSign    ->setPixmap( getPNG( "vol_speaker" ) );


    //do before we set the widget's state
    applySettings();

    //set interface to correct state
    engineStateChanged( engine->state() );

    createAnalyzer( 0 );

    //so we get circulation events to x11Event()
    //XSelectInput( x11Display(), winId(), StructureNotifyMask );

    //Yagami mode!
    //KWin::setState( winId(), NET::KeepBelow | NET::SkipTaskbar | NET::SkipPager );
    //KWin::setType( winId(), NET::Override );
    //KWin::setOnAllDesktops( winId(), true );

    connect( m_pAnimTimer, SIGNAL( timeout() ), SLOT( drawScroll() ) );

    kdDebug() << "END " << k_funcinfo << endl;
}


PlayerWidget::~PlayerWidget()
{
    EngineController::instance()->detach( this );

    AmarokConfig::setPlayerPos( pos() );
    AmarokConfig::setPlaylistWindowEnabled( m_pPlaylistButton->isOn() );
}


// METHODS ----------------------------------------------------------------

void PlayerWidget::setScroll( const QStringList &list )
{
    QString text;
    QStringList list2( list );

    for( QStringList::Iterator it = list2.begin(); it != list2.end(); )
    {
        if( !(*it).isEmpty() )
        {
            text.append( *it );
            ++it;
        }
        else it = list2.remove( it );
    }

    //FIXME empty QString would crash due to NULL Pixmaps
    if( text.isEmpty() ) text = i18n( "Please report this message to amarok-devel@lists.sf.net, thanks!" );

    QFont font( m_pScrollFrame->font() );
    QFontMetrics fm( font );
    const uint separatorWidth = 21;
    const uint baseline = font.pixelSize(); //the font actually extends below its pixelHeight
    const uint separatorYPos = baseline - fm.boundingRect( "x" ).height() + 1;

    m_scrollTextPixmap.resize( fm.width( text ) + list2.count() * separatorWidth, m_pScrollFrame->height() );
    m_scrollTextPixmap.fill( backgroundColor() );

    QPainter p( &m_scrollTextPixmap );
    p.setPen( foregroundColor() );
    p.setFont( font );
    uint x = 0;

    for( QStringList::ConstIterator it = list2.constBegin();
         it != list2.constEnd();
         ++it )
    {
        p.drawText( x, baseline, *it );
        x += fm.width( *it );
        p.fillRect( x + 8, separatorYPos, 4, 4, amaroK::ColorScheme::Foreground );
        x += separatorWidth;
    }

    drawScroll();
}


void PlayerWidget::drawScroll()
{
    static uint phase = 0;

    QPixmap* const buffer = &m_scrollBuffer;
    QPixmap* const scroll = &m_scrollTextPixmap;

    const uint topMargin  = 0; //moved margins into widget placement
    const uint leftMargin = 0; //as this makes it easier to fiddle
    const uint w = m_scrollTextPixmap.width();
    const uint h = m_scrollTextPixmap.height();

    phase += SCROLL_RATE;
    if( phase >= w ) phase = 0;

    int subs = 0;
    int dx = leftMargin;
    uint phase2 = phase;

    while( dx < m_pScrollFrame->width() )
    {
        subs = -m_pScrollFrame->width() + topMargin;
        subs += dx + ( w - phase2 );
        if( subs < 0 ) subs = 0;

        bitBlt( buffer, dx, topMargin, scroll, phase2, 0, w - phase2 - subs, h, Qt::CopyROP );

        dx     += w - phase2;
        phase2 += w - phase2;

        if( phase2 >= w ) phase2 = 0;
    }

    bitBlt( m_pScrollFrame, 0, 0, buffer );
}


void PlayerWidget::engineStateChanged( Engine::State state )
{
    switch( state )
    {
        case Engine::Empty:
            m_pButtonPlay->setOn( false );
            m_pButtonPause->setOn( false );
            m_pSlider->setValue( 0 );
            m_pSlider->setMaxValue( 0 );
            m_pTimeLabel->hide();
            m_pTimeSign->hide();
            m_rateString = QString::null;
            m_pSlider->setEnabled( false );
            setScroll( i18n( "Welcome to amaroK" ) );
            update();
            break;

        case Engine::Playing:
            m_pTimeLabel->show();
            m_pTimeSign->show();
            m_pButtonPlay->setOn( true );
            m_pButtonPause->setOn( false );
            break;

        case Engine::Paused:
            m_pButtonPause->setOn( true );
            break;

        case Engine::Idle: //don't really want to do anything when idle
            break;
    }
}


void PlayerWidget::engineVolumeChanged( int percent )
{
    m_pVolSlider->setValue( percent );
}


void PlayerWidget::engineNewMetaData( const MetaBundle &bundle, bool )
{
    m_pSlider->setMaxValue( bundle.length() * 1000 );
    m_pSlider->setEnabled( bundle.length() > 0 );

    m_rateString     = bundle.prettyBitrate();
    const QString Hz = bundle.prettySampleRate( true );
    if( !Hz.isEmpty() )
    {
        if( !m_rateString.isEmpty() ) m_rateString += " - ";
        m_rateString += Hz;
    }

    QStringList list( bundle.prettyTitle() );
    list << bundle.album();
    if( bundle.length() ) list << bundle.prettyLength();
    setScroll( list );

    //update image tooltip
    TrackToolTip::add( m_pScrollFrame, bundle );

    update(); //we need to update rateString
}


void PlayerWidget::engineTrackPositionChanged( long position )
{
    m_pSlider->setValue( position );

    if( !m_pSlider->isEnabled() ) timeDisplay( position );
}


void PlayerWidget::timeDisplay( int ms )
{
    int seconds = ms / 1000;
    const int songLength = EngineController::instance()->bundle().length();
    const bool showRemaining = AmarokConfig::timeDisplayRemaining() && songLength > 0;

    if( showRemaining ) seconds = songLength - seconds;

    m_timeBuffer.fill( backgroundColor() );
    QPainter p( &m_timeBuffer );
    p.setPen( foregroundColor() );
    p.setFont( m_pTimeLabel->font() );
    p.drawText( 0, 16, MetaBundle::prettyTime( seconds ) ); //FIXME remove padding, instead move()!
    bitBlt( m_pTimeLabel, 0, 0, &m_timeBuffer );

    m_pTimeSign->setPixmap( showRemaining ? m_minusPixmap : m_plusPixmap );
}


static inline QColor comodulate( int hue, QColor target )
{
    ///this function is only used by determineAmarokColors()
    int ignore, s, v;
    target.getHsv( &ignore, &s, &v );
    return QColor( hue, s, v, QColor::Hsv );
}

void PlayerWidget::determineAmarokColors() //static
{
    int hue, s, v;

    (!AmarokConfig::schemeKDE()
        ? AmarokConfig::playlistWindowBgColor()
        : KGlobalSettings::highlightColor()
      ).getHsv( &hue, &s, &v );

    using namespace amaroK::ColorScheme;

    Text       = Qt::white;
    Background = comodulate( hue, 0x002090 );
    Foreground = comodulate( hue, 0x80A0FF );

    //ensure the base colour does not conflict with the window decoration colour
    //however generally it is nice to leave the other colours with the highlight hue
    //because the scheme is then "complimentary"
    //TODO schemes that have totally different active/inactive decoration colours need to be catered for too!
    if ( AmarokConfig::schemeKDE() ) {
        int h;
        KGlobalSettings::activeTitleColor().getHsv( &h, &s, &v );
        if( QABS( hue - h ) > 120 )
           hue = h;
    }

    Base = comodulate( hue, amaroK::blue );
}

void PlayerWidget::setModifiedPalette()
{
    QPalette p = QApplication::palette();
    QColorGroup cg = p.active();
    cg.setColor( QColorGroup::Background, amaroK::ColorScheme::Base );
    cg.setColor( QColorGroup::Foreground, amaroK::ColorScheme::Text );
    setPalette( QPalette(cg, p.disabled(), cg) );
}

void PlayerWidget::applySettings()
{
    //NOTE DON'T use unsetFont(), we use custom font sizes (for now)
    QFont phont = font();
    phont.setFamily( AmarokConfig::useCustomFonts()
        ? AmarokConfig::playerWidgetFont().family()
        : QApplication::font().family() );
    setFont( phont );

    setModifiedPalette();

    //update the scroller
    switch( EngineController::engine()->state() ) {
    case Engine::Empty:
        m_scrollTextPixmap.fill( amaroK::ColorScheme::Base );
        update();
        break;
    default:
        engineNewMetaData( EngineController::instance()->bundle(), false );
    }
}


// EVENTS -----------------------------------------------------------------

static bool dontChangeButtonState = false; //FIXME I hate this hack

bool PlayerWidget::event( QEvent *e )
{
    switch( e->type() )
    {
    case QEvent::Wheel:
    case QEvent::DragEnter:
    case QEvent::Drop:
    case QEvent::Close:

        amaroK::genericEventHandler( this, e );
        return TRUE; //we handled it

    case QEvent::ApplicationPaletteChange:

        if( AmarokConfig::schemeKDE() )
        {
            determineAmarokColors();
            applySettings();
        }
        return TRUE;

    case 6/*QEvent::KeyPress*/:
        if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_D/* && (m_pAnalyzer->inherits("QGLWidget")*/)
        {
            if (m_pAnalyzer->parent() != 0)
            {
                m_pAnalyzer->reparent(0, QPoint(50,50), true);
                m_pAnalyzer->setCaption( kapp->makeStdCaption( i18n("Analyzer") ) );
                m_pAnalyzer->installEventFilter( this );
                QToolTip::remove( m_pAnalyzer );
            }
            else createAnalyzer( 0 );

            return TRUE; //eat event
        }
        return FALSE; //don't eat event

    case QEvent::Show:

        kdDebug() << "Qt::showEvent\n";

        m_pAnimTimer->start( ANIM_TIMER );

        if( AmarokConfig::hidePlaylistWindow() && m_pPlaylistButton->isOn() )
        {
            //IMPORTANT! If the PlaylistButton is on then we MUST be shown
            //we leave the PlaylistButton "on" to signify that we should restore it here
            //we leave it on when we do a hidePlaylistWithPlayerWindow type action

            //IMPORTANT - I beg of you! Please leave all this alone, it was hell to
            //create! If you have an issue with the behaviour bring it up on the mailing
            //list before you even think about committing. Thanks! (includes case Hide)

            const WId id = parentWidget()->winId();
            const uint desktop = KWin::windowInfo( winId() ).desktop();
            const KWin::WindowInfo info = KWin::windowInfo( id );

            //check the Playlist Window is on the correct desktop
            if( !info.isOnDesktop( desktop ) ) KWin::setOnDesktop( id, desktop );

            if( info.mappingState() == NET::Withdrawn )
            {
                //extern Atom qt_wm_state; //XAtom defined by Qt

                //TODO prevent the active Window flicker from playlist to player window please!
                //TODO look at code for QWidget::show();

                //XDeleteProperty( qt_xdisplay(), id, qt_wm_state );

                //parentWidget()->show();
                //if( !parentWidget()->isShown() ) XMapWindow( qt_xdisplay(), id );
//                 unsigned long data[2];
//                 data[0] = (unsigned long) NormalState;
//                 data[1] = (unsigned long) None;
//
//                 XChangeProperty( qt_xdisplay(), id, qt_wm_state, qt_wm_state, 32,
//                      PropModeReplace, (unsigned char *)data, 2);
//
//                 KWin::clearState( id, NET::Hidden );
//
//                 XMapWindow( qt_xdisplay(), id );
//
                //KWin::deIconifyWindow( id, false );
                parentWidget()->show();
            }


            if( info.isMinimized() )
            {
                //then the user will expect us to deiconify the Playlist Window
                //the PlaylistButton would be off otherwise (honest!)
                KWin::deIconifyWindow( id, false );

            }
        }

        return FALSE;

    case QEvent::Hide:
        m_pAnimTimer->stop();

        if( AmarokConfig::hidePlaylistWindow() )
        {
            //this prevents the PlaylistButton being set to off (see the eventFilter)
            //by leaving it on we ensure that we show the Playlist Window again when
            //we are next shown (see Show event handler above)
            if( parentWidget()->isShown() ) dontChangeButtonState = true;

            if( e->spontaneous() ) //the window system caused the event
            {
                //if we have been iconified, iconify the Playlist Window too
                //if we have been shaded, hide the PlaylistWindow
                //if the user is on another desktop to amaroK, do nothing

                const KWin::WindowInfo info = KWin::windowInfo( winId() );

                #if KDE_IS_VERSION(3,2,1)
                if( info.hasState( NET::Shaded ) )
                {
                    //FIXME this works if the OSD is up, and only then sometimes
                    //      I think it's maybe a KWin bug..
                    parentWidget()->hide();
                }
                else
                #endif
                if( info.isMinimized() ) KWin::iconifyWindow( parentWidget()->winId(), false );
                else
                    //this may seem strange, but it is correct
                    //we have a handler in eventFilter for all other eventualities
                    dontChangeButtonState = false;

            }
            else
                //we caused amaroK to hide, so we should hide the Playlist Window
                //NOTE we "override" closeEvents and thus they count as non-spontaneous
                //hideEvents; which frankly is a huge relief!
                parentWidget()->hide();
        }

        return FALSE;

    default:
        return QWidget::event( e );
    }
}

// bool
// PlayerWidget::x11Event( XEvent *e )
// {
//     if( e->type == ConfigureNotify )
//     {
//         kdDebug() << "CirculateNotify\n";
//         XRaiseWindow( x11Display(), playlistWindow()->winId() );
//     }
//
//     return false;
// }

bool
PlayerWidget::eventFilter( QObject *o, QEvent *e )
{
    //NOTE we only monitor for parent() - which is the PlaylistWindow

    if( o == m_pAnalyzer )
    {
        //delete analyzer, create same one back in Player Window
        if( e->type() == QEvent::Close )
        {
            createAnalyzer( 0 );
            return TRUE;
        }
        return FALSE;
    }

    switch( e->type() )
    {
    case QEvent::Close:

        static_cast<QCloseEvent*>(e)->accept(); //close the window!
        return TRUE; //don't let PlaylistWindow have the event - see PlaylistWindow::closeEvent()

    case QEvent::Hide:

        if( dontChangeButtonState )
        {
            //we keep the PlaylistButton set to "on" - see event() for more details
            //NOTE the Playlist Window will still be hidden

            dontChangeButtonState = false;
            break;
        }

        if( e->spontaneous() )
        {
            //we want to avoid setting the button for most spontaneous events
            //since they are not user driven, two are however:

            KWin::WindowInfo info = KWin::windowInfo( parentWidget()->winId() );

            if( !(info.isMinimized()
            #if KDE_IS_VERSION(3,2,1)
            || info.hasState( NET::Shaded )
            #endif
            ) ) break;
        }

        //FALL THROUGH

    case QEvent::Show:

        if( isShown() )
        {
            //only when shown means thaman:mkreiserfst using the global Show/Hide Playlist shortcut
            //when in the tray doesn't effect the state of the PlaylistButton
            //this is a good thing, but we have to set the state correctly when we are shown

            m_pPlaylistButton->blockSignals( true );
            m_pPlaylistButton->setOn( e->type() == QEvent::Show );
            m_pPlaylistButton->blockSignals( false );
        }
        break;

    default:
        break;
    }

    return FALSE;
}


void PlayerWidget::paintEvent( QPaintEvent* )
{
    //uses widget's font and foregroundColor() - see ctor
    QPainter p( this );
    p.drawText( 6, 68, m_rateString );

    bitBlt( m_pScrollFrame, 0, 0, &m_scrollBuffer );
    bitBlt( m_pTimeLabel,   0, 0, &m_timeBuffer );
}


void PlayerWidget::mousePressEvent( QMouseEvent *e )
{
    if ( e->button() == QMouseEvent::RightButton )
    {
        amaroK::Menu::instance()->exec( e->globalPos() );
    }
    else if ( m_pAnalyzer->geometry().contains( e->pos() ) )
    {
        createAnalyzer( e->state() & Qt::ControlButton ? -1 : +1 );
    }
    else
    {
        QRect
        rect  = m_pTimeLabel->geometry();
        rect |= m_pTimeSign->geometry();

        if ( rect.contains( e->pos() ) )
        {
            AmarokConfig::setTimeDisplayRemaining( !AmarokConfig::timeDisplayRemaining() );
            timeDisplay( EngineController::engine()->position() / 1000 );
        }
        else m_startDragPos = e->pos();
    }
}


void PlayerWidget::mouseMoveEvent( QMouseEvent *e )
{
    if( e->state() & Qt::LeftButton )
    {
        const int distance = (e->pos() - m_startDragPos).manhattanLength();

        if( distance > QApplication::startDragDistance() ) startDrag();
    }
}


// SLOTS ---------------------------------------------------------------------

void PlayerWidget::createAnalyzer( int increment )
{
    AmarokConfig::setCurrentAnalyzer( AmarokConfig::currentAnalyzer() + increment );

    delete m_pAnalyzer;

    m_pAnalyzer = Analyzer::Factory::createAnalyzer( this );
    m_pAnalyzer->setGeometry( 120,40, 168,56 );
    QToolTip::add( m_pAnalyzer, i18n( "Click for more analyzers, press 'd' to detach." ) );
    m_pAnalyzer->show();

}


void PlayerWidget::startDrag()
{
    QDragObject *d = new QTextDrag( EngineController::instance()->bundle().prettyTitle(), this );
    d->dragCopy();
    // Qt will delete d for us.
}


void PlayerWidget::setEffectsWindowShown( bool on )
{
    m_pButtonFx->blockSignals( true );
    m_pButtonFx->setOn( on );
    m_pButtonFx->blockSignals( false );
}


//////////////////////////////////////////////////////////////////////////////////////////
// CLASS NavButton
//////////////////////////////////////////////////////////////////////////////////////////

#include <kiconeffect.h>
#include <kimageeffect.h>
NavButton::NavButton( QWidget *parent, const QString &icon, KAction *action )
    : QToolButton( parent )
    , m_glowIndex( 0 )
{
    // Prevent flicker
    setWFlags( Qt::WNoAutoErase );

    QPixmap pixmap( getPNG( "b_" + icon ) );
    KIconEffect ie;

    // Tint icon blueish for "off" state
    m_pixmapOff = ie.apply( pixmap, KIconEffect::Colorize, 0.5, QColor( 0x30, 0x10, 0xff ), false );
    // Tint gray and make pseudo-transparent for "disabled" state
    m_pixmapDisabled = ie.apply( pixmap, KIconEffect::ToGray, 0.7, QColor(), true );

    int r = 0x20, g = 0x10, b = 0xff;
    float percentRed = 0.0;
    QPixmap temp;
    // Precalculate pixmaps for "on" icon state
    for ( int i = 0; i < NUMPIXMAPS; i++ ) {
        QImage img = pixmap.convertToImage();
        temp = KImageEffect::channelIntensity( img, percentRed, KImageEffect::Red );
        temp = ie.apply( temp, KIconEffect::Colorize, 1.0, QColor( r, 0x10, 0x30 ), false );
        temp = ie.apply( temp, KIconEffect::Colorize, 1.0, QColor( r, g, b ), false );

        // Create new pixmap on the heap and add pointer to list
        m_glowPixmaps.append( temp );

        percentRed = percentRed + 1.0 / NUMPIXMAPS;
        r += 14;
        g += 2;
        b -= 0;
    }
    // And the the same reversed
    for ( int i = NUMPIXMAPS - 1; i > 0; i-- )
        m_glowPixmaps.append( m_glowPixmaps[i] );

    // This is just for initialization
    QIconSet iconSet;
    iconSet.setPixmap( pixmap, QIconSet::Automatic, QIconSet::Normal, QIconSet::Off );
    iconSet.setPixmap( pixmap, QIconSet::Automatic, QIconSet::Normal, QIconSet::On );
    iconSet.setPixmap( pixmap, QIconSet::Automatic, QIconSet::Disabled, QIconSet::Off );
    setIconSet( iconSet );

    setFocusPolicy( QWidget::NoFocus );
    setEnabled( action->isEnabled() );

    connect( action, SIGNAL(enabled( bool )), SLOT(setEnabled( bool )) );
    connect( this, SIGNAL(clicked()), action, SLOT(activate()) );
    startTimer( GLOW_INTERVAL );
}


void NavButton::timerEvent( QTimerEvent* )
{
    if ( isOn() ) {
        m_glowIndex++;
        m_glowIndex %= NUMPIXMAPS * 2 - 1;

        // Repaint widget with new pixmap
        update();
    }
}


void NavButton::drawButtonLabel( QPainter* p )
{
    int x = width() / 2 - m_pixmapOff.width() / 2;
    int y = height() / 2 - m_pixmapOff.height() / 2;

    if ( !isEnabled() )
        p->drawPixmap( x, y, m_pixmapDisabled );
    else if ( isOn() )
        p->drawPixmap( x + 2, y + 1, m_glowPixmaps[m_glowIndex] );
    else
        p->drawPixmap( x, y, m_pixmapOff );
}


//////////////////////////////////////////////////////////////////////////////////////////
// CLASS IconButton
//////////////////////////////////////////////////////////////////////////////////////////

IconButton::IconButton( QWidget *parent, const QString &icon, const char *signal )
    : QButton( parent )
    , m_up(   getPNG( icon + "_active2" ) ) //TODO rename files better (like the right way round for one!)
    , m_down( getPNG( icon + "_inactive2" ) )
{
    connect( this, SIGNAL(toggled( bool )), parent, signal );

    setToggleButton( true );
    setFocusPolicy( NoFocus ); //we have no way to show focus on these widgets currently
}

void IconButton::drawButton( QPainter *p )
{
    p->drawPixmap( 0, 0, (isOn()||isDown()) ? m_down : m_up );
}

#include "playerwindow.moc"
