// (c) Pierpaolo Di Panfilo 2004
// See COPYING file for licensing information

#include "amarok.h"
#include "amarokconfig.h"
#include "browserToolBar.h"
#include "clicklineedit.h"
#include "collectiondb.h"
#include "config.h"
#include "coverfetcher.h"
#include "covermanager.h"
#include "debug.h"

#include <qdesktopwidget.h>  //ctor: desktop size
#include <qfile.h>
#include <qfontmetrics.h>    //paintItem()
#include <qhbox.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qobjectlist.h>    //used to delete all cover fetchers
#include <qpainter.h>    //paintItem()
#include <qpalette.h>    //paintItem()
#include <qpixmap.h>
#include <qpoint.h>
#include <qprogressdialog.h>
#include <qrect.h>
#include <qstringlist.h>
#include <qtimer.h>    //search filter timer
#include <qtooltip.h>
#include <qvbox.h>

#include <kapplication.h>
#include <kconfig.h>
#include <kcursor.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <klistview.h>
#include <klocale.h>
#include <kmessagebox.h>    //showCoverMenu()
#include <kmultipledrag.h>
#include <kio/netaccess.h>
#include <kpopupmenu.h>    //showCoverMenu()
#include <kprogress.h>
#include <kpushbutton.h>
#include <ksqueezedtextlabel.h> //status label
#include <kstandarddirs.h>   //KGlobal::dirs()
#include <kstatusbar.h>
#include <ktoolbar.h>
#include <ktoolbarbutton.h>    //clear filter button
#include <kurl.h>
#include <kurldrag.h>


static QString artistToSelectInInitFunction;
CoverManager *CoverManager::s_instance = 0;


