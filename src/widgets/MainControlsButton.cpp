/***************************************************************************
 *   Copyright (c) 2008  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>    *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/
 
#include "MainControlsButton.h"

#include "Debug.h"
#include "SvgHandler.h"

#include <QStyleOptionGraphicsItem>

MainControlsButton::MainControlsButton( QGraphicsItem * parent )
    : QGraphicsItem( parent )
    , m_action( 0 )
    , m_mouseOver( false )
    , m_mouseDown( false )
{
    setAcceptHoverEvents ( true );
}


MainControlsButton::~MainControlsButton()
{
}

void MainControlsButton::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * /*widget*/)
{
    DEBUG_BLOCK

    if ( m_mouseOver && !m_mouseDown ) {
        painter->drawPixmap( -2, -2, The::svgHandler()->renderSvg( m_prefix, option->rect.width() + 4, option->rect.height() + 4, m_prefix ) );
    } else {
    
        painter->drawPixmap( 0, 0, The::svgHandler()->renderSvg( m_prefix, option->rect.width(), option->rect.height(), m_prefix ) );

    }

}

void MainControlsButton::setSvgPrefix(const QString & prefix)
{
    m_prefix = prefix;
}

QRectF MainControlsButton::boundingRect() const
{
    return QRectF( 0.0, 0.0, 54.0, 54.0 );
}

void MainControlsButton::setAction(QAction * action)
{
    m_action = action;
}

void MainControlsButton::mousePressEvent(QGraphicsSceneMouseEvent * /*event*/)
{
    m_mouseDown = true;
    update();
}

void MainControlsButton::hoverEnterEvent(QGraphicsSceneHoverEvent * event)
{
    DEBUG_BLOCK
    m_mouseOver = true;
    update();
}

void MainControlsButton::hoverLeaveEvent(QGraphicsSceneHoverEvent * event)
{
    DEBUG_BLOCK
    m_mouseOver = false;
    m_mouseDown = false;
    update();
}


void MainControlsButton::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
    m_mouseDown = false;
    update();
    if ( m_action != 0 )
        m_action->trigger();
}

QPainterPath MainControlsButton::shape() const
{
    QPainterPath path;
    path.addEllipse(boundingRect());
    return path;
}

