// (c) Pierpaolo Di Panfilo 2004
// (c) 2005 Isaiah Damron <xepo@trifault.net>
// (c) 2007 Dan Meltzer <hydrogen@notyetimplemented.com>
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include "CoverManager.h"

#include "amarok.h"
#include "amarokconfig.h"
#include "collection/BlockingQuery.h"
#include "collection/Collection.h"
#include "CollectionManager.h"
#include "browserToolBar.h"
#include "debug.h"
#include "meta/Meta.h"
#include "QueryMaker.h"
#include "config-amarok.h"
#include "PixmapViewer.h"
#include "playlist/PlaylistModel.h"

#include "TheInstances.h"

#include <k3multipledrag.h>
#include <k3urldrag.h>
#include <KApplication>
#include <KConfig>
#include <KCursor>
#include <KFileDialog>
#include <KIconLoader>
#include <KIO/NetAccess>
#include <KLineEdit>
#include <KLocale>
#include <KMenu>    //showCoverMenu()
#include <KMessageBox>    //showCoverMenu()
#include <KPushButton>
#include <KSqueezedTextLabel> //status label
#include <KStandardDirs>   //KGlobal::dirs()
#include <KStatusBar>
#include <KToolBar>
#include <KUrl>
#include <KVBox>
#include <KWindowSystem>

#include <QDesktopWidget>  //ctor: desktop size
#include <QFile>
#include <QFontMetrics>    //paintItem()
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QObject>    //used to delete all cover fetchers
#include <QPainter>    //paintItem()
#include <QPalette>    //paintItem()
#include <QPixmap>
#include <QPoint>
#include <QProgressBar>
#include <QProgressDialog>
#include <QRect>
#include <QStringList>
#include <QToolTip>
#include <QTimer>    //search filter timer
//Added by qt3to4:
#include <QDropEvent>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

static QString artistToSelectInInitFunction;
CoverManager *CoverManager::s_instance = 0;

class ArtistItem : public QTreeWidgetItem
{
    public:
    ArtistItem(QTreeWidget *parent, Meta::ArtistPtr artist )
        : QTreeWidgetItem( parent )
        , m_artist( artist )
        { setText( 0, artist->prettyName() ); }
    ArtistItem(const QString &text, QTreeWidget *parent = 0 )
        : QTreeWidgetItem( parent )
        , m_artist( 0 ) { setText( 0, text ); }

        Meta::ArtistPtr artist() const { return m_artist; }

    private:
        Meta::ArtistPtr m_artist;
};