CoverManager::CoverManager()
        : QSplitter( 0, "TheCoverManager" )
        , m_timer( new QTimer( this ) )    //search filter timer
        , m_fetchCounter( 0 )
        , m_fetchingCovers( 0 )
        , m_coversFetched( 0 )
        , m_coverErrors( 0 )
{
    DEBUG_BLOCK

    s_instance = this;

    // Sets caption and icon correctly (needed e.g. for GNOME)
    kapp->setTopWidget( this );
    this->setCaption( kapp->makeStdCaption( i18n("Cover Manager") ) );
    this->setWFlags( WDestructiveClose );
    this->setMargin( 4 );

    //artist listview
    m_artistView = new KListView( this );
    m_artistView->addColumn(i18n( "Albums By" ));
    m_artistView->setFullWidth( true );
    m_artistView->setSorting( -1 );    //no sort
    m_artistView->setMinimumWidth( 180 );
    KListViewItem *item = new KListViewItem( m_artistView, i18n( "All Albums" ) );
    item->setPixmap( 0, SmallIcon("cdrom_unmount") );

    //load artists from the collection db
    const QStringList artists = CollectionDB::instance()->artistList( false, false );
    foreach( artists )  {
        item = new KListViewItem( m_artistView, item, *it );
        item->setPixmap( 0, SmallIcon("personal") );
    }

    QVBox *vbox = new QVBox( this );
    QHBox *hbox = new QHBox( vbox );

    vbox->setSpacing( 4 );
    hbox->setSpacing( 4 );

    { //<Search LineEdit>
        QHBox *searchBox = new QHBox( hbox );
        KToolBar* searchToolBar = new Browser::ToolBar( searchBox );
        KToolBarButton *button = new KToolBarButton( "locationbar_erase", 0, searchToolBar );
        m_searchEdit = new ClickLineEdit( i18n( "Filter here..." ), searchToolBar );
        m_searchEdit->setFrame( QFrame::Sunken );

        searchToolBar->setStretchableWidget( m_searchEdit );
        connect( button, SIGNAL(clicked()), m_searchEdit, SLOT(clear()) );

        QToolTip::add( button, i18n( "Clear filter" ) );
        QToolTip::add( m_searchEdit, i18n( "Enter space-separated terms to filter albums" ) );

        hbox->setStretchFactor( searchBox, 1 );
    } //</Search LineEdit>

    // view menu
    m_viewMenu = new KPopupMenu( this );
    m_viewMenu->insertItem( i18n("All Albums"), AllAlbums );
    m_viewMenu->insertItem( i18n("Albums With Cover"), AlbumsWithCover );
    m_viewMenu->insertItem( i18n("Albums Without Cover"), AlbumsWithoutCover );
    m_viewMenu->setItemChecked( AllAlbums, true );
    connect( m_viewMenu, SIGNAL( activated(int) ), SLOT( changeView(int) ) );

    #ifdef AMAZON_SUPPORT
    // amazon locale menu
    m_amazonLocaleMenu = new KPopupMenu( this );
    m_amazonLocaleMenu->insertItem( i18n("International"), International );
    m_amazonLocaleMenu->insertItem( i18n("France"), France );
    m_amazonLocaleMenu->insertItem( i18n("Germany"), Germany );
    m_amazonLocaleMenu->insertItem( i18n("Japan"), Japan);
    m_amazonLocaleMenu->insertItem( i18n("United Kingdom"), UK );
    connect( m_amazonLocaleMenu, SIGNAL( activated(int) ), SLOT( changeLocale(int) ) );
    #endif

    KToolBar* toolBar = new KToolBar( hbox );
    toolBar->setIconText( KToolBar::IconTextRight );
    toolBar->setFrameShape( QFrame::NoFrame );
    toolBar->insertButton( "view_choose", 1, m_viewMenu, true, i18n( "View" ) );
    #ifdef AMAZON_SUPPORT
    const QString babelfish = locate( "data", QString( "amarok/images/babelfish.png" ) );
    toolBar->insertButton( babelfish, 2, m_amazonLocaleMenu, true, i18n( "Amazon Locale" ) );

    QString locale = AmarokConfig::amazonLocale();

         if( locale == "fr" ) m_currentLocale = France;
    else if( locale == "de" ) m_currentLocale = Germany;
    else if( locale == "jp" ) m_currentLocale = Japan;
    else if( locale == "uk" ) m_currentLocale = UK;
    else {
        // make sure we handle old config files correctly
        locale = "us";
        m_currentLocale = International;
    }

    m_amazonLocaleMenu->setItemChecked( m_currentLocale, true );

    //fetch missing covers button
    m_fetchButton = new KPushButton( KGuiItem( i18n("Fetch Missing Covers"), "cdrom_unmount" ), hbox );
    connect( m_fetchButton, SIGNAL(clicked()), SLOT(fetchMissingCovers()) );
    #endif

    //cover view
    m_coverView = new CoverView( vbox );

    //status bar
    KStatusBar *m_statusBar = new KStatusBar( vbox );
    m_statusBar->addWidget( m_statusLabel = new KSqueezedTextLabel( m_statusBar ), 4 );
    m_statusLabel->setIndent( 3 );
    m_statusBar->addWidget( m_progressBox = new QHBox( m_statusBar ), 1, true );
    KPushButton *stopButton = new KPushButton( KGuiItem(i18n("Abort"), "stop"), m_progressBox );
    connect( stopButton, SIGNAL(clicked()), SLOT(stopFetching()) );
    m_progress = new KProgress( m_progressBox );
    m_progress->setCenterIndicator( true );

    const int h = m_statusLabel->height() + 3;
    m_statusLabel->setFixedHeight( h );
    m_progressBox->setFixedHeight( h );
    m_progressBox->hide();


    // signals and slots connections
    connect( m_artistView, SIGNAL(selectionChanged( QListViewItem* ) ), SLOT(slotArtistSelected( QListViewItem* )) );
    connect( m_coverView, SIGNAL(rightButtonPressed( QIconViewItem*, const QPoint& )), SLOT(showCoverMenu( QIconViewItem*, const QPoint& )) );
    connect( m_coverView, SIGNAL(executed( QIconViewItem* )), SLOT(coverItemExecuted( QIconViewItem* )) );
    connect( m_timer, SIGNAL(timeout()), SLOT(slotSetFilter()) );
    connect( m_searchEdit, SIGNAL(textChanged( const QString& )), SLOT(slotSetFilterTimeout()) );

    #ifdef AMAZON_SUPPORT
    connect( CollectionDB::instance(), SIGNAL(coverFetched( const QString&, const QString& )), SLOT(coverFetched( const QString&, const QString& )) );
    connect( CollectionDB::instance(), SIGNAL(coverRemoved( const QString&, const QString& )), SLOT(coverRemoved( const QString&, const QString& )) );
    connect( CollectionDB::instance(), SIGNAL(coverFetcherError( const QString& )), SLOT(coverFetcherError()) );
    #endif

    m_currentView = AllAlbums;

    QSize size = QApplication::desktop()->screenGeometry( this ).size() / 1.5;
    resize( amaroK::config( "Cover Manager" )->readSizeEntry( "Window Size", &size ) );

    show();

    QTimer::singleShot( 0, this, SLOT(init()) );
}


