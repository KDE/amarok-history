// (c) 2004 Mark Kretschmann <markey@web.de>
// (c) 2004 Stefan Bogner <bochi@online.ms>
// See COPYING file for licensing information.

#include "amazonsearch.h"
#include "collectiondb.h"
#include "coverfetcher.h"

#include <qcombobox.h>
#include <qdom.h>
#include <qlabel.h>
#include <qvbox.h>

#include <kapplication.h>
#include <kcursor.h> //waiting cursor
#include <kdebug.h>
#include <kinputdialog.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <klineedit.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpushbutton.h>


CoverFetcher::CoverFetcher( const QString& license, QObject* parent)
    : QObject( parent, "CoverFetcher" )
    , m_license( license )
    , m_buffer( 0 )
{
    QApplication::setOverrideCursor( KCursor::workingCursor() );
    kdDebug() << k_funcinfo << endl;
}


CoverFetcher::~CoverFetcher()
{
    QApplication::restoreOverrideCursor();
    delete[] m_buffer;
}


//////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC
//////////////////////////////////////////////////////////////////////////////////////////

void
CoverFetcher::getCover( const QString& artist, const QString& album, const QString& saveas, QueryMode mode, bool noedit, int size, bool albumonly )
{
    kdDebug() << k_funcinfo << endl;
    m_artist = artist;
    m_album = album;
    if ( artist == album ) m_keyword = album;
    else m_keyword = artist + " - " + album;
    m_saveas = saveas;
    m_noedit = noedit;
    m_size = size;
    m_albumonly = albumonly;

    /* reset all values (if search isn't started as new CoverFetcher) */
    delete m_buffer;
    m_bufferIndex = 0;
    m_xmlDocument = "";

    QString url = QString( "http://xml.amazon.com/onca/xml3?t=webservices-20&dev-t=%1"
                           "&KeywordSearch=%2&mode=music&type=%3&page=1&f=xml" )
                           .arg( m_license )
                           .arg( m_keyword )
                           .arg( mode == lite ? "lite" : "heavy" );

    kdDebug() << "Using this url: " << url << endl;

    KIO::TransferJob* job = KIO::get( url, false, false );
    connect( job, SIGNAL( result( KIO::Job* ) ),
             this,  SLOT( xmlResult( KIO::Job* ) ) );
    connect( job, SIGNAL( data( KIO::Job*, const QByteArray& ) ),
             this,  SLOT( xmlData( KIO::Job*, const QByteArray& ) ) );
}


//////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE SLOTS
//////////////////////////////////////////////////////////////////////////////////////////

void
CoverFetcher::xmlData( KIO::Job*, const QByteArray& data ) //SLOT
{
    // Append new chunk of string
    m_xmlDocument += QString( data );
}


void
CoverFetcher::xmlResult( KIO::Job* job ) //SLOT
{
    if ( !job->error() == 0 ) {
        kdWarning() << "[CoverFetcher] KIO error! errno: " << job->error() << endl;
        emit error();
        deleteLater();
        return;
    }
    kdDebug() << m_xmlDocument << endl;

    QDomDocument doc;
    doc.setContent( m_xmlDocument );

    // Fetch url for product info page
    m_url = doc.documentElement()
               .namedItem( "Details" )
               .attributes()
               .namedItem( "url" ).toAttr().value();

    if ( m_size == 0 )
    {
        m_imageUrl = doc.documentElement()
                        .namedItem( "Details" )
                        .namedItem( "ImageUrlSmall" )
                        .firstChild().toText().nodeValue();
    }
    else if ( m_size == 1 )
    {
        m_imageUrl = doc.documentElement()
                        .namedItem( "Details" )
                        .namedItem( "ImageUrlMedium" )
                        .firstChild().toText().nodeValue();
    }
    else
    {
        m_imageUrl = doc.documentElement()
                        .namedItem( "Details" )
                        .namedItem( "ImageUrlLarge" )
                        .firstChild().toText().nodeValue();
    }

//     kdDebug() << "imageUrl: " << m_imageUrl << endl;
    m_buffer = new uchar[BUFFER_SIZE];

    KIO::TransferJob* imageJob = KIO::get( m_imageUrl, false, false );
    connect( imageJob, SIGNAL( result( KIO::Job* ) ),
             this,       SLOT( imageResult( KIO::Job* ) ) );
    connect( imageJob, SIGNAL( data( KIO::Job*, const QByteArray& ) ),
             this,       SLOT( imageData( KIO::Job*, const QByteArray& ) ) );
}


void
CoverFetcher::imageData( KIO::Job*, const QByteArray& data ) //SLOT
{
    if ( m_bufferIndex + (uint) data.size() >= BUFFER_SIZE ) {
        emit error();
        KMessageBox::error( 0, i18n( "CoverFetcher buffer overflow. Image is bigger than <i>1</i> byte. Aborting.",
                                     "CoverFetcher buffer overflow. Image is bigger than <i>%n</i> bytes. Aborting.",
                                     BUFFER_SIZE ) );
        deleteLater();
        return;
    }

    // Append new chunk of data to buffer
    memcpy( m_buffer + m_bufferIndex, data.data(), data.size() );
    m_bufferIndex += data.size();
}