CoverManager::CoverManager()
        : QSplitter( 0 )
        , m_timer( new QTimer( this ) )    //search filter timer
        , m_fetchingCovers( 0 )
        , m_coversFetched( 0 )
        , m_coverErrors( 0 )
{
    DEBUG_BLOCK

    setObjectName( "TheCoverManager" );

    s_instance = this;

    // Sets caption and icon correctly (needed e.g. for GNOME)
    kapp->setTopWidget( this );
    setWindowTitle( KDialog::makeStandardCaption( i18n("Cover Manager") ) );
    setAttribute( Qt::WA_DeleteOnClose );
    setContentsMargins( 4, 4, 4, 4 );

    //artist listview
    m_artistView = new QTreeWidget( this );
    m_artistView->setHeaderLabel( i18n( "Albums By" ) );
    m_artistView->setSortingEnabled( false );
    m_artistView->setTextElideMode( Qt::ElideRight );
    m_artistView->setMinimumWidth( 140 );
    m_artistView->setColumnCount( 1 );

    setSizes( QList<int>() << 120 << width() - 120 );

    ArtistItem *item = 0;
    QList<QTreeWidgetItem*> items;
    item = new ArtistItem( i18n( "All Artists" ) );
    item->setIcon(0, SmallIcon( "media-optical-audio-amarok" ) );
    items.append( item );

    Collection *coll;
    foreach( coll, CollectionManager::instance()->collections() )
        if( coll->collectionId() == "localCollection" )
            break;
    QueryMaker *qm = coll->queryMaker();
    qm->startArtistQuery();
    BlockingQuery bq( qm );
    bq.startQuery();
    Meta::ArtistList artists = bq.artists( coll->collectionId() );
    foreach( Meta::ArtistPtr artist, artists )
    {
        item = new ArtistItem( m_artistView, artist );
        item->setIcon( 0, SmallIcon( "view-media-artist-amarok" ) );
        items.append( item );
    }
    m_artistView->insertTopLevelItems( 0, items );



    //TODO: Port
//     ArtistItem *last = static_cast<ArtistItem *>(m_artistView->item( m_artistView->count() - 1));
//     QueryBuilder qb;
//     qb.addReturnValue( QueryBuilder::tabAlbum, QueryBuilder::valName );
//     qb.groupBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
//     qb.setOptions( QueryBuilder::optOnlyCompilations );
//     qb.setLimit( 0, 1 );
//     if ( qb.run().count() ) {
//         item = new ArtistItem( m_artistView, last, i18n( "Various Artists" ) );
//         item->setPixmap( 0, SmallIcon("personal") );
//     }

    KVBox *vbox = new KVBox( this );
    KHBox *hbox = new KHBox( vbox );

    vbox->setSpacing( 4 );
    hbox->setSpacing( 4 );

    { //<Search LineEdit>
//         KHBox *searchBox = new KHBox( hbox );
        m_searchEdit = new KLineEdit( hbox );
        m_searchEdit->setClickMessage( i18n( "Enter search terms here" ) );
        m_searchEdit->setFrame( QFrame::Sunken );

        m_searchEdit->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
        m_searchEdit->setClearButtonShown( true );

        m_searchEdit->setToolTip( i18n( "Enter space-separated terms to search in the albums" ) );

        hbox->setStretchFactor( m_searchEdit, 1 );
    } //</Search LineEdit>

    // view menu
    m_viewMenu = new KMenu( this );
    m_selectAllAlbums          = m_viewMenu->addAction( i18n("All Albums"),           this, SLOT( slotShowAllAlbums() ) );
    m_selectAlbumsWithCover    = m_viewMenu->addAction( i18n("Albums With Cover"),    this, SLOT( slotShowAlbumsWithCover() ) );
    m_selectAlbumsWithoutCover = m_viewMenu->addAction( i18n("Albums Without Cover"), this, SLOT( slotShowAlbumsWithoutCover() ) );

    QActionGroup *viewGroup = new QActionGroup( this );
    viewGroup->setExclusive( true );
    viewGroup->addAction( m_selectAllAlbums );
    viewGroup->addAction( m_selectAlbumsWithCover );
    viewGroup->addAction( m_selectAlbumsWithoutCover );
    m_selectAllAlbums->setChecked( true );

    // amazon locale menu
    QString locale = AmarokConfig::amazonLocale();
    m_currentLocale = CoverFetcher::localeStringToID( locale );

    QAction *a;
    QActionGroup *localeGroup = new QActionGroup( this );
    localeGroup->setExclusive( true );

    m_amazonLocaleMenu = new KMenu( this );

    a = m_amazonLocaleMenu->addAction( i18nc( "The locale to use when fetching covers from amazon.com", "International"),  this, SLOT( slotSetLocaleIntl() ) );
    if( m_currentLocale == CoverFetcher::International ) a->setChecked( true );
    localeGroup->addAction( a );

    a = m_amazonLocaleMenu->addAction( i18n("Canada"),         this, SLOT( slotSetLocaleCa() )   );
    if( m_currentLocale == CoverFetcher::Canada ) a->setChecked( true );
    localeGroup->addAction( a );

    a = m_amazonLocaleMenu->addAction( i18n("France"),         this, SLOT( slotSetLocaleFr() )   );
    if( m_currentLocale == CoverFetcher::France ) a->setChecked( true );
    localeGroup->addAction( a );

    a = m_amazonLocaleMenu->addAction( i18n("Germany"),        this, SLOT( slotSetLocaleDe() )   );
    if( m_currentLocale == CoverFetcher::Germany ) a->setChecked( true );
    localeGroup->addAction( a );

    a = m_amazonLocaleMenu->addAction( i18n("Japan"),          this, SLOT( slotSetLocaleJp() )   );
    if( m_currentLocale == CoverFetcher::Japan ) a->setChecked( true );
    localeGroup->addAction( a );

    a = m_amazonLocaleMenu->addAction( i18n("United Kingdom"), this, SLOT( slotSetLocaleUk() )   );
    if( m_currentLocale == CoverFetcher::UK ) a->setChecked( true );
    localeGroup->addAction( a );

    KToolBar* toolBar = new KToolBar( hbox );
    toolBar->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    {
        QAction* viewMenuAction = new QAction( KIcon( "view-list-icons" ), i18nc( "@title buttontext for popup-menu", "View" ), this );
        viewMenuAction->setMenu( m_viewMenu );
        toolBar->addAction( viewMenuAction );
    }
    {
        QAction* localeMenuAction = new QAction( KIcon( "preferences-desktop-locale-amarok" ),  i18n( "Amazon Locale" ), this );
        localeMenuAction->setMenu( m_amazonLocaleMenu );
        toolBar->addAction( localeMenuAction );
    }

    //fetch missing covers button
    m_fetchButton = new KPushButton( KGuiItem( i18n("Fetch Missing Covers"), "get-hot-new-stuff-amarok" ), hbox );
    connect( m_fetchButton, SIGNAL(clicked()), SLOT(fetchMissingCovers()) );


    //cover view
    m_coverView = new CoverView( vbox );

    //status bar
    KStatusBar *m_statusBar = new KStatusBar( vbox );

    m_statusLabel = new KSqueezedTextLabel( m_statusBar );
    m_statusLabel->setIndent( 3 );
    m_progressBox = new KHBox( m_statusBar );

    m_statusBar->addWidget( m_statusLabel, 4 );
    m_statusBar->addPermanentWidget( m_progressBox, 1 );

    KPushButton *stopButton = new KPushButton( KGuiItem(i18n("Abort"), "stop"), m_progressBox );
    connect( stopButton, SIGNAL(clicked()), SLOT(stopFetching()) );
    m_progress = new QProgressBar( m_progressBox );
    m_progress->setTextVisible( true );

    const int h = m_statusLabel->height() + 3;
    m_statusLabel->setFixedHeight( h );
    m_progressBox->setFixedHeight( h );
    m_progressBox->hide();


    // signals and slots connections
    connect( m_artistView, SIGNAL(itemSelectionChanged() ),
                           SLOT( slotArtistSelected() ) );
/*    connect( m_coverView,  SIGNAL(contextMenuRequested( Q3IconViewItem*, const QPoint& )),
                           SLOT(showCoverMenu( Q3IconViewItem*, const QPoint& )) );*/
    connect( m_coverView,  SIGNAL(itemActivated( QListWidgetItem* )),
                           SLOT(coverItemExecuted( QListWidgetItem* )) );
    connect( m_timer,      SIGNAL(timeout()),
                           SLOT(slotSetFilter()) );
    connect( m_searchEdit, SIGNAL(textChanged( const QString& )),
                           SLOT(slotSetFilterTimeout()) );

    m_currentView = AllAlbums;

    QSize size = QApplication::desktop()->screenGeometry( this ).size() / 1.5;
    QSize sz = Amarok::config( "Cover Manager" ).readEntry( "Window Size", size );
    resize( sz.width(), sz.height() );

    show();

    m_fetcher = The::coverFetcher();

    QTimer::singleShot( 0, this, SLOT(init()) );
}