CoverManager::~CoverManager()
{
    DEBUG_BLOCK

    amaroK::config( "Cover Manager" )->writeEntry( "Window Size", size() );

    s_instance = 0;
}


void CoverManager::init()
{
    DEBUG_BLOCK

    QListViewItem *item = 0;

    if ( !artistToSelectInInitFunction.isEmpty() )
        for ( item = m_artistView->firstChild(); item; item = item->nextSibling() )
            if ( item->text( 0 ) == artistToSelectInInitFunction )
                break;

    if ( item == 0 )
        item = m_artistView->firstChild();

    m_artistView->setSelected( item, true );
}


void CoverManager::viewCover( const QString& artist, const QString& album, QWidget *parent ) //static
{
    //QDialog means "escape" works as expected
    QDialog *dialog = new QDialog( parent, 0, false, WDestructiveClose | WType_TopLevel );
    kapp->setTopWidget( dialog );
    dialog->setCaption( kapp->makeStdCaption( artist + " - " + album ) );

    QPixmap pixmap( CollectionDB::instance()->albumImage( artist, album, 0 ) );
    QLabel* label = new QLabel( dialog );
    label->setFrameShape( QFrame::NoFrame );
    dialog->setFixedSize( pixmap.size() );
    label->setFixedSize( pixmap.size() );
    label->setPixmap( pixmap );

    dialog->show();
}
QString CoverManager::amazonTld() //static
{
    if (AmarokConfig::amazonLocale() == "us")
        return "com";
            else if (AmarokConfig::amazonLocale()== "jp")
        return "co.jp";
            else if (AmarokConfig::amazonLocale() == "uk")
        return "co.uk";
            else
        return AmarokConfig::amazonLocale();
}

void CoverManager::fetchMissingCovers() //SLOT
{
    #ifdef AMAZON_SUPPORT

    DEBUG_BLOCK

    for ( QIconViewItem *item = m_coverView->firstItem(); item; item = item->nextItem() ) {
        CoverViewItem *coverItem = static_cast<CoverViewItem*>( item );
        if( !coverItem->hasCover() ) {
            m_fetchCovers += coverItem->artist() + " @@@ " + coverItem->album();
            m_fetchingCovers++;
        }
    }

    if( !m_fetchCounter )    //loop isn't started yet
        fetchCoversLoop();

    updateStatusBar();
    m_fetchButton->setEnabled( false );

    #endif
}


void CoverManager::fetchCoversLoop() //SLOT
{
    #ifdef AMAZON_SUPPORT

    if( m_fetchCounter < m_fetchCovers.count() ) {
        //get artist and album from keyword
        const QStringList values = QStringList::split( " @@@ ", m_fetchCovers[m_fetchCounter], true );

        if( values.count() > 1 )
           CollectionDB::instance()->fetchCover( this, values[0], values[1], m_fetchCovers.count() != 1); //edit mode when fetching 1 cover

        m_fetchCounter++;

        // Wait 1 second, since amazon caps the number of accesses per client
        QTimer::singleShot( 1000, this, SLOT( fetchCoversLoop() ) );
    }
    else {
        m_fetchCovers.clear();
        m_fetchCounter = 0;
    }

    #endif
}


void CoverManager::showOnce( const QString &artist )
{
    if ( !s_instance ) {
        artistToSelectInInitFunction = artist;
        new CoverManager(); //shows itself
    }
    else {
        s_instance->setActiveWindow();
        s_instance->raise();
    }
}

