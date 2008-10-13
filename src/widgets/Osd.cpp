/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * osd.cpp:   Shows some text in a pretty way independent to the WM
 * begin:     Fre Sep 26 2003
 * copyright: (C) 2004 Christian Muehlhaeuser <chris@chris.de>
 *            (C) 2004-2006 Seb Ruiz <ruiz@kde.org>
 *            (C) 2004, 2005 Max Howell
 *            (C) 2005 Gábor Lehel <illissius@gmail.com>
 *            (C) 2008 Mark Kretschmann <kretschmann@kde.org>
 */

#define DEBUG_PREFIX "OSD"

#include "Osd.h"

#include "Amarok.h"
#include "Debug.h"
#include "EngineController.h"
#include "StarManager.h"
#include "SvgHandler.h"
#include "amarokconfig.h"
#include "meta/MetaUtility.h"

#include <plasma/panelsvg.h>

#include <KApplication>
#include <KIcon>
#include <KStandardDirs>   //locate

#include <QDesktopWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRegExp>
#include <QTimer>

namespace ShadowEngine
{
    QImage makeShadow( const QPixmap &textPixmap, const QColor &bgColor );
}

namespace Amarok
{
    QImage icon() { return QImage( KIconLoader::global()->iconPath( "amarok", -KIconLoader::SizeHuge ) ); }
}

OSDWidget::OSDWidget( QWidget *parent, const char *name )
        : QWidget( parent )
        , m_duration( 2000 )
        , m_timer( new QTimer( this ) )
        , m_alignment( Middle )
        , m_screen( 0 )
        , m_y( MARGIN )
        , m_drawShadow( true )
        , m_rating( 0 )
        , m_volume( false )
{
    Qt::WindowFlags flags;
    flags = Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint;
    // The best of both worlds.  On Windows, setting the widget as a popup avoids a task manager entry.  On linux, a popup steals focus.
    // Therefore we go need to do it platform specific :(
    #ifdef Q_OS_WIN
    flags |= Qt::Popup;
    #else
    flags |= Qt::Window | Qt::X11BypassWindowManagerHint;
    #endif
    setWindowFlags( flags );
    setObjectName( name );
    setFocusPolicy( Qt::NoFocus );
    unsetColors();

    m_timer->setSingleShot( true );


    connect( m_timer, SIGNAL(timeout()), SLOT(hide()) );
    //PORT 2.0
//     connect( CollectionDB::instance(), SIGNAL( ratingChanged( const QString&, int ) ),
//              this, SLOT( ratingChanged( const QString&, int ) ) );

    //or crashes, KWindowSystem bug I think, crashes in QWidget::icon()
    kapp->setTopWidget( this );
}

OSDWidget::~OSDWidget()
{
    DEBUG_BLOCK
}

