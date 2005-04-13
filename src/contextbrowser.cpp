// (c) 2004 Christian Muehlhaeuser <chris@chris.de>
// See COPYING file for licensing information

#define DEBUG_PREFIX "ContextBrowser"

#include "amarok.h"
#include "amarokconfig.h"
#include "collectionbrowser.h" //FIXME for setupDirs()
#include "collectiondb.h"
#include "colorgenerator.h"
#include "config.h"        //for AMAZON_SUPPORT
#include "contextbrowser.h"
#include "coverfetcher.h"
#include "covermanager.h"
#include "debug.h"
#include "enginecontroller.h"
#include "metabundle.h"
#include "playlist.h"      //appendMedia()
#include "qstringx.h"
#include "statusbar.h"
#include "tagdialog.h"
#include "threadweaver.h"

#include <qdatetime.h>
#include <qfile.h> // External CSS opening
#include <qimage.h>
#include <qregexp.h>
#include <qtextstream.h>  // External CSS reading

#include <kapplication.h> //kapp
#include <kcalendarsystem.h>  // for verboseTimeSince()
#include <kfiledialog.h>
#include <kglobal.h>
#include <khtml_part.h>
#include <khtmlview.h>
#include <kiconloader.h>
#include <kimageeffect.h> // gradient background image
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpopupmenu.h>
#include <kstandarddirs.h> //locate file
#include <ktempfile.h>
#include <kurl.h>

#define escapeHTML(s)     QString(s).replace( "&", "&amp;" ).replace( "<", "&lt;" ).replace( ">", "&gt;" )
#define escapeHTMLAttr(s) QString(s).replace( "%", "%25" ).replace( "'", "%27" ).replace( "#", "%23" ).replace( "?", "%3F" )


using amaroK::QStringx;

/**
 * Function that must be used when separating contextBrowser escaped urls
 */
static inline
void albumArtistTrackFromUrl( QString url, QString &artist, QString &album, QString &track )
{
    //KHTML removes the trailing space!
    if ( url.endsWith( " @@@" ) )
        url += ' ';

    const QStringList list = QStringList::split( " @@@ ", url, true );

    Q_ASSERT( !list.isEmpty() );

    artist = list[0];
    album  = list[1];
    track = list[2];
}


ContextBrowser *ContextBrowser::s_instance = 0;


ContextBrowser::ContextBrowser( const char *name )
        : QTabWidget( 0, name )
        , EngineObserver( EngineController::instance() )
        , m_dirtyHomePage( true )
        , m_dirtyCurrentTrackPage( true )
        , m_dirtyLyricsPage( true )
        , m_emptyDB( CollectionDB::instance()->isEmpty() )
        , m_bgGradientImage( 0 )
        , m_headerGradientImage( 0 )
        , m_shadowGradientImage( 0 )
        , m_suggestionsOpen( true )
        , m_favouritesOpen( true )
{
    s_instance = this;

    m_homePage = new KHTMLPart( this, "home_page" );
    m_homePage->setDNDEnabled( true );
    m_currentTrackPage = new KHTMLPart( this, "current_track_page" );
    m_currentTrackPage->setJScriptEnabled( true );
    m_currentTrackPage->setDNDEnabled( true );
    m_lyricsPage = new KHTMLPart( this, "lyrics_page" );
    m_lyricsPage->setJavaEnabled( false );
    m_lyricsPage->setPluginsEnabled( false );
    m_lyricsPage->setDNDEnabled( true );

    //aesthetics - no double frame
//     m_homePage->view()->setFrameStyle( QFrame::NoFrame );
//     m_currentTrackPage->view()->setFrameStyle( QFrame::NoFrame );
//     m_lyricsPage->view()->setFrameStyle( QFrame::NoFrame );

    addTab( m_homePage->view(),         SmallIconSet( "gohome" ),   i18n( "Home" ) );
    addTab( m_currentTrackPage->view(), SmallIconSet( "today" ),    i18n( "Current" ) );
    addTab( m_lyricsPage->view(),       SmallIconSet( "document" ), i18n( "Lyrics" ) );

    setTabEnabled( m_currentTrackPage->view(), false );
    setTabEnabled( m_lyricsPage->view(), false );


    connect( this, SIGNAL( currentChanged( QWidget* ) ), SLOT( tabChanged( QWidget* ) ) );

    connect( m_homePage->browserExtension(), SIGNAL( openURLRequest( const KURL &, const KParts::URLArgs & ) ),
             this,                             SLOT( openURLRequest( const KURL & ) ) );
    connect( m_homePage,                     SIGNAL( popupMenu( const QString&, const QPoint& ) ),
             this,                             SLOT( slotContextMenu( const QString&, const QPoint& ) ) );
    connect( m_currentTrackPage->browserExtension(), SIGNAL( openURLRequest( const KURL &, const KParts::URLArgs & ) ),
             this,                                     SLOT( openURLRequest( const KURL & ) ) );
    connect( m_currentTrackPage,                     SIGNAL( popupMenu( const QString&, const QPoint& ) ),
             this,                                     SLOT( slotContextMenu( const QString&, const QPoint& ) ) );
    connect( m_lyricsPage->browserExtension(), SIGNAL( openURLRequest( const KURL &, const KParts::URLArgs & ) ),
             this,                               SLOT( openURLRequest( const KURL & ) ) );
    connect( m_lyricsPage,                     SIGNAL( popupMenu( const QString&, const QPoint& ) ),
             this,                               SLOT( slotContextMenu( const QString&, const QPoint& ) ) );

    connect( CollectionDB::instance(), SIGNAL( scanStarted() ), SLOT( collectionScanStarted() ) );
    connect( CollectionDB::instance(), SIGNAL( scanDone( bool ) ), SLOT( collectionScanDone() ) );
    connect( CollectionDB::instance(), SIGNAL( databaseEngineChanged() ), SLOT( renderView() ) );
    connect( CollectionDB::instance(), SIGNAL( coverFetched( const QString&, const QString& ) ),
             this,                       SLOT( coverFetched( const QString&, const QString& ) ) );
    connect( CollectionDB::instance(), SIGNAL( coverRemoved( const QString&, const QString& ) ),
             this,                       SLOT( coverRemoved( const QString&, const QString& ) ) );
    connect( CollectionDB::instance(), SIGNAL( similarArtistsFetched( const QString& ) ),
             this,                       SLOT( similarArtistsFetched( const QString& ) ) );

    //the stylesheet will be set up and home will be shown later due to engine signals and doodaa
    //if we call it here setStyleSheet is called 3 times during startup!!
}


ContextBrowser::~ContextBrowser()
{
    delete m_bgGradientImage;
    delete m_headerGradientImage;
    delete m_shadowGradientImage;
}


//////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////////////////////

void ContextBrowser::setFont( const QFont &newFont )
{
    QWidget::setFont( newFont );
    setStyleSheet();
}


//////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC SLOTS
//////////////////////////////////////////////////////////////////////////////////////////

void ContextBrowser::openURLRequest( const KURL &url )
{
    QString artist, album, track;
    albumArtistTrackFromUrl( url.path(), artist, album, track );

    if ( url.protocol() == "album" )
    {
        QueryBuilder qb;
        qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
        qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valArtistID, artist );
        qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valAlbumID, album );
        qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
        QStringList values = qb.run();

        KURL::List urls;
        KURL url;

        for( QStringList::ConstIterator it = values.begin(), end = values.end(); it != end; ++it ) {
            url.setPath( *it );
            urls.append( url );
        }

        Playlist::instance()->insertMedia( urls, Playlist::Unique );

        return;
    }

    if ( url.protocol() == "compilation" )
    {
        QueryBuilder qb;
        qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
        qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valAlbumID, url.path() );
        qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
        qb.setOptions( QueryBuilder::optOnlyCompilations );
        QStringList values = qb.run();

        KURL::List urls;
        KURL url;

        for( QStringList::ConstIterator it = values.begin(), end = values.end(); it != end; ++it ) {
            url.setPath( *it );
            urls.append( url );
        }

        Playlist::instance()->insertMedia( urls, Playlist::Unique );

        return;
    }

    if ( url.protocol() == "file" )
        Playlist::instance()->insertMedia( url, Playlist::DirectPlay | Playlist::Unique );

    if ( url.protocol() == "show" )
    {
        if ( url.path() == "home" )
            showHome();
        else if ( url.path() == "context" || url.path() == "stream" )
        {
            // NOTE: not sure if rebuild is needed, just to be safe
            m_dirtyCurrentTrackPage = true;
            showCurrentTrack();
        }
        else if ( url.path().contains( "suggestLyric-" ) )
        {
            m_lyrics = QString::null;
            QString hash = url.path().mid( url.path().find( QString( "-" ) ) +1 );
            m_dirtyLyricsPage = true;
            showLyrics( hash );
        }
        else if ( url.path() == "lyrics" )
            showLyrics();
        else if ( url.path() == "collectionSetup" )
        {
            //TODO if we do move the configuration to the main configdialog change this,
            //     otherwise we need a better solution
            QObject *o = parent()->child( "CollectionBrowser" );
            if ( o )
                static_cast<CollectionBrowser*>( o )->setupDirs();
        }
    }

    // When left-clicking on cover image, open browser with amazon site
    if ( url.protocol() == "fetchcover" )
    {
        if ( CollectionDB::instance()->findImageByArtistAlbum (artist, album, 0 )
           == CollectionDB::instance()->notAvailCover( 0 ) ) {
            CollectionDB::instance()->fetchCover( this, artist, album, false );
            return;
        }

        QImage img( CollectionDB::instance()->albumImage( artist, album, 0 ) );
        const QString amazonUrl = img.text( "amazon-url" );
        debug() << "Embedded amazon url in cover image: " << amazonUrl << endl;

        if ( amazonUrl.isEmpty() )
            KMessageBox::information( this, i18n( "<p>There is no product information available for this image.<p>Right-click on image for menu." ) );
        else
            kapp->invokeBrowser( amazonUrl );
    }

    /* open konqueror with musicbrainz search result for artist-album */
    if ( url.protocol() == "musicbrainz" )
    {
        const QString url = "http://www.musicbrainz.org/taglookup.html?artist=%1&album=%2&track=%3";
        kapp->invokeBrowser( url.arg( KURL::encode_string_no_slash( artist, 106 /*utf-8*/ ),
	    KURL::encode_string_no_slash( album, 106 /*utf-8*/ ),
	    KURL::encode_string_no_slash( track, 106 /*utf-8*/ ) ) );
    }

    if ( url.protocol() == "externalurl" )
    {
        kapp->invokeBrowser( url.url().replace("externalurl:", "http:") );
    }

    if ( url.protocol() == "togglebox" )
    {
        if ( url.path() == "ss" ) m_suggestionsOpen ^= true;
        if ( url.path() == "ft" ) m_favouritesOpen ^= true;
    }
}