void CoverManager::slotArtistSelected( QListViewItem *item ) //SLOT
{
    if( item->depth() ) //album item
        return;

    m_coverView->clear();
    m_coverItems.clear();

    QProgressDialog progress( this, 0, true );
    progress.setLabelText( i18n("Loading Thumbnails...") );
    progress.QDialog::setCaption( "..." );

    //NOTE we MUST show the dialog, otherwise the closeEvents get processed
    // in the processEvents() calls below, GRUMBLE! Qt sux0rs
    progress.show();
    progress.repaint( false );  //ensures the dialog isn't blank

    //this is an extra processEvent call for the sake of init() and aesthetics
    //it isn't necessary
    kapp->processEvents();

    //this can be a bit slow
    QApplication::setOverrideCursor( KCursor::waitCursor() );
    QueryBuilder qb;
    QStringList albums;

    qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );
    qb.addReturnValue( QueryBuilder::tabAlbum,  QueryBuilder::valName );

    qb.excludeMatch( QueryBuilder::tabAlbum, i18n( "Unknown" ) );
    qb.sortBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.setOptions( QueryBuilder::optRemoveDuplicates );
    qb.setOptions( QueryBuilder::optNoCompilations );

    if ( item != m_artistView->firstChild() )
        qb.addMatches( QueryBuilder::tabArtist, item->text( 0 ) );

    albums = qb.run();

    //also retrieve compilations when we're showing all items (first treenode)
    if ( item == m_artistView->firstChild() )
    {
        QStringList cl;

        qb.clear();
        qb.addReturnValue( QueryBuilder::tabDummy, QueryBuilder::valDummy );
        qb.addReturnValue( QueryBuilder::tabAlbum,  QueryBuilder::valName );

        qb.excludeMatch( QueryBuilder::tabAlbum, i18n( "Unknown" ) );
        qb.sortBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
        qb.setOptions( QueryBuilder::optRemoveDuplicates );
        qb.setOptions( QueryBuilder::optOnlyCompilations );
        cl = qb.run();

        for ( uint i = 0; i < cl.count(); i++ )
            albums.append( cl[ i ] );
    }

    QApplication::restoreOverrideCursor();

    progress.setTotalSteps( (albums.count()/2) + (albums.count()/10) );

    //insert the covers first because the list view is soooo paint-happy
    //doing it in the second loop looks really bad, unfortunately
    //this is the slowest step in the bit that we can't process events
    uint x = 0;
    for( QStringList::ConstIterator it = albums.begin(), end = albums.end(); it != end; ++it ) {
        const QString artist = *it;
        const QString album = *(++it);
        m_coverItems.append( new CoverViewItem( m_coverView, m_coverView->lastItem(), artist, album ) );

        if ( ++x % 50 == 0 ) {
            progress.setProgress( x / 5 ); // we do it less often due to bug in Qt, ask Max
            kapp->processEvents(); // QProgressDialog also calls this, but not always due to Qt bug!

            //only worth testing for after processEvents() is called
            if( progress.wasCancelled() )
               break;
        }
    }

    //now, load the thumbnails
    for( QIconViewItem *item = m_coverView->firstItem(); item; item = item->nextItem() ) {
        progress.setProgress( progress.progress() + 1 );
        kapp->processEvents();

        if( progress.wasCancelled() )
           break;

        static_cast<CoverViewItem*>(item)->loadCover();
    }

    updateStatusBar();
}