CoverManager::~CoverManager()
{
    DEBUG_BLOCK

    Amarok::config( "Cover Manager" ).writeEntry( "Window Size", size() );

    s_instance = 0;
}


void CoverManager::init()
{
    DEBUG_BLOCK

    QTreeWidgetItem *item = 0;

    int i = 0;
    if ( !artistToSelectInInitFunction.isEmpty() )
        for( item = m_artistView->invisibleRootItem()->child( 0 );
           i < m_artistView->invisibleRootItem()->childCount();
           item = m_artistView->invisibleRootItem()->child( i++ ) )
            if ( item->text( 0 ) == artistToSelectInInitFunction )
                break;

    if ( item == 0 )
        item = m_artistView->invisibleRootItem()->child( 0 );

    item->setSelected( true );
}


CoverViewDialog::CoverViewDialog( Meta::AlbumPtr album, QWidget *parent )
    : QDialog( parent, Qt::WType_TopLevel | Qt::WNoAutoErase )
{
    m_pixmap = album->image( 0 ); // full sized image
    setAttribute( Qt::WA_DeleteOnClose );

#ifdef Q_WS_X11
    KWindowSystem::setType( winId(), NET::Utility );
#endif

    kapp->setTopWidget( this );
    setWindowTitle( KDialog::makeStandardCaption( i18n("%1 - %2",
                    album->albumArtist()->prettyName(), album->prettyName() ) ) );

    m_layout = new QHBoxLayout( this );
    m_layout->setSizeConstraint( QLayout::SetFixedSize );
    m_pixmapViewer = new PixmapViewer( this, m_pixmap );
    m_layout->addWidget( m_pixmapViewer );
}


void CoverManager::viewCover( Meta::AlbumPtr album, QWidget *parent ) //static
{
    //QDialog means "escape" works as expected
    QDialog *dialog = new CoverViewDialog( album, parent );
    dialog->show();
}


QString CoverManager::amazonTld() //static
{
    if( AmarokConfig::amazonLocale() == "us" )
        return "com";
    else if( AmarokConfig::amazonLocale()== "jp" )
        return "co.jp";
    else if( AmarokConfig::amazonLocale() == "uk" )
        return "co.uk";
    else if( AmarokConfig::amazonLocale() == "ca" )
        return "ca";
    else
        return AmarokConfig::amazonLocale();
}