void ContextBrowser::collectionScanStarted()
{
    if( m_emptyDB && !AmarokConfig::collectionFolders().isEmpty() )
       showScanning();
}


void ContextBrowser::collectionScanDone()
{
    if ( CollectionDB::instance()->isEmpty() )
    {
        showIntroduction();
        m_emptyDB = true;
    } else if ( m_emptyDB )
    {
        showHome();
        m_emptyDB = false;
    }
}


void ContextBrowser::renderView()
{
    m_dirtyHomePage = true;
    m_dirtyCurrentTrackPage = true;
    m_dirtyLyricsPage = true;

    // TODO: Show CurrentTrack or Lyric tab if they were selected
    if ( CollectionDB::instance()->isEmpty() )
    {
        showIntroduction();
    }
    else
    {
        showHome();
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
// PROTECTED
//////////////////////////////////////////////////////////////////////////////////////////

void ContextBrowser::engineNewMetaData( const MetaBundle& bundle, bool /*trackChanged*/ )
{
    bool newMetaData = false;
    m_dirtyHomePage = true;
    m_dirtyCurrentTrackPage = true;
    m_dirtyLyricsPage = true;

    // Prepend stream metadata history item to list
    if ( !m_metadataHistory.first().contains( bundle.prettyTitle() ) )
    {
        newMetaData = true;
        const QString timeString = KGlobal::locale()->formatTime( QTime::currentTime() );
        m_metadataHistory.prepend( QString( "<td valign='top'>" + timeString + "&nbsp;</td><td align='left'>" + escapeHTML( bundle.prettyTitle() ) + "</td>" ) );
    }

    if ( currentPage() != m_currentTrackPage->view() || bundle.url() != m_currentURL || newMetaData )
        showCurrentTrack();
    else if ( CollectionDB::instance()->isEmpty() || !CollectionDB::instance()->isValid() )
        showIntroduction();
}


void ContextBrowser::engineStateChanged( Engine::State state )
{
    DEBUG_BLOCK

    m_dirtyHomePage = true;
    m_dirtyCurrentTrackPage = true;
    m_dirtyLyricsPage = true;

    switch( state )
    {
        case Engine::Empty:
            m_metadataHistory.clear();
            showHome();
            blockSignals( true );
            setTabEnabled( m_currentTrackPage->view(), false );
            setTabEnabled( m_lyricsPage->view(), false );
            blockSignals( false );
            break;
        case Engine::Playing:
            m_metadataHistory.clear();
            blockSignals( true );
            setTabEnabled( m_currentTrackPage->view(), true );
            setTabEnabled( m_lyricsPage->view(), true );
            blockSignals( false );
            break;
        default:
            ;
    }
}


void ContextBrowser::saveHtmlData()
{
    QFile exportedDocument( amaroK::saveLocation() + "contextbrowser.html" );
    exportedDocument.open(IO_WriteOnly);
    QTextStream stream( &exportedDocument );
    stream.setEncoding( QTextStream::UnicodeUTF8 );
    stream << m_HTMLSource // the pure html data..
        .replace("<html>",QString("<html><head><style type=\"text/css\">%1</style></head>").arg(m_styleSheet) ); // and the stylesheet code
    exportedDocument.close();
}


void ContextBrowser::paletteChange( const QPalette& pal )
{
    QTabWidget::paletteChange( pal );
    setStyleSheet();
}


//////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE SLOTS
//////////////////////////////////////////////////////////////////////////////////////////

void ContextBrowser::tabChanged( QWidget *page )
{
    setFocusProxy( page ); //so focus is given to a sensible widget when the tab is opened
    if ( m_dirtyHomePage && ( page == m_homePage->view() ) )
        showHome();
    else if ( m_dirtyCurrentTrackPage && ( page == m_currentTrackPage->view() ) )
        showCurrentTrack();
    else if ( m_dirtyLyricsPage && ( page == m_lyricsPage->view() ) )
        showLyrics();
}


void ContextBrowser::slotContextMenu( const QString& urlString, const QPoint& point )
{
    enum { SHOW, FETCH, CUSTOM, DELETE, APPEND, ASNEXT, MAKE, INFO, MANAGER, TITLE };

    if( urlString.isEmpty() || urlString.startsWith( "musicbrainz" ) || urlString.startsWith( "lyricspage" ) )
        return;

    KURL url( urlString );
    if( url.path().contains( "lyric", FALSE ) )
        return;

    KPopupMenu menu;
    KURL::List urls( url );
    QString artist, album, track; // track unused here
    albumArtistTrackFromUrl( url.path(), artist, album, track );

    if ( url.protocol() == "fetchcover" )
    {
        menu.insertTitle( i18n( "Cover Image" ) );

        menu.insertItem( SmallIconSet( "viewmag" ), i18n( "&Show Fullsize" ), SHOW );
        menu.insertItem( SmallIconSet( "www" ), i18n( "&Fetch From amazon.%1" ).arg(CoverManager::amazonTld()), FETCH );
        menu.insertItem( SmallIconSet( "folder_image" ), i18n( "Set &Custom Cover" ), CUSTOM );
        menu.insertSeparator();

        menu.insertItem( SmallIconSet( "editdelete" ), i18n("&Unset Cover"), DELETE );
        menu.insertSeparator();
        menu.insertItem( QPixmap( locate( "data", "amarok/images/covermanager.png" ) ), i18n( "Cover Manager" ), MANAGER );

        #ifndef AMAZON_SUPPORT
        menu.setItemEnabled( FETCH, false );
        #endif
        menu.setItemEnabled( SHOW, !CollectionDB::instance()->albumImage( artist, album, 0 ).contains( "nocover" ) );
    }
    else {
        //TODO it would be handy and more usable to have this menu under the cover one too

        menu.insertTitle( i18n("Track"), TITLE );

        menu.insertItem( SmallIconSet( "1downarrow" ), i18n( "&Append to Playlist" ), APPEND );
        menu.insertItem( SmallIconSet( "2rightarrow" ), i18n( "&Queue After Current Track" ), ASNEXT );
        menu.insertItem( SmallIconSet( "player_playlist_2" ), i18n( "&Make Playlist" ), MAKE );

        menu.insertSeparator();
        menu.insertItem( SmallIconSet( "info" ), i18n( "&View/Edit Meta Information..." ), INFO );

        if ( url.protocol() == "album" )
        {
            QueryBuilder qb;
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
            qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valArtistID, artist );
            qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valAlbumID, album );
            qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
            QStringList values = qb.run();

            urls.clear(); //remove urlString
            KURL url;
            for( QStringList::ConstIterator it = values.begin(); it != values.end(); ++it )
            {
                url.setPath( *it );
                urls.append( url );
            }

            menu.changeTitle( TITLE, i18n("Album") );
        }
    }

    //Not all these are used in the menu, it depends on the context
    switch( menu.exec( point ) )
    {
    case SHOW:
        CoverManager::viewCover( artist, album, this );
        break;

    case DELETE:
    {
        const int button = KMessageBox::warningContinueCancel( this,
            i18n( "Are you sure you want to remove this cover from the Collection?" ),
            QString::null,
            i18n("&Remove") );

        if ( button == KMessageBox::Continue )
        {
            CollectionDB::instance()->removeAlbumImage( artist, album );
            showCurrentTrack();
        }
        break;
    }

    case ASNEXT:
        Playlist::instance()->insertMedia( urls, Playlist::Queue );
        break;

    case INFO:
    {
        if ( urls.count() > 1 ) {
          TagDialog* dialog = new TagDialog( urls, instance() );
          dialog->show();
        } else if ( !urls.isEmpty() ) {
            TagDialog* dialog = new TagDialog( urls.first() );
            dialog->show();
        }
        break;
    }
    case MAKE:
        Playlist::instance()->clear();

        //FALL_THROUGH

    case APPEND:
        Playlist::instance()->insertMedia( urls, Playlist::Unique );
        break;

    case FETCH:
    #ifdef AMAZON_SUPPORT
        CollectionDB::instance()->fetchCover( this, artist, album, false );
        break;
    #endif

    case CUSTOM:
    {
        QString artist_id; artist_id.setNum( CollectionDB::instance()->artistID( artist ) );
        QString album_id; album_id.setNum( CollectionDB::instance()->albumID( album ) );
        QStringList values = CollectionDB::instance()->albumTracks( artist_id, album_id );
        QString startPath = ":homedir";

        if ( !values.isEmpty() ) {
            KURL url;
            url.setPath( values.first() );
            startPath = url.directory();
        }

        KURL file = KFileDialog::getImageOpenURL( startPath, this, i18n("Select Cover Image File") );
        if ( !file.isEmpty() ) {
            CollectionDB::instance()->setAlbumImage( artist, album, file );
            m_dirtyCurrentTrackPage = true;
            showCurrentTrack();
        }
        break;
    }

    case MANAGER:
        CoverManager::showOnce( album );
        break;
    }
}