void CoverManager::showCoverMenu( QIconViewItem *item, const QPoint &p ) //SLOT
{
    #define item static_cast<CoverViewItem*>(item)
    if( !item ) return;

    enum { SHOW, FETCH, CUSTOM, DELETE };

    KPopupMenu menu;

    menu.insertTitle( i18n( "Cover Image" ) );

    QPtrList<CoverViewItem> selected = selectedItems();
    if( selected.count() > 1 ) {
        #ifdef AMAZON_SUPPORT
        menu.insertItem( SmallIconSet( "www" ), i18n( "&Fetch Selected Covers" ), FETCH );
        #endif
        menu.insertItem( SmallIconSet("folder_image"), i18n("Set &Custom Cover for Selected Albums"), CUSTOM );
        menu.insertItem( SmallIconSet("editdelete"), i18n("&Unset Selected Covers"), DELETE );
    }
    else {
        menu.insertItem( SmallIconSet("viewmag"), i18n("&Show Fullsize"), SHOW );

        #ifdef AMAZON_SUPPORT
        menu.insertItem( SmallIconSet("www"), i18n("&Fetch From amazon.%1").arg( CoverManager::amazonTld() ), FETCH );
        menu.insertSeparator();
        menu.insertItem( SmallIconSet("folder_image"), i18n("Set &Custom Image"), CUSTOM );
        #else
        menu.insertItem( SmallIconSet("folder_image"), i18n("Set &Cover Image"), CUSTOM );
        #endif
        menu.insertSeparator();
        menu.insertItem( SmallIconSet("editdelete"), i18n("&Unset Cover Image"), DELETE );

        menu.setItemEnabled( SHOW, item->hasCover() );
        menu.setItemEnabled( DELETE, item->hasCover() );
    }

    switch( menu.exec(p) ) {
        case SHOW:
            viewCover( item->artist(), item->album(), this );
            break;

        #ifdef AMAZON_SUPPORT
        case FETCH:
            fetchSelectedCovers();
            break;
        #endif

        case CUSTOM:
        {
            setCustomSelectedCovers();
            break;
        }

        case DELETE:
            deleteSelectedCovers();
            break;

        default: ;
    }

    #undef item
}


void CoverManager::coverItemExecuted( QIconViewItem *item ) //SLOT
{
    #define item static_cast<CoverViewItem*>(item)

    if( !item ) return;

    item->setSelected( true );
    if ( item->hasCover() )
        viewCover( item->artist(), item->album(), this );
    else
        fetchSelectedCovers();

    #undef item
}


void CoverManager::slotSetFilter() //SLOT
{
    m_filter = m_searchEdit->text();

    m_coverView->selectAll( false);
    QIconViewItem *item = m_coverView->firstItem();
    while ( item ) {
        QIconViewItem *tmp = item->nextItem();
        m_coverView->takeItem( item );
        item = tmp;
    }

    m_coverView->setAutoArrange( false );
    for( QIconViewItem *item = m_coverItems.first(); item; item = m_coverItems.next() ) {
        CoverViewItem *coverItem = static_cast<CoverViewItem*>(item);
        if( coverItem->album().contains( m_filter, FALSE ) || coverItem->artist().contains( m_filter, FALSE ) )
            m_coverView->insertItem( item, m_coverView->lastItem() );
    }
    m_coverView->setAutoArrange( true );

    m_coverView->arrangeItemsInGrid();
    updateStatusBar();
}


void CoverManager::slotSetFilterTimeout() //SLOT
{
    if ( m_timer->isActive() ) m_timer->stop();
    m_timer->start( 180, true );
}


void CoverManager::changeView( int id  ) //SLOT
{
    if( m_currentView == id ) return;

    //clear the iconview without deleting items
    m_coverView->selectAll( false);
    QIconViewItem *item = m_coverView->firstItem();
    while ( item ) {
        QIconViewItem *tmp = item->nextItem();
        m_coverView->takeItem( item );
        item = tmp;
    }

    m_coverView->setAutoArrange(false );
    for( QIconViewItem *item = m_coverItems.first(); item; item = m_coverItems.next() ) {
        bool show = false;
        CoverViewItem *coverItem = static_cast<CoverViewItem*>(item);
        if( !m_filter.isEmpty() ) {
            if( !coverItem->album().contains( m_filter, FALSE ) && !coverItem->artist().contains( m_filter, FALSE ) )
                continue;
        }

        if( id == AllAlbums )    //show all albums
            show = true;
        else if( id == AlbumsWithCover && coverItem->hasCover() )    //show only albums with cover
            show = true;
        else if( id == AlbumsWithoutCover && !coverItem->hasCover() )   //show only albums without cover
            show = true;

        if( show )    m_coverView->insertItem( item, m_coverView->lastItem() );
    }
    m_coverView->setAutoArrange( true );

    m_viewMenu->setItemChecked( m_currentView, false );
    m_viewMenu->setItemChecked( id, true );

    m_coverView->arrangeItemsInGrid();
    m_currentView = id;
}