void
CoverManager::metadataChanged( Meta::Album* album )
{
    DEBUG_BLOCK

    ArtistItem *selectedItem = static_cast<ArtistItem*>(m_artistView->selectedItems().first());
    if( selectedItem->text( 0 ) != i18n( "All Artists" ) )
    {
        if ( album->albumArtist() != selectedItem->artist() )
            return;
    }
    else
    {
        foreach( CoverViewItem *item, m_coverItems )
        {
            if( album->name() == item->albumPtr()->name() )
                item->loadCover();
        }
        // Update have/missing count.
        updateStatusBar();
    }
}

void CoverManager::fetchMissingCovers() //SLOT
{
    DEBUG_BLOCK

    int i = 0;
    for ( QListWidgetItem *item = m_coverView->item( i );
          i < m_coverView->count();
          item =  m_coverView->item( i++ ) )
    {
        CoverViewItem *coverItem = static_cast<CoverViewItem*>( item );
        if( !coverItem->hasCover() ) {
            m_fetchCovers += coverItem->albumPtr();
        }
    }

    m_fetcher->queueAlbums( m_fetchCovers );

    updateStatusBar();
    m_fetchButton->setEnabled( false );

}

void CoverManager::showOnce( const QString &artist )
{
    if ( !s_instance ) {
        artistToSelectInInitFunction = artist;
        new CoverManager(); //shows itself
    }
    else {
        s_instance->activateWindow();
        s_instance->raise();
    }
}

void CoverManager::slotArtistSelected() //SLOT
{
    QTreeWidgetItem *item = m_artistView->selectedItems().first();
    ArtistItem *artistItem = static_cast< ArtistItem* >(item);
    Meta::ArtistPtr artist = artistItem->artist();

    //TODO: port?
//     if( artist->prettyName().endsWith( ", The" ) )
//        Amarok::manipulateThe( artist->prettyName(), false );

    m_coverView->clear();
    m_coverItems.clear();

    // reset current view mode state to "AllAlbum" which is the default on artist change in left panel
    m_currentView = AllAlbums;
    m_selectAllAlbums->trigger();
    m_selectAllAlbums->setChecked( true );

    QProgressDialog progress( this );
    progress.setLabelText( i18n("Loading Thumbnails...") );
    progress.setWindowModality( Qt::WindowModal );

    //this is an extra processEvent call for the sake of init() and aesthetics
    //it isn't necessary
    kapp->processEvents();

    //this can be a bit slow
    QApplication::setOverrideCursor( Qt::WaitCursor );

    Meta::AlbumList albums;

    Collection *coll;
    foreach( coll, CollectionManager::instance()->collections() )
        if( coll->collectionId() == "localCollection" )
            break;
    QueryMaker *qm = coll->queryMaker();
    if ( item != m_artistView->invisibleRootItem()->child( 0 ) )
        qm->addMatch( artist );

    qm->startAlbumQuery();
    BlockingQuery bq( qm );
    bq.startQuery();
    albums = bq.albums( coll->collectionId() );


    //TODO: Port 2.0
    //also retrieve compilations when we're showing all items (first treenode) or
    //"Various Artists" (last treenode)
//     if ( item == m_artistView->firstChild() || item == m_artistView->lastChild() )
//     {
//         QStringList cl;
// 
//         qb.clear();
//         qb.addReturnValue( QueryBuilder::tabAlbum,  QueryBuilder::valName );
// 
//         qb.excludeMatch( QueryBuilder::tabAlbum, i18n( "Unknown" ) );
//         qb.sortBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
//         qb.setOptions( QueryBuilder::optRemoveDuplicates );
//         qb.setOptions( QueryBuilder::optOnlyCompilations );
//         cl = qb.run();
// 
//         for( int i = 0; i < cl.count(); i++ ) {
//             albums.append( i18n( "Various Artists" ) );
//             albums.append( cl[ i ] );
//         }
//     }

    QApplication::restoreOverrideCursor();

    progress.setMaximum( albums.count() );

    //insert the covers first because the list view is soooo paint-happy
    //doing it in the second loop looks really bad, unfortunately
    //this is the slowest step in the bit that we can't process events
    uint x = 0;
    foreach( Meta::AlbumPtr album, albums )
    {
        m_coverItems.append( new CoverViewItem( m_coverView, album ) );

        if ( ++x % 50 == 0 ) {
            progress.setValue( x );
            kapp->processEvents(); // QProgressDialog also calls this, but not always due to Qt bug!

            //only worth testing for after processEvents() is called
            if( progress.wasCanceled() )
               break;
        }
    }

    //now, load the thumbnails
    QList<QListWidgetItem*> items;
    int i = 0;
    for ( QListWidgetItem *item = m_coverView->item( i );
          i < m_coverView->count();
          item = m_coverView->item( i++ ) )
    {
        progress.setValue( progress.value() + 1 );
        kapp->processEvents();

        if( progress.wasCanceled() )
           break;

        static_cast<CoverViewItem*>(item)->loadCover();
    }

    updateStatusBar();
}

