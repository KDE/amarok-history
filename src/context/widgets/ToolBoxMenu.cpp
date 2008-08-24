/*******************************************************************************
* copyright              : (C) 2008 William Viana Soares <vianasw@gmail.com>   *
*                                                                              *
********************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "ToolBoxMenu.h"

#include "Debug.h"

#include "plasma/applet.h"

namespace Context

{
    
AmarokToolBoxMenu::AmarokToolBoxMenu( QGraphicsItem *parent, bool runningAppletsOnly )
    : QGraphicsItem( parent )
    , m_containment( 0 )
    , m_removeApplets( false )
    , m_menuSize( 4 )
    , m_showing( 0 )
    , m_delay( 250 )
{
    QMap< QString, QString > allApplets;
    QStringList appletsToShow; // we need to store the list of
    // all appletname->appletpluginname pairs, even if we dont show them
    
    foreach ( const KPluginInfo& info, Plasma::Applet::listAppletInfo( QString(), "amarok" ) )
    {
        if ( info.property( "NoDisplay" ).toBool() )
        {
            // we don't want to show the hidden category
            continue;
        }

        allApplets.insert( info.name(), info.pluginName() );
        if( !runningAppletsOnly )
            appletsToShow << info.name();        
    }
    
    if( runningAppletsOnly )
    {
       m_removeApplets = true;
       Containment* cont = dynamic_cast<Containment *>( parent );
       if( cont )
       {
           foreach( Plasma::Applet* applet, cont->applets() )
           {
               appletsToShow << applet->name();
           }
       }
    }
    
    init( allApplets, appletsToShow );
}

AmarokToolBoxMenu::~AmarokToolBoxMenu()
{
    delete m_hideIcon;
    delete m_downArrow;
    delete m_upArrow;
    delete m_timer;
    delete m_scrollDelay;
}

void
AmarokToolBoxMenu::init( QMap< QString, QString > allApplets, QStringList appletsToShow )
{
    setAcceptsHoverEvents( true );

    m_appletsList = allApplets;
    
    m_timer = new QTimer( this );
    m_scrollDelay = new QTimer( this );

    connect( m_timer, SIGNAL( timeout() ), this, SLOT( timeToHide() ) );
    connect( m_scrollDelay, SIGNAL( timeout() ), this, SLOT( delayedScroll() ) );

    //insert in the stack so the first applet in alphabetical order is the first 
    for( int i = appletsToShow.size() - 1; i >= 0; i-- )
        m_bottomMenu.push( appletsToShow[i] );

    populateMenu();

    m_hideIcon = new ToolBoxIcon( this );

    QAction *hideMenu = new QAction( "", this );
    hideMenu->setIcon( KIcon( "window-close" ) );
    hideMenu->setEnabled( true );
    hideMenu->setVisible( true );

    connect( hideMenu, SIGNAL( triggered() ), this, SLOT( hide() ) );
    m_hideIcon->setAction( hideMenu );
    m_hideIcon->setToolTip( i18n( "Hide menu" ) );
    QSizeF iconSize = m_hideIcon->sizeFromIconSize( 22 );

    m_hideIcon->setMinimumSize( iconSize );
    m_hideIcon->setMaximumSize( iconSize );
    m_hideIcon->resize( m_hideIcon->size() );

    m_hideIcon->setPos( 5, boundingRect().height() - 32 * m_menuSize - 50 );
    m_hideIcon->setZValue( zValue() + 1 );
    m_hideIcon->hide();

    m_upArrow = new ToolBoxIcon( this );
    m_downArrow = new ToolBoxIcon( this );
    createArrow( m_upArrow, "up" );
    createArrow( m_downArrow, "down" );
}

void
AmarokToolBoxMenu::setContainment( Containment *newContainment )
{
    m_containment = newContainment;
    initRunningApplets();
}

Containment *
AmarokToolBoxMenu::containment() const
{
    return m_containment;
}

QRectF
AmarokToolBoxMenu::boundingRect() const
{
    return QRectF( QPointF( 0, 0 ), QSize( 185, 42 * ( m_menuSize + 2 ) ) );
}

void
AmarokToolBoxMenu::populateMenu()
{
    for( int i = 0; i < m_menuSize; i++ )
    {
        if( m_bottomMenu.isEmpty() )
            continue;
        ToolBoxIcon *entry = new ToolBoxIcon( this );

        const QString appletName = m_bottomMenu.pop();

        setupMenuEntry( entry, appletName );
        entry->hide();
        m_currentMenu << entry;
    }
}

void
AmarokToolBoxMenu::initRunningApplets()
{
    DEBUG_BLOCK
    if( !containment() )
        return;

    Plasma::Corona *corona = containment()->corona();

    if( !corona )
        return;

    m_runningApplets.clear();
    QList<Plasma::Containment *> containments = corona->containments();
    foreach( Plasma::Containment *containment, containments )
    {
        connect( containment, SIGNAL( appletAdded( Plasma::Applet *, QPointF ) ),
                 this, SLOT( appletAdded( Plasma::Applet *) ) );
        connect( containment, SIGNAL( appletRemoved( Plasma::Applet * ) ),
                 this, SLOT( appletRemoved( Plasma::Applet * ) ) );
        QList<QString> appletNames;
        foreach( Plasma::Applet *applet, containment->applets() )
        {
            appletNames << applet->pluginName();
            m_appletNames[applet] = applet->pluginName();
        }
        m_runningApplets[containment] = appletNames;
    }
}

void
AmarokToolBoxMenu::appletAdded( Plasma::Applet *applet )
{
    if( sender() != 0 )
    {
        Plasma::Containment *containment = dynamic_cast<Plasma::Containment *>( sender() );
        if( containment )
        {
            m_runningApplets[containment] << applet->pluginName();
            m_appletNames[applet] = applet->pluginName();
        }
    }
}

void
AmarokToolBoxMenu::appletRemoved( Plasma::Applet *applet )
{
    DEBUG_BLOCK
    if( sender() != 0 )
    {
        Plasma::Containment *containment = dynamic_cast<Plasma::Containment *>( sender() );
        if( containment )
        {
            QString name = m_appletNames.take( applet );
            m_runningApplets[containment].removeAll( name );
        }
    }
}

bool
AmarokToolBoxMenu::showing() const
{
    return m_showing;
}

void
AmarokToolBoxMenu::show()
{
    if( showing() )
        return;

    m_showing = true;

    if( m_removeApplets ) // we need to refresh on view to get all running applets
    {
        m_bottomMenu.clear();
        m_topMenu.clear();
        m_currentMenu.clear();
        foreach( Plasma::Applet* applet, containment()->applets() )
        {
            m_bottomMenu.push( applet->name() );
        }
        populateMenu();
    }
    if( m_bottomMenu.count() > 0 )
    {
        m_downArrow->setPos( boundingRect().width() / 2 - m_downArrow->size().width()/2,
                            boundingRect().height() - 20 );
        m_downArrow->resetTransform();
        m_downArrow->show();
    }

    if( m_topMenu.count() > 0 )
    {
        const int height = static_cast<int>( m_currentMenu.first()->boundingRect().height() ) + 9;
        m_upArrow->resetTransform();
        m_upArrow->setPos( boundingRect().width()/2 - m_upArrow->size().width()/2,
                            boundingRect().height() - m_menuSize * height - 50 );
        m_upArrow->show();
    }

    m_hideIcon->show();
    for( int i = m_currentMenu.count() - 1; i >= 0; i-- )
    {
        ToolBoxIcon *entry = m_currentMenu[m_currentMenu.count() - i - 1];
        entry->show();
        const int height = static_cast<int>( entry->boundingRect().height() ) + 9;

        Plasma::Animator::self()->moveItem( entry, Plasma::Animator::SlideInMovement,
                                            QPoint( 5, boundingRect().height() - height * i - 50 ) );
    }
}

void
AmarokToolBoxMenu::hide()
{
    if( !showing() )
        return;
    m_showing = false;
    foreach( QGraphicsItem *c, QGraphicsItem::children() )
        c->hide();
    emit menuHidden();
}

void
AmarokToolBoxMenu::setupMenuEntry( ToolBoxIcon *entry, const QString &appletName )
{
        entry->setDrawBackground( true );
        entry->setOrientation( Qt::Horizontal );
        entry->setText( appletName );

        QSizeF size( 180, 24 );
        entry->setMinimumSize( size );
        entry->setMaximumSize( size );
        entry->resize( size );

        entry->setPos( 5, boundingRect().height() );

        entry->setZValue( zValue() + 1 );
        entry->setData( 0, QVariant( m_appletsList[appletName] ) );
        entry->show();
        if( m_removeApplets )
        {
            connect( entry, SIGNAL( appletChosen( const QString & ) ),
                     this, SLOT( removeApplet( const QString & ) ) );
        } else
        {
            connect( entry, SIGNAL( appletChosen( const QString & ) ), this, SLOT( addApplet( const QString & ) ) );
        }
}

void
AmarokToolBoxMenu::addApplet( const QString &pluginName )
{
    DEBUG_BLOCK
    if( pluginName != QString() )
    {
        bool appletFound = false;
        //First we check if the applet is already running and in that case we just change
        //to the containment where the applet is otherwise we add the applet to the current containment.
        foreach( Plasma::Containment *containment, m_runningApplets.keys() )
        {
            if( m_runningApplets[containment].contains( pluginName ) )
            {
                emit changeContainment( containment );
                appletFound = true;
                break;
            }
        }
        if( !appletFound && containment() )
            containment()->addApplet( pluginName );
    }
}

void
AmarokToolBoxMenu::removeApplet( const QString& pluginName )
{
    DEBUG_BLOCK
    if( pluginName == QString() )
        return;
    
    // this is not ideal, but we look through all running applets to find
    // the one that we want
    foreach( Plasma::Applet* applet, containment()->applets() )
    {
        if( applet->pluginName() == pluginName )
        {
            // this is the applet we want to remove
            applet->destroy();
            // the rest of the cleanup should happen in
            // appletRemoved, which is called by the containment
        }
    }
}
    
void
AmarokToolBoxMenu::createArrow( ToolBoxIcon *arrow, const QString &direction )
{
    QAction *action = new QAction( "", this );

    if( direction == "up" )
        action->setIcon( KIcon( "arrow-up" ) );
    else
        action->setIcon( KIcon( "arrow-down" ) );

    action->setVisible( true );
    action->setEnabled( true );
    if( direction == "up" )
        connect( action, SIGNAL( triggered() ), this, SLOT( scrollUp() ) );
    else
        connect( action, SIGNAL( triggered() ), this, SLOT( scrollDown() ) );

    arrow->setAction( action );
    arrow->setDrawBackground( false );
    arrow->setOrientation( Qt::Horizontal );

    QSizeF iconSize = arrow->sizeFromIconSize( 22 );

    arrow->setMinimumSize( iconSize );
    arrow->setMaximumSize( iconSize );
    arrow->resize( arrow->size() );

    arrow->setZValue( zValue() + 1 );
    arrow->hide();

}

void
AmarokToolBoxMenu::scrollDown()
{
    DEBUG_BLOCK
    if( !m_bottomMenu.empty() )
    {
        ToolBoxIcon *entryToRemove = m_currentMenu.first();
        m_currentMenu.removeFirst();
        int i = m_menuSize - 1;
        const int height = static_cast<int>( entryToRemove->boundingRect().height() ) + 9;
        m_topMenu.push( entryToRemove->text() );
        delete entryToRemove;

        foreach( ToolBoxIcon *entry, m_currentMenu )
        {
            Plasma::Animator::self()->moveItem( entry, Plasma::Animator::SlideInMovement,
                                            QPoint( 5, boundingRect().height() - height * i - 50 ) );
            i--;
        }

        ToolBoxIcon *entryToAdd = new ToolBoxIcon( this );
        const QString appletName = m_bottomMenu.pop();
        setupMenuEntry( entryToAdd, appletName );
        m_currentMenu << entryToAdd;
        entryToAdd->setPos( 5, boundingRect().height() - 50 );
        Plasma::Animator::self()->animateItem( entryToAdd, Plasma::Animator::AppearAnimation );

        if( m_bottomMenu.isEmpty() )
            Plasma::Animator::self()->animateItem( m_downArrow, Plasma::Animator::DisappearAnimation );

        if( m_topMenu.count() > 0 && !m_upArrow->isVisible() )
        {
            m_upArrow->resetTransform();
            m_upArrow->setPos( boundingRect().width()/2 - m_upArrow->size().width()/2,
                               boundingRect().height() - m_menuSize * height - 50 );
            m_upArrow->show();
        }
    }
}

void
AmarokToolBoxMenu::scrollUp()
{
    DEBUG_BLOCK
    if( !m_topMenu.empty() )
    {
        ToolBoxIcon *entryToRemove = m_currentMenu.last();
        m_currentMenu.removeLast();
        const int height = static_cast<int>( entryToRemove->boundingRect().height() ) + 9;
        m_bottomMenu.push( entryToRemove->text() );
        delete entryToRemove;

        int entries = m_currentMenu.count();
        for( int i = entries - 1; i >= 0; i-- )
        {
            ToolBoxIcon *entry = m_currentMenu[i];
            Plasma::Animator::self()->moveItem( entry, Plasma::Animator::SlideInMovement,
                                            QPoint( 5, boundingRect().height() - height * ( entries - i - 1 ) - 50 ) );
        }

        ToolBoxIcon *entryToAdd = new ToolBoxIcon( this );
        const QString appletName = m_topMenu.pop();
        setupMenuEntry( entryToAdd, appletName );
        m_currentMenu.prepend( entryToAdd );
        entryToAdd->setPos( 5, boundingRect().height() - height * ( m_menuSize - 1 ) - 50 );
        Plasma::Animator::self()->animateItem( entryToAdd, Plasma::Animator::AppearAnimation );

        if( m_topMenu.isEmpty() )
            Plasma::Animator::self()->animateItem( m_upArrow, Plasma::Animator::DisappearAnimation );

        if( m_bottomMenu.count() > 0 && !m_downArrow->isVisible() )
        {
            m_downArrow->resetTransform();
            m_downArrow->show();
        }
    }
}

void
AmarokToolBoxMenu::paint( QPainter *painter,  const QStyleOptionGraphicsItem *option, QWidget *widget )
{
    Q_UNUSED( painter )
    Q_UNUSED( option )
    Q_UNUSED( widget )
}

void
AmarokToolBoxMenu::hoverEnterEvent( QGraphicsSceneHoverEvent *event )
{
    if( m_timer->isActive() )
        m_timer->stop();
    QGraphicsItem::hoverEnterEvent( event );
}

void
AmarokToolBoxMenu::hoverLeaveEvent( QGraphicsSceneHoverEvent *event )
{
   m_timer->start( 2000 );
   QGraphicsItem::hoverLeaveEvent( event );
}

void
AmarokToolBoxMenu::wheelEvent( QGraphicsSceneWheelEvent *event )
{
    DEBUG_BLOCK

    if( event->orientation() == Qt::Horizontal || !showing() )
        return;
    if( m_pendingScrolls.size() > 0 )
    {
        if( m_pendingScrolls.last() == ScrollDown && event->delta() > 0 )
            m_pendingScrolls.clear();
        else if( m_pendingScrolls.last() == ScrollUp && event->delta() < 0 )
            m_pendingScrolls.clear();
    }

    if( event->delta() < 0 )
        m_pendingScrolls << ScrollDown;
    else
        m_pendingScrolls << ScrollUp;

    if( !m_scrollDelay->isActive() )
        m_scrollDelay->start( m_delay );
}

void
AmarokToolBoxMenu::delayedScroll()
{
    if( m_pendingScrolls.empty() )
        return;

    if( m_pendingScrolls.first() == ScrollDown )
        scrollDown();
    else
        scrollUp();

    m_pendingScrolls.removeFirst();

    m_scrollDelay->stop();
    if( !m_pendingScrolls.empty() )
        m_scrollDelay->start( qMax( 150, m_delay - m_pendingScrolls.size() * 20 ) );
}

void
AmarokToolBoxMenu::timeToHide()
{
    m_timer->stop();
    hide();
}

}

#include "ToolBoxMenu.moc"