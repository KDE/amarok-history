/***************************************************************************
                          browserwin.cpp  -  description
                             -------------------
    begin                : Fre Nov 15 2002
    copyright            : (C) 2002 by Mark Kretschmann
    email                :
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "browserwin.h"
#include "browserwidget.h"
#include "playlistwidget.h"
#include "playlistitem.h"
#include "expandbutton.h"
#include "playerapp.h"

#include <vector>

#include <qwidget.h>
#include <qlayout.h>
#include <qmessagebox.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qtextstream.h>
#include <qsplitter.h>
#include <klistview.h>
#include <qcolor.h>
#include <qfont.h>
#include <qpopupmenu.h>
#include <qdir.h>
#include <qbitmap.h>
#include <qpixmap.h>
#include <qptrlist.h>
#include <qtooltip.h>

#include <kaction.h>
#include <kapplication.h>
#include <kcompletion.h>
#include <kdebug.h>
#include <kdirlister.h>
#include <kfileitem.h>
#include <kfilemetainfo.h>
#include <kglobalsettings.h>
#include <klineedit.h>
#include <kmimetype.h>
#include <krandomsequence.h>
#include <kstandarddirs.h>
#include <ktip.h>
#include <kurl.h>
#include <kurlcompletion.h>
#include <kurlrequesterdlg.h>

#include <arts/kplayobject.h>
#include <arts/kmedia2.h>
#include <arts/soundserver.h>

BrowserWin::BrowserWin( QWidget *parent, const char *name ) :
QWidget( parent, name, Qt::WPaintUnclipped )
{
    setName( "BrowserWin" );
    setIcon( QPixmap( locate( "icon", "locolor/32x32/apps/amarok.png" ) ) );
    setFont ( QFont( "Arial", 9 ) );

    setCaption( "amaroK Playlist" );
    setAcceptDrops( true );

    m_pActionCollection = new KActionCollection( this );
    m_pActionCollection->setAutoConnectShortcuts( true );

    initChildren();

    connect( m_pBrowserWidget, SIGNAL( doubleClicked( QListViewItem* ) ),
        this, SLOT( slotBrowserDoubleClicked( QListViewItem*) ) );

    connect( m_pBrowserWidget, SIGNAL( browserDrop() ),
        this, SLOT( slotBrowserDrop() ) );

    connect( m_pPlaylistWidget, SIGNAL( rightButtonPressed( QListViewItem*, const QPoint&, int ) ),
        this, SLOT( slotPlaylistRightButton( QListViewItem*, const QPoint&) ) );

    connect( m_pButtonSort, SIGNAL( clicked() ),
        this, SLOT( slotSortPlaylist() ) );

    connect( m_pButtonSortDesc, SIGNAL( clicked() ),
        this, SLOT( slotSortDescPlaylist() ) );

    connect( m_pButtonShuffle, SIGNAL( clicked() ),
        this, SLOT( slotShufflePlaylist() ) );
}



BrowserWin::~BrowserWin()
{
}



// INIT -------------------------------------------------------------

void BrowserWin::initChildren()
{
    m_pButtonAdd = new ExpandButton("Add Item", this );

    m_pButtonClear = new ExpandButton("Clear", this );

    m_pButtonSort = new ExpandButton("Sort", this );
    QToolTip::add( m_pButtonSort, "Keep button pressed for sub-menu" );
    m_pButtonSortDesc = new ExpandButton("Sort Descending", m_pButtonSort );
    m_pButtonShuffle = new ExpandButton("Shuffle", m_pButtonSort );
    m_pButtonSave = new ExpandButton("Save Playlist", m_pButtonSort );

    m_pButtonPlay = new ExpandButton("Play", this );
    QToolTip::add( m_pButtonPlay, "Keep button pressed for sub-menu" );
    m_pButtonPause = new ExpandButton("Pause", m_pButtonPlay );
    m_pButtonStop = new ExpandButton("Stop", m_pButtonPlay );
    m_pButtonNext = new ExpandButton("Next", m_pButtonPlay );
    m_pButtonPrev = new ExpandButton("Previous", m_pButtonPlay );

    m_pSplitter = new QSplitter( this );

    QWidget *pBrowserWidgetContainer = new QWidget( m_pSplitter );
    m_pBrowserWidget = new BrowserWidget( pBrowserWidgetContainer );
    m_pBrowserWidget->setAcceptDrops( true );
    m_pBrowserWidget->setSorting( -1 );
    m_pBrowserWidget->setSelectionMode( QListView::Extended );

    QWidget *pPlaylistWidgetContainer = new QWidget( m_pSplitter );
    m_pPlaylistWidget = new PlaylistWidget( pPlaylistWidgetContainer );
    m_pPlaylistWidget->setAcceptDrops( true );
    m_pPlaylistWidget->setSorting( -1 );
    m_pPlaylistWidget->setSelectionMode( QListView::Extended );

    m_pBrowserLineEdit = new KLineEdit( pBrowserWidgetContainer );
    QToolTip::add( m_pBrowserLineEdit, "Enter directory/URL" );
    m_pBrowserLineEdit->setPaletteBackgroundColor( pApp->m_bgColor );
    m_pBrowserLineEdit->setPaletteForegroundColor( pApp->m_fgColor );
    m_pBrowserLineEdit->setFont ( QFont( "Arial", 9 ) );
    KURLCompletion *compBrowser = new KURLCompletion();
    m_pBrowserLineEdit->setCompletionMode( KGlobalSettings::CompletionAuto );
    m_pBrowserLineEdit->setCompletionObject( compBrowser );
    connect( m_pBrowserLineEdit, SIGNAL( returnPressed( const QString& ) ), m_pBrowserWidget, SLOT( slotReturnPressed( const QString& ) ) );

    m_pPlaylistLineEdit = new KLineEdit( pPlaylistWidgetContainer );
    QToolTip::add( m_pPlaylistLineEdit, "Enter Filter String" );
    m_pPlaylistLineEdit->setPaletteBackgroundColor( pApp->m_bgColor );
    m_pPlaylistLineEdit->setPaletteForegroundColor( pApp->m_fgColor );
    m_pPlaylistLineEdit->setFont ( QFont( "Arial", 9 ) );
    connect( m_pPlaylistLineEdit, SIGNAL( textChanged( const QString& ) ), m_pPlaylistWidget, SLOT( slotTextChanged( const QString& ) ) );

    QBoxLayout *layBrowserWidget = new QVBoxLayout( pBrowserWidgetContainer );
    layBrowserWidget->addWidget( m_pBrowserLineEdit );
    layBrowserWidget->addWidget( m_pBrowserWidget );

    QBoxLayout *layPlaylistWidget = new QVBoxLayout( pPlaylistWidgetContainer );
    layPlaylistWidget->addWidget( m_pPlaylistLineEdit );
    layPlaylistWidget->addWidget( m_pPlaylistWidget );

    m_pSplitter->setResizeMode( pBrowserWidgetContainer, QSplitter::Stretch );
    m_pSplitter->setResizeMode( pPlaylistWidgetContainer, QSplitter::Stretch );

    QBoxLayout *layV = new QVBoxLayout(this);
    layV->addWidget( m_pSplitter );

    QBoxLayout *layH = new QHBoxLayout(layV);
    layH->addWidget( m_pButtonAdd );
    layH->addWidget( m_pButtonClear );
    layH->addWidget( m_pButtonSort );
    layH->addWidget( m_pButtonPlay );

    m_pBrowserWidget->addColumn( "Filebrowser" );
    m_pPlaylistWidget->addColumn( "Playlist" );
}



// METHODS -----------------------------------------------------------------

void BrowserWin::closeEvent( QCloseEvent *e )
{
    e->accept();

    emit signalHide();
}



// SLOTS --------------------------------------------------------------------

void BrowserWin::slotBrowserDoubleClicked( QListViewItem* pItem )
{
    if ( pItem )
    {
        PlaylistItem *pPlayItem = static_cast<PlaylistItem*>( pItem );
        KFileItem fileItem( KFileItem::Unknown, KFileItem::Unknown, pPlayItem->url() );

        if ( pPlayItem->text( 0 ) == ".." )
        {
            m_pBrowserWidget->readDir( m_pBrowserWidget->m_pDirLister->url().upURL() );
        }

        else if ( pPlayItem->isDir() )
        {
            m_pBrowserWidget->readDir( fileItem.url() );
        }

        else if ( isFileValid( fileItem.url() ) )
            m_pPlaylistWidget->addItem( (PlaylistItem*) 1, fileItem.url() );
    }
}



void BrowserWin::slotSortPlaylist()
{
    m_pPlaylistWidget->setSorting( 0, true );
    m_pPlaylistWidget->sort();
    m_pPlaylistWidget->setSorting( -1 );
}



void BrowserWin::slotSortDescPlaylist()
{
    m_pPlaylistWidget->setSorting( 0, false );
    m_pPlaylistWidget->sort();
    m_pPlaylistWidget->setSorting( -1 );
}



void BrowserWin::slotShufflePlaylist()
{
// not evil, but corrrrect :)
    QPtrList<QListViewItem> list;

    while ( m_pPlaylistWidget->childCount() )
    {
        list.append( m_pPlaylistWidget->firstChild() );
        m_pPlaylistWidget->takeItem( m_pPlaylistWidget->firstChild() );
    }

                                                  // initalize with seed
    KRandomSequence seq( static_cast<long>( KApplication::random() ) );
    seq.randomize( &list );

    for ( unsigned int i = 0; i < list.count(); i++ )
    {
        m_pPlaylistWidget->insertItem( list.at( i ) );
    }
}



void BrowserWin::slotBrowserDrop()
{
    QListViewItem *item, *item1;
    item = m_pPlaylistWidget->firstChild();

    while ( item != NULL )
    {
        item1 = item;
        item = item->nextSibling();

        if ( item1->isSelected()    )
            delete item1;
    }
}



bool BrowserWin::isFileValid( const KURL &url )
{
    KFileItem fileItem( KFileItem::Unknown, KFileItem::Unknown, url );
    KMimeType::Ptr mimeTypePtr = fileItem.determineMimeType();

    Arts::TraderQuery query;
    query.supports( "Interface", "Arts::PlayObject" );
    query.supports( "MimeType", mimeTypePtr->name().latin1() );
    std::vector<Arts::TraderOffer> *offers = query.query();

    if ( offers->empty() )
    {
        delete offers;
        return false;
    }

    delete offers;
    return true;
}



void BrowserWin::slotPlaylistRightButton( QListViewItem */*pItem*/, const QPoint &rPoint )
{
    QPopupMenu popup( this );
    int item1 = popup.insertItem( "Show File Info", this, SLOT( slotShowInfo() ) );

// only enable when file is selected
    if ( !m_pPlaylistWidget->currentItem() )
        popup.setItemEnabled( item1, false );

    /*int item2 = */popup.insertItem( "Play Track", this, SLOT( slotMenuPlay() ) );
    /*int item3 = */popup.insertItem( "Remove Selected", this, SLOT( slotBrowserDrop() ) );

    popup.exec( rPoint );
}