void CoverManager::showCoverMenu( QListWidgetItem *item, const QPoint &p ) //SLOT
{
    Q_UNUSED( item );
    Q_UNUSED( p );
    //TODO: PORT
#if 0
    #define item static_cast<CoverViewItem*>(item)
    if( !item ) return;

    KMenu menu;

    menu.addTitle( i18n( "Cover Image" ) );

    QList<CoverViewItem*> selected = selectedItems();
    const int nSelected = selected.count();

    QAction* fetchSelectedAction = new QAction( KIcon( "get-hot-new-stuff-amarok" )
        , ( nSelected == 1 ? i18n( "&Fetch From amazon.%1", CoverManager::amazonTld() )
                           : i18n( "&Fetch Selected Covers" ) )
        , &menu );
    connect( fetchSelectedAction, SIGNAL( triggered() ), this, SLOT( fetchSelectedCovers() ) );

    QAction* setCustomAction = new QAction( KIcon( "folder-amarok" )
        , i18np( "Set &Custom Cover", "Set &Custom Cover for Selected Albums", nSelected )
        , &menu );
    connect( setCustomAction, SIGNAL( triggered() ), this, SLOT( setCustomSelectedCovers() ) );
    QAction* unsetAction = new QAction( KIcon( "edit-delete-amarok" ), i18np( "&Unset Cover", "&Unset Selected Covers", nSelected ), &menu );
    connect( unsetAction, SIGNAL( triggered() ), this, SLOT ( deleteSelectedCovers() ) );

    QAction* playAlbumAction = new QAction( KIcon( "list-add-amarok" )
        , i18np( "&Append to Playlist", "&Append Selected Albums to Playlist", nSelected )
        , &menu );

    connect( playAlbumAction, SIGNAL( triggered() ), this, SLOT( playSelectedAlbums() ) );

    if( nSelected > 1 ) {
        menu.addAction( fetchSelectedAction );
        menu.addAction( setCustomAction );
        menu.addAction( playAlbumAction );
        menu.addAction( unsetAction );
    }
    else {
        QAction* viewAction = new QAction( KIcon( "zoom-in-amarok" ), i18n( "&Show Fullsize" ), &menu );
        connect( viewAction, SIGNAL( triggered() ), this, SLOT( viewSelectedCover() ) );
        viewAction ->setEnabled( item->hasCover() );
        unsetAction->setEnabled( item->canRemoveCover() );
        menu.addAction( viewAction );
        menu.addAction( fetchSelectedAction );
        menu.addAction( setCustomAction );
        menu.addAction( playAlbumAction );
        menu.addSeparator();

        menu.addAction( unsetAction );
    }

    menu.exec( p );

    #undef item
#endif
}

void CoverManager::viewSelectedCover()
{
    CoverViewItem* item = selectedItems().first();
    viewCover( item->albumPtr(), this );
}

void CoverManager::coverItemExecuted( QListWidgetItem *item ) //SLOT
{
    #define item static_cast<CoverViewItem*>(item)

    if( !item ) return;

    item->setSelected( true );
    if ( item->hasCover() )
        viewCover( item->albumPtr(), this );
    else
        m_fetcher->manualFetch( item->albumPtr() );

    #undef item
}


void CoverManager::slotSetFilter() //SLOT
{
    m_filter = m_searchEdit->text();

    m_coverView->clearSelection();
    uint i = 0;
    QListWidgetItem *item = m_coverView->item( i );
    while ( item )
    {
        QListWidgetItem *tmp = m_coverView->item( i + 1 );
        m_coverView->takeItem( i );
        item = tmp;
    }

//     m_coverView->setAutoArrange( false );
    foreach( QListWidgetItem *item, m_coverItems )
    {
        CoverViewItem *coverItem = static_cast<CoverViewItem*>(item);
        if( coverItem->album().contains( m_filter, Qt::CaseInsensitive ) || coverItem->artist().contains( m_filter, Qt::CaseInsensitive ) )
            m_coverView->insertItem( m_coverView->count() -  1, item );
    }
//     m_coverView->setAutoArrange( true );

//     m_coverView->arrangeItemsInGrid();
    updateStatusBar();
}


void CoverManager::slotSetFilterTimeout() //SLOT
{
    if ( m_timer->isActive() ) m_timer->stop();
    m_timer->setSingleShot( true );
    m_timer->start( 180 );
}