static QString
verboseTimeSince( const QDateTime &datetime )
{
    const QDateTime now = QDateTime::currentDateTime();
    const int datediff = datetime.daysTo( now );

    if( datediff >= 6*7 /*six weeks*/ ) {  // return absolute month/year
        const KCalendarSystem *cal = KGlobal::locale()->calendar();
        const QDate date = datetime.date();
        return i18n( "monthname year", "%1 %2" ).arg( cal->monthName(date), cal->yearString(date, false) );
    }

    //TODO "last week" = maybe within 7 days, but prolly before last sunday

    if( datediff >= 7 )  // return difference in weeks
        return i18n( "One week ago", "%n weeks ago", (datediff+3)/7 );

    const int timediff = datetime.secsTo( now );

    if( timediff >= 24*60*60 /*24 hours*/ )  // return difference in days
        return datediff == 1 ?
                i18n( "Yesterday" ) :
                i18n( "One day ago", "%n days ago", (timediff+12*60*60)/(24*60*60) );

    if( timediff >= 90*60 /*90 minutes*/ )  // return difference in hours
        return i18n( "One hour ago", "%n hours ago", (timediff+30*60)/(60*60) );

    //TODO are we too specific here? Be more fuzzy? ie, use units of 5 minutes, or "Recently"

    if( timediff >= 0 )  // return difference in minutes
        return timediff/60 ?
                i18n( "One minute ago", "%n minutes ago", (timediff+30)/60 ) :
                i18n( "Within the last minute" );

    return i18n( "The future" );
}


void ContextBrowser::showHome() //SLOT
{
    DEBUG_BLOCK

    if ( currentPage() != m_homePage->view() )
    {
        blockSignals( true );
        showPage( m_homePage->view() );
        blockSignals( false );
    }

    if ( CollectionDB::instance()->isEmpty() || !CollectionDB::instance()->isValid() ) {
        //TODO show scanning message if scanning, not the introduction
        showIntroduction();
        return;
    }

    // Do we have to rebuild the page?
    if ( !m_dirtyHomePage ) return;

    QueryBuilder qb;
    qb.clear();
    qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
    qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
    qb.addReturnValue( QueryBuilder::tabStats, QueryBuilder::valScore );
    qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );
    qb.addReturnValue( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabStats, QueryBuilder::valPercentage, true );
    qb.setLimit( 0, 10 );
    QStringList fave = qb.run();

    qb.clear();
    qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
    qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
    qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );
    qb.addReturnValue( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valCreateDate, true );
    qb.setLimit( 0, 10 );
    QStringList recent = qb.run();

    qb.clear();
    qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
    qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
    qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );
    qb.addReturnValue( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.addReturnValue( QueryBuilder::tabStats, QueryBuilder::valAccessDate );
    qb.sortBy( QueryBuilder::tabStats, QueryBuilder::valAccessDate );
    qb.setLimit( 0, 10 );
    QStringList least = qb.run();

    m_homePage->begin();
    m_HTMLSource="";
    m_homePage->setUserStyleSheet( m_styleSheet );

    // <Favorite Tracks Information>
    m_HTMLSource.append(
            "<html>"
            "<div id='favorites_box' class='box'>"
                "<div id='favorites_box-header' class='box-header'>"
                    "<span id='favorites_box-header-title' class='box-header-title'>"
                    + i18n( "Your Favorite Tracks" ) +
                    "</span>"
                "</div>" );

    if ( fave.count() == 0 )
    {
        m_HTMLSource.append(
                "<div class='info'><p>" +
                i18n( "A list of your favorite tracks will appear here, once you have played a few of your songs." ) +
                "</p></div>" );
    }
    else
    {
        m_HTMLSource.append( "<div id='favorites_box-body' class='box-body'>"
                             "<table border='0' cellspacing='0' cellpadding='0' width='100%' class='song'>" );

        for( uint i = 0; i < fave.count(); i = i + 5 )
        {
            m_HTMLSource.append(    "<tr class='" + QString( (i % 10) ? "box-row-alt" : "box-row" ) + "'>"
                                    "<td width='30' align='center' class='song-place'>" + QString::number( ( i / 5 ) + 1 ) + "</td>"
                                    "<td>"
                                        "<a href=\"file:" + fave[i + 1].replace( '"', QCString( "%22" ) ) + "\">"
                                            "<span class='song-title'>" + fave[i] + "</span><br /> "
                                            "<span class='song-artist'>" + fave[i + 3] + "</span>" );

            if ( !fave[i + 4].isEmpty() )
                m_HTMLSource.append(        "<span class='song-separator'> - </span>"
                                            "<span class='song-album'>"+ fave[i + 4] +"</span>" );

            m_HTMLSource.append(        "</a>"
                                    "</td>"
                                    "<td class='sbtext' width='1'>" + fave[i + 2] + "</td>"
                                    "<td width='1' title='" + i18n( "Score" ) + "'>"
                                        "<div class='sbouter'>"
                                            "<div class='sbinner' style='width: " + QString::number( fave[i + 2].toInt() / 2 ) + "px;'></div>"
                                        "</div>"
                                    "</td>"
                                    "<td width='1'></td>"
                                "</tr>" );
        }

        m_HTMLSource.append( "</table>"
                             "</div>" );
    }

    m_HTMLSource.append(
            "</div>"

    // </Favorite Tracks Information>

    // <Recent Tracks Information>
            "<div id='newest_box' class='box'>"
                "<div id='newest_box-header' class='box-header'>"
                    "<span id='newest_box-header-title' class='box-header-title'>"
                    + i18n( "Your Newest Tracks" ) +
                    "</span>"
                "</div>"
                "<div id='newest_box-body' class='box-body'>" );

    for( uint i = 0; i < recent.count(); i = i + 4 )
    {
        m_HTMLSource.append(
                 "<div class='" + QString( (i % 8) ? "box-row-alt" : "box-row" ) + "'>"
                    "<div class='song'>"
                        "<a href=\"file:" + recent[i + 1].replace( '"', QCString( "%22" ) ) + "\">"
                        "<span class='song-title'>" + recent[i] + "</span><br />"
                        "<span class='song-artist'>" + recent[i + 2] + "</span>"
                    );

        if ( !recent[i + 3].isEmpty() )
            m_HTMLSource.append(
                        "<span class='song-separator'> - </span>"
                        "<span class='song-album'>" + recent[i + 3] + "</span>"
                        );

        m_HTMLSource.append(
                        "</a>"
                    "</div>"
                "</div>"
                    );
    }

    m_HTMLSource.append(
                "</div>"
            "</div>"

    // </Recent Tracks Information>

    // <Songs least listened Information>

            "<div id='least_box' class='box'>"
                "<div id='least_box-header' class='box-header'>"
                    "<span id='least_box-header-title' class='box-header-title'>"
                    + i18n( "Least Played Tracks" ) +
                    "</span>"
                "</div>"
                "<div id='least_box-body' class='box-body'>" );

    if ( least.count() == 0 )
    {
        m_HTMLSource.append(
                    "<div class='info'><p>" +
                    i18n( "A list of songs, which you have not played for a long time, will appear here." ) +
                    "</p></div>"
                           );
    }
    else
    {
        QDateTime lastPlay = QDateTime();
        for( uint i = 0; i < least.count(); i = i + 5 )
        {
            lastPlay.setTime_t( least[i + 4].toUInt() );
            m_HTMLSource.append(
                    "<div class='" + QString( (i % 8) ? "box-row-alt" : "box-row" ) + "'>"
                        "<div class='song'>"
                            "<a href=\"file:" + least[i + 1].replace( '"', QCString( "%22" ) ) + "\">"
                            "<span class='song-title'>" + least[i] + "</span><br />"
                            "<span class='song-artist'>" + least[i + 2] + "</span>"
                        );

            if ( !least[i + 3].isEmpty() )
                m_HTMLSource.append(
                            "<span class='song-separator'> - </span>"
                            "<span class='song-album'>" + least[i + 3] + "</span>"
                            );

            m_HTMLSource.append(
                            "<br /><span class='song-time'>" + i18n( "Last played: %1" ).arg( verboseTimeSince( lastPlay ) ) + "</span>"
                            "</a>"
                        "</div>"
                    "</div>"
                        );
        }

        m_HTMLSource.append( "</div>" );
    }

    m_HTMLSource.append(
                "</div>"
            "</div>"
            "</html>"
                       );

    // </Songs least listened Information>

    m_homePage->write( m_HTMLSource );
    m_homePage->end();

    m_dirtyHomePage = false;
    saveHtmlData(); // Send html code to file
}


