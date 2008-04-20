/******************************************************************************
 * copyright: (c) 2007 Alexandre Pereira de Oliveira <aleprj@gmail.com>       *
 *            (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com> *
 *            (c) 2007 Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>         *
 *   This program is free software; you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License version 2           *
 *   as published by the Free Software Foundation.                            *
 ******************************************************************************/


#ifndef COLLECTIONTREEITEMMODELBASE_H
#define COLLECTIONTREEITEMMODELBASE_H

#include "amarok_export.h"

#include "meta/Meta.h"


#include <QAbstractItemModel>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QTimeLine>

class CollectionTreeItem;
class Collection;
class QueryMaker;
typedef QPair<Collection*, CollectionTreeItem* > CollectionRoot;

namespace CategoryId
{
    enum CatMenuId {
    None = 0,
    Album,
    Artist,
    Composer,
    Genre,
    Year
    };
}


/**
	@author Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>
*/
class AMAROK_EXPORT CollectionTreeItemModelBase : public QAbstractItemModel {
Q_OBJECT

    public:

        CollectionTreeItemModelBase( );
        virtual ~CollectionTreeItemModelBase();

        virtual Qt::ItemFlags flags(const QModelIndex &index) const;
        virtual QVariant headerData(int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole) const;
        virtual QModelIndex index(int row, int column,
                        const QModelIndex &parent = QModelIndex()) const;
        virtual QModelIndex parent(const QModelIndex &index) const;
        virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
        virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

        virtual QStringList mimeTypes() const;
        virtual QMimeData* mimeData( const QModelIndexList &indices ) const;

        virtual QPixmap iconForLevel( int level ) const;
        virtual void listForLevel( int level, QueryMaker *qm, CollectionTreeItem* parent ) const;


        virtual void setLevels( const QList<int> &levelType ) = 0;
        virtual QList<int> levels() const { return m_levelType; }

        virtual void addFilters( QueryMaker *qm ) const;

        void setCurrentFilter( const QString &filter );

    signals:
        void expandIndex( const QModelIndex &index );

    public slots:

        virtual void queryDone();
        virtual void newResultReady( const QString &collectionId, Meta::DataList data );

        void slotFilter();

        void slotCollapsed( const QModelIndex &index );

    protected:

        virtual void populateChildren(const Meta::DataList &dataList, CollectionTreeItem *parent) const;
        virtual void ensureChildrenLoaded( CollectionTreeItem *item ) const = 0;
        virtual void updateHeaderText();
        virtual QString nameForLevel( int level ) const;

        virtual void filterChildren() = 0;

        void handleCompilations( CollectionTreeItem *parent ) const;

        QString m_headerText;
        CollectionTreeItem *m_rootItem;
        QList<int> m_levelType;

        struct Private;
        Private * const d;

        QTimeLine *m_timeLine;
        int m_animFrame;
        QPixmap m_loading1, m_loading2, m_currentAnimPixmap;    //icons for loading animation

        QString m_currentFilter;
        QSet<Meta::DataPtr> m_expandedItems;
        QSet<Collection*> m_expandedCollections;
    protected slots:

        void loadingAnimationTick();
        void update();


};

struct CollectionTreeItemModelBase::Private
{
    QMap<QString, CollectionRoot > m_collections;  //I'll concide this one... :-)
    QMap<QueryMaker* , CollectionTreeItem* > m_childQueries;
    QMap<QueryMaker* , CollectionTreeItem* > m_compilationQueries;
};

#endif