// (c) 2004 Mark Kretschmann <markey@web.de>
// (c) 2004 Christian Muehlhaeuser <chris@chris.de>
// See COPYING file for licensing information.

#ifndef AMAROK_COLLECTIONBROWSER_H
#define AMAROK_COLLECTIONBROWSER_H

#include "qvbox.h"           //baseclass

#include <klistview.h>       //baseclass
#include <klineedit.h>       //baseclass
#include <qstringlist.h>     //stack allocated
#include <kurl.h>            //stack allocated

class CollectionDB;
class sqlite;

class QCString;
class QCustomEvent;
class QDragObject;
class QPixmap;
class QPoint;
class QStringList;

class KAction;
class KPopupMenu;
class KProgress;

class CollectionBrowser: public QVBox
{
    Q_OBJECT
    friend class CollectionView;

    public:
        CollectionBrowser( const char* name );

    public slots:
        void scan();
        void setupDirs();

    private slots:
        void slotSetFilterTimeout();
        void slotSetFilter();
        void clearFilter();

    private:
        bool eventFilter( QObject*, QEvent* );

    //attributes:
        enum CatMenuId { IdScan, IdAlbum, IdArtist, IdGenre, IdYear, IdNone };

        KAction* m_configureAction;
        KAction* m_scanAction;

        KPopupMenu* m_categoryMenu;
        KPopupMenu* m_cat1Menu;
        KPopupMenu* m_cat2Menu;
        KPopupMenu* m_cat3Menu;
        KLineEdit* m_searchEdit;
        CollectionView* m_view;
        QTimer* m_timer;
};


class CollectionView : public KListView
{
    Q_OBJECT
    friend class CollectionBrowser;

    signals:
        void sigScanStarted();
        void sigScanDone();

    public:
        class Item : public KListViewItem {
            public:
                Item( QListView* parent )
                    : KListViewItem( parent ) {};
                Item( QListViewItem* parent )
                    : KListViewItem( parent ) {};
                void setUrl( const QString& url ) { m_url.setPath( url ); }
                const KURL& url() const { return m_url; }

            //attributes:
                KURL m_url;
        };
        friend class Item; // for access to m_category2

        CollectionView( CollectionBrowser* parent );
        ~CollectionView();

        static CollectionView* instance() { return m_instance; }
        /** Rebuilds and displays the treeview by querying the database. */
        void renderView();
        void setFilter( QString filter ) { m_filter = filter; }
        QString filter() { return m_filter; }
        Item* currentItem() { return static_cast<Item*>( KListView::currentItem() ); }

    private slots:
        void setupDirs();
        void scan();
        void scanMonitor();
        void scanDone( bool changed = true );

        void cacheItem( QListViewItem* item );
        void slotExpand( QListViewItem* );
        void slotCollapse( QListViewItem* );
        void cat1Menu( int id, bool rerender = true );
        void cat2Menu( int id, bool rerender = true );
        void cat3Menu( int id, bool rerender = true );
        void enableCat3Menu( bool );
        void doubleClicked( QListViewItem*, const QPoint&, int );
        void rmbPressed( QListViewItem*, const QPoint&, int );

        /** Tries to download the cover image from Amazon.com */
        void fetchCover();
        /** Shows dialog with information on selected track */
        void showTrackInfo();

    protected:
        /** Manages regular folder monitoring scan */
        void timerEvent( QTimerEvent* e );
        /** Processes progress events from CollectionReader */
        void customEvent( QCustomEvent* e );

    private:
        void startDrag();
        KURL::List listSelected();

        QString catForId( int id ) const;
        int idForCat( const QString& cat ) const;
        QPixmap iconForCat( const QString& cat ) const;
        QString tableForCat( const QString& cat ) const;

    //attributes:
        //bump DATABASE_VERSION whenever changes to the table structure are made. will remove old db file.
        static const int DATABASE_VERSION = 12;
        static const int DATABASE_STATS_VERSION = 2;
        static CollectionDB* m_db;
        static CollectionDB* m_insertdb;
        static CollectionView* m_instance;

        CollectionBrowser* m_parent;
        QString m_filter;
        QString m_category1;
        QString m_category2;
        QString m_category3;

        bool m_isScanning;
        QHBox* m_progressBox;
        KProgress* m_progress;

        QStringList m_cacheItem;
};


#endif /* AMAROK_COLLECTIONBROWSER_H */