void
OSDWidget::show( const QString &text, QImage newImage )
{
    DEBUG_BLOCK
    m_volume = false;
    if ( !newImage.isNull() )
    {
        m_cover = newImage;
        int w = m_scaledCover.width();
        int h = m_scaledCover.height();
        m_scaledCover = QPixmap::fromImage( m_cover.scaled( w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
    }
    else
        m_cover = Amarok::icon();

    m_text = text;
    if( !isVisible() )
        show();

    update(); // needed if e.g. cover image changed
}

void
OSDWidget::ratingChanged( const QString& path, int rating )
{
    Meta::TrackPtr track = The::engineController()->currentTrack();
    if( !track )
        return;
    if( track->playableUrl().isLocalFile() && track->playableUrl().path() == path )
        ratingChanged( rating );
}

void
OSDWidget::ratingChanged( const short rating )
{
    m_text = '\n' + i18n( "Rating changed" );
    setRating( rating ); //Checks isEnabled() before doing anything

    show();
}

void
OSDWidget::volChanged( unsigned char volume )
{
    if ( isEnabled() )
    {
        m_volume = true;
        m_newvolume = volume;
        m_text = m_newvolume ? i18n("Volume: %1%", m_newvolume) : i18nc( "State, as in, The playback is silent", "Mute");

        show();
    }
}

void
OSDWidget::show() //virtual
{
    if ( !isEnabled() || m_text.isEmpty() )
        return;

    const uint M = fontMetrics().width( 'x' );

    const QRect oldGeometry = QRect( pos(), size() );
    const QRect newGeometry = determineMetrics( M );

    if( newGeometry.width() > 0 && newGeometry.height() > 0 )
    {
        m_m = M;
        m_size = newGeometry.size();
        setGeometry( newGeometry );
        QWidget::show();

        if( m_duration ) //duration 0 -> stay forever
        {
            m_timer->start( m_duration ); //calls hide()
        }
    }
    else
        warning() << "Attempted to make an invalid sized OSD\n";

    update();
}

QRect
OSDWidget::determineMetrics( const int M )
{
    // sometimes we only have a tiddly cover
    const QSize minImageSize = m_cover.size().boundedTo( QSize(100,100) );

    // determine a sensible maximum size, don't cover the whole desktop or cross the screen
    const QSize margin( (M + MARGIN) * 2, (M + MARGIN) * 2 ); //margins
    const QSize image = m_cover.isNull() ? QSize( 0, 0 ) : minImageSize;
    const QSize max = QApplication::desktop()->screen( m_screen )->size() - margin;

    // If we don't do that, the boundingRect() might not be suitable for drawText() (Qt issue N67674)
    m_text.replace( QRegExp(" +\n"), "\n" );
    // remove consecutive line breaks
    m_text.replace( QRegExp("\n+"), "\n" );

    // The osd cannot be larger than the screen
    QRect rect = fontMetrics().boundingRect( 0, 0, max.width() - image.width(), max.height(),
            Qt::AlignCenter | Qt::TextWordWrap, m_text );

    if( m_volume )
    {
        static const QString tmp = QString ("******").insert( 3,
            ( i18n("Volume: 100%").length() >= i18nc( "State, as in, the playback is silent", "Mute" ).length() )?
            i18n("Volume: 100%") : i18nc( "State, as in, the playback is silent", "Mute" ) );

        QRect tmpRect = fontMetrics().boundingRect( 0, 0,
            max.width() - image.width(), max.height() - fontMetrics().height(),
            Qt::AlignCenter | Qt::TextWordWrap, tmp );
        tmpRect.setHeight( tmpRect.height() + fontMetrics().height() / 2 );

        rect = tmpRect;

        if( m_newvolume > 66 )
            m_cover = KIcon( "audio-volume-high-amarok" ).pixmap( 100, 100 ).toImage();
        else if ( m_newvolume > 33 )
            m_cover = KIcon( "audio-volume-medium-amarok" ).pixmap( 100, 100 ).toImage();
        else if ( m_newvolume > 0 )
            m_cover = KIcon( "audio-volume-low-amarok" ).pixmap( 100, 100 ).toImage();
        else
            m_cover = KIcon( "audio-volume-muted-amarok" ).pixmap( 100, 100 ).toImage();

    }

    if( m_rating )
    {
        QPixmap* star = StarManager::instance()->getStar( 1 );
        if( rect.width() < star->width() * 5 )
            rect.setWidth( star->width() * 5 ); //changes right edge position
        rect.setHeight( rect.height() + star->height() + M ); //changes bottom edge pos
    }

    if( !m_cover.isNull() )
    {
        const int availableWidth = max.width() - rect.width() - M; //WILL be >= (minImageSize.width() - M)

        m_scaledCover = QPixmap::fromImage(
                m_cover.scaled(
                    qMin( availableWidth, m_cover.width() ),
                    qMin( rect.height(), m_cover.height() ),
                    Qt::KeepAspectRatio, Qt::SmoothTransformation
                              )
                                          ); //this will force us to be with our bounds

        int shadowWidth = 0;
        if( m_drawShadow && !m_scaledCover.hasAlpha() &&
          ( m_scaledCover.width() > 22 || m_scaledCover.height() > 22 ) )
            shadowWidth = static_cast<uint>( m_scaledCover.width() / 100.0 * 6.0 );

        const int widthIncludingImage = rect.width()
                + m_scaledCover.width()
                + shadowWidth
                + M; //margin between text + image

        rect.setWidth( widthIncludingImage );
    }

    // expand in all directions by M
    rect.adjust( -M, -M, M, M );

    const QSize newSize = rect.size();
    const QRect screen = QApplication::desktop()->screenGeometry( m_screen );
    QPoint newPos( MARGIN, m_y );

    switch( m_alignment )
    {
        case Left:
            break;

        case Right:
            newPos.rx() = screen.width() - MARGIN - newSize.width();
            break;

        case Center:
            newPos.ry() = (screen.height() - newSize.height()) / 2;

            //FALL THROUGH

        case Middle:
            newPos.rx() = (screen.width() - newSize.width()) / 2;
            break;
    }

    //ensure we don't dip below the screen
    if ( newPos.y() + newSize.height() > screen.height() - MARGIN )
        newPos.ry() = screen.height() - MARGIN - newSize.height();

    // correct for screen position
    newPos += screen.topLeft();

    return QRect( newPos, rect.size() );
}

void
OSDWidget::paintEvent( QPaintEvent *e )
{
    int M = m_m;
    QSize size = m_size;

    QPoint point;
    QRect rect( point, size );
    rect.adjust( 0, 0, -1, -1 );

    QColor shadowColor;
    {
        int h,s,v;
        palette().color(QPalette::Normal, QPalette::Foreground ).getHsv( &h, &s, &v );
        shadowColor = v > 128 ? Qt::black : Qt::white;
    }

    int align = Qt::AlignCenter | Qt::TextWordWrap;

    QPainter p( this );
    p.setRenderHints( QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform | QPainter::HighQualityAntialiasing );

    p.setClipRect(e->rect());


    QPixmap background = The::svgHandler()->renderSvgWithDividers( "service_list_item", width(), height(), "service_list_item" );
    p.drawPixmap( 0, 0, background );

    
//     p.setPen( palette().color( QPalette::Active, QPalette::HighlightedText ) );
    p.setPen( Qt::white ); // Revert this when the background can be colorized again.
    rect.adjust( M, M, -M, -M );

    if( !m_cover.isNull() )
    {
        QRect r( rect );
        r.setTop( (size.height() - m_scaledCover.height()) / 2 );
        r.setSize( m_scaledCover.size() );

        if( !m_scaledCover.hasAlpha() && m_drawShadow &&
          ( m_scaledCover.width() > 22 || m_scaledCover.height() > 22 ) ) {
            // don't draw a shadow for eg, the Amarok icon
            QImage shadow;
            const uint shadowSize = static_cast<uint>( m_scaledCover.width() / 100.0 * 6.0 );

            const QString folder = Amarok::saveLocation( "covershadow-cache/" );
            const QString file = QString( "shadow_albumcover%1x%2.png" ).arg( m_scaledCover.width()  + shadowSize )
                                                                        .arg( m_scaledCover.height() + shadowSize );
            if ( QFile::exists( folder + file ) )
                shadow.load( folder + file );
            else {
                shadow.load( KStandardDirs::locate( "data", "amarok/images/shadow_albumcover.png" ) );
                shadow = shadow.scaled( m_scaledCover.width() + shadowSize, m_scaledCover.height() + shadowSize,
                                                                Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
                shadow.save( folder + file, "PNG" );
            }

            QPixmap target = QPixmap::fromImage( shadow ); //FIXME slow
            QPainter painter( &target );
            painter.drawPixmap( 0, 0, m_scaledCover );
            m_scaledCover = target;

            r.setTop( (size.height() - m_scaledCover.height()) / 2 );
            r.setSize( m_scaledCover.size() );
        }

        p.drawPixmap( r.topLeft(), m_scaledCover );

        rect.setLeft( rect.left() + m_scaledCover.width() + M );
    }

    QPixmap* star = StarManager::instance()->getStar( m_rating/2 );
    int graphicsHeight = 0;

    if( m_rating > 0 )
    {
        QRect r( rect );

        //Align to center...
        r.setLeft(( rect.left() + rect.width() / 2 ) - star->width() * m_rating / 4 );
        r.setTop( rect.bottom() - star->height() );
        graphicsHeight += star->height() + M;

        bool half = m_rating%2;

        if( half )
        {
            QPixmap* halfStar = StarManager::instance()->getHalfStar( m_rating/2 + 1 );
            p.drawPixmap( r.left() + star->width() * ( m_rating / 2 ), r.top(), *halfStar );
            star = StarManager::instance()->getStar( m_rating/2 + 1 );
        }

        for( int i = 0; i < m_rating/2; i++ )
        {
            p.drawPixmap( r.left() + i * star->width(), r.top(), *star );
        }

    }

    rect.setBottom( rect.bottom() - graphicsHeight );

    // Draw "shadow" text effect (black outline)
    if( m_drawShadow )
    {
        QPixmap pixmap( rect.size() + QSize(10,10) );
        pixmap.fill( Qt::black );

        QPainter p2( &pixmap );
        p2.setFont( font() );
        p2.setPen( Qt::white );
        p2.setBrush( Qt::white );
        p2.drawText( QRect(QPoint(5,5), rect.size()), align , m_text );
        p2.end();

        p.drawImage( rect.topLeft() - QPoint(5,5), ShadowEngine::makeShadow( pixmap, shadowColor ) );
    }
    p.setPen( palette().color( QPalette::Active, QPalette::WindowText ) );
    //p.setPen( Qt::white ); // This too.
    p.setFont( font() );
    p.drawText( rect, align, m_text );
}

void
OSDWidget::resizeEvent(QResizeEvent *e)
{
    //setMask(m_background->mask());
    QWidget::resizeEvent(e);
}

bool
OSDWidget::event( QEvent *e )
{
    switch( e->type() )
    {
    case QEvent::ApplicationPaletteChange:
        if( !AmarokConfig::osdUseCustomColors() )
            unsetColors(); //use new palette's colours
        return true;

    default:
        return QWidget::event( e );
    }
}

void
OSDWidget::mousePressEvent( QMouseEvent* )
{
    hide();
}

void
OSDWidget::unsetColors()
{
    QPalette p = QApplication::palette();
    QPalette newPal = palette();

    newPal.setColor( QPalette::Active, QPalette::WindowText, p.color( QPalette::Active, QPalette::WindowText ) );
    newPal.setColor( QPalette::Active, QPalette::Window    , p.color( QPalette::Active, QPalette::Window ) );
    setPalette( newPal );
}

void
OSDWidget::setScreen( int screen )
{
    const int n = QApplication::desktop()->numScreens();
    m_screen = (screen >= n) ? n-1 : screen;
}


/////////////////////////////////////////////////////////////////////////////////////////
// Class OSDPreviewWidget
/////////////////////////////////////////////////////////////////////////////////////////

OSDPreviewWidget::OSDPreviewWidget( QWidget *parent )
        : OSDWidget( parent )
        , m_dragging( false )
{
    setObjectName( "osdpreview" );
    m_text = i18n( "OSD Preview - drag to reposition" );
    m_duration = 0;
    m_alignment = static_cast<Alignment>( AmarokConfig::osdAlignment() );
    m_y = AmarokConfig::osdYOffset();
    m_cover = Amarok::icon();
    QFont f = font();
    f.setPointSize( 16 );
    setFont( f );
    setTranslucent( AmarokConfig::osdUseTranslucency() );
    show( m_text, m_cover );
}

void
OSDPreviewWidget::mousePressEvent( QMouseEvent *event )
{
    m_dragOffset = event->pos();

    if( event->button() == Qt::LeftButton && !m_dragging ) {
        grabMouse( Qt::SizeAllCursor );
        m_dragging = true;
    }
}

void
OSDPreviewWidget::mouseReleaseEvent( QMouseEvent * /*event*/ )
{
    if( m_dragging )
    {
        m_dragging = false;
        releaseMouse();

        // compute current Position && offset
        QDesktopWidget *desktop = QApplication::desktop();
        int currentScreen = desktop->screenNumber( pos() );

        if( currentScreen != -1 ) {
            // set new data
            m_screen = currentScreen;
            m_y      = QWidget::y();

            emit positionChanged();
        }
    }
}

void
OSDPreviewWidget::mouseMoveEvent( QMouseEvent *e )
{
    if( m_dragging && this == mouseGrabber() )
    {
        // Here we implement a "snap-to-grid" like positioning system for the preview widget

        const QRect screen      = QApplication::desktop()->screenGeometry( m_screen );
        const uint  hcenter     = screen.width() / 2;
        const uint  eGlobalPosX = e->globalPos().x() - screen.left();
        const uint  snapZone    = screen.width() / 24;

        QPoint destination = e->globalPos() - m_dragOffset - screen.topLeft();
        int maxY = screen.height() - height() - MARGIN;
        if( destination.y() < MARGIN ) destination.ry() = MARGIN;
        if( destination.y() > maxY ) destination.ry() = maxY;

        if( eGlobalPosX < (hcenter-snapZone) ) {
            m_alignment = Left;
            destination.rx() = MARGIN;
        }
        else if( eGlobalPosX > (hcenter+snapZone) ) {
            m_alignment = Right;
            destination.rx() = screen.width() - MARGIN - width();
        }
        else {
            const uint eGlobalPosY = e->globalPos().y() - screen.top();
            const uint vcenter     = screen.height()/2;

            destination.rx() = hcenter - width()/2;

            if( eGlobalPosY >= (vcenter-snapZone) && eGlobalPosY <= (vcenter+snapZone) )
            {
                m_alignment = Center;
                destination.ry() = vcenter - height()/2;
            }
            else m_alignment = Middle;
        }

        destination += screen.topLeft();

        move( destination );
    }
}


/////////////////////////////////////////////////////////////////////////////////////////
// Class OSD
/////////////////////////////////////////////////////////////////////////////////////////

Amarok::OSD* Amarok::OSD::s_instance = 0;

Amarok::OSD*
Amarok::OSD::instance()
{
    return s_instance ? s_instance : new OSD();
}

void
Amarok::OSD::destroy() {
    if (s_instance) {
        delete s_instance;
        s_instance = 0;
    }
}

Amarok::OSD::OSD()
    : OSDWidget( 0 )
{
    s_instance = this;
    The::engineController()->attach( this );
}

Amarok::OSD::~OSD()
{
    The::engineController()->detach( this );
}

void
Amarok::OSD::show( Meta::TrackPtr track ) //slot
{
    setAlignment( static_cast<OSDWidget::Alignment>( AmarokConfig::osdAlignment() ) );
    setOffset( AmarokConfig::osdYOffset() );

    QString text;
    if( !track || track->playableUrl().isEmpty() )
        text = i18n( "No track playing" );

    else
    {
        setRating( track->rating() );
        text = track->prettyName();
        if( track->artist() )
            text = track->artist()->prettyName() + " - " + text;
        if( track->album() )
            text += "\n (" + track->album()->prettyName() + ") ";
        if( track->length() > 0)
            text += Meta::secToPrettyTime( track->length() );
    }

    if( text.isEmpty() )
        text =  track->playableUrl().fileName();

    if( text.startsWith( "- " ) ) //When we only have a title tag, _something_ prepends a fucking hyphen. Remove that.
        text = text.mid( 2 );

    if( text.isEmpty() ) //still
        text = i18n("No information available for this track");

    QImage image;
    if( track && track->album() )
        image = track->album()->imageWithBorder( 100, 5 ).toImage();

    OSDWidget::show( text, image );
}

void
Amarok::OSD::applySettings()
{
    setAlignment( static_cast<OSDWidget::Alignment>( AmarokConfig::osdAlignment() ) );
    setDuration( AmarokConfig::osdDuration() );
    setEnabled( AmarokConfig::osdEnabled() );
    setOffset( AmarokConfig::osdYOffset() );
    setScreen( AmarokConfig::osdScreen() );
    setFont( AmarokConfig::osdFont() );

    if( AmarokConfig::osdUseCustomColors() )
        setTextColor( AmarokConfig::osdTextColor() );
    else
        unsetColors();

    setTranslucent( AmarokConfig::osdUseTranslucency() );
}

void
Amarok::OSD::forceToggleOSD()
{
    if ( !isVisible() ) {
        const bool b = isEnabled();
        setEnabled( true );
        show( The::engineController()->currentTrack() );
        setEnabled( b );
    }
    else
        hide();
}

void
Amarok::OSD::engineNewMetaData( const QHash<qint64, QString> &newMetaData, bool trackChanged )
{
    if (!trackChanged && !isMetaDataSpam( newMetaData ))// metadata spam protection from streams
        show( The::engineController()->currentTrack() );
    else
        show( The::engineController()->currentTrack() );
}

void
Amarok::OSD::engineNewTrackPlaying()
{
    show( The::engineController()->currentTrack() );
}

void
Amarok::OSD::engineVolumeChanged( int newVolume )
{
    volChanged( newVolume );
}

/* Try to detect MetaData spam in Streams. */
bool
Amarok::OSD::isMetaDataSpam( const QHash<qint64, QString> &newMetaData )
{
    // search for Metadata in history
    for( int i = 0; i < m_metaDataHistory.size(); i++)
    {
        if( m_metaDataHistory.at( i ) == newMetaData ) // we already had that one -> spam!
        {
            m_metaDataHistory.move( i, 0 ); // move spam to the beginning of the list
            return true;
        }
    }

    if( m_metaDataHistory.size() == 12 )
        m_metaDataHistory.removeLast();

    m_metaDataHistory.insert( 0, newMetaData);
    return false;
}

/* Code copied from kshadowengine.cpp
 *
 * Copyright (C) 2003 Laur Ivan <laurivan@eircom.net>
 *
 * Many thanks to:
 *  - Bernardo Hung <deciare@gta.igs.net> for the enhanced shadow
 *    algorithm (currently used)
 *  - Tim Jansen <tim@tjansen.de> for the API updates and fixes.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

namespace ShadowEngine
{
    // Not sure, doesn't work above 10
    static const int    MULTIPLICATION_FACTOR = 3;
    // Multiplication factor for pixels directly above, under, or next to the text
    static const double AXIS_FACTOR = 2.0;
    // Multiplication factor for pixels diagonal to the text
    static const double DIAGONAL_FACTOR = 0.1;
    // Self explanatory
    static const int    MAX_OPACITY = 200;

    double decay( QImage&, int, int );

    QImage makeShadow( const QPixmap& textPixmap, const QColor &bgColor )
    {
        const int w   = textPixmap.width();
        const int h   = textPixmap.height();
        const int bgr = bgColor.red();
        const int bgg = bgColor.green();
        const int bgb = bgColor.blue();

        int alphaShadow;

        // This is the source pixmap
        QImage img = textPixmap.toImage();

        QImage result( w, h, QImage::Format_ARGB32 );
        result.fill( 0 ); // fill with black

        static const int M = 5;
        for( int i = M; i < w - M; i++) {
            for( int j = M; j < h - M; j++ )
            {
                alphaShadow = (int) decay( img, i, j );

                result.setPixel( i,j, qRgba( bgr, bgg , bgb, qMin( MAX_OPACITY, alphaShadow ) ) );
            }
        }

        return result;
    }

    double decay( QImage& source, int i, int j )
    {
        //if ((i < 1) || (j < 1) || (i > source.width() - 2) || (j > source.height() - 2))
        //    return 0;

        double alphaShadow;
        alphaShadow =(qGray(source.pixel(i-1,j-1)) * DIAGONAL_FACTOR +
                qGray(source.pixel(i-1,j  )) * AXIS_FACTOR +
                qGray(source.pixel(i-1,j+1)) * DIAGONAL_FACTOR +
                qGray(source.pixel(i  ,j-1)) * AXIS_FACTOR +
                0                         +
                qGray(source.pixel(i  ,j+1)) * AXIS_FACTOR +
                qGray(source.pixel(i+1,j-1)) * DIAGONAL_FACTOR +
                qGray(source.pixel(i+1,j  )) * AXIS_FACTOR +
                qGray(source.pixel(i+1,j+1)) * DIAGONAL_FACTOR) / MULTIPLICATION_FACTOR;

        return alphaShadow;
    }
}

#include "Osd.moc"