/** This is the slowest part of track change, so we thread it */
class CurrentTrackJob : public ThreadWeaver::DependentJob
{
public:
    CurrentTrackJob( ContextBrowser *parent )
            : ThreadWeaver::DependentJob( parent, "CurrentTrackJob" )
            , b( parent ) {}

private:
    virtual bool doJob();
    virtual void completeJob()
    {
        // are we still showing the currentTrack page?
        if( b->currentPage() != b->m_currentTrackPage->view() )
            return;

        b->m_currentTrackPage->begin();
        b->m_HTMLSource = m_HTMLSource;
        b->m_currentTrackPage->setUserStyleSheet( b->m_styleSheet );
        b->m_currentTrackPage->write( m_HTMLSource );
        b->m_currentTrackPage->end();

        b->m_dirtyCurrentTrackPage = false;
        b->saveHtmlData(); // Send html code to file
    }

    QString m_HTMLSource;

    ContextBrowser *b;
};


void ContextBrowser::showCurrentTrack() //SLOT
{
    if ( currentPage() != m_currentTrackPage->view() ) {
        blockSignals( true );
        showPage( m_currentTrackPage->view() );
        blockSignals( false );
    }

    if( !m_dirtyCurrentTrackPage )
        return;

    m_currentURL = EngineController::instance()->bundle().url();
    m_currentTrackPage->write( QString::null );

    ThreadWeaver::instance()->onlyOneJob( new CurrentTrackJob( this ) );
}


