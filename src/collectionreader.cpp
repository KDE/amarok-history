// (c) The amaroK developers 2003-4
// See COPYING file that comes with this distribution
//

#define DEBUG_PREFIX "CollectionReader"

#include "amarok.h"
#include "amarokconfig.h"
#include "collectiondb.h"
#include "collectionreader.h"
#include "debug.h"
#include <kapplication.h>
#include <kglobal.h>
#include <klocale.h>
#include <iostream>
#include "metabundle.h"
#include "playlistbrowser.h"
#include "statusbar.h"

extern "C"
{
    #include <dirent.h>    //stat
    #include <errno.h>
    #include <sys/types.h> //stat
    #include <sys/stat.h>  //stat
    #include <unistd.h>    //stat
}


CollectionReader::CollectionReader( CollectionDB* parent, const QStringList& folders )
        : DependentJob( parent, "CollectionReader" )
        , m_db( CollectionDB::instance()->getStaticDbConnection() )
        , m_folders( folders )
        , m_recursively( AmarokConfig::scanRecursively() )
        , m_importPlaylists( AmarokConfig::importPlaylists() )
        , m_incremental( false )
        , log( QFile::encodeName( amaroK::saveLocation( QString::null ) + "collection_scan.log" ) )
{
    setDescription( i18n( "Building collection" ) );
}


CollectionReader::~CollectionReader()
{
    CollectionDB::instance()->returnStaticDbConnection( m_db );
}



IncrementalCollectionReader::IncrementalCollectionReader( CollectionDB *parent )
        : CollectionReader( parent, QStringList() )
{
    m_importPlaylists = false;
    m_incremental     = true;

    setDescription( i18n( "Updating collection" ) );
}

bool
IncrementalCollectionReader::doJob()
{
    /**
     * The Incremental Reader works as follows: Here we check the mtime of every directory in the "directories"
     * table and store all changed directories in m_folders.
     *
     * These directories are then scanned in CollectionReader::doJob(), with m_recursively set according to the
     * user's preference, so the user can add directories or whole directory trees, too. Since we don't want to
     * rescan unchanged subdirectories, CollectionReader::readDir() checks if we are scanning recursively and
     * prevents that.
     */

    struct stat statBuf;
    const QStringList values = CollectionDB::instance()->query( "SELECT dir, changedate FROM directories;" );

    foreach( values ) {
        const QString folder = *it;
        const QString mtime  = *++it;

        if ( stat( QFile::encodeName( folder ), &statBuf ) == 0 ) {
            if ( QString::number( (long)statBuf.st_mtime ) != mtime ) {
                m_folders << folder;
                debug() << "Collection dir changed: " << folder << endl;
            }
        }
        else {
            // this folder has been removed
            m_folders << folder;
            debug() << "Collection dir removed: " << folder << endl;
        }
    }

    if ( !m_folders.isEmpty() )
        amaroK::StatusBar::instance()->shortMessage( i18n( "Updating Collection..." ) );

    return CollectionReader::doJob();
}

bool
CollectionReader::doJob()
{
    if (!m_db->isConnected())
        return false;

    log << "Collection Scan Log\n";
    log << "===================\n";
    log << i18n( "Report this file if amaroK crashes when building the Collection." ).local8Bit();
    log << "\n\n\n";

    // we need to create the temp tables before readDir gets called ( for the dir stats )
    CollectionDB::instance()->createTables( m_db );
    setProgressTotalSteps( 100 );


    QStringList entries;
    foreach( m_folders ) {
        if( (*it).isEmpty() )
            //apparently somewhere empty strings get into the mix
            //which results in a full-system scan! Which we can't allow
            continue;

        QString dir = *it;
        if ( !dir.endsWith( "/" ) )
            dir += '/';

        setStatus( i18n("Reading directory structure") );
        readDir( dir, entries );
    }

    if ( !entries.empty() ) {
        setStatus( i18n("Reading metadata") );
        setProgressTotalSteps( entries.count() );
        readTags( entries );
    }

    if ( !isAborted() ) {
        if ( !m_incremental )
            CollectionDB::instance()->clearTables();
        else
            foreach( m_folders )
                CollectionDB::instance()->removeSongsInDir( *it );

        // rename tables
        CollectionDB::instance()->moveTempTables( m_db );
    }

    CollectionDB::instance()->dropTables( m_db );

    log.close();

    return !isAborted();
}


