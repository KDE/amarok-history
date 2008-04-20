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

#include "VerticalLayout.h"

#include "debug.h"

#include "plasma/layouts/layoutitem.h"
#include "plasma/layouts/layoutanimator.h"

namespace Context
{


class VerticalLayout::Private
{
public:
    QList<LayoutItem*> children;
    QRectF geometry;
};

VerticalLayout::VerticalLayout(LayoutItem *parent)
    : Layout(parent),
      d(new Private)
{
}

VerticalLayout::~VerticalLayout()
{
    kDebug() << "help help, I'm being repressed: " << this;
    delete d;
}

Qt::Orientations VerticalLayout::expandingDirections() const
{
    return Qt::Vertical;
}

void VerticalLayout::addItem(LayoutItem *item)
{
    if (d->children.contains(item)) {
        return;
    }

    d->children << item;
    item->setManagingLayout(this);
    relayout();
}

void VerticalLayout::removeItem(LayoutItem *item)
{
    if (!item) {
        return;
    }

    d->children.removeAll(item);
    item->unsetManagingLayout(this);
    relayout();
}

int VerticalLayout::indexOf(LayoutItem *item) const
{
    return d->children.indexOf(item);
}

Plasma::LayoutItem* VerticalLayout::itemAt(int i) const
{
    return d->children[i];
}

int VerticalLayout::count() const
{
    return d->children.count();
}

Plasma::LayoutItem* VerticalLayout::takeAt(int i)
{
    Plasma::LayoutItem* item = d->children.takeAt(i);
    relayout();
    return item;
}

void VerticalLayout::relayout()
{

    QRectF rect = geometry().adjusted(0, 0, 0, 0);

    qreal top = 10.0;
    qreal left = 10.0; //Plasma::Layout::margin( Plasma::LeftMargin );

    foreach (LayoutItem *child , d->children) {
        qreal height = 0.0;

        if( child->hasHeightForWidth() )
            height = child->heightForWidth( rect.width() );
        else
            height = sizeHint().height();

        const QRectF newgeom( rect.topLeft().x() + left,
                              rect.topLeft().y() + top,
                                           rect.width() - 6,
                                            height );

        top += height /*+ spacing()*/;

        if ( animator() )
            animator()->setGeometry( child , newgeom );
        else
            child->setGeometry( newgeom );

    }

}

void VerticalLayout::setGeometry(const QRectF &geometry)
{
    d->geometry = geometry;
    relayout();
}

QRectF VerticalLayout::geometry() const
{
    return d->geometry;
}

QSizeF VerticalLayout::sizeHint() const
{
    qreal height = 0.0;
    foreach( LayoutItem* child, d->children )
        height += child->sizeHint().height();
    return QSizeF( geometry().width(), height );
}

}

void Context::VerticalLayout::releaseManagedItems()
{
    //FIXME!!
}