bool CurrentTrackJob::doJob()
{
    DEBUG_BLOCK

    const MetaBundle &currentTrack = EngineController::instance()->bundle();

    m_HTMLSource.append( "<html>"
                    "<script type='text/javascript'>"
                      //Toggle visibility of a block. NOTE: if the block ID starts with the T
                      //letter, 'Table' display will be used instead of the 'Block' one.
                      "function toggleBlock(ID) {"
                        "if ( document.getElementById(ID).style.display != 'none' ) {"
                          "document.getElementById(ID).style.display = 'none';"
                        "} else {"
                          "if ( ID[0] != 'T' ) {"
                            "document.getElementById(ID).style.display = 'block';"
                          "} else {"
                            "document.getElementById(ID).style.display = 'table';"
                          "}"
                        "}"
                      "}"
                    "</script>" );

    if ( EngineController::engine()->isStream() )
    {
        m_HTMLSource.append( QStringx(
                "<div id='current_box' class='box'>"
                    "<div id='current_box-header' class='box-header'>"
                        "<span id='current_box-header-stream' class='box-header-title'>%1</span> "
                    "</div>"
                    "<table class='box-body' width='100%' border='0' cellspacing='0' cellpadding='1'>"
                        "<tr class='box-row'>"
                            "<td height='42' valign='top' width='90%'>"
                                "<b>%2</b>"
                                "<br />"
                                "<br />"
                                "%3"
                                "<br />"
                                "<br />"
                                "%4"
                                "<br />"
                                "%5"
                                "<br />"
                                "%6"
                                "<br />"
                                "%7"
                            "</td>"
                        "</tr>"
                    "</table>"
                "</div>" )
            .args( QStringList()
                << i18n( "Stream Details" )
                << escapeHTML( currentTrack.prettyTitle() )
                << escapeHTML( currentTrack.streamName() )
                << escapeHTML( currentTrack.genre() )
                << escapeHTML( currentTrack.prettyBitrate() )
                << escapeHTML( currentTrack.streamUrl() )
                << escapeHTML( currentTrack.prettyURL() ) ) );

        if ( b->m_metadataHistory.count() > 2 )
        {
            m_HTMLSource.append(
                "<div class='box'>"
                 "<div class='box-header'>" + i18n( "Metadata History" ) + "</div>"
                 "<table class='box-body' width='100%' border='0' cellspacing='0' cellpadding='1'>" );

            // Ignore last two items, as they don't belong in the history
            for ( uint i = 0; i < b->m_metadataHistory.count() - 2; ++i )
            {
                const QString str = b->m_metadataHistory[i];
                m_HTMLSource.append( QStringx( "<tr class='box-row'><td>%1</td></tr>" ).arg( str ) );
            }

            m_HTMLSource.append(
                 "</table>"
                "</div>" );
        }

        m_HTMLSource.append("</html>" );
        return true;
    }

    const uint artist_id = CollectionDB::instance()->artistID( currentTrack.artist() );
    const uint album_id  = CollectionDB::instance()->albumID ( currentTrack.album() );

    QueryBuilder qb;
    // <Current Track Information>
    qb.clear();
    qb.addReturnValue( QueryBuilder::tabStats, QueryBuilder::valCreateDate );
    qb.addReturnValue( QueryBuilder::tabStats, QueryBuilder::valAccessDate );
    qb.addReturnValue( QueryBuilder::tabStats, QueryBuilder::valPlayCounter );
    qb.addReturnValue( QueryBuilder::tabStats, QueryBuilder::valScore );
    qb.addMatch( QueryBuilder::tabStats, QueryBuilder::valURL, currentTrack.url().path() );
    QStringList values = qb.run();

    //making 2 tables is most probably not the cleanest way to do it, but it works.
    QString albumImageTitleAttr;
    QString albumImage = CollectionDB::instance()->albumImage( currentTrack );
    if ( albumImage == CollectionDB::instance()->notAvailCover( 0 ) )
        albumImageTitleAttr = i18n( "Click to fetch cover from amazon.%1, right-click for menu." ).arg( CoverManager::amazonTld() );
    else
        albumImageTitleAttr = i18n( "Click for information from amazon.%1, right-click for menu." ).arg( CoverManager::amazonTld() );

    m_HTMLSource.append( QStringx(
        "<div id='current_box' class='box'>"
            "<div id='current_box-header' class='box-header'>"
                "<span id='current_box-header-songname' class='box-header-title'>%1</span> "
                "<span id='current_box-header-separator' class='box-header-title'>-</span> "
                "<span id='current_box-header-artist' class='box-header-title'>%2</span>"
                "<br />"
                "<span id='current_box-header-album' class='box-header-title'>%3</span>"
            "</div>"
            "<table id='current_box-table' class='box-body' width='100%' cellpadding='0' cellspacing='0'>"
                "<tr>"
                    "<td id='current_box-largecover-td'>"
                        "<a id='current_box-largecover-a' href='fetchcover:%4 @@@ %5'>"
                            "<img id='current_box-largecover-image' src='%6' title='%7'>"
                        "</a>"
                    "</td>"
                    "<td id='current_box-information-td' align='right'>"
                        "<div id='musicbrainz-div'>"
                            "<a id='musicbrainz-a' title='%8' href='musicbrainz:%9 @@@ %10 @@@ %11'>"
                            "<img id='musicbrainz-image' src='%12' />"
                            "</a>"
                        "</div>"
                                 )
        .args( QStringList()
            << escapeHTML( currentTrack.title() )
            << escapeHTML( currentTrack.artist() )
            << escapeHTML( currentTrack.album() )
            << escapeHTMLAttr( currentTrack.artist() )
            << escapeHTMLAttr( currentTrack.album() )
            << escapeHTMLAttr( CollectionDB::instance()->albumImage( currentTrack ) )
            << albumImageTitleAttr
            << i18n( "Look up this track at musicbrainz.org" )
            << escapeHTMLAttr( currentTrack.artist() )
            << escapeHTMLAttr( currentTrack.album() )
            << escapeHTMLAttr( currentTrack.title() )
            << escapeHTML( locate( "data", "amarok/images/musicbrainz.png" ) ) ) );

    if ( !values.isEmpty() )
    {
        QDateTime firstPlay = QDateTime();
        firstPlay.setTime_t( values[0].toUInt() );
        QDateTime lastPlay = QDateTime();
        lastPlay.setTime_t( values[1].toUInt() );

        const uint playtimes = values[2].toInt();
        const uint score = values[3].toInt();

        const QString scoreBox =
                "<table class='scoreBox' border='0' cellspacing='0' cellpadding='0' title='" + i18n("Score") + " %2'>"
                    "<tr>"
                        "<td nowrap>%1&nbsp;</td>"
                            "<td>"
                            "<div class='sbouter'>"
                                "<div class='sbinner' style='width: %3px;'></div>"
                            "</div>"
                        "</td>"
                    "</tr>"
                "</table>";

        //SAFE   = .arg( x, y )
        //UNSAFE = .arg( x ).arg( y )
        m_HTMLSource.append( QString(
                "<span>%1</span><br />"
                "%2"
                "<span>%3</span><br />"
                "<span>%4</span>"
                                    )
            .arg( i18n( "Track played once", "Track played %n times", playtimes ),
                  scoreBox.arg( score ).arg( score ).arg( score / 2 ),
                  i18n( "Last played: %1" ).arg( verboseTimeSince( lastPlay ) ),
                  i18n( "First played: %1" ).arg( verboseTimeSince( firstPlay ) ) ) );
   }
   else
        m_HTMLSource.append( i18n( "Never played before" ) );

    m_HTMLSource.append(
                    "</td>"
                "</tr>"
            "</table>"
        "</div>" );
    // </Current Track Information>

    if ( !CollectionDB::instance()->isFileInCollection( currentTrack.url().path() ) )
    {
        m_HTMLSource.append(
        "<div id='notindb_box' class='box'>"
            "<div id='notindb_box-header' class='box-header'>"
                "<span id='notindb_box-header-title' class='box-header-title'>"
                + i18n( "This file is not in your Collection!" ) +
                "</span>"
            "</div>"
            "<div id='notindb_box-body' class='box-body'>"
                "<div class='info'><p>"
                + i18n( "If you would like to see contextual information about this track,"
                        " you should add it to your Collection." ) +
                "</p></div>"
                "<div align='center'>"
                "<input type='button' onClick='window.location.href=\"show:collectionSetup\";' value='"
                + i18n( "Change Collection Setup" ) +
                "' class='button' /></div><br />"
            "</div>"
        "</div>"
                           );
    }

    // <Suggested Songs>
    QStringList relArtists;
    relArtists = CollectionDB::instance()->similarArtists( currentTrack.artist(), 10 );
    if ( !relArtists.isEmpty() ) {
        QString token;

        qb.clear();
        qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
        qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
        qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );
        qb.addReturnValue( QueryBuilder::tabStats, QueryBuilder::valScore );
        qb.addMatches( QueryBuilder::tabArtist, relArtists );
        qb.sortBy( QueryBuilder::tabStats, QueryBuilder::valScore, true );
        qb.setLimit( 0, 5 );
        values = qb.run();

        // not enough items returned, let's fill the list with score-less tracks
        if ( values.count() < 10 * qb.countReturnValues() )
        {
            qb.clear();
            qb.exclusiveFilter( QueryBuilder::tabSong, QueryBuilder::tabStats, QueryBuilder::valURL );
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
            qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );
            qb.addMatches( QueryBuilder::tabArtist, relArtists );
            qb.setOptions( QueryBuilder::optRandomize );
            qb.setLimit( 0, 10 - values.count() / 4 );

            QStringList sl;
            sl = qb.run();
            for ( uint i = 0; i < sl.count(); i += qb.countReturnValues() )
            {
                values << sl[i];
                values << sl[i + 1];
                values << sl[i + 2];
                values << "0";
            }
        }

        if ( !values.isEmpty() )
        {
            m_HTMLSource.append(
            "<div id='suggested_box' class='box'>"
                "<div id='suggested_box-header' class='box-header' onClick=\"toggleBlock('T_SS'); window.location.href='togglebox:ss';\" style='cursor: pointer;'>"
                    "<span id='suggested_box-header-title' class='box-header-title'>"
                    + i18n( "Suggested Songs" ) +
                    "</span>"
                "</div>"
                "<table class='box-body' id='T_SS' width='100%' border='0' cellspacing='0' cellpadding='1'>" );

            for ( uint i = 0; i < values.count(); i += 4 )
                m_HTMLSource.append(
                    "<tr class='" + QString( (i % 8) ? "box-row-alt" : "box-row" ) + "'>"
                        "<td class='song'>"
                            "<a href=\"file:" + values[i].replace( '"', QCString( "%22" ) ) + "\">"
                            "<span class='album-song-title'>"+ values[i + 2] + "</span>"
                            "<span class='song-separator'> - </span>"
                            "<span class='album-song-title'>" + values[i + 1] + "</span>"
                            "</a>"
                        "</td>"
                        "<td class='sbtext' width='1'>" + values[i + 3] + "</td>"
                        "<td width='1' title='" + i18n( "Score" ) + "'>"
                            "<div class='sbouter'>"
                                "<div class='sbinner' style='width: " + QString::number( values[i + 3].toInt() / 2 ) + "px;'></div>"
                            "</div>"
                        "</td>"
                        "<td width='1'></td>"
                    "</tr>" );

            m_HTMLSource.append(
                 "</table>"
             "</div>" );

            if ( !b->m_suggestionsOpen )
                m_HTMLSource.append( "<script language='JavaScript'>toggleBlock('T_SS');</script>" );
        }
    }
    // </Suggested Songs>

    // <Favourite Tracks Information>
    QString artistName = currentTrack.artist().isEmpty() ? i18n( "This Artist" ) : escapeHTML( currentTrack.artist() );
    qb.clear();
    qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
    qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
    qb.addReturnValue( QueryBuilder::tabStats, QueryBuilder::valScore );
    qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valArtistID, QString::number( artist_id ) );
    qb.sortBy( QueryBuilder::tabStats, QueryBuilder::valPercentage, true );
    qb.setLimit( 0, 10 );
    values = qb.run();

    if ( !values.isEmpty() )
    {
        m_HTMLSource.append(
        "<div id='favoritesby_box' class='box'>"
            "<div id='favoritesby-header' class='box-header' onClick=\"toggleBlock('T_FT'); window.location.href='togglebox:ft';\" style='cursor: pointer;'>"
                "<span id='favoritesby_box-header-title' class='box-header-title'>"
                + i18n( "Favorite Tracks By %1" ).arg( artistName ) +
                "</span>"
            "</div>"
            "<table class='box-body' id='T_FT' width='100%' border='0' cellspacing='0' cellpadding='1'>" );

        for ( uint i = 0; i < values.count(); i += 3 )
            m_HTMLSource.append(
                "<tr class='" + QString( (i % 6) ? "box-row-alt" : "box-row" ) + "'>"
                    "<td class='song'>"
                        "<a href=\"file:" + values[i + 1].replace( '"', QCString( "%22" ) ) + "\">"
                        "<span class='album-song-title'>" + values[i] + "</span>"
                        "</a>"
                    "</td>"
                    "<td class='sbtext' width='1'>" + values[i + 2] + "</td>"
                    "<td width='1' title='" + i18n( "Score" ) + "'>"
                        "<div class='sbouter'>"
                            "<div class='sbinner' style='width: " + QString::number( values[i + 2].toInt() / 2 ) + "px;'></div>"
                        "</div>"
                    "</td>"
                    "<td width='1'></td>"
                "</tr>"
                               );

        m_HTMLSource.append(
                "</table>"
            "</div>" );

        if ( !b->m_favouritesOpen )
            m_HTMLSource.append( "<script language='JavaScript'>toggleBlock('T_FT');</script>" );

    }
    // </Favourite Tracks Information>

    // <Albums by this artist>
    qb.clear();
    qb.addReturnValue( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.addReturnValue( QueryBuilder::tabAlbum, QueryBuilder::valID );
    qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valArtistID, QString::number( artist_id ) );
    qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName, true );
    qb.sortBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.setOptions( QueryBuilder::optRemoveDuplicates );
    qb.setOptions( QueryBuilder::optNoCompilations );
    values = qb.run();

    if ( !values.isEmpty() )
    {
        // write the script to toggle blocks visibility
        m_HTMLSource.append(
        "<div id='albums_box' class='box'>"
            "<div id='albums_box-header' class='box-header'>"
                "<span id='albums_box-header-title' class='box-header-title'>"
                + i18n( "Albums By %1" ).arg( artistName ) +
                "</span>"
            "</div>"
            "<table id='albums_box-body' class='box-body' width='100%' border='0' cellspacing='0' cellpadding='0'>" );

        uint vectorPlace = 0;
        // find album of the current track (if it exists)
        while ( vectorPlace < values.count() && values[ vectorPlace+1 ] != QString::number( album_id ) )
            vectorPlace += 2;
        for ( uint i = 0; i < values.count(); i += 2 )
        {
            qb.clear();
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTrack );
            qb.addReturnValue( QueryBuilder::tabYear, QueryBuilder::valName );
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valLength );
            qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valAlbumID, values[ i + 1 ] );
            qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valArtistID, QString::number( artist_id ) );
            qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
            qb.setOptions( QueryBuilder::optNoCompilations );
            QStringList albumValues = qb.run();

            QString albumYear;
            if ( !albumValues.isEmpty() )
            {
                albumYear = albumValues[ 3 ];
                for ( uint j = 0; j < albumValues.count(); j += 5 )
                    if ( albumValues[j + 3] != albumYear || albumYear == "0" )
                    {
                        albumYear = QString::null;
                        break;
                    }
            }

            m_HTMLSource.append( QStringx (
            "<tr class='" + QString( (i % 4) ? "box-row-alt" : "box-row" ) + "'>"
                "<td>"
                    "<div class='album-header' onClick=\"toggleBlock('IDA%1')\">"
                    "<table width='100%' border='0' cellspacing='0' cellpadding='0'>"
                    "<tr>"
                        "<td width='1'>"
                            "<a href='fetchcover:%2 @@@ %3'>"
                            "<img class='album-image' align='left' vspace='2' hspace='2' title='%4' src='%5'/>"
                            "</a>"
                        "</td>"
                        "<td valign='middle' align='left'>"
                            "<span class='album-info'>%6</span> "
                            "<a href='album:%7 @@@ %8'><span class='album-title'>%9</span></a>"
                            "<br />"
                            "<span class='album-year'>%10</span>"
                        "</td>"
                    "</tr>"
                    "</table>"
                    "</div>"
                    "<div class='album-body' style='display:%11;' id='IDA%12'>" )
                .args( QStringList()
                    << values[ i + 1 ]
                    << escapeHTMLAttr( currentTrack.artist() ) // artist name
                    << escapeHTMLAttr( values[ i ].isEmpty() ? i18n( "Unknown" ) : values[ i ] ) // album.name
                    << i18n( "Click for information from amazon.com, right-click for menu." )
                    << escapeHTMLAttr( CollectionDB::instance()->albumImage( currentTrack.artist(), values[ i ], 50 ) )
                    << i18n( "Single", "%n Tracks",  albumValues.count() / 5 )
                    << QString::number( artist_id )
                    << values[ i + 1 ] //album.id
                    << escapeHTML( values[ i ].isEmpty() ? i18n( "Unknown" ) : values[ i ] )
                    << albumYear
                    << ( i!=vectorPlace ? "none" : "block" ) /* shows it if it's the current track album */
                    << values[ i + 1 ] ) );

            if ( !albumValues.isEmpty() )
                for ( uint j = 0; j < albumValues.count(); j += 5 )
                {
                    //FIXME this seems redundant, why not just use the result of stripWhitespace?
                    QString track = albumValues[j + 2].stripWhiteSpace().isEmpty() ? "" : albumValues[j + 2];
                    if( track.length() > 0 ) {
                        if( track.length() == 1 )
                            track.prepend( "0" );

                        track = "<span class='album-song-trackno'><b>" + track + "</b>&nbsp;</span>";
                    }

                    QString length;
                    if( albumValues[j + 4] != "0" )
                        length = "<span class='album-song-time'>(" + MetaBundle::prettyTime( QString(albumValues[j + 4]).toInt(), false ) + ")</span>";

                    m_HTMLSource.append(
                        "<div class='album-song'>"
                            "<a href=\"file:" + albumValues[j + 1].replace( "\"", QCString( "%22" ) ) + "\">"
                            + track +
                            "<span class='album-song-title'>" + albumValues[j] + "</span>&nbsp;"
                            + length +
                            "</a>"
                        "</div>" );
                }

            m_HTMLSource.append(
                  "</div>"
                 "</td>"
                "</tr>" );
        }
        m_HTMLSource.append(
               "</table>"
              "</div>" );
    }
    // </Albums by this artist>

    // <Compilations with this artist>
    qb.clear();
    qb.addReturnValue( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.addReturnValue( QueryBuilder::tabAlbum, QueryBuilder::valID );
    qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valArtistID, QString::number( artist_id ) );
    qb.sortBy( QueryBuilder::tabYear, QueryBuilder::valName, true );
    qb.sortBy( QueryBuilder::tabAlbum, QueryBuilder::valName );
    qb.setOptions( QueryBuilder::optRemoveDuplicates );
    qb.setOptions( QueryBuilder::optOnlyCompilations );
    values = qb.run();

    if ( !values.isEmpty() )
    {
        // write the script to toggle blocks visibility
        m_HTMLSource.append(
        "<div id='albums_box' class='box'>"
            "<div id='albums_box-header' class='box-header'>"
                "<span id='albums_box-header-title' class='box-header-title'>"
                + i18n( "Compilations with %1" ).arg( artistName ) +
                "</span>"
            "</div>"
            "<table id='albums_box-body' class='box-body' width='100%' border='0' cellspacing='0' cellpadding='0'>" );

        uint vectorPlace = 0;
        // find album of the current track (if it exists)
        while ( vectorPlace < values.count() && values[ vectorPlace+1 ] != QString::number( album_id ) )
            vectorPlace += 2;
        for ( uint i = 0; i < values.count(); i += 2 )
        {
            qb.clear();
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTitle );
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valURL );
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valTrack );
            qb.addReturnValue( QueryBuilder::tabYear, QueryBuilder::valName );
            qb.addReturnValue( QueryBuilder::tabSong, QueryBuilder::valLength );
            qb.addReturnValue( QueryBuilder::tabArtist, QueryBuilder::valName );
            qb.addMatch( QueryBuilder::tabSong, QueryBuilder::valAlbumID, values[ i + 1 ] );
            qb.sortBy( QueryBuilder::tabSong, QueryBuilder::valTrack );
            qb.setOptions( QueryBuilder::optOnlyCompilations );
            QStringList albumValues = qb.run();

            QString albumYear;
            if ( !albumValues.isEmpty() )
            {
                albumYear = albumValues[ 3 ];
                for ( uint j = 0; j < albumValues.count(); j += qb.countReturnValues() )
                    if ( albumValues[j + 3] != albumYear || albumYear == "0" )
                    {
                        albumYear = QString::null;
                        break;
                    }
            }

            m_HTMLSource.append( QStringx (
            "<tr class='" + QString( (i % 4) ? "box-row-alt" : "box-row" ) + "'>"
                "<td>"
                    "<div class='album-header' onClick=\"toggleBlock('IDA%1')\">"
                    "<table width='100%' border='0' cellspacing='0' cellpadding='0'>"
                    "<tr>"
                        "<td width='1'>"
                            "<a href='fetchcover: @@@ %2'>"
                            "<img class='album-image' align='left' vspace='2' hspace='2' title='%3' src='%4'/>"
                            "</a>"
                        "</td>"
                        "<td valign='middle' align='left'>"
                            "<span class='album-info'>%5</span> "
                            "<a href='compilation:%6'><span class='album-title'>%7</span></a>"
                            "<br />"
                            "<span class='album-year'>%8</span>"
                        "</td>"
                    "</tr>"
                    "</table>"
                    "</div>"
                    "<div class='album-body' style='display:%9;' id='IDA%10'>" )
                .args( QStringList()
                    << values[ i + 1 ]
                    << escapeHTMLAttr( values[ i ].isEmpty() ? i18n( "Unknown" ) : values[ i ] ) // album.name
                    << i18n( "Click for information from amazon.com, right-click for menu." )
                    << escapeHTMLAttr( CollectionDB::instance()->albumImage( currentTrack.artist(), values[ i ], 50 ) )
                    << i18n( "Single", "%n Tracks",  albumValues.count() / 5 )
                    << values[ i + 1 ] //album.id
                    << escapeHTML( values[ i ].isEmpty() ? i18n( "Unknown" ) : values[ i ] )
                    << albumYear
                    << ( i!=vectorPlace ? "none" : "block" ) /* shows it if it's the current track album */
                    << values[ i + 1 ] ) );

            if ( !albumValues.isEmpty() )
                for ( uint j = 0; j < albumValues.count(); j += qb.countReturnValues() )
                {
                    QString track = albumValues[j + 2].stripWhiteSpace().isEmpty() ? "" : albumValues[j + 2];
                    if( track.length() > 0 ) {
                        if( track.length() == 1 )
                            track.prepend( "0" );

                        track = "<span class='album-song-trackno'><b>" + track + "</b>&nbsp;</span>";
                    }

                    QString length;
                    if( albumValues[j + 4] != "0" )
                        length = "<span class='album-song-time'>(" + MetaBundle::prettyTime( QString(albumValues[j + 4]).toInt(), false ) + ")</span>";
                    m_HTMLSource.append(
                        "<div class='album-song'>"
                            "<a href=\"file:" + albumValues[j + 1].replace( "\"", QCString( "%22" ) ) + "\">"
                            + track +
                            "<span class='album-song-title'>" + albumValues[j + 5] + " - " + albumValues[j] + "</span>&nbsp;"
                            + length +
                            "</a>"
                        "</div>" );
                }

            m_HTMLSource.append(
                  "</div>"
                 "</td>"
                "</tr>" );
        }
        m_HTMLSource.append(
               "</table>"
              "</div>" );
    }
    // </Compilations with this artist>

    m_HTMLSource.append( "</html>" );

    return true;
}


void ContextBrowser::setStyleSheet()
{
    DEBUG_FUNC_INFO

    QString themeName = AmarokConfig::contextBrowserStyleSheet().latin1();
    const QString file = kapp->dirs()->findResource( "data","amarok/themes/" + themeName + "/stylesheet.css" );

    if ( themeName != "default" && QFile::exists( file ) )
        setStyleSheet_ExternalStyle( m_styleSheet, themeName );
    else
        setStyleSheet_Default( m_styleSheet );

    m_homePage->setUserStyleSheet( m_styleSheet );
    m_currentTrackPage->setUserStyleSheet( m_styleSheet );
    m_lyricsPage->setUserStyleSheet( m_styleSheet );
}


void ContextBrowser::setStyleSheet_Default( QString& styleSheet )
{
    //colorscheme/font dependant parameters
    int pxSize = fontMetrics().height() - 4;
    const QString fontFamily = AmarokConfig::useCustomFonts() ? AmarokConfig::contextBrowserFont().family() : QApplication::font().family();
    const QString text = colorGroup().text().name();
    const QString fg   = colorGroup().highlightedText().name();
    const QString bg   = colorGroup().highlight().name();
    const QColor baseColor = colorGroup().base();
    const QColor bgColor = colorGroup().highlight();
    const amaroK::Color gradientColor = bgColor;

    delete m_bgGradientImage;
    delete m_headerGradientImage;
    delete m_shadowGradientImage;

    m_bgGradientImage = new KTempFile( locateLocal( "tmp", "gradient" ), ".png", 0600 );
    QImage image = KImageEffect::gradient( QSize( 600, 1 ), gradientColor, gradientColor.light( 130 ), KImageEffect::PipeCrossGradient );
    image.save( m_bgGradientImage->file(), "PNG" );
    m_bgGradientImage->close();

    m_headerGradientImage = new KTempFile( locateLocal( "tmp", "gradient_header" ), ".png", 0600 );
    QImage imageH = KImageEffect::unbalancedGradient( QSize( 1, 10 ), bgColor, gradientColor.light( 130 ), KImageEffect::VerticalGradient, 100, -100 );
    imageH.copy( 0, 1, 1, 9 ).save( m_headerGradientImage->file(), "PNG" );
    m_headerGradientImage->close();

    m_shadowGradientImage = new KTempFile( locateLocal( "tmp", "gradient_shadow" ), ".png", 0600 );
    QImage imageS = KImageEffect::unbalancedGradient( QSize( 1, 10 ), baseColor, Qt::gray, KImageEffect::VerticalGradient, 100, -100 );
    imageS.save( m_shadowGradientImage->file(), "PNG" );
    m_shadowGradientImage->close();

    //unlink the files for us on deletion
    m_bgGradientImage->setAutoDelete( true );
    m_headerGradientImage->setAutoDelete( true );
    m_shadowGradientImage->setAutoDelete( true );

    //we have to set the color for body due to a KHTML bug
    //KHTML sets the base color but not the text color
    styleSheet = QString( "body { margin: 8px; font-size: %1px; color: %2; background-color: %3; background-image: url( %4 ); background-repeat: repeat; font-family: %5; }" )
            .arg( pxSize )
            .arg( text )
            .arg( AmarokConfig::schemeAmarok() ? fg : gradientColor.name() )
            .arg( m_bgGradientImage->name() )
            .arg( fontFamily );

    //text attributes
    styleSheet += QString( "a { font-size: %1px; color: %2; }" ).arg( pxSize ).arg( text );
    styleSheet += QString( ".info { display: block; margin-left: 4px; font-weight: normal; }" );

    styleSheet += QString( ".song a { display: block; padding: 1px 2px; font-weight: normal; text-decoration: none; }" );
    styleSheet += QString( ".song a:hover { color: %1; background-color: %2; }" ).arg( fg ).arg( bg );
    styleSheet += QString( ".song-title { font-weight: bold; }" );
    styleSheet += QString( ".song-place { font-size: %1px; font-weight: bold; }" ).arg( pxSize + 3 );

    //box: the base container for every block (border hilighted on hover, 'A' without underlining)
    styleSheet += QString( ".box { border: solid %1 1px; text-align: left; margin-bottom: 10px; }" ).arg( bg );
    styleSheet += QString( ".box a { text-decoration: none; }" );
    styleSheet += QString( ".box:hover { border: solid %1 1px; }" ).arg( text );

    //box contents: header, body, rows and alternate-rows
    styleSheet += QString( ".box-header { color: %1; background-color: %2; background-image: url( %4 ); background-repeat: repeat-x; font-size: %3px; font-weight: bold; padding: 1px 0.5em; border-bottom: 1px solid #000; }" )
            .arg( fg )
            .arg( bg )
            .arg( pxSize + 2 )
            .arg( m_headerGradientImage->name() );

    styleSheet += QString( ".box-body { padding: 2px; background-color: %1; background-image: url( %2 ); background-repeat: repeat-x; font-size:%3px; }" )
            .arg( colorGroup().base().name() )
            .arg( m_shadowGradientImage->name() )
            .arg( pxSize );

    //"Albums by ..." related styles
    styleSheet += QString( ".album-header:hover { color: %1; background-color: %2; cursor: pointer; }" ).arg( fg ).arg( bg );
    styleSheet += QString( ".album-header:hover a { color: %1; }" ).arg( fg );
    styleSheet += QString( ".album-body { background-color: %1; border-bottom: solid %2 1px; border-top: solid %3 1px; }" ).arg( colorGroup().base().name() ).arg( bg ).arg( bg );
    styleSheet += QString( ".album-title { font-weight: bold; }" );
    styleSheet += QString( ".album-info { float:right; padding-right:4px; font-size: %1px }" ).arg( pxSize );
    styleSheet += QString( ".album-image { padding-right: 4px; }" );
    styleSheet += QString( ".album-song a { display: block; padding: 1px 2px; font-weight: normal; text-decoration: none; }" );
    styleSheet += QString( ".album-song a:hover { color: %1; background-color: %2; }" ).arg( fg ).arg( bg );

    styleSheet += QString( ".button { width: 100%; }" );

    //boxes used to display score (sb: score box)
    styleSheet += QString( ".sbtext { text-align: center; padding: 0px 4px; border-left: solid %1 1px; }" ).arg( colorGroup().base().dark( 120 ).name() );
    styleSheet += QString( ".sbouter { width: 52px; height: 10px; background-color: %1; border: solid %2 1px; }" ).arg( colorGroup().base().dark( 120 ).name() ).arg( bg );
    styleSheet += QString( ".sbinner { height: 8px; background-color: %1; border: solid %2 1px; }" ).arg( bg ).arg( fg );

    styleSheet += QString( "#current_box-header-album { font-weight: normal; }" );
    styleSheet += QString( "#current_box-information-td { text-align: right; vertical-align: bottom; padding: 3px; }" );
    styleSheet += QString( "#current_box-largecover-td { text-align: left; width: 100px; padding: 0; vertical-align: bottom; }" );
    styleSheet += QString( "#current_box-largecover-image { padding: 4px; vertical-align: bottom; }" );
}


void ContextBrowser::setStyleSheet_ExternalStyle( QString& styleSheet, QString& themeName )
{
    //colorscheme/font dependant parameters
    const QString pxSize = QString::number( fontMetrics().height() - 4 );
    const QString fontFamily = AmarokConfig::useCustomFonts() ? AmarokConfig::contextBrowserFont().family() : QApplication::font().family();
    const QString text = colorGroup().text().name();
    const QString fg   = colorGroup().highlightedText().name();
    const QString bg   = colorGroup().highlight().name();
    const QString base   = colorGroup().base().name();
    const QColor bgColor = colorGroup().highlight();
    amaroK::Color gradientColor = bgColor;

    //we have to set the color for body due to a KHTML bug
    //KHTML sets the base color but not the text color
    styleSheet = QString( "body { margin: 8px; font-size: %1px; color: %2; background-color: %3; font-family: %4; }" )
            .arg( pxSize )
            .arg( text )
            .arg( AmarokConfig::schemeAmarok() ? fg : gradientColor.name() )
            .arg( fontFamily );

    const QString CSSLocation = kapp->dirs()->findResource( "data","amarok/themes/" + themeName + "/stylesheet.css" );

    QFile ExternalCSS( CSSLocation );
    if ( !ExternalCSS.open( IO_ReadOnly ) )
        return;

    QTextStream eCSSts( &ExternalCSS );
    QString tmpCSS = eCSSts.read();
    ExternalCSS.close();

    tmpCSS.replace( "./", KURL::fromPathOrURL( CSSLocation ).directory( false ) );
    tmpCSS.replace( "AMAROK_FONTSIZE-2", pxSize );
    tmpCSS.replace( "AMAROK_FONTSIZE", pxSize );
    tmpCSS.replace( "AMAROK_FONTSIZE+2", pxSize );
    tmpCSS.replace( "AMAROK_FONTFAMILY", fontFamily );
    tmpCSS.replace( "AMAROK_TEXTCOLOR", text );
    tmpCSS.replace( "AMAROK_BGCOLOR", bg );
    tmpCSS.replace( "AMAROK_FGCOLOR", fg );
    tmpCSS.replace( "AMAROK_BASECOLOR", base );
    tmpCSS.replace( "AMAROK_DARKBASECOLOR", colorGroup().base().dark( 120 ).name() );
    tmpCSS.replace( "AMAROK_GRADIENTCOLOR", gradientColor.name() );

    styleSheet += tmpCSS;
}


void ContextBrowser::showIntroduction()
{
    DEBUG_BLOCK

    if ( currentPage() != m_homePage->view() )
    {
        blockSignals( true );
        showPage( m_homePage->view() );
        blockSignals( false );
    }

    // Do we have to rebuild the page? I don't care
    m_homePage->begin();
    m_HTMLSource = QString::null;
    m_homePage->setUserStyleSheet( m_styleSheet );

    m_HTMLSource.append(
            "<html>"
            "<div id='introduction_box' class='box'>"
                "<div id='introduction_box-header' class='box-header'>"
                    "<span id='introduction_box-header-title' class='box-header-title'>"
                    + i18n( "Hello amaroK user!" ) +
                    "</span>"
                "</div>"
                "<div id='introduction_box-body' class='box-body'>"
                    "<div class='info'><p>" +
                    i18n( "This is the Context Browser: "
                          "it shows you contextual information about the currently playing track. "
                          "In order to use this feature of amaroK, you need to build a Collection."
                        ) +
                    "</p></div>"
                    "<div align='center'>"
                    "<input type='button' onClick='window.location.href=\"show:collectionSetup\";' value='" +
                    i18n( "Build Collection..." ) +
                    "'></div><br />"
                "</div>"
            "</div>"
            "</html>"
                       );

    m_homePage->write( m_HTMLSource );
    m_homePage->end();
    saveHtmlData(); // Send html code to file
}


void ContextBrowser::showScanning()
{
    if ( currentPage() != m_homePage->view() )
    {
        blockSignals( true );
        showPage( m_homePage->view() );
        blockSignals( false );
    }

    // Do we have to rebuild the page? I don't care
    m_homePage->begin();
    m_HTMLSource="";
    m_homePage->setUserStyleSheet( m_styleSheet );

    m_HTMLSource.append(
            "<html>"
            "<div id='building_box' class='box'>"
                "<div id='building_box-header' class='box-header'>"
                    "<span id='building_box-header-title' class='box-header-title'>"
                    + i18n( "Building Collection Database..." ) +
                    "</span>"
                "</div>"
                "<div id='building_box-body' class='box-body'>"
                    "<div class='info'><p>" + i18n( "Please be patient while amaroK scans your music collection. You can watch the progress of this activity in the statusbar." ) + "</p></div>"
                "</div>"
            "</div>"
            "</html>"
                       );

    m_homePage->write( m_HTMLSource );
    m_homePage->end();
    saveHtmlData(); // Send html code to file
}


// THE FOLLOWING CODE IS COPYRIGHT BY
// Christian Muehlhaeuser, Seb Ruiz
// <chris at chris.de>, <seb100 at optusnet.com.au>
// If I'm violating any copyright or such
// please contact / sue me. Thanks.