void CoverManager::changeLocale( int id ) //SLOT
{
    switch ( id )
    {
        case International:
            AmarokConfig::setAmazonLocale( "us" );
            break;
        case France:
            AmarokConfig::setAmazonLocale( "fr" );
            break;
        case Germany:
            AmarokConfig::setAmazonLocale( "de" );
            break;
        case Japan:
            AmarokConfig::setAmazonLocale( "jp" );
            break;
        case UK:
            AmarokConfig::setAmazonLocale( "uk" );
            break;
    }
    m_amazonLocaleMenu->setItemChecked( m_currentLocale, false );
    m_amazonLocaleMenu->setItemChecked( id, true );
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

    m_fetchCovers.clear();
    m_fetchCounter = 0;

    //delete all cover fetchers
    QObjectList* list = queryList( "CoverFetcher" );
    for( QObject *obj = list->first(); obj; obj = list->next()  )
        obj->deleteLater();

    delete list;

    m_fetchingCovers = 0;
    updateStatusBar();
}

// PRIVATE

void CoverManager::loadCover( const QString &artist, const QString &album )
{
    for( QIconViewItem *item = m_coverItems.first(); item; item = m_coverItems.next() )
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
    QPtrList<CoverViewItem> selected = selectedItems();
    CoverViewItem* first = selected.getFirst();

    QString artist_id; artist_id.setNum( CollectionDB::instance()->artistID( first->artist() ) );
    QString album_id; album_id.setNum( CollectionDB::instance()->albumID( first->album() ) );
    QStringList values = CollectionDB::instance()->albumTracks( artist_id, album_id );

    QString startPath = ":homedir";
    if ( !values.isEmpty() ) {
        KURL url;
        url.setPath( values.first() );
        startPath = url.directory();
    }
    KURL file = KFileDialog::getImageOpenURL( startPath, this, i18n( "Select Cover Image File" ) );
    if ( !file.isEmpty() ) {
        qApp->processEvents();    //it may takes a while so process pending events
        QString tmpFile;
        QImage image = CollectionDB::fetchImage(file, tmpFile);
        for ( CoverViewItem* item = selected.first(); item; item = selected.next() ) {
            CollectionDB::instance()->setAlbumImage( item->artist(), item->album(), image );
            item->loadCover();
        }
        KIO::NetAccess::removeTempFile( tmpFile );
    }
}

void CoverManager::fetchSelectedCovers()
{
    QPtrList<CoverViewItem> selected = selectedItems();
    for ( CoverViewItem* item = selected.first(); item; item = selected.next() )
        m_fetchCovers += item->artist() + " @@@ " + item->album();

    m_fetchingCovers += selected.count();

    if( !m_fetchCounter )    //loop isn't started yet
        fetchCoversLoop();

    updateStatusBar();
}


void CoverManager::deleteSelectedCovers()
{
    QPtrList<CoverViewItem> selected = selectedItems();

    int button = KMessageBox::warningContinueCancel( this,
                            i18n( "Are you sure you want to remove this cover from the Collection?",
                                  "Are you sure you want to delete these %n covers from the Collection?",
                                  selected.count() ),
                            QString::null,
                            i18n("&Remove") );

    if ( button == KMessageBox::Continue ) {
        for ( CoverViewItem* item = selected.first(); item; item = selected.next() ) {
            qApp->processEvents();
            if ( CollectionDB::instance()->removeAlbumImage( item->artist(), item->album() ) )    //delete selected cover
                  coverRemoved( item->artist(), item->album() );
        }
    }
}


QPtrList<CoverViewItem> CoverManager::selectedItems()
{
    QPtrList<CoverViewItem> selectedItems;
    for ( QIconViewItem* item = m_coverView->firstItem(); item; item = item->nextItem() )
        if ( item->isSelected() )
              selectedItems.append( static_cast<CoverViewItem*>(item) );

    return selectedItems;
}