void CoverManager::changeView( int id  ) //SLOT
{
    if( m_currentView == id ) return;

    //clear the iconview without deleting items
    m_coverView->clearSelection();
    QListWidgetItem *item = m_coverView->item( 0 );
    uint i = 0;
    while ( item ) {
        m_coverView->takeItem( i );
        i++;
    }

//     m_coverView->setAutoArrange(false );
    foreach( QListWidgetItem *item, m_coverItems )
    {
        bool show = false;
        CoverViewItem *coverItem = static_cast<CoverViewItem*>(item);
        if( !m_filter.isEmpty() ) {
            if( !coverItem->album().contains( m_filter, Qt::CaseInsensitive ) && !coverItem->artist().contains( m_filter, Qt::CaseInsensitive ) )
                continue;
        }

        if( id == AllAlbums )    //show all albums
            show = true;
        else if( id == AlbumsWithCover && coverItem->hasCover() )    //show only albums with cover
            show = true;
        else if( id == AlbumsWithoutCover && !coverItem->hasCover() )   //show only albums without cover
            show = true;

        if( show )    m_coverView->insertItem( m_coverView->count() - 1, item );
    }
//     m_coverView->setAutoArrange( true );

//     m_coverView->arrangeItemsInGrid();
    m_currentView = id;
}

void CoverManager::changeLocale( int id ) //SLOT
{
    QString locale = CoverFetcher::localeIDToString( id );
    AmarokConfig::setAmazonLocale( locale );
    m_currentLocale = id;
}


void CoverManager::coverFetched( const QString &artist, const QString &album ) //SLOT
{
    loadCover( artist, album );
    m_coversFetched++;
    updateStatusBar();
}


void CoverManager::coverRemoved( const QString &artist, const QString &album ) //SLOT
{
    loadCover( artist, album );
    m_coversFetched--;
    updateStatusBar();
}


void CoverManager::coverFetcherError()
{
    DEBUG_FUNC_INFO

    m_coverErrors++;
    updateStatusBar();
}


void CoverManager::stopFetching()
{
    Debug::Block block( __PRETTY_FUNCTION__ );
    updateStatusBar();
}

// PRIVATE

void CoverManager::loadCover( const QString &artist, const QString &album )
{
    foreach( QListWidgetItem *item, m_coverItems )
    {
        CoverViewItem *coverItem = static_cast<CoverViewItem*>(item);
        if ( album == coverItem->album() && ( artist == coverItem->artist() || ( artist.isEmpty() && coverItem->artist().isEmpty() ) ) )
        {
            coverItem->loadCover();
            return;
        }
    }
}

void CoverManager::setCustomSelectedCovers()
{
    //function assumes something is selected
    CoverViewItem* first = selectedItems().first();

    Meta::TrackPtr track = first->albumPtr()->tracks().first();
    QString startPath;
    if( track )
    {
        KUrl url = track->playableUrl();
        startPath = url.directory();
    }
    KUrl file = KFileDialog::getImageOpenUrl( startPath, this, i18n( "Select Cover Image File" ) );
    if ( !file.isEmpty() ) {
        kapp->processEvents();    //it may takes a while so process pending events
        QString tmpFile;
        QImage image( file.fileName() );
        foreach( CoverViewItem *item, selectedItems() )
        {
            item->albumPtr()->setImage( image );
            item->loadCover();
        }
        KIO::NetAccess::removeTempFile( tmpFile );
    }
}

void CoverManager::fetchSelectedCovers()
{
    foreach( CoverViewItem *item, selectedItems() )
        m_fetchCovers += item->albumPtr();

    m_fetchingCovers += selectedItems().count();

    m_fetcher->queueAlbums( m_fetchCovers );

    updateStatusBar();
}


void CoverManager::deleteSelectedCovers()
{
    QList<CoverViewItem*> selected = selectedItems();

    int button = KMessageBox::warningContinueCancel( this,
                            i18np( "Are you sure you want to remove this cover from the Collection?",
                                  "Are you sure you want to delete these %1 covers from the Collection?",
                                  selected.count() ),
                            QString(),
                            KStandardGuiItem::del() );

    if ( button == KMessageBox::Continue ) {
        foreach( CoverViewItem *item, selected ) {
            kapp->processEvents();
            if( item->albumPtr()->canUpdateImage() )
            {
                item->albumPtr()->setImage( QImage() );
                coverRemoved( item->artist(), item->album() );
            }
        }
    }
}


void CoverManager::playSelectedAlbums()
{
    Collection *coll;
    foreach( coll, CollectionManager::instance()->collections() )
        if( coll->collectionId() == "localCollection" )
            break;
    QueryMaker *qm = coll->queryMaker();
    foreach( CoverViewItem *item, selectedItems() )
    {
        qm->addMatch( item->albumPtr() );
    }
    The::playlistModel()->insertOptioned( qm, Playlist::Append );
}

