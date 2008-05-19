/*
 *  Copyright (c) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include "SqlQueryMaker.h"

#define DEBUG_PREFIX "SqlQueryMaker"

#include "Debug.h"

#include "mountpointmanager.h"
#include "SqlCollection.h"

#include <QStack>

#include <threadweaver/Job.h>
#include <threadweaver/ThreadWeaver.h>

using namespace Meta;

class SqlWorkerThread : public ThreadWeaver::Job
{
    public:
        SqlWorkerThread( SqlQueryMaker *queryMaker )
            : ThreadWeaver::Job()
            , m_queryMaker( queryMaker )
            , m_aborted( false )
        {
            //nothing to do
        }

        virtual void requestAbort()
        {
            m_aborted = true;
        }

    protected:
        virtual void run()
        {
            QString query = m_queryMaker->query();
            QStringList result = m_queryMaker->runQuery( query );
            if( !m_aborted )
                m_queryMaker->handleResult( result );
            setFinished( !m_aborted );
        }
    private:
        SqlQueryMaker *m_queryMaker;

        bool m_aborted;
};

struct SqlQueryMaker::Private
{
    enum QueryType { NONE, TRACK, ARTIST, ALBUM, COMPOSER, YEAR, GENRE, CUSTOM };
    enum { TAGS_TAB = 1, ARTIST_TAB = 2, ALBUM_TAB = 4, GENRE_TAB = 8, COMPOSER_TAB = 16, YEAR_TAB = 32, STATISTICS_TAB = 64, URLS_TAB = 128, ALBUMARTIST_TAB = 256 };
    int linkedTables;
    QueryType queryType;
    QString query;
    QString queryReturnValues;
    QString queryFrom;
    QString queryMatch;
    QString queryFilter;
    QString queryOrderBy;
    bool includedBuilder;
    bool collectionRestriction;
    bool resultAsDataPtrs;
    bool withoutDuplicates;
    int maxResultSize;
    AlbumQueryMode albumMode;
    SqlWorkerThread *worker;

    QStack<bool> andStack;
};

SqlQueryMaker::SqlQueryMaker( SqlCollection* collection )
    : QueryMaker()
    , m_collection( collection )
    , d( new Private )
{
    d->includedBuilder = true;
    d->collectionRestriction = false;
    d->worker = 0;
    reset();
}

SqlQueryMaker::~SqlQueryMaker()
{
    delete d;
}

QueryMaker*
SqlQueryMaker::reset()
{
    d->query.clear();
    d->queryType = Private::NONE;
    d->queryReturnValues.clear();
    d->queryFrom.clear();
    d->queryMatch.clear();
    d->queryFilter.clear();
    d->queryOrderBy.clear();
    d->linkedTables = 0;
    if( d->worker && d->worker->isFinished() )
        delete d->worker;   //TODO error handling
    d->resultAsDataPtrs = false;
    d->withoutDuplicates = false;
    d->albumMode = AllAlbums;
    d->maxResultSize = -1;
    d->andStack.clear();
    d->andStack.push( true );   //and is default
    return this;
}

void
SqlQueryMaker::abortQuery()
{
    if( d->worker )
        d->worker->requestAbort();
}

QueryMaker*
SqlQueryMaker::returnResultAsDataPtrs( bool resultAsDataPtrs )
{
    d->resultAsDataPtrs = resultAsDataPtrs;
    return this;
}

void
SqlQueryMaker::run()
{
    if( d->queryType == Private::NONE )
        return; //better error handling?
    if( d->worker && !d->worker->isFinished() )
    {
        //the worker thread seems to be running
        //TODO: wait or job to complete

    }
    else
    {
        //debug() << "Query is " << query();
        d->worker = new SqlWorkerThread( this );
        connect( d->worker, SIGNAL( done( ThreadWeaver::Job* ) ), SLOT( done( ThreadWeaver::Job* ) ) );
        ThreadWeaver::Weaver::instance()->enqueue( d->worker );
    }
}

void
SqlQueryMaker::done( ThreadWeaver::Job *job )
{
    ThreadWeaver::Weaver::instance()->dequeue( job );
    job->deleteLater();
    d->worker = 0;
    emit queryDone();
}

QueryMaker*
SqlQueryMaker::startTrackQuery()
{
    //make sure to keep this method in sync with handleTracks(QStringList) and the SqlTrack ctor
    if( d->queryType == Private::NONE )
    {
        d->queryType = Private::TRACK;
        d->linkedTables |= Private::URLS_TAB;
        d->linkedTables |= Private::TAGS_TAB;
        d->linkedTables |= Private::GENRE_TAB;
        d->linkedTables |= Private::ARTIST_TAB;
        d->linkedTables |= Private::ALBUM_TAB;
        d->linkedTables |= Private::COMPOSER_TAB;
        d->linkedTables |= Private::YEAR_TAB;
        d->linkedTables |= Private::STATISTICS_TAB;
        d->queryReturnValues = SqlTrack::getTrackReturnValues();
    }
    return this;
}

QueryMaker*
SqlQueryMaker::startArtistQuery()
{
    if( d->queryType == Private::NONE )
    {
        d->queryType = Private::ARTIST;
        d->withoutDuplicates = true;
        d->linkedTables |= Private::ARTIST_TAB;
        //reading the ids from the database means we don't have to query for them later
        d->queryReturnValues = "artists.name, artists.id";
    }
    return this;
}

QueryMaker*
SqlQueryMaker::startAlbumQuery()
{
    if( d->queryType == Private::NONE )
    {
        d->queryType = Private::ALBUM;
        d->withoutDuplicates = true;
        d->linkedTables |= Private::ALBUM_TAB;
        //add whatever is necessary to identify compilations
        d->queryReturnValues = "albums.name, albums.id, albums.artist";
    }
    return this;
}

QueryMaker*
SqlQueryMaker::startComposerQuery()
{
    if( d->queryType == Private::NONE )
    {
        d->queryType = Private::COMPOSER;
        d->withoutDuplicates = true;
        d->linkedTables |= Private::COMPOSER_TAB;
        d->queryReturnValues = "composers.name, composers.id";
    }
    return this;
}

QueryMaker*
SqlQueryMaker::startGenreQuery()
{
    if( d->queryType == Private::NONE )
    {
        d->queryType = Private::GENRE;
        d->withoutDuplicates = true;
        d->linkedTables |= Private::GENRE_TAB;
        d->queryReturnValues = "genres.name, genres.id";
    }
    return this;
}

QueryMaker*
SqlQueryMaker::startYearQuery()
{
    if( d->queryType == Private::NONE )
    {
        d->queryType = Private::YEAR;
        d->withoutDuplicates = true;
        d->linkedTables |= Private::YEAR_TAB;
        d->queryReturnValues = "years.name, years.id";
    }
    return this;
}

QueryMaker*
SqlQueryMaker::startCustomQuery()
{
    if( d->queryType == Private::NONE )
        d->queryType = Private::CUSTOM;
    return this;
}

QueryMaker*
SqlQueryMaker::includeCollection( const QString &collectionId )
{
    if( !d->collectionRestriction )
    {
        d->includedBuilder = false;
        d->collectionRestriction = true;
    }
    if( m_collection->collectionId() == collectionId )
        d->includedBuilder = true;
    return this;
}

QueryMaker*
SqlQueryMaker::excludeCollection( const QString &collectionId )
{
    d->collectionRestriction = true;
    if( m_collection->collectionId() == collectionId )
        d->includedBuilder = false;
    return this;
}

QueryMaker*
SqlQueryMaker::addMatch( const TrackPtr &track )
{
    QString url = track->url();
    int deviceid = MountPointManager::instance()->getIdForUrl( url );
    QString rpath = MountPointManager::instance()->getRelativePath( deviceid, url );
    d->queryMatch += QString( " AND urls.deviceid = %1 AND urls.rpath = '%2'" )
                        .arg( QString::number( deviceid ), escape( rpath ) );
    return this;
}

QueryMaker*
SqlQueryMaker::addMatch( const ArtistPtr &artist )
{
    d->linkedTables |= Private::ARTIST_TAB;
    d->queryMatch += QString( " AND artists.name = '%1'" ).arg( escape( artist->name() ) );
    return this;
}

QueryMaker*
SqlQueryMaker::addMatch( const AlbumPtr &album )
{
    d->linkedTables |= Private::ALBUM_TAB;
    //handle compilations
    d->queryMatch += QString( " AND albums.name = '%1'" ).arg( escape( album->name() ) );
    ArtistPtr albumArtist = album->albumArtist();
    if( albumArtist )
    {
        d->linkedTables |= Private::ALBUMARTIST_TAB;
        d->queryMatch += QString( " AND albumartists.name = '%1'" ).arg( escape( albumArtist->name() ) );
    }
    else
    {
        d->queryMatch += " AND albums.artist IS NULL";
    }
    return this;
}

QueryMaker*
SqlQueryMaker::addMatch( const GenrePtr &genre )
{
    d->linkedTables |= Private::GENRE_TAB;
    d->queryMatch += QString( " AND genres.name = '%1'" ).arg( escape( genre->name() ) );
    return this;
}

QueryMaker*
SqlQueryMaker::addMatch( const ComposerPtr &composer )
{
    d->linkedTables |= Private::COMPOSER_TAB;
    d->queryMatch += QString( " AND composers.name = '%1'" ).arg( escape( composer->name() ) );
    return this;
}

QueryMaker*
SqlQueryMaker::addMatch( const YearPtr &year )
{
    d->linkedTables |= Private::YEAR_TAB;
    d->queryMatch += QString( " AND years.name = '%1'" ).arg( escape( year->name() ) );
    return this;
}

QueryMaker*
SqlQueryMaker::addMatch( const DataPtr &data )
{
    ( const_cast<DataPtr&>(data) )->addMatchTo( this );
    return this;
}

QueryMaker*
SqlQueryMaker::addFilter( qint64 value, const QString &filter, bool matchBegin, bool matchEnd )
{
    QString like = likeCondition( escape( filter ), !matchBegin, !matchEnd );
    d->queryFilter += QString( " %1 %2 %3 " ).arg( andOr(), nameForValue( value ), like );
    return this;
}

QueryMaker*
SqlQueryMaker::excludeFilter( qint64 value, const QString &filter, bool matchBegin, bool matchEnd )
{
    QString like = likeCondition( escape( filter ), !matchBegin, !matchEnd );
    d->queryFilter += QString( " %1 NOT %2 %3 " ).arg( andOr(), nameForValue( value ), like );
    return this;
}

QueryMaker*
SqlQueryMaker::addNumberFilter( qint64 value, qint64 filter, QueryMaker::NumberComparison compare )
{
    QString comparison;
    switch( compare )
    {
        case QueryMaker::Equals:
            comparison = "=";
            break;
        case QueryMaker::GreaterThan:
            comparison = ">";
            break;
        case QueryMaker::LessThan:
            comparison = "<";
            break;
    }
    d->queryFilter += QString( " %1 %2 %3 %4 " ).arg( andOr(), nameForValue( value ), comparison, QString::number( filter ) );
    return this;
}

QueryMaker*
SqlQueryMaker::excludeNumberFilter( qint64 value, qint64 filter, QueryMaker::NumberComparison compare )
{
    QString comparison;
    switch( compare )
    {
        case QueryMaker::Equals:
            comparison = "!=";
            break;
        case QueryMaker::GreaterThan:   //negating greater than is less or equal
            comparison = "<=";
            break;
        case QueryMaker::LessThan:      //negating less than is greater or equal
            comparison = ">=";
            break;
    }
    d->queryFilter += QString( " %1 %2 %3 %4 " ).arg( andOr(), nameForValue( value ), comparison, QString::number( filter ) );
    return this;
}

QueryMaker*
SqlQueryMaker::addReturnValue( qint64 value )
{
    if( d->queryType == Private::CUSTOM )
    {
        if ( !d->queryReturnValues.isEmpty() )
            d->queryReturnValues += ',';
        d->queryReturnValues += nameForValue( value );
    }
    return this;
}

QueryMaker*
SqlQueryMaker::addReturnFunction( ReturnFunction function, qint64 value )
{
    if( d->queryType == Private::CUSTOM )
    {
        if( !d->queryReturnValues.isEmpty() )
            d->queryReturnValues += ',';
        QString sqlfunction;
        switch( function )
        {
            case QueryMaker::Count:
                sqlfunction = "COUNT";
                break;
            case QueryMaker::Sum:
                sqlfunction = "SUM";
                break;
            case QueryMaker::Max:
                sqlfunction = "MAX";
                break;
            case QueryMaker::Min:
                sqlfunction = "MIN";
                break;
            default:
                sqlfunction = "Unknown function in SqlQueryMaker::addReturnFunction, function was: " + function;
        }
        d->queryReturnValues += QString( "%1(%2)" ).arg( sqlfunction, nameForValue( value ) );
    }
    return this;
}

QueryMaker*
SqlQueryMaker::orderBy( qint64 value, bool descending )
{
    if ( d->queryOrderBy.isEmpty() )
        d->queryOrderBy = " ORDER BY ";
    d->queryOrderBy += nameForValue( value );
    d->queryOrderBy += QString( " %1 " ).arg( descending ? "DESC" : "ASC" );
    return this;
}

QueryMaker*
SqlQueryMaker::limitMaxResultSize( int size )
{
    d->maxResultSize = size;
    return this;
}

QueryMaker*
SqlQueryMaker::setAlbumQueryMode( AlbumQueryMode mode )
{
    if( mode != AllAlbums )
    {
        d->linkedTables |= Private::ALBUM_TAB;
    }
    d->albumMode = mode;
    return this;
}

QueryMaker*
SqlQueryMaker::beginAnd()
{
    d->queryFilter += andOr();
    d->queryFilter += " ( 1 ";
    d->andStack.push( true );
    return this;
}

QueryMaker*
SqlQueryMaker::beginOr()
{
    d->queryFilter += andOr();
    d->queryFilter += " ( 0 ";
    d->andStack.push( false );
    return this;
}

QueryMaker*
SqlQueryMaker::endAndOr()
{
    d->queryFilter += ')';
    d->andStack.pop();
    return this;
}

void
SqlQueryMaker::linkTables()
{
    d->linkedTables |= Private::TAGS_TAB; //HACK!!!
    //assuming that tracks is always included for now
    if( !d->linkedTables )
        return;

    if( d->linkedTables & Private::URLS_TAB )
        d->queryFrom += " urls";
    if( d->linkedTables & Private::TAGS_TAB )
    {
        if( d->linkedTables & Private::URLS_TAB )
            d->queryFrom += " LEFT JOIN tracks ON urls.id = tracks.url";
        else
            d->queryFrom += " tracks";
    }
    if( d->linkedTables & Private::ARTIST_TAB )
        d->queryFrom += " LEFT JOIN artists ON tracks.artist = artists.id";
    if( d->linkedTables & Private::ALBUM_TAB )
        d->queryFrom += " LEFT JOIN albums ON tracks.album = albums.id";
    if( d->linkedTables & Private::ALBUMARTIST_TAB )
        d->queryFrom += " LEFT JOIN artists AS albumartists ON albums.artist = albumartists.id";
    if( d->linkedTables & Private::GENRE_TAB )
        d->queryFrom += " LEFT JOIN genres ON tracks.genre = genres.id";
    if( d->linkedTables & Private::COMPOSER_TAB )
        d->queryFrom += " LEFT JOIN composers ON tracks.composer = composers.id";
    if( d->linkedTables & Private::YEAR_TAB )
        d->queryFrom += " LEFT JOIN years ON tracks.year = years.id";
    if( d->linkedTables & Private::STATISTICS_TAB )
        if( d->linkedTables & Private::URLS_TAB )
            d->queryFrom += " LEFT JOIN statistics ON urls.id = statistics.url";
        else if( d->linkedTables & Private::TAGS_TAB )
            d->queryFrom += " LEFT JOIN statistics ON tracks.url = statistics.url";
        else
            d->queryFrom += " statistics";
}

void
SqlQueryMaker::buildQuery()
{
    linkTables();
    QString query = "SELECT ";
    if ( d->withoutDuplicates )
        query += "DISTINCT ";
    query += d->queryReturnValues;
    query += " FROM ";
    query += d->queryFrom;
    query += " WHERE 1 ";
    switch( d->albumMode )
    {
        case OnlyNormalAlbums:
            query += " AND albums.artist IS NOT NULL ";
            break;
        case OnlyCompilations:
            query += " AND albums.artist IS NULL ";
            break;
        case AllAlbums:
            //do nothing
            break;
    }
    query += d->queryMatch;
    if ( !d->queryFilter.isEmpty() )
    {
        query += " AND ( 1 ";
        query += d->queryFilter;
        query += " ) ";
    }
    query += d->queryOrderBy;
    if ( d->maxResultSize > -1 )
        query += QString( " LIMIT %1 OFFSET 0 " ).arg( d->maxResultSize );
    query += ';';
    d->query = query;
}

QString
SqlQueryMaker::query()
{
    if ( d->query.isEmpty() )
        buildQuery();
    return d->query;
}

QStringList
SqlQueryMaker::runQuery( const QString &query )
{
    return m_collection->query( query );
}

void
SqlQueryMaker::handleResult( const QStringList &result )
{
    if( !result.isEmpty() )
    {
        switch( d->queryType ) {
        case Private::CUSTOM:
            emit newResultReady( m_collection->collectionId(), result );
            break;
        case Private::TRACK:
            handleTracks( result );
            break;
        case Private::ARTIST:
            handleArtists( result );
            break;
        case Private::ALBUM:
            handleAlbums( result );
            break;
        case Private::GENRE:
            handleGenres( result );
            break;
        case Private::COMPOSER:
            handleComposers( result );
            break;
        case Private::YEAR:
            handleYears( result );
            break;

        case Private::NONE:
            debug() << "Warning: queryResult with queryType == NONE";
        }
    }

    //queryDone will be emitted in done(Job*)
}

QString
SqlQueryMaker::nameForValue( qint64 value )
{
    switch( value )
    {
        case valUrl:
            d->linkedTables |= Private::TAGS_TAB;
            return "tags.url";  //TODO figure out how to handle deviceid
        case valTitle:
            d->linkedTables |= Private::TAGS_TAB;
            return "tracks.title";
        case valArtist:
            d->linkedTables |= Private::ARTIST_TAB;
            return "artists.name";
        case valAlbum:
            d->linkedTables |= Private::ALBUM_TAB;
            return "albums.name";
        case valGenre:
            d->linkedTables |= Private::GENRE_TAB;
            return "genres.name";
        case valComposer:
            d->linkedTables |= Private::COMPOSER_TAB;
            return "composers.name";
        case valYear:
            d->linkedTables |= Private::YEAR_TAB;
            return "years.name";
        case valComment:
            d->linkedTables |= Private::TAGS_TAB;
            return "tracks.comment";
        case valTrackNr:
            d->linkedTables |= Private::TAGS_TAB;
            return "tracks.tracknumber";
        case valDiscNr:
            d->linkedTables |= Private::TAGS_TAB;
            return "tracks.discnumber";
        case valLength:
            d->linkedTables |= Private::TAGS_TAB;
            return "tracks.length";
        case valBitrate:
            d->linkedTables |= Private::TAGS_TAB;
            return "tracks.bitrate";
        case valSamplerate:
            d->linkedTables |= Private::TAGS_TAB;
            return "tracks.samplerate";
        case valFilesize:
            d->linkedTables |= Private::TAGS_TAB;
            return "tracks.filesize";
        case valFormat:
            d->linkedTables |= Private::TAGS_TAB;
            return "tracks.filetype";
        case valScore:
            d->linkedTables |= Private::STATISTICS_TAB;
            return "statistics.score";
        case valRating:
            d->linkedTables |= Private::STATISTICS_TAB;
            return "statistics.rating";
        case valFirstPlayed:
            d->linkedTables |= Private::STATISTICS_TAB;
            return "statistics.createdate";
        case valLastPlayed:
            d->linkedTables |= Private::STATISTICS_TAB;
            return "statistics.accessdate";
        case valPlaycount:
            d->linkedTables |= Private::STATISTICS_TAB;
            return "statistics.playcount";
        default:
            return "ERROR: unknown value in SqlQueryMaker::nameForValue(qint64): value=" + value;
    }
}

QString
SqlQueryMaker::andOr() const
{
    return d->andStack.top() ? " AND " : " OR ";
}

// What's worse, a bunch of almost identical repeated code, or a not so obvious macro? :-)
// The macro below will emit the proper result signal. If m_resultAsDataPtrs is true,
// it'll emit the signal that takes a list of DataPtrs. Otherwise, it'll call the
// signal that takes the list of the specific class.

#define emitProperResult( PointerType, list ) { \
            if ( d->resultAsDataPtrs ) { \
                DataList data; \
                foreach( PointerType p, list ) { \
                    data << DataPtr::staticCast( p ); \
                } \
                emit newResultReady( m_collection->collectionId(), data ); \
            } \
            else { \
                emit newResultReady( m_collection->collectionId(), list ); \
            } \
        }

void
SqlQueryMaker::handleTracks( const QStringList &result )
{
    TrackList tracks;
    SqlRegistry* reg = m_collection->registry();
    //there are 29 columns in the result set as generated by startTrackQuery()
    int returnCount = SqlTrack::getTrackReturnValueCount();
    int resultRows = result.size() / returnCount;
    for( int i = 0; i < resultRows; i++ )
    {
        QStringList row = result.mid( i*returnCount, returnCount );
        tracks.append( reg->getTrack( row ) );
    }
    emitProperResult( TrackPtr, tracks );
}

void
SqlQueryMaker::handleArtists( const QStringList &result )
{
    ArtistList artists;
    SqlRegistry* reg = m_collection->registry();
    for( QStringListIterator iter( result ); iter.hasNext(); )
    {
        QString name = iter.next();
        QString id = iter.next();
        artists.append( reg->getArtist( name, id.toInt() ) );
    }
    emitProperResult( ArtistPtr, artists );
}

void
SqlQueryMaker::handleAlbums( const QStringList &result )
{
    AlbumList albums;
    SqlRegistry* reg = m_collection->registry();
    for( QStringListIterator iter( result ); iter.hasNext(); )
    {
        QString name = iter.next();
        QString id = iter.next();
        QString artist = iter.next();
        albums.append( reg->getAlbum( name, id.toInt(), artist.toInt() ) );
    }
    emitProperResult( AlbumPtr, albums );
}

void
SqlQueryMaker::handleGenres( const QStringList &result )
{
    GenreList genres;
    SqlRegistry* reg = m_collection->registry();
    for( QStringListIterator iter( result ); iter.hasNext(); )
    {
        QString name = iter.next();
        QString id = iter.next();
        genres.append( reg->getGenre( name, id.toInt() ) );
    }
    emitProperResult( GenrePtr, genres );
}

void
SqlQueryMaker::handleComposers( const QStringList &result )
{
    ComposerList composers;
    SqlRegistry* reg = m_collection->registry();
    for( QStringListIterator iter( result ); iter.hasNext(); )
    {
        QString name = iter.next();
        QString id = iter.next();
        composers.append( reg->getComposer( name, id.toInt() ) );
    }
    emitProperResult( ComposerPtr, composers );
}

void
SqlQueryMaker::handleYears( const QStringList &result )
{
    YearList years;
    SqlRegistry* reg = m_collection->registry();
    for( QStringListIterator iter( result ); iter.hasNext(); )
    {
        QString name = iter.next();
        QString id = iter.next();
        years.append( reg->getYear( name, id.toInt() ) );
    }
    emitProperResult( YearPtr, years );
}

QString
SqlQueryMaker::escape( QString text ) const           //krazy:exclude=constref
{
    return text.replace( '\'', "''" );;
}

QString
SqlQueryMaker::likeCondition( const QString &text, bool anyBegin, bool anyEnd ) const
{
    if( anyBegin || anyEnd )
    {
        QString escaped = text;
        escaped.replace( '/', "//" ).replace( '%', "/%" ).replace( '_', "/_" );
        escaped = escape( escaped );

        QString ret = " LIKE ";

        ret += '\'';
        if ( anyBegin )
            ret += '%';
        ret += escaped;
        if ( anyEnd )
            ret += '%';
        ret += '\'';

        //Use / as the escape character
        ret += " ESCAPE '/' ";

        return ret;
    }
    else
    {
        return QString( " = '%1' " ).arg( text );
    }
}

#include "SqlQueryMaker.moc"
