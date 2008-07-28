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
 
#include "UserPlaylistTreeView.h"

#include "playlist/PlaylistModel.h"
#include "context/ContextView.h"
#include "context/popupdropper/PopupDropperAction.h"
#include "context/popupdropper/PopupDropperItem.h"
#include "context/popupdropper/PopupDropper.h"
#include "PopupDropperFactory.h"
#include "SqlPlaylist.h"
#include "SqlPlaylistGroup.h"
#include "SvgHandler.h"
#include "StatusBar.h"
#include "UserPlaylistModel.h"

#include <KAction>
#include <KMenu>

#include <QMouseEvent>
#include <QModelIndex>
#include <QPoint>

#include <typeinfo>

PlaylistBrowserNS::UserPlaylistTreeView::UserPlaylistTreeView( QWidget *parent )
    : QTreeView( parent )
    , m_pd( 0 )
    , m_appendAction( 0 )
    , m_loadAction( 0 )
    , m_deleteAction( 0 )
    , m_renameAction( 0 )
    , m_addGroupAction( 0 )
{
}


PlaylistBrowserNS::UserPlaylistTreeView::~UserPlaylistTreeView()
{
}

void PlaylistBrowserNS::UserPlaylistTreeView::mousePressEvent( QMouseEvent * event )
{
    if( event->button() == Qt::LeftButton )
        m_dragStartPosition = event->pos();

    QTreeView::mousePressEvent( event );
}

void PlaylistBrowserNS::UserPlaylistTreeView::mouseReleaseEvent( QMouseEvent * event )
{
    Q_UNUSED( event )

    if( m_pd )
    {
        connect( m_pd, SIGNAL( fadeHideFinished() ), m_pd, SLOT( deleteLater() ) );
        m_pd->hide();
    }
    m_pd = 0;
}

void PlaylistBrowserNS::UserPlaylistTreeView::mouseDoubleClickEvent( QMouseEvent * event )
{
    QModelIndex index = indexAt( event->pos() );

    if( index.isValid() && index.internalPointer()  /*&& index.parent().isValid()*/ )
    {
        SqlPlaylistViewItem *item = static_cast<SqlPlaylistViewItem*>( index.internalPointer() );

        if ( typeid( * item ) == typeid( Meta::SqlPlaylist ) ) {
            Meta::SqlPlaylist * playlist = static_cast< Meta::SqlPlaylist* >( item );
            The::playlistModel()->insertOptioned( playlist->tracks(), Playlist::Append );
        }

    }
}

void PlaylistBrowserNS::UserPlaylistTreeView::startDrag( Qt::DropActions supportedActions )
{
    DEBUG_BLOCK

    //Waah? when a parent item is dragged, startDrag is called a bunch of times
    static bool ongoingDrags = false;
    if( ongoingDrags )
        return;
    ongoingDrags = true;

    if( !m_pd )
        m_pd = The::popupDropperFactory()->createPopupDropper( Context::ContextView::self() );

    if( m_pd && m_pd->isHidden() )
    {

        QModelIndexList indices = selectedIndexes();

        QList<PopupDropperAction*> actions = createCommonActions( indices );

        foreach( PopupDropperAction * action, actions ) {
            m_pd->addItem( The::popupDropperFactory()->createItem( action ), false );
        }

        m_pd->show();
    }

    QTreeView::startDrag( supportedActions );
    debug() << "After the drag!";

    if( m_pd )
    {
        debug() << "clearing PUD";
        connect( m_pd, SIGNAL( fadeHideFinished() ), m_pd, SLOT( clear() ) );
        m_pd->hide();
    }
    ongoingDrags = false;
}