QList<CoverViewItem*> CoverManager::selectedItems()
{
    QList<CoverViewItem*> selectedItems;
    foreach( QListWidgetItem *item, m_coverView->selectedItems() )
        selectedItems.append( static_cast<CoverViewItem*>(item) );

    return selectedItems;
}


void CoverManager::updateStatusBar()
{
    QString text;

    //cover fetching info
    if( m_fetchingCovers ) {
        //update the progress bar
        m_progress->setMaximum( m_fetchingCovers );
        m_progress->setValue( m_coversFetched + m_coverErrors );
        if( m_progressBox->isHidden() )
            m_progressBox->show();

        //update the status text
        if( m_coversFetched + m_coverErrors >= m_progress->value() ) {
            //fetching finished
            text = i18nc( "The fetching is done.", "Finished." );
            if( m_coverErrors )
                text += i18np( " Cover not found", " <b>%1</b> covers not found", m_coverErrors );
            //reset counters
            m_fetchingCovers = 0;
            m_coversFetched = 0;
            m_coverErrors = 0;
            QTimer::singleShot( 2000, this, SLOT( updateStatusBar() ) );
        }

        if( m_fetchingCovers == 1 ) {
            foreach( Meta::AlbumPtr album, m_fetchCovers )
            {
                if( album->albumArtist()->prettyName().isEmpty() )
                    text = i18n( "Fetching cover for %1..." , album->prettyName() );
                else
                    text = i18n( "Fetching cover for %1 - %2...",
                                 album->albumArtist()->prettyName(),
                                 album->prettyName() );
            }
        }
        else if( m_fetchingCovers ) {
            text = i18np( "Fetching 1 cover: ", "Fetching <b>%1</b> covers... : ", m_fetchingCovers );
            if( m_coversFetched )
                text += i18np( "1 fetched", "%1 fetched", m_coversFetched );
            if( m_coverErrors ) {
            if( m_coversFetched ) text += i18n(" - ");
                text += i18np( "1 not found", "%1 not found", m_coverErrors );
            }
            if( m_coversFetched + m_coverErrors == 0 )
                text += i18n( "Connecting..." );
        }
    }
    else {
        m_coversFetched = 0;
        m_coverErrors = 0;

        uint totalCounter = 0, missingCounter = 0;

        if( m_progressBox->isVisible() )
            m_progressBox->hide();

        //album info
        int i = 0;
        for( QListWidgetItem *item = m_coverView->item( i );
             i < m_coverView->count();
             item = m_coverView->item( i++ ) )
        {
            totalCounter++;
            if( !static_cast<CoverViewItem*>( item )->hasCover() )
            {
                missingCounter++;    //counter for albums without cover
            }
        }

        if( !m_filter.isEmpty() )
            text = i18np( "1 result for \"%2\"", "%1 results for \"%2\"", totalCounter, m_filter );
        else if( m_artistView->selectedItems().count() > 0 ) {
            text = i18np( "1 album", "%1 albums", totalCounter );
            if( m_artistView->selectedItems().first() != m_artistView->invisibleRootItem()->child( 0 ) ) //showing albums by an artist
            {
                QString artist = m_artistView->selectedItems().first()->text(0);
                if( artist.endsWith( ", The" ) )
                    Amarok::manipulateThe( artist, false );
                text += i18n( " by " ) + artist;
            }
        }

        if( missingCounter )
            text += i18n(" - ( <b>%1</b> without cover )", missingCounter );

        m_fetchButton->setEnabled( missingCounter );
    }

    m_statusLabel->setText( text );
}

void CoverManager::setStatusText( QString text )
{
    m_oldStatusText = m_statusLabel->text();
    m_statusLabel->setText( text );
}
//////////////////////////////////////////////////////////////////////
//    CLASS CoverView
/////////////////////////////////////////////////////////////////////

CoverView::CoverView( QWidget *parent, const char *name, Qt::WFlags f )
    : QListWidget( parent )
{
    Debug::Block block( __PRETTY_FUNCTION__ );

    setObjectName( name );
    setWindowFlags( f );
    setViewMode( QListView::IconMode );
    setMovement( QListView::Static );
    setResizeMode( QListView::Adjust );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setWrapping( true );
    setSpacing( 4 );
    setWordWrap( true );
    setIconSize( QSize(100,100) );
    setTextElideMode( Qt::ElideRight );
    setMouseTracking( true );

    // as long as QIconView only shows tooltips when the cursor is over the
    // icon (and not the text), we have to create our own tooltips
//     setShowToolTips( false );

    connect( this, SIGNAL( itemEntered( QListWidgetItem * ) ), SLOT( setStatusText( QListWidgetItem * ) ) );
    connect( this, SIGNAL( viewportEntered() ), CoverManager::instance(), SLOT( updateStatusBar() ) );
}