void CoverManager::updateStatusBar()
{
    QString text;

    //cover fetching info
    if( m_fetchingCovers ) {
        //update the progress bar
        m_progress->setTotalSteps( m_fetchingCovers );
        m_progress->setProgress( m_coversFetched + m_coverErrors );
        if( m_progressBox->isHidden() )
            m_progressBox->show();

        //update the status text
        if( m_coversFetched + m_coverErrors >= m_progress->totalSteps() ) {
            //fetching finished
            text = i18n( "Finished." );
            if( m_coverErrors )
                text += i18n( " Cover not found", " <b>%n</b> covers not found", m_coverErrors );
            //reset counters
            m_fetchingCovers = 0;
            m_coversFetched = 0;
            m_coverErrors = 0;
            QTimer::singleShot( 2000, this, SLOT( updateStatusBar() ) );
        }

        if( m_fetchingCovers == 1 ) {
            QStringList values = QStringList::split( " @@@ ", m_fetchCovers[0], true );    //get artist and album name
            if ( values.count() >= 2 )
            {
                text = i18n( "Fetching cover for " );
                if ( !values[0].isEmpty() ) text += values[0] + " - ";
                text += values[1] + "...";
            }
        }
        else if( m_fetchingCovers ) {
            text = i18n( "Fetching 1 cover: ", "Fetching <b>%n</b> covers... : ", m_fetchingCovers );
            if( m_coversFetched )
                text += i18n( "1 fetched", "%n fetched", m_coversFetched );
            if( m_coverErrors ) {
                if( m_coversFetched ) text += " - ";
                text += i18n( "1 not found", "%n not found", m_coverErrors );
            }
            if( m_coversFetched + m_coverErrors == 0 )
                text += i18n( "Connecting..." );
        }
    }
    else {
        m_coversFetched = 0;
        m_coverErrors = 0;

        uint totalCounter = 0, missingCounter = 0;

        if( m_progressBox->isShown() )
            m_progressBox->hide();

        //album info
        for( QIconViewItem *item = m_coverView->firstItem(); item; item = item->nextItem() ) {
            totalCounter++;
            if( !((CoverViewItem*)item)->hasCover() )
                missingCounter++;    //counter for albums without cover
        }

        if( !m_filter.isEmpty() )
            text = i18n( "1 result for \"%1\"", "%n results for \"%1\"", totalCounter ).arg( m_filter );
        else if( m_artistView->selectedItem() ) {
            text = i18n( "1 album", "%n albums", totalCounter );
            if( m_artistView->selectedItem() != m_artistView->firstChild() ) //showing albums by an artist
                text += i18n( " by " ) + m_artistView->selectedItem()->text(0);
        }

        if( missingCounter )
            text += i18n(" - ( <b>%1</b> without cover )" ).arg( missingCounter );

        #ifdef AMAZON_SUPPORT
        m_fetchButton->setEnabled( missingCounter );
        #endif
    }

    m_statusLabel->setText( text );
}

//////////////////////////////////////////////////////////////////////
//    CLASS CoverView
/////////////////////////////////////////////////////////////////////

CoverView::CoverView( QWidget *parent, const char *name, WFlags f )
    : KIconView( parent, name, f )
{
    Debug::Block block( __PRETTY_FUNCTION__ );

    setArrangement( QIconView::LeftToRight );
    setResizeMode( QIconView::Adjust );
    setSelectionMode( QIconView::Extended );
    arrangeItemsInGrid();
    setAutoArrange( true );
    setItemsMovable( false );
}


QDragObject *CoverView::dragObject()
{
    CoverViewItem *item = static_cast<CoverViewItem*>( currentItem() );
    if( !item )
       return 0;

    const QString sql = "SELECT tags.url FROM tags, album WHERE album.name LIKE '%1' AND tags.album = album.id ORDER BY tags.track;";
    const QStringList values = CollectionDB::instance()->query( sql.arg( item->album() ) );

    KURL::List urls;
    for( QStringList::ConstIterator it = values.begin(), end = values.end(); it != end; ++it )
        urls += *it;

    QString imagePath = CollectionDB::instance()->albumImage( item->artist(), item->album(), 1 );
    KMultipleDrag *drag = new KMultipleDrag( this );
    drag->setPixmap( item->coverPixmap() );
    drag->addDragObject( new QIconDrag( this ) );
    drag->addDragObject( new QImageDrag( QImage( imagePath ) ) );
    drag->addDragObject( new KURLDrag( urls ) );

    return drag;
}

//////////////////////////////////////////////////////////////////////
//    CLASS CoverViewItem
/////////////////////////////////////////////////////////////////////