void BrowserWin::slotShowInfo()
{
    PlaylistItem *pItem = static_cast<PlaylistItem*>( m_pPlaylistWidget->currentItem() );

    QMessageBox *box = new QMessageBox( "Track Information", 0,
        QMessageBox::Information, QMessageBox::Ok, QMessageBox::NoButton,
        QMessageBox::NoButton, 0, "Track Information", true,
        Qt::WDestructiveClose | Qt::WStyle_DialogBorder );

    QString str( "<html><body><table border=""1"">" );

    if ( pItem->url().protocol() == "file" )
    {
        KFileMetaInfo metaInfo( pItem->url().path(), QString::null, KFileMetaInfo::Everything );
//    KFileItem fileItem( KFileItem::Unknown, KFileItem::Unknown, pItem->url() );
//    KFileMetaInfo metaInfo = fileItem.metaInfo();
        if ( metaInfo.isValid() && !metaInfo.isEmpty() )
        {
            str += "<tr><td>Title   </td><td>" + metaInfo.item( "Title" ).string() + "</td></tr>";
            str += "<tr><td>Artist  </td><td>" + metaInfo.item( "Artist" ).string() + "</td></tr>";
            str += "<tr><td>Album   </td><td>" + metaInfo.item( "Album" ).string() + "</td></tr>";
            str += "<tr><td>Genre   </td><td>" + metaInfo.item( "Genre" ).string() + "</td></tr>";
            str += "<tr><td>Date    </td><td>" + metaInfo.item( "Date" ).string() + "</td></tr>";
            str += "<tr><td>Comment </td><td>" + metaInfo.item( "Comment" ).string() + "</td></tr>";
            str += "<tr><td>Length  </td><td>" + metaInfo.item( "Length" ).string() + "</td></tr>";
            str += "<tr><td>Bitrate </td><td>" + metaInfo.item( "Bitrate" ).string() + "</td></tr>";
            str += "<tr><td>Samplerate  </td><td>" + metaInfo.item( "Sample Rate" ).string() + "</td></tr>";
        }
        else
        {
            str += "<tr><td>Error    </td><td>No metaInfo found    </td></tr>";
        }
    }
    else
    {
        str += "<tr><td>Stream   </td><td>" + pItem->url().url() + "</td></tr>";
        str += "<tr><td>Title    </td><td>" + pItem->text( 0 ) + "</td></tr>";
    }

    str.append( "</table></body></html>" );
    box->setText( str );
    box->setTextFormat( Qt::RichText );
    box->show();
}