//TODO: PORT 2.0
// Q3DragObject *CoverView::dragObject()
// {
//     CoverViewItem *item = static_cast<CoverViewItem*>( currentItem() );
//     if( !item )
//        return 0;
// 
//     const QString sql = "SELECT tags.url FROM tags, album WHERE album.name %1 AND tags.album = album.id ORDER BY tags.track;";
//     const QStringList values = CollectionDB::instance()->query( sql.arg( CollectionDB::likeCondition( item->album() ) ) );
// 
//     KUrl::List urls;
//     for( QStringList::ConstIterator it = values.constBegin(), end = values.constEnd(); it != end; ++it )
//         urls += *it;
// 
//     QString imagePath = CollectionDB::instance()->albumImage( item->artist(), item->album(), false, 1 );
//     K3MultipleDrag *drag = new K3MultipleDrag( this );
//     drag->setPixmap( item->coverPixmap() );
//     drag->addDragObject( new Q3IconDrag( this ) );
//     drag->addDragObject( new Q3ImageDrag( QImage( imagePath ) ) );
//     drag->addDragObject( new K3URLDrag( urls ) );
// 
//     return drag;
// }

void CoverView::setStatusText( QListWidgetItem *item )
{
    #define item static_cast<CoverViewItem *>( item )
    if ( !item )
        return;
    
    bool sampler = false;
    //compilations have valDummy for artist.  see QueryBuilder::addReturnValue(..) for explanation
    //FIXME: Don't rely on other independent code, use an sql query
    if( item->artist().isEmpty() ) sampler = true;

    QString tipContent = i18n( "%1 - %2", sampler ? i18n("Various Artists") : item->artist() , item->album() );

    CoverManager::instance()->setStatusText( tipContent );

    #undef item
}

//////////////////////////////////////////////////////////////////////
//    CLASS CoverViewItem
/////////////////////////////////////////////////////////////////////

CoverViewItem::CoverViewItem( QListWidget *parent, Meta::AlbumPtr album )
    : QListWidgetItem( parent )
    , m_albumPtr( album)
    , m_parent( parent )
    , m_coverPixmap( )
{
    m_album = album->prettyName();
    if( album->albumArtist() )
        m_artist = album->albumArtist()->prettyName();
    else
        m_artist = i18n( "No Artist" );
    setText( album->prettyName() );
    setIcon( album->image( 100 ) );
    album->subscribe( qobject_cast<CoverManager*>(parent->parent()->parent()) );
//     setDragEnabled( true );
//     setDropEnabled( true );
    calcRect();
}

CoverViewItem::~CoverViewItem()
{
    m_albumPtr->unsubscribe(  qobject_cast<CoverManager*>( m_parent->parent()->parent()) );
}
bool CoverViewItem::hasCover() const
{
    return albumPtr()->hasImage();
}

void CoverViewItem::loadCover()
{
    m_coverPixmap = m_albumPtr->image();  //create the scaled cover
    setIcon( m_coverPixmap );

//     repaint();
}


void CoverViewItem::calcRect( const QString& )
{
#if 0
    int thumbWidth = AmarokConfig::coverPreviewSize();

    QFontMetrics fm = listWidget()->fontMetrics();
    QRect itemPixmapRect( 5, 1, thumbWidth, thumbWidth );
    QRect itemRect = rect();
    itemRect.setWidth( thumbWidth + 10 );
    itemRect.setHeight( thumbWidth + fm.lineSpacing() + 2 );
    QRect itemTextRect( 0, thumbWidth+2, itemRect.width(), fm.lineSpacing() );

    setPixmapRect( itemPixmapRect );
    setTextRect( itemTextRect );
    setItemRect( itemRect );
#endif
}


//TODO: Port
// void CoverViewItem::dropped( QDropEvent *e, const Q3ValueList<Q3IconDragItem> & )
// {
//     if( Q3ImageDrag::canDecode( e ) ) {
//        if( hasCover() ) {
//            KGuiItem continueButton = KStandardGuiItem::cont();
//            continueButton.setText( i18n("&Overwrite") );
//            int button = KMessageBox::warningContinueCancel( iconView(),
//                             i18n( "Are you sure you want to overwrite this cover?"),
//                             i18n("Overwrite Confirmation"),
//                             continueButton );
//            if( button == KMessageBox::Cancel )
//                return;
//        }
// 
//        QImage img;
//        Q3ImageDrag::decode( e, img );
//        m_albumPtr->setImage( img );
//        loadCover();
//     }
// }


void CoverViewItem::dragEntered()
{
    setSelected( true );
}


void CoverViewItem::dragLeft()
{
    setSelected( false );
}

#include "CoverManager.moc"