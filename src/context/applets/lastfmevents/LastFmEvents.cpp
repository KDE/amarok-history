/***************************************************************************
* copyright            : (C) 2007 Leo Franchi <lfranchi@gmail.com>        *
**************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "LastFmEvents.h"

#include "debug.h"
#include "Theme.h"

#include <QGraphicsTextItem>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QPainter>
#include <QVariant>
#include <QFontMetrics>

#include <KDialog>
#include <KLocale>

#define DEBUG_PREFIX "LastFmEvents"

LastFmEvents::LastFmEvents( QObject* parent, const QVariantList& args )
    : Context::Applet( parent, args )
    , m_config( 0 )
    , m_configLayout( 0 )
    , m_friendBox( 0 )
    , m_sysBox( 0 )
    , m_userBox( 0 )
    , m_friendEnabled( false )
    , m_sysEnabled( false )
    , m_userEnabled( false )
{
    DEBUG_BLOCK
    Context::Theme::self()->setApplication( "amarok" );

    debug() << "Loading LastFmEvents applet";

    setHasConfigurationInterface( true );

    KConfigGroup conf = globalConfig();
    m_userEnabled = conf.readEntry( "user", false );
    m_sysEnabled = conf.readEntry( "sys", false );
    m_friendEnabled = conf.readEntry( "friend", false );
    m_width = conf.readEntry( "width" , 400 );

    if( !m_userEnabled && !m_friendEnabled && m_sysEnabled )
        showConfigurationInterface();

    if( args.size() > 0 ) // we are being told what position to start at
        setPos( (qreal)args[0].toInt(), (qreal)args[1].toInt() );

    m_theme = new Context::Svg( "widgets/amarok-lastfm", this );
    m_theme->setContentType( Plasma::Svg::SingleImage );

    for( int i = 0; i < 14; i++ ) // create all the items
    {
        m_titles << new QGraphicsSimpleTextItem( this );
        m_dates << new QGraphicsSimpleTextItem( this );
        m_cities << new QGraphicsSimpleTextItem( this );
        // white font for now
        m_titles[ i ]->setBrush( Qt::white );
        m_dates[ i ]->setBrush( Qt::white );
        m_cities[ i ]->setBrush( Qt::white );
    }

    // calculate aspect ratio, and resize to desired width
    m_theme->resize();
    m_aspectRatio = (qreal)m_theme->size().height() / (qreal)m_theme->size().width();
    resize( m_width, m_aspectRatio );

    dataEngine( "amarok-lastfm" )->connectSource( I18N_NOOP( "sysevents" ), this );
    dataEngine( "amarok-lastfm" )->connectSource( I18N_NOOP( "userevents" ), this );
    dataEngine( "amarok-lastfm" )->connectSource( I18N_NOOP( "friendevents" ), this );

    dataUpdated( "sysevents", dataEngine( "amarok-lastfm" )->query( "sysevents" ) );
    dataUpdated( "userevents", dataEngine( "amarok-lastfm" )->query( "userevents" ) );
    dataUpdated( "friendevents", dataEngine( "amarok-lastfm" )->query( "friendevents" ) );

    constraintsUpdated();
    updateGeometry();

}

LastFmEvents::~LastFmEvents()
{
}
/*
void LastFmEvents::setGeometry( const QRectF& rect )
{
    DEBUG_BLOCK
    debug() << "setting geometry to" << rect;
    setPos( rect.topLeft() );
//     resize( rect.width(), m_aspectRatio );
    setSize( rect.size() );
    Applet::setGeometry( rect );
}*/

QSizeF LastFmEvents::contentSizeHint() const 
{
    debug() << "returning size:" << size();
    return size();
}

void LastFmEvents::constraintsUpdated( Plasma::Constraints constr )
{
    prepareGeometryChange();

    for( int i = 0; i < 14; i++ ) // go through each row
    {
        QString titleElement = QString( "title%1" ).arg( i );
        QString dateElement = QString( "date%1" ).arg( i );
        QString cityElement = QString( "city%1" ).arg( i );

        m_titles[ i ]->setPos( m_theme->elementRect( titleElement ).topLeft() );
        m_dates[ i ]->setPos( m_theme->elementRect( dateElement ).topLeft() );
        m_cities[ i ]->setPos( m_theme->elementRect( cityElement ).topLeft() );

        m_titles[ i ]->setFont( shrinkTextSizeToFit( m_titles[ i ]->text(), m_theme->elementRect( titleElement ) ) );
        m_dates[ i ]->setFont( shrinkTextSizeToFit( m_dates[ i ]->text(), m_theme->elementRect( dateElement ) ) );
        m_cities[ i ]->setFont( shrinkTextSizeToFit( m_cities[ i ]->text(), m_theme->elementRect( cityElement ) ) );
    }
}