void
CollectionReader::readDir( const QString& dir, QStringList& entries )
{
    // linux specific, but this fits the 90% rule
    if ( dir == "/dev" || dir == "/sys" || dir == "/proc" )
        return;

    QCString dir8Bit = QFile::encodeName( dir );

    if ( m_processedDirs.contains( dir ) ) {
        debug() << "Skipping, already scanned: " << dir << endl;
        return;
    }

    m_processedDirs << dir;

    struct stat statBuf;
    //update dir statistics for rescanning purposes
    if ( stat( dir8Bit, &statBuf ) == 0 )
        CollectionDB::instance()->updateDirStats( dir, (long)statBuf.st_mtime, !m_incremental ? m_db : 0 );
    else {
        if ( m_incremental ) {
            CollectionDB::instance()->removeSongsInDir( dir );
            CollectionDB::instance()->removeDirFromCollection( dir );
        }
        return;
    }


    DIR *d = opendir( dir8Bit );
    if( d == NULL ) {
        if( errno == EACCES )
            warning() << "Skipping, no access permissions: " << dir << endl;
        return;
    }


    for( dirent *ent; (ent = readdir( d )) && !isAborted(); ) {
        QCString entry = ent->d_name;

        if ( entry == "." || entry == ".." )
            continue;

        entry.prepend( dir8Bit );

        if ( stat( entry, &statBuf ) != 0 )
            continue;

        if ( S_ISDIR( statBuf.st_mode ) && m_recursively )
        {
            const QString file = QFile::decodeName( entry );
            const QFileInfo info( file );
            const QString readLink = info.readLink();

            if ( readLink == "/" || info.isSymLink() && m_processedDirs.contains( readLink ) )
                warning() << "Skipping symlink which points to: " << readLink << endl;

            else if( !m_incremental || !CollectionDB::instance()->isDirInCollection( file ) )
                // we MUST add a '/' after the dirname
                readDir( file + '/', entries );
        }

        else if( S_ISREG( statBuf.st_mode ) )
        {
            const QString file = QFile::decodeName( entry );

            if ( m_importPlaylists ) {
                QString ext = file.right( 4 ).lower();
                if ( ext == ".m3u" || ext == ".pls" )
                    QApplication::postEvent( PlaylistBrowser::instance(), new PlaylistFoundEvent( file ) );
            }

            entries += file;
        }
    }

    closedir( d );
}

void
CollectionReader::readTags( const QStringList& entries )
{
    DEBUG_BLOCK

    typedef QPair<QString, QString> CoverBundle;

    QStringList validImages; validImages << "jpg" << "png" << "gif" << "jpeg";
//    QStringList validMusic; validMusic << "mp3" << "ogg" << "wav" << "flac";

    QValueList<CoverBundle> covers;
    QStringList images;

    foreach( entries )
    {
        // Check if we shall abort the scan
        if( isAborted() )
           return;

        incrementProgress();

        const QString path = *it;
        KURL url; url.setPath( path );
        const QString ext = amaroK::extension( *it );
        const QString dir = amaroK::directory( *it );

        // Append path to logfile
        log << path.local8Bit() << std::endl;
        log.flush();

        // Tests reveal the following:
        //
        // TagLib::AudioProperties   Relative Time Taken
        //
        //  No AudioProp Reading        1
        //  Fast                        1.18
        //  Average                     Untested
        //  Accurate                    Untested

        // don't use the KURL ctor as it checks the db first
        MetaBundle bundle;
        bundle.setPath( path );
        bundle.readTags( TagLib::AudioProperties::Fast );

        if( validImages.contains( ext ) )
           images += path;

        else if( bundle.isValidMedia() )
        {
            CoverBundle cover( bundle.artist(), bundle.album() );

            if( !covers.contains( cover ) )
                covers += cover;

           CollectionDB::instance()->addSong( &bundle, m_incremental, m_db );
        }

        // Update Compilation-flag, when this is the last loop-run
        // or we're going to switch to another dir in the next run
        if( it == entries.fromLast() || dir != amaroK::directory( *++QStringList::ConstIterator( it ) ) )
        {
            // we entered the next directory
            foreach( images )
                CollectionDB::instance()->addImageToAlbum( *it, covers, m_db );

            CollectionDB::instance()->checkCompilations( dir, !m_incremental, m_db );

            // clear now because we've processed them
            covers.clear();
            images.clear();
        }
    }
}