void
CoverFetcher::imageResult( KIO::Job* job ) //SLOT
{
    kdDebug() << k_funcinfo << endl;

    /* Okay, this next thing may appear a bit weird ;) Short explanation:
       If there's no result for the search string (artist - album), search for the album tag only.
       If there's no (large) cover is found, try the next smaller image (medium) etc. If no covers, open editSearch.
       If we fetch all covers (covermanager), do not show any errors/previews, just save (m_noedit).  */

    if ( job->error() )
    {
        kdDebug() << "[CoverFetcher] KIO Job error.\n";

        if ( !m_albumonly )
            getCover( m_album, m_album, m_saveas, CoverFetcher::heavy, m_noedit, 1, true );

        else if ( !m_noedit ) {
            m_text = i18n( "<h3>No cover image found.</h3>"
                           "If you would like to search again, you can edit the search string below and press <b>OK</b>." );
            editSearch();
        }
        else {
            kdDebug() << "[CoverFetcher] Image not found in amazon.com database.\n";
            emit error();
            deleteLater();
            return;
        }
    }
    else
    {
        m_text = i18n( "<h3>New Search</h3>Please edit the search string below and press <b>OK</b>." );
        m_image.loadFromData( m_buffer, m_bufferIndex );

        if ( m_image.width() == 1 )
        {
            if ( m_size )
            {
                if ( m_albumonly ) getCover( m_album, m_album, m_saveas, CoverFetcher::heavy, m_noedit, m_size-1, m_albumonly );
                else getCover( m_artist, m_album, m_saveas, CoverFetcher::heavy, m_noedit, m_size-1, m_albumonly );
            }
            else if ( !m_noedit )
            {
                m_text = i18n( "<h3>Cover found, but without images.</h3>"
                               "If you would like to search again, you can edit the search string below and press <b>OK</b>." );
                editSearch();
            }
            else
            {
                kdDebug() << "[CoverFetcher] Image is invalid." << endl;
                emit error();
                deleteLater();
                return;
            }
        }
        else if ( !m_noedit )
        {
            QVBox* container = new QVBox( 0, 0, WDestructiveClose );
            /* we show m_album here, since it's always the filename on save */
            container->setCaption( kapp->makeStdCaption( m_album ) );
            connect( this, SIGNAL( destroyed() ), container, SLOT( deleteLater() ) );

            QWidget* widget = new QWidget( container );
            widget->setPaletteBackgroundPixmap( QPixmap( m_image ) );
            widget->setFixedSize( m_image.size() );

            QHBox* buttons = new QHBox( container );
            KPushButton* save = new KPushButton( i18n( "Save" ), buttons );
            KPushButton* newsearch = new KPushButton( i18n( "New search" ), buttons );
            KPushButton* cancel = new KPushButton( i18n( "Cancel" ), buttons );
            connect( cancel, SIGNAL( clicked() ), this, SLOT( deleteLater() ) );
            connect( newsearch, SIGNAL( clicked() ), this, SLOT( editSearch() ) );
            connect( save, SIGNAL( clicked() ), this, SLOT( saveCover() ) );

            container->adjustSize();
            container->setFixedSize( container->size() );
            container->show();
        }
        else
        {
            if ( !m_image.loadFromData( m_buffer, m_bufferIndex ) ) {
                kdDebug() << "[CoverFetcher] Image is invalid." << endl;
                emit error();
                deleteLater();
                return;
            }
            saveCover();
        }
    }
}

void
CoverFetcher::editSearch() //SLOT
{
    AmazonSearch* sdlg = new AmazonSearch();
    sdlg->m_textLabel->setText( m_text );
    sdlg->m_searchString->setText( m_saveas );
    sdlg->setModal( true );
    connect( sdlg, SIGNAL( imageReady( const QImage& ) ), this, SLOT( saveCover( const QImage& ) ) );

    if ( sdlg->exec() == QDialog::Accepted )
    {
        m_album = sdlg->m_searchString->text();
        getCover( m_album, m_album, m_saveas, CoverFetcher::heavy );
        return;
    }
    else
    {
        emit error();
        deleteLater();
        return;
    }
}

void
CoverFetcher::saveCover() //SLOT
{
    kdDebug() << k_funcinfo << endl;

    emit imageReady( m_saveas, m_url, m_image );
    deleteLater();
}

void
CoverFetcher::saveCover( const QImage& image ) //SLOT
{
    kdDebug() << k_funcinfo << endl;

    emit imageReady( m_saveas, m_url, image );
    deleteLater();
}


#include "coverfetcher.moc"