QList< PopupDropperAction * > PlaylistBrowserNS::UserPlaylistTreeView::createCommonActions( QModelIndexList indices )
{

    QList< PopupDropperAction * > actions;
    
    if ( m_appendAction == 0 ) {
        m_appendAction = new PopupDropperAction( The::svgHandler()->getRenderer( "amarok/images/pud_items.svg" ), "append", KIcon( "media-track-add-amarok" ), i18n( "&Append to Playlist" ), this );
        connect( m_appendAction, SIGNAL( triggered() ), this, SLOT( slotAppend() ) );
    }
    
    if ( m_loadAction == 0 ) {
        m_loadAction = new PopupDropperAction( The::svgHandler()->getRenderer( "amarok/images/pud_items.svg" ), "load", KIcon( "folder-open" ), i18nc( "Replace the currently loaded tracks with these", "&Load" ), this );
        connect( m_loadAction, SIGNAL( triggered() ), this, SLOT( slotLoad() ) );
    }


    if ( m_deleteAction == 0 ) {
        m_deleteAction = new PopupDropperAction( The::svgHandler()->getRenderer( "amarok/images/pud_items.svg" ), "delete", KIcon( "media-track-remove-amarok" ), i18n( "&Delete" ), this );
        connect( m_deleteAction, SIGNAL( triggered() ), this, SLOT( slotDelete() ) );
    }

    if ( m_renameAction == 0 ) {
        m_renameAction =  new PopupDropperAction( The::svgHandler()->getRenderer( "amarok/images/pud_items.svg" ), "edit", KIcon( "media-track-edit-amarok" ), i18n( "&Rename" ), this );
        connect( m_renameAction, SIGNAL( triggered() ), this, SLOT( slotRename() ) );
    }
    
    if ( indices.count() > 0 ) {
        actions << m_appendAction;
        actions << m_loadAction;
        //menu.addSeparator();
    }

    if ( indices.count() == 1 )
        actions << m_renameAction;

    if ( indices.count() > 0 ) {
        actions << m_deleteAction;
    }


    return actions;
}

void PlaylistBrowserNS::UserPlaylistTreeView::slotLoad() {
    DEBUG_BLOCK
    foreach( SqlPlaylistViewItemPtr item, m_currentItems )
        if ( typeid( * item ) == typeid( Meta::SqlPlaylist ) ) {
        Meta::SqlPlaylistPtr playlist = Meta::SqlPlaylistPtr::staticCast( item );
        //debug() << "playlist name: " << playlist->name();
        //The::playlistModel()->insertOptioned( Meta::PlaylistPtr( playlist ), Playlist::Append );
        The::playlistModel()->insertOptioned( playlist->tracks(), Playlist::Replace );
        }
}

void PlaylistBrowserNS::UserPlaylistTreeView::slotAppend()
{
    DEBUG_BLOCK
    foreach( SqlPlaylistViewItemPtr item, m_currentItems )
        if ( typeid( * item ) == typeid( Meta::SqlPlaylist ) ) {
        Meta::SqlPlaylistPtr playlist = Meta::SqlPlaylistPtr::staticCast( item );
        //debug() << "playlist name: " << playlist->name();
        //The::playlistModel()->insertOptioned( Meta::PlaylistPtr( playlist ), Playlist::Append );
        The::playlistModel()->insertOptioned( playlist->tracks(), Playlist::Append );
        }
}


void PlaylistBrowserNS::UserPlaylistTreeView::slotDelete()
{
    DEBUG_BLOCK
    foreach( SqlPlaylistViewItemPtr item, m_currentItems )
    {
        debug() << "deleting " << item->name();
        item->removeFromDb();
        item->parent()->deleteChild( item );
    }
    PlaylistBrowserNS::UserModel::instance()->reloadFromDb();
}

void PlaylistBrowserNS::UserPlaylistTreeView::slotRename()
{
    DEBUG_BLOCK
    edit( selectionModel()->selectedIndexes().first() );
}

void PlaylistBrowserNS::UserPlaylistTreeView::contextMenuEvent( QContextMenuEvent * event )
{

    DEBUG_BLOCK

    QModelIndexList indices = selectionModel()->selectedIndexes();

    KMenu menu;

    QList<PopupDropperAction *> actions = createCommonActions( indices );

    foreach( PopupDropperAction * action, actions ) {
        menu.addAction( action );
    }

    if ( indices.count() == 0 ) {
        menu.addAction( m_addGroupAction );
    }

    m_currentItems.clear();
    foreach( const QModelIndex &index, indices )
    {
        if( index.isValid() && index.internalPointer() )
            m_currentItems.insert( PlaylistBrowserNS::UserModel::instance()->data( index, 0xf00d ).value<SqlPlaylistViewItemPtr>() );
    }


    KAction* result = dynamic_cast< KAction* > ( menu.exec( mapToGlobal( event->pos() ) ) );
    if ( result == 0 ) return;
    else if( result == m_addGroupAction )
    {
        PlaylistBrowserNS::UserModel::instance()->createNewGroup();
    }
}

void PlaylistBrowserNS::UserPlaylistTreeView::setNewGroupAction( KAction * action )
{
    m_addGroupAction = action;
}

#include "UserPlaylistTreeView.moc"