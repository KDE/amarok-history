/***************************************************************************
                         directorylist.h
                            -------------------
   begin                : Tue Feb 4 2003
   copyright            : (C) 2003 Scott Wheeler <wheeler@kde.org>
                        : (C) 2004 Max Howell <max.howell@methylblue.com>
                        : (C) 2004 Mark Kretschmann <markey@web.de>
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef AMAROK_DIRECTORYLIST_H
#define AMAROK_DIRECTORYLIST_H

#include <qcheckbox.h>  //inlined functions
#include <qlistview.h>  //baseclass
#include <qvbox.h>      //baseclass

#include <kdirlister.h> //stack allocated
#include <kurl.h>       //stack allocated


namespace Collection { class Item; }


class CollectionSetup : public QVBox
{
    friend class Collection::Item;

public:
    static CollectionSetup* instance() { return s_instance; }

    CollectionSetup( QWidget* );
    void writeConfig();

    QStringList dirs() const { return m_dirs; }
    bool recursive() const { return m_recursive->isChecked(); }
    bool monitor() const { return m_monitor->isChecked(); }
    bool importPlaylists() const { return m_playlists->isChecked(); }

private:
    static CollectionSetup* s_instance;

    QListView *m_view;
    QStringList m_dirs;
    QCheckBox *m_recursive;
    QCheckBox *m_monitor;
    QCheckBox *m_playlists;
};


namespace Collection { //just to keep it out of the global namespace

class Item : public QObject, public QCheckListItem
{
Q_OBJECT
public:
    Item( QListView *parent );
    Item( QListViewItem *parent, const KURL &url );

    QCheckListItem *parent() const { return (QCheckListItem*)QListViewItem::parent(); }
    bool isDisabled() const { return CollectionSetup::instance()->recursive() && parent() && parent()->isOn(); }
    QString fullPath() const;

    void setOpen( bool b ); // reimpl.
    void stateChange( bool ); // reimpl.
    void activate(); // reimpl.
    void paintCell( QPainter * p, const QColorGroup & cg, int column, int width, int align ); // reimpl.

public slots:
    void newItems( const KFileItemList& );
    void completed() { if( childCount() == 0 ) { setExpandable( false ); repaint(); } }

private:
    KDirLister m_lister;
    KURL       m_url;
    bool       m_listed;
};
}

#endif