void LastFmEvents::dataUpdated( const QString& name, const Context::DataEngine::Data& data )
{
    DEBUG_BLOCK
//         debug() << "got data from engine: " << data;
    Context::DataEngine::DataIterator iter( data );
    if( m_sysEnabled && name == QString( "sysevents" ) )
    {
        int count = 4; // system events are the 5th-10th rows
        while( iter.hasNext() )
        {
            iter.next();
            const QVariantList event = iter.value().toList();
            if( event.size() == 0 || count > 9 ) continue; // empty event, or we are done

            m_titles[ count ]->setFont( shrinkTextSizeToFit( m_titles[ count ]->text(), m_theme->elementRect(  QString( "title%1" ).arg( count ) ) ) );
            m_dates[ count ]->setFont( shrinkTextSizeToFit( m_dates[ count ]->text(), m_theme->elementRect( QString( "date%1" ).arg( count ) ) ) );
            m_cities[ count ]->setFont( shrinkTextSizeToFit( m_cities[ count ]->text(), m_theme->elementRect( QString( "city%1" ).arg( count ) ) ) );

            m_titles[ count ]->setText( truncateTextToFit( event[ 0 ].toString(),
                m_titles[ count ]->font(),
                m_theme->elementRect(  QString( "title%1" ).arg( count ) ) ) );
            m_dates[ count ]->setText( truncateTextToFit( event[ 1 ].toString(),
                m_dates[ count ]->font(),
                m_theme->elementRect(  QString( "date%1" ).arg( count ) ) ) );
            m_cities[ count ]->setText( truncateTextToFit( event[ 3 ].toString(),
                m_cities[ count ]->font(),
                m_theme->elementRect(  QString( "city%1" ).arg( count ) ) ) );


            count++;
        }
    }
    if( m_friendEnabled && name == QString( "friendevents" ) )
    {
        int count = 0; // first 5 rows
        while( iter.hasNext() )
        {
            iter.next();
            const QVariantList event = iter.value().toList();
            if( event.size() == 0 || count > 4) continue; // empty event

            m_titles[ count ]->setFont( shrinkTextSizeToFit( m_titles[ count ]->text(), m_theme->elementRect( QString( "title%1" ).arg( count ) ) ) );
            m_dates[ count ]->setFont( shrinkTextSizeToFit( m_dates[ count ]->text(), m_theme->elementRect( QString( "date%1" ).arg( count ) ) ) );
            m_cities[ count ]->setFont( shrinkTextSizeToFit( m_cities[ count ]->text(), m_theme->elementRect( QString( "city%1" ).arg( count ) ) ) );

            m_titles[ count ]->setText( truncateTextToFit( event[ 0 ].toString(),
                m_titles[ count ]->font(),
                m_theme->elementRect(  QString( "title%1" ).arg( count ) ) ) );
            m_dates[ count ]->setText( truncateTextToFit( event[ 1 ].toString(),
                m_dates[ count ]->font(),
                m_theme->elementRect(  QString( "date%1" ).arg( count ) ) ) );
            m_cities[ count ]->setText( truncateTextToFit( event[ 3 ].toString(),
                m_cities[ count ]->font(),
                m_theme->elementRect(  QString( "city%1" ).arg( count ) ) ) );

            count++;
        }
    }
    if( m_userEnabled && name == QString( "userevents" ) )
    {
        int count = 10;
        while( iter.hasNext() )
        {
            iter.next();
            const QVariantList event = iter.value().toList();
            if( event.size() == 0 || count > 14) continue; // empty event

            m_titles[ count ]->setFont( shrinkTextSizeToFit( m_titles[ count ]->text(), m_theme->elementRect(  QString( "title%1" ).arg( count ) ) ) );
            m_dates[ count ]->setFont( shrinkTextSizeToFit( m_dates[ count ]->text(), m_theme->elementRect( QString( "date%1" ).arg( count ) ) ) );
            m_cities[ count ]->setFont( shrinkTextSizeToFit( m_cities[ count ]->text(), m_theme->elementRect( QString( "city%1" ).arg( count ) ) ) );

            m_titles[ count ]->setText( truncateTextToFit( event[ 0 ].toString(),
                m_titles[ count ]->font(),
                m_theme->elementRect(  QString( "title%1" ).arg( count ) ) ) );
            m_dates[ count ]->setText( truncateTextToFit( event[ 1 ].toString(),
                m_dates[ count ]->font(),
                m_theme->elementRect(  QString( "date%1" ).arg( count ) ) ) );
            m_cities[ count ]->setText( truncateTextToFit( event[ 3 ].toString(),
                m_cities[ count ]->font(),
                m_theme->elementRect(  QString( "city%1" ).arg( count ) ) ) );

            count++;
        }
    }

    update();
}

void LastFmEvents::paintInterface( QPainter *p, const QStyleOptionGraphicsItem *option, const QRect& contentsRect )
{
    DEBUG_BLOCK
    Q_UNUSED( option )

    p->setRenderHint(QPainter::SmoothPixmapTransform);

    debug() << "painting rect: " << contentsRect << endl;
    m_theme->paint( p, contentsRect );

    for( int i = 0; i < 14; i++ )
    {
        QString titleElement = QString( "title%1" ).arg( i );
        QString dateElement = QString( "date%1" ).arg( i );
        QString cityElement = QString( "city%1" ).arg( i );

        QRectF titleRect = m_theme->elementRect( titleElement );
        QRectF dateRect = m_theme->elementRect( dateElement );
        QRectF cityRect = m_theme->elementRect( cityElement );

        m_titles[ i ]->setPos( titleRect.topLeft() );
        m_dates[ i ]->setPos( dateRect.topLeft() );
        m_cities[ i ]->setPos( cityRect.topLeft() );

    }

}

void LastFmEvents::showConfigurationInterface()
{
    DEBUG_BLOCK
    if (m_config == 0)
    {
        m_config = new KDialog;
        m_config->setCaption( i18n( "Configure Last.Fm Events" ) );

        QWidget* widget = new QWidget( m_config );
        m_config->setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply );
        connect( m_config, SIGNAL(applyClicked()), this, SLOT(configAccepted()) );
        connect( m_config, SIGNAL(okClicked()), this, SLOT(configAccepted()) );

        m_configLayout = new QVBoxLayout( widget );
        m_friendBox = new QCheckBox();
        m_friendBox->setText( i18n( "Show Friend Events" ) );
        m_sysBox = new QCheckBox();
        m_sysBox->setText( i18n( "Show Recommended Events" ) );
        m_userBox = new QCheckBox();
        m_userBox->setText( i18n( "Show Your Future Events" ) );

        m_configLayout->addWidget( m_friendBox );
        m_configLayout->addWidget( m_sysBox );
        m_configLayout->addWidget( m_userBox );

        m_config->setMainWidget( widget );
    }

    m_config->show();
}

void LastFmEvents::configAccepted() // SLOT
{
    DEBUG_BLOCK
    debug() << "saving config";
    KConfigGroup cg = globalConfig();

    m_friendBox->checkState() ? m_friendEnabled = true : m_friendEnabled = false;
    m_sysBox->checkState() ? m_sysEnabled = true : m_sysEnabled = false;
    m_userBox->checkState() ? m_userEnabled = true : m_userEnabled = false;

    cg.writeEntry( "friend" , m_friendEnabled );
    cg.writeEntry( "sys" , m_sysEnabled );
    cg.writeEntry( "user" , m_userEnabled );

    cg.sync();

    constraintsUpdated();
}

QFont LastFmEvents::shrinkTextSizeToFit( const QString& text, const QRectF& bounds )
{
    Q_UNUSED( text );
    int size = 12; // start here, shrink if needed
//     QString font = "Arial";
    QFontMetrics fm( QFont( QString(), size ) );
    while( fm.height() > bounds.height() + 4 )
    {
//         debug() << "trying to get size: " << fm.height() << " less than: " << bounds.height();
        size--;
        fm = QFontMetrics( QFont( QString(), size ) );
    }

    // for aesthetics, we make it one smaller
    size--;

//     debug() << "resulting after shrink: " << ":" << size;
    return QFont( QString(), size );
}

// returns truncated text with ... appended.
QString LastFmEvents::truncateTextToFit( QString text, const QFont& font, const QRectF& bounds )
{
    QFontMetrics fm( font );
    while( fm.width( text) > bounds.width() )
    {
        text.chop( 4 );
        text += "...";
    }
    return text;
}

void LastFmEvents::resize( qreal newWidth, qreal aspectRatio )
{
    DEBUG_BLOCK
//     debug() << "aspectRatio:" << aspectRatio;
//     debug() << "resizing to:" << newWidth;
    qreal height = aspectRatio * newWidth;
    setSize( QSizeF( newWidth, height ) );
//     m_theme->resize( size() );
    debug() << "set new size: " << size();
    constraintsUpdated();
}

#include "LastFmEvents.moc"
