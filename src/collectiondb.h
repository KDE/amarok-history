// (c) 2004 Mark Kretschmann <markey@web.de>
// (c) 2004 Christian Muehlhaeuser <chris@chris.de>
// See COPYING file for licensing information.

#ifndef AMAROK_COLLECTIONDB_H
#define AMAROK_COLLECTIONDB_H

#include <amarokconfig.h>
#include <qobject.h>         //baseclass
#include <qstringlist.h>     //stack allocated
#include <qdir.h>            //stack allocated
#include "sqlite/sqlite3.h"

class ThreadWeaver;
class MetaBundle;

class CollectionDB : public QObject
{
    Q_OBJECT

    public:

        CollectionDB();
        ~CollectionDB();

        bool isDbValid();
        bool isEmpty();
        QString albumSongCount( const QString artist_id, const QString album_id );

        QString getPathForAlbum( const uint artist_id, const uint album_id );
        QString getPathForAlbum( const QString artist, const QString album );

        bool setImageForAlbum( const QString& artist, const QString& album, const QString& url, QImage pix );
        QString getImageForAlbum( const uint artist_id, const uint album_id, const uint width = AmarokConfig::coverPreviewSize() );
        QString getImageForAlbum( const QString artist, const QString album, const uint width = AmarokConfig::coverPreviewSize() );
        bool removeImageFromAlbum( const uint artist_id, const uint album_id );
        bool removeImageFromAlbum( const QString artist, const QString album );

        QString getImageForPath( const QString path, uint width = AmarokConfig::coverPreviewSize() );
        void addImageToPath( const QString path, const QString image, bool temporary );

        QStringList artistList( bool withUnknown = true, bool withCompilations = true );
        QStringList albumList( bool withUnknown = true, bool withCompilations = true );
        QStringList albumListOfArtist( const QString artist, bool withUnknown = true, bool withCompilations = true );
        QStringList artistAlbumList( bool withUnknown = true, bool withCompilations = true );

        bool getMetaBundleForUrl( const QString url, MetaBundle *bundle );
        uint addSongPercentage( const QString url, const int percentage );
        uint getSongPercentage( const QString url );
        void updateDirStats( QString path, const long datetime );
        void removeSongsInDir( QString path );
        bool isDirInCollection( QString path );
        bool isFileInCollection( const QString url );
        bool isSamplerAlbum( const QString album );
        void removeDirFromCollection( QString path );

        /**
         * Executes an SQL statement on the already opened database
         * @param statement SQL program to execute. Only one SQL statement is allowed.
         * @retval values   will contain the queried data, set to NULL if not used
         * @retval names    will contain all column names, set to NULL if not used
         * @return          true if successful
         */
        bool execSql( const QString& statement, QStringList* const values = 0, QStringList* const names = 0, const bool debug = false );

        /**
         * Returns the rowid of the most recently inserted row
         * @return          int rowid
         */
        int sqlInsertID();
        QString escapeString( QString string );

        uint getValueID( QString name, QString value, bool autocreate = true, bool useTempTables = false );
        QString getValueFromID( QString table, uint id );
        void createTables( const bool temporary = false );
        void dropTables( const bool temporary = false );
        void moveTempTables();
        void createStatsTable();
        void dropStatsTable();

        void purgeDirCache();
        void scanModifiedDirs( bool recursively, bool importPlaylists );
        void scan( const QStringList& folders, bool recursively, bool importPlaylists );
        void updateTags( const QString &url, const MetaBundle &bundle, bool updateCB=true );
        void updateTag( const QString &url, const QString &field, const QString &newTag );

        void retrieveFirstLevel( QString category1, QString category2, QString category3,
                                            QString filter, QStringList* const values, QStringList* const names );
        void retrieveSecondLevel( QString itemText, QString category1, QString category2, QString category3,
                                                 QString filter, QStringList* const values, QStringList* const names );
        void retrieveThirdLevel( QString itemText1, QString itemText2, QString category1, QString category2,
                                             QString category3, QString filter, QStringList* const values, QStringList* const names );
        void retrieveFourthLevel( QString itemText1, QString itemText2, QString itemText3, QString category1,
                                               QString category2, QString category3, QString filter,
                                               QStringList* const values, QStringList* const names );

        void retrieveFirstLevelURLs( QString itemText, QString category1, QString category2, QString category3,
                                                    QString filter, QStringList* const values, QStringList* const names );
        void retrieveSecondLevelURLs( QString itemText1, QString itemText2, QString category1, QString category2,
                                                         QString category3, QString filter,
                                                         QStringList* const values, QStringList* const names );
        void retrieveThirdLevelURLs( QString itemText1, QString itemText2, QString itemText3, QString category1,
                                                     QString category2, QString category3, QString filter,
                                                     QStringList* const values, QStringList* const names );

        QString m_amazonLicense;

    signals:
        void scanDone( bool changed );
        void coverFetched( const QString &keyword );
        void coverFetched();
        void coverFetcherError();

    public slots:
        void fetchCover( QObject* parent, const QString& artist, const QString& album, bool noedit );
        void stopScan();

    private slots:
        void dirDirty( const QString& path );
        void saveCover( const QString& keyword, const QString& url, const QImage& image );
        void fetcherError();

    private:
        void customEvent( QCustomEvent* );

        sqlite3* m_db;
        ThreadWeaver* m_weaver;
        bool m_monitor;
        QDir m_cacheDir;
        QDir m_coverDir;

        QString m_cacheArtist;
        uint m_cacheArtistID;
        QString m_cacheAlbum;
        uint m_cacheAlbumID;
};


#endif /* AMAROK_COLLECTIONDB_H */