void BrowserWin::slotMenuPlay()
{
    m_pPlaylistWidget->setCurrentTrack( m_pPlaylistWidget->currentItem() );
    pApp->slotPlay();
}



void BrowserWin::slotKeyUp()
{
    KListView *pListView = 0L;

    if ( m_pPlaylistLineEdit->hasFocus() )
        pListView = m_pPlaylistWidget;

    if ( m_pBrowserLineEdit->hasFocus() )
        pListView = m_pBrowserWidget;

    QListViewItem *item = pListView->currentItem();

    if ( item->itemAbove() )
    {
        item = item->itemAbove();

        pListView->setSelected( pListView->currentItem(), false );
        pListView->ensureItemVisible( item );
        pListView->setSelected( item, true );
        pListView->setCurrentItem( item );
    }
}



void BrowserWin::slotKeyDown()
{
    KListView *pListView = 0L;

    if ( m_pPlaylistLineEdit->hasFocus() )
        pListView = m_pPlaylistWidget;

    if ( m_pBrowserLineEdit->hasFocus() )
        pListView = m_pBrowserWidget;

    QListViewItem *item = pListView->currentItem();

    if ( item->itemBelow() )
    {
        item = item->itemBelow();

        pListView->setSelected( pListView->currentItem(), false );
        pListView->ensureItemVisible( item );
        pListView->setSelected( item, true );
        pListView->setCurrentItem( item );
    }
}



void BrowserWin::slotKeyPageUp()
{
    KListView *pListView = 0L;

    if ( m_pPlaylistLineEdit->hasFocus() )
        pListView = m_pPlaylistWidget;

    if ( m_pBrowserLineEdit->hasFocus() )
        pListView = m_pBrowserWidget;

    QListViewItem *item = pListView->currentItem();

    for ( int i = 1; i < pListView->visibleHeight() / item->height(); i++ )
    {
        if ( item->itemAbove() == NULL )
            break;
        item = item->itemAbove();
    }

    if ( item )
    {
        pListView->setSelected( pListView->currentItem(), false );
        pListView->ensureItemVisible( item );
        pListView->setSelected( item, true );
        pListView->setCurrentItem( item );
    }
}



void BrowserWin::slotKeyPageDown()
{
    KListView *pListView = 0L;

    if ( m_pPlaylistLineEdit->hasFocus() )
        pListView = m_pPlaylistWidget;

    if ( m_pBrowserLineEdit->hasFocus() )
        pListView = m_pBrowserWidget;

    QListViewItem *item = pListView->currentItem();

    for ( int i = 1; i < pListView->visibleHeight() / item->height(); i++ )
    {
        if ( item->itemBelow() == NULL )
            break;
        item = item->itemBelow();
    }

    if ( item )
    {
        pListView->setSelected( pListView->currentItem(), false );
        pListView->ensureItemVisible( item );
        pListView->setSelected( item, true );
        pListView->setCurrentItem( item );
    }
}



void BrowserWin::slotKeyEnter()
{
    if ( m_pPlaylistLineEdit->hasFocus() )
    {
        if ( m_pPlaylistWidget->currentItem() )
        {
            pApp->slotStop();
            m_pPlaylistWidget->setCurrentTrack( m_pPlaylistWidget->currentItem() );
            pApp->slotPlay();
        }
    }

    if ( m_pBrowserLineEdit->hasFocus() )
    {
        if ( m_pBrowserWidget->currentItem() )
        {
            slotBrowserDoubleClicked( m_pBrowserWidget->currentItem() );
        }
    }
}



void BrowserWin::slotKeyDelete()
{
    if ( m_pPlaylistLineEdit->hasFocus() )
    {
        slotBrowserDrop();
    }

    if ( m_pBrowserLineEdit->hasFocus() )
    {
    }
}

#include "browserwin.moc"