void ContextBrowser::showLyrics( const QString &hash )
{
    if ( currentPage() != m_lyricsPage->view() )
    {
        blockSignals( true );
        showPage( m_homePage->view() );
        blockSignals( false );
    }

    if ( !m_dirtyLyricsPage ) return;

    m_lyricsPage->begin();
    m_lyricsPage->setUserStyleSheet( m_styleSheet );
    m_HTMLSource = QString (
        "<html>"
        "<div id='lyrics_box' class='box'>"
            "<div id='lyrics_box-header' class='box-header'>"
                "<span id='lyrics_box-header-title' class='box-header-title'>"
                + i18n( "Fetching Lyrics" ) +
                "</span>"
            "</div>"
            "<div id='lyrics_box-body' class='box-body'>"
                + i18n( "Fetching Lyrics" ) +
            " ...</div>"
        "</div>"
        "</html>"
                           );
    m_lyricsPage->write( m_HTMLSource );
    m_lyricsPage->end();

    //remove all matches to the regExp and the song production type.
    //NOTE: use i18n'd and english equivalents since they are very common int'lly.
    QString replaceMe = " \\([^}]*%1[^}]*\\)";
    QStringList production;
    production << i18n( "live" ) << i18n( "acoustic" ) << i18n( "cover" ) << i18n( "mix" )
               << i18n( "edit" ) << i18n( "medley" ) << i18n( "unplugged" ) << i18n( "bonus" )
               << QString( "live" ) << QString( "acoustic" ) << QString( "cover" ) << QString( "mix" )
               << QString( "edit" ) << QString( "medley" ) << QString( "unplugged" ) << QString( "bonus" );

    QString title  = EngineController::instance()->bundle().title();

    for ( uint x = 0; x < production.count(); ++x )
    {
        QRegExp re = replaceMe.arg( production[x] );
        re.setCaseSensitive( false );
        title.remove( re );
    }

    QString url;
    if ( !hash.isEmpty() )
        url = QString( "http://lyrc.com.ar/en/tema1en.php?hash=%1" )
                  .arg( hash );
    else
        url = QString( "http://lyrc.com.ar/en/tema1en.php?artist=%1&songname=%2" )
                .arg(
                KURL::encode_string_no_slash( EngineController::instance()->bundle().artist() ),
                KURL::encode_string_no_slash( title ) );


    debug() << "Using this url: " << url << endl;

    m_lyrics = QString::null;
    m_lyricAddUrl = QString( "externalurl://lyrc.com.ar/en/add/add.php?grupo=%1&tema=%2&disco=%3&ano=%4" ).arg(
            KURL::encode_string_no_slash( EngineController::instance()->bundle().artist() ),
            KURL::encode_string_no_slash( title ),
            KURL::encode_string_no_slash( EngineController::instance()->bundle().album() ),
            KURL::encode_string_no_slash( EngineController::instance()->bundle().year() ) );
    m_lyricSearchUrl = QString( "externalurl://www.google.com/search?ie=UTF-8&q=lyrics %1 %2" )
        .arg( KURL::encode_string_no_slash( '"'+EngineController::instance()->bundle().artist()+'"', 106 /*utf-8*/ ),
              KURL::encode_string_no_slash( '"'+title+'"', 106 /*utf-8*/ ) );

    KIO::TransferJob* job = KIO::get( url, false, false );

    amaroK::StatusBar::instance()->newProgressOperation( job )
            .setDescription( i18n( "Fetching Lyrics" ) );

    connect( job, SIGNAL( result( KIO::Job* ) ),
             this,  SLOT( lyricsResult( KIO::Job* ) ) );
    connect( job, SIGNAL( data( KIO::Job*, const QByteArray& ) ),
             this,  SLOT( lyricsData( KIO::Job*, const QByteArray& ) ) );
}


void
ContextBrowser::lyricsData( KIO::Job*, const QByteArray& data ) //SLOT
{
    // Append new chunk of string
    m_lyrics += QString( data );
}


void
ContextBrowser::lyricsResult( KIO::Job* job ) //SLOT
{
    if ( !job->error() == 0 )
    {
        kdWarning() << "[LyricsFetcher] KIO error! errno: " << job->error() << endl;
        return;
    }

    /* We don't want to display any links or images in our lyrics */
    m_lyrics.replace( QRegExp("<[aA][^>]*>[^<]*</[aA]>"), QString::null );
    m_lyrics.replace( QRegExp("<[iI][mM][gG][^>]*>"), QString::null );

    if ( m_lyrics.find( "<font size='2'>" ) != -1 )
    {
        m_lyrics = m_lyrics.mid( m_lyrics.find( "<font size='2'>" ) );
        if ( m_lyrics.find( "<p><hr" ) != -1 )
            m_lyrics = m_lyrics.mid( 0, m_lyrics.find( "<p><hr" ) );
        else
            m_lyrics = m_lyrics.mid( 0, m_lyrics.find( "<br /><br />" ) );
    }
    else if ( m_lyrics.find( "Suggestions : " ) != -1 )
    {
        m_lyrics = m_lyrics.mid( m_lyrics.find( "Suggestions : " ), m_lyrics.find( "<br /><br />" ) );
        showLyricSuggestions();
    }
    else
    {
        m_lyrics = i18n( "Lyrics not found." );
        m_lyrics += QString( "<div id='lyrics_box_addlyrics'>"
            "<input type='button' onClick='window.location.href=\"" + m_lyricAddUrl + "\";' value='"
            + i18n("Add Lyrics") + "' class='button' /></div>" );
        m_lyrics += QString( "<div id='lyrics_box_searchlyrics'>"
            "<input type='button' onClick='window.location.href=\"" + m_lyricSearchUrl + "\";' value='"
            + i18n("Search Lyrics") + "' class='button' /></div>" );
    }


    m_lyricsPage->begin();
    m_HTMLSource="";
    m_lyricsPage->setUserStyleSheet( m_styleSheet );

    m_HTMLSource.append(
            "<html>"
            "<div id='lyrics_box' class='box'>"
                "<div id='lyrics_box-header' class='box-header'>"
                    "<span id='lyrics_box-header-title' class='box-header-title'>"
                    + i18n( "Lyrics" ) +
                    "</span>"
                "</div>"
                "<div id='lyrics_box-body' class='box-body'>"
                    + m_lyrics +
                "</div>"
            "</div>"
            "</html>"
                       );
    m_lyricsPage->write( m_HTMLSource );
    m_lyricsPage->end();
    m_dirtyLyricsPage = false;
    saveHtmlData(); // Send html code to file
}


void
ContextBrowser::showLyricSuggestions()
{
    m_lyricHashes.clear();
    m_lyricSuggestions.clear();

    m_lyrics.replace( QString( "<font color='white'>" ), QString::null );
    m_lyrics.replace( QString( "</font>" ), QString::null );
    m_lyrics.replace( QString( "<br /><br />" ), QString::null );

    while ( !m_lyrics.isEmpty() )
    {
        m_lyrics = m_lyrics.mid( m_lyrics.find( "hash=" ) );
        m_lyricHashes << m_lyrics.mid( 5, m_lyrics.find( ">" ) - 6 );
        m_lyrics = m_lyrics.mid( m_lyrics.find( ">" ) );
        m_lyricSuggestions << m_lyrics.mid( 1, m_lyrics.find( "</a>" ) - 1 );
    }
    m_lyrics = i18n( "Lyrics for track not found, here are some suggestions:" ) + QString("<br /><br />");

    for ( uint i=0; i < m_lyricHashes.count() - 1; ++i )
    {
        m_lyrics += QString( "<a href='show:suggestLyric-%1'>" ).arg( m_lyricHashes[i] );
        m_lyrics += QString( "%1</a><br />" ).arg( m_lyricSuggestions[i] );
    }
    m_lyrics += QString( "<div id='lyrics_box_addlyrics'>"
        "<input type='button' onClick='window.location.href=\"" + m_lyricAddUrl + "\";' value='"
        + i18n("Add Lyrics") + "' class='button' /></div>" );
    m_lyrics += QString( "<div id='lyrics_box_searchlyrics'>"
        "<input type='button' onClick='window.location.href=\"" + m_lyricSearchUrl + "\";' value='"
        + i18n("Search Lyrics") + "' class='button' /></div>" );


}


void
ContextBrowser::coverFetched( const QString &artist, const QString &album )
{
    const MetaBundle &currentTrack = EngineController::instance()->bundle();
    if ( currentTrack.artist().isEmpty() && currentTrack.album().isEmpty() )
        return;

    if ( currentTrack.artist() == artist || currentTrack.album() == album ) // this is for compilations or artist == ""
    {
        m_dirtyCurrentTrackPage = true;
        showCurrentTrack();
    }
}


void
ContextBrowser::coverRemoved( const QString &artist, const QString &album )
{
    const MetaBundle &currentTrack = EngineController::instance()->bundle();
    if ( currentTrack.artist().isEmpty() && currentTrack.album().isEmpty() )
        return;

    if ( currentTrack.artist() == artist || currentTrack.album() == album ) // this is for compilations or artist == ""
    {
        m_dirtyCurrentTrackPage = true;
        showCurrentTrack();
    }
}


void
ContextBrowser::similarArtistsFetched( const QString &artist )
{
    if ( EngineController::instance()->bundle().artist() == artist ) {
        m_dirtyCurrentTrackPage = true;
        showCurrentTrack();
    }
}


#include "contextbrowser.moc"