CoverViewItem::CoverViewItem( QIconView *parent, QIconViewItem *after, QString artist, QString album )
    : KIconViewItem( parent, after, album )
    , m_artist( artist )
    , m_album( album )
    , m_coverImagePath( CollectionDB::instance()->albumImage( m_artist, m_album, 0 ) )
    , m_coverPixmap( 0 )
{
    setDragEnabled( true );
    setDropEnabled( true );
    calcRect();
}

bool CoverViewItem::hasCover() const
{
    return !m_coverImagePath.endsWith( "nocover.png" ) && QFile::exists( m_coverImagePath );
}

void CoverViewItem::loadCover()
{
    m_coverImagePath = CollectionDB::instance()->albumImage( m_artist, m_album );
    m_coverPixmap = QPixmap( m_coverImagePath );  //create the scaled cover

    repaint();
}


void CoverViewItem::calcRect( const QString& )
{
    int thumbWidth = AmarokConfig::coverPreviewSize();

    QFontMetrics fm = iconView()->fontMetrics();
    QRect itemPixmapRect( 5, 1, thumbWidth, thumbWidth );
    QRect itemRect = rect();
    itemRect.setWidth( thumbWidth + 10 );
    itemRect.setHeight( thumbWidth + fm.lineSpacing() + 2 );
    QRect itemTextRect( 0, thumbWidth+2, itemRect.width(), fm.lineSpacing() );

    setPixmapRect( itemPixmapRect );
    setTextRect( itemTextRect );
    setItemRect( itemRect );
}


void CoverViewItem::paintItem(QPainter* p, const QColorGroup& cg)
{
    QRect itemRect = rect();

    p->save();
    p->translate( itemRect.x(), itemRect.y() );

    // draw the border
    p->setPen( cg.mid() );
    p->drawRect( 0, 0, itemRect.width(), pixmapRect().height()+2 );

    // draw the cover image
    if( !m_coverPixmap.isNull() )
        p->drawPixmap( pixmapRect().x() + (pixmapRect().width() - m_coverPixmap.width())/2,
            pixmapRect().y() + (pixmapRect().height() - m_coverPixmap.height())/2, m_coverPixmap );

    //justify the album name
    QString str = text();
    QFontMetrics fm = p->fontMetrics();
    int nameWidth = fm.width( str );
    if( nameWidth > textRect().width() )
    {
        QString nameJustify = "...";
        int i = 0;
        while ( fm.width( nameJustify + str[ i ] ) < textRect().width() )
            nameJustify += str[ i++ ];
        nameJustify.remove( 0, 3 );
        if ( nameJustify.isEmpty() )
            nameJustify = str.left( 1 );
        nameJustify += "...";
        str = nameJustify;
    }
    p->setPen( cg.text() );
    p->drawText( textRect(), Qt::AlignCenter, str );

    if( isSelected() ) {
       p->setPen( cg.highlight() );
       p->drawRect( pixmapRect() );
       p->drawRect( pixmapRect().left()+1, pixmapRect().top()+1, pixmapRect().width()-2, pixmapRect().height()-2);
       p->drawRect( pixmapRect().left()+2, pixmapRect().top()+2, pixmapRect().width()-4, pixmapRect().height()-4);
    }

    p->restore();
}


void CoverViewItem::dropped( QDropEvent *e, const QValueList<QIconDragItem> & )
{
    if( QImageDrag::canDecode( e ) ) {
       if( hasCover() ) {
           int button = KMessageBox::warningContinueCancel( iconView(),
                            i18n( "Are you sure you want to overwrite this cover?"),
                            QString::null,
                            i18n("&Overwrite Confirmation") );
           if( button == KMessageBox::Cancel )
               return;
       }

       QImage img;
       QImageDrag::decode( e, img );
       CollectionDB::instance()->setAlbumImage( artist(), album(), img );
       m_coverImagePath = CollectionDB::instance()->albumImage( m_artist, m_album, 0 );
       loadCover();
    }
}


void CoverViewItem::dragEntered()
{
    setSelected( true );
}


void CoverViewItem::dragLeft()
{
    setSelected( false );
}

#include "covermanager.moc"
