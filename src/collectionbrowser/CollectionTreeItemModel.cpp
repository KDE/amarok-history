/******************************************************************************
 * copyright: (c) 2007 Alexandre Pereira de Oliveira <aleprj@gmail.com>       *
 *            (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com> *
 *            (c) 2007 Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>         *
 *   This program is free software; you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License version 2           *
 *   as published by the Free Software Foundation.                            *
 ******************************************************************************/

#define DEBUG_PREFIX "CollectionTreeItemModel"

#include "CollectionTreeItemModel.h"

#include "CollectionTreeItem.h"
//#include "collection/sqlregistry.h"
#include "debug.h"
#include "amarok.h"
#include "Collection.h"
#include "CollectionManager.h"
#include "CoverFetcher.h"
#include "QueryMaker.h"

#include <KLocale>
#include <KIcon>
#include <KIconLoader>
#include <QMimeData>
#include <QPixmap>
#include <QTimer>



CollectionTreeItemModel::CollectionTreeItemModel( const QList<int> &levelType )
    :CollectionTreeItemModelBase()

{
    CollectionManager* collMgr = CollectionManager::instance();
    connect( collMgr, SIGNAL( collectionAdded( Collection* ) ), this, SLOT( collectionAdded( Collection* ) ), Qt::QueuedConnection );
    connect( collMgr, SIGNAL( collectionRemoved( QString ) ), this, SLOT( collectionRemoved( QString ) ) );
    setLevels( levelType );
    debug() << "Collection root has " << m_rootItem->childCount() << " childrens";
}


void
CollectionTreeItemModel::setLevels( const QList<int> &levelType ) {

    m_levelType = levelType;
    update();
    if ( d->m_collections.count() == 1 )
        QTimer::singleShot( 0, this, SLOT( requestCollectionsExpansion() ) );
}

QVariant
CollectionTreeItemModel::data(const QModelIndex &index, int role) const
{
     if (!index.isValid())
         return QVariant();

    CollectionTreeItem *item = static_cast<CollectionTreeItem*>(index.internalPointer());

    if ( item->isDataItem() )
    {
        if ( role == Qt::DecorationRole ) {
            int level = item->level() -1;

            if ( d->m_childQueries.values().contains( item ) ) {
                if ( level < m_levelType.count() )
                    return m_currentAnimPixmap;
            }

            if ( level < m_levelType.count() ) {
                if (  m_levelType[level] == CategoryId::Album ) {
//                     const Meta::Album* album = static_cast< const Meta::Album*> ( item->data().data() );
                    Meta::AlbumPtr album = Meta::AlbumPtr::dynamicCast( item->data() );
                    if( album )
                    {
                        return album->image( 32, false );
                    }
                    else
                        return iconForLevel( level  );
                } else {
                    return iconForLevel( level );
                }
            }
        }
    }

    return item->data( role );
}


bool
CollectionTreeItemModel::hasChildren ( const QModelIndex & parent ) const {
     if (!parent.isValid())
         return true; // must be root item!

    CollectionTreeItem *item = static_cast<CollectionTreeItem*>(parent.internalPointer());
    //we added the collection level so we have to be careful with the item level
    return !item->isDataItem() || item->level() <= m_levelType.count(); 

}

void
CollectionTreeItemModel::ensureChildrenLoaded( CollectionTreeItem *item ) const {
    if ( !item->childrenLoaded() ) {
        listForLevel( item->level() /* +1 -1 */, item->queryMaker(), item );
        item->setChildrenLoaded( true );
    }
}

bool
CollectionTreeItemModel::canFetchMore( const QModelIndex &parent ) const {
    if ( !parent.isValid() )
        return false;       //children of the root item are the collections, and they are alwas known
    CollectionTreeItem *item = static_cast<CollectionTreeItem*>( parent.internalPointer() );
    return item->level() <= m_levelType.count() && !item->childrenLoaded();
}

void
CollectionTreeItemModel::fetchMore( const QModelIndex &parent ) {
    if ( !parent.isValid() )
        return;

    CollectionTreeItem *item = static_cast<CollectionTreeItem*>( parent.internalPointer() );
    ensureChildrenLoaded( item );
}

void
CollectionTreeItemModel::collectionAdded( Collection *newCollection ) {
    DEBUG_BLOCK
    if ( !newCollection )
        return;

     connect( newCollection, SIGNAL( updated() ), this, SLOT( update() ) ) ;

    QString collectionId = newCollection->collectionId();
    if ( d->m_collections.contains( collectionId ) )
        return;
    //inserts new collection at the end. sort collection alphabetically?
    beginInsertRows( QModelIndex(), m_rootItem->childCount(), m_rootItem->childCount() );
    d->m_collections.insert( collectionId, CollectionRoot( newCollection, new CollectionTreeItem( newCollection, m_rootItem ) ) );
    endInsertRows();
    if ( d->m_collections.count() == 1 )
        QTimer::singleShot( 0, this, SLOT( requestCollectionsExpansion() ) );
}

void
CollectionTreeItemModel::collectionRemoved( const QString &collectionId ) {
    int count = m_rootItem->childCount();
    for ( int i = 0; i < count; i++ ) {
        CollectionTreeItem *item = m_rootItem->child( i );
        if ( item && !item->isDataItem() && item->parentCollection()->collectionId() == collectionId ) {
            beginRemoveRows( QModelIndex(), i, i );
            m_rootItem->removeChild( i );
            d->m_collections.remove( collectionId );
            m_expandedCollections.remove( item->parentCollection() );
            endRemoveRows();
        }
    }
}

void
CollectionTreeItemModel::filterChildren()
{
    int count = m_rootItem->childCount();
    for ( int i = 0; i < count; i++ )
    {
        CollectionTreeItem *item = m_rootItem->child( i );
        item->setChildrenLoaded( false );
    }
}

void
CollectionTreeItemModel::requestCollectionsExpansion() {
    DEBUG_BLOCK
    for( int i = 0, count = m_rootItem->childCount(); i < count; i++ )
    {
        emit expandIndex( createIndex( i, 0, m_rootItem->child( i ) ) );
    }
}


namespace Amarok {
    // TODO Internationlise
    void manipulateThe( QString &str, bool reverse )
    {
        if( reverse )
        {
            QString begin = str.left( 3 );
            str = str.append( ", %1" ).arg( begin );
            str = str.mid( 4 );
            return;
        }

        if( !str.endsWith( ", the", Qt::CaseInsensitive ) )
            return;

        QString end = str.right( 3 );
        str = str.prepend( "%1 " ).arg( end );

        uint newLen = str.length() - end.length() - 2;

        str.truncate( newLen );
    }
}

void CollectionTreeItemModel::update()
{
    DEBUG_BLOCK
    delete m_rootItem; //clears the whole tree!
    m_rootItem = new CollectionTreeItem( Meta::DataPtr(0), 0 );
    d->m_collections.clear();
    QList<Collection*> collections = CollectionManager::instance()->collections();
    foreach( Collection *coll, collections )
    {
        connect( coll, SIGNAL( updated() ), this, SLOT( update() ) ) ;
        d->m_collections.insert( coll->collectionId(), CollectionRoot( coll, new CollectionTreeItem( coll, m_rootItem ) ) );
    }
    m_rootItem->setChildrenLoaded( true ); //children of the root item are the collection items
    updateHeaderText();
    m_expandedItems.clear();
    reset();
}
#include "CollectionTreeItemModel.moc"