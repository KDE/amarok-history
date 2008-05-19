/*
   Copyright (C) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>
             (c) 2007  Nikolaj Hald Nielsen <nhnFreespirit@gmail.com> 

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include "MemoryQueryMaker.h"
#include "MemoryFilter.h"
#include "MemoryMatcher.h"
#include "Debug.h"

#include <threadweaver/Job.h>
#include <threadweaver/ThreadWeaver.h>

#include <QSet>
#include <QStack>

using namespace Meta;

//QueryJob

class QueryJob : public ThreadWeaver::Job
{
    public:
        QueryJob( MemoryQueryMaker *qm )
            : ThreadWeaver::Job()
            , m_queryMaker( qm )
        {
            //nothing to do
        }

    protected:
        void run()
        {
            m_queryMaker->runQuery();
            setFinished( true );
        }

    private:
        MemoryQueryMaker *m_queryMaker;
};

struct MemoryQueryMaker::Private {
    enum QueryType { NONE, TRACK, ARTIST, ALBUM, COMPOSER, YEAR, GENRE, CUSTOM };
    QueryType type;
    bool returnDataPtrs;
    MemoryMatcher* matcher;
    QueryJob *job;
    int maxsize;
    QStack<ContainerMemoryFilter*> containerFilters;
    bool usingFilters;
};

MemoryQueryMaker::MemoryQueryMaker( MemoryCollection *mc, const QString &collectionId )
    : QueryMaker()
    , m_memCollection( mc )
    , m_collectionId( collectionId )
    ,d( new Private )
{
    d->matcher = 0;
    d->job = 0;
    reset();
}

MemoryQueryMaker::~MemoryQueryMaker()
{
    if( !d->containerFilters.isEmpty() )
        delete d->containerFilters.first();
    delete d;
}

QueryMaker*
MemoryQueryMaker::reset()
{
    d->type = Private::NONE;
    d->returnDataPtrs = false;
    delete d->matcher;
    delete d->job;
    d->maxsize = -1;
    if( !d->containerFilters.isEmpty() )
        delete d->containerFilters.first();
    d->containerFilters.clear();
    d->containerFilters.push( new AndContainerMemoryFilter() );
    d->usingFilters = false;
    return this;
}

void
MemoryQueryMaker::run()
{
    if ( d->type == Private::NONE )
        //TODO error handling
        return;
    else if( d->job && !d->job->isFinished() )
    {
        //the worker thread seems to be running
        //TODO: wait or job to complete

    }
    else
    {
        d->job = new QueryJob( this );
        connect( d->job, SIGNAL( done( ThreadWeaver::Job * ) ), SLOT( done( ThreadWeaver::Job * ) ) );
        ThreadWeaver::Weaver::instance()->enqueue( d->job );
    }
}

void
MemoryQueryMaker::abortQuery()
{
}

void
MemoryQueryMaker::runQuery()
{
    m_memCollection->acquireReadLock();
    //naive implementation, fix this
    //note: we are not handling filtering yet
    if ( d->matcher )
    {
        TrackList result = d->matcher->match( m_memCollection );
        if ( d->usingFilters )
        {
            TrackList filtered;
            foreach( TrackPtr track, result )
                if( d->containerFilters.first()->filterMatches( track ) )
                    filtered.append( track );
            handleResult( filtered );
        }
        else
            handleResult( result );
    }
    else if ( d->usingFilters )
    {
        TrackList tracks = m_memCollection->trackMap().values();
        TrackList filtered;
        foreach( TrackPtr track, tracks )
            if ( d->containerFilters.first()->filterMatches( track ) )
                filtered.append( track );
        handleResult( filtered );
    }
    else
        handleResult();
    m_memCollection->releaseLock();
}

// What's worse, a bunch of almost identical repeated code, or a not so obvious macro? :-)
// The macro below will emit the proper result signal. If m_resultAsDataPtrs is true,
// it'll emit the signal that takes a list of DataPtrs. Otherwise, it'll call the
// signal that takes the list of the specific class.
// (copied from sqlquerybuilder.cpp with a few minor tweaks)

#define emitProperResult( PointerType, list ) { \
            if ( d->returnDataPtrs ) { \
                DataList data; \
                foreach( PointerType p, list ) { \
                    data << DataPtr::staticCast( p ); \
                } \
                emit newResultReady( m_collectionId, data ); \
            } \
            else { \
                emit newResultReady( m_collectionId, list ); \
            } \
        }

void
MemoryQueryMaker::handleResult()
{
    //this gets called when we want to return all values for the given query type
    switch( d->type )
    {
        case Private::TRACK :
        {
            TrackList tracks = m_memCollection->trackMap().values();
            if ( d->maxsize >= 0 && tracks.count() > d->maxsize )
                tracks = tracks.mid( 0, d->maxsize );
            emitProperResult( TrackPtr, tracks );
            break;
        }
        case Private::ALBUM :
        {
            AlbumList albums = m_memCollection->albumMap().values();
            if ( d->maxsize >= 0 && albums.count() > d->maxsize )
                albums = albums.mid( 0, d->maxsize );
            emitProperResult( AlbumPtr, albums );
            break;
        }
        case Private::ARTIST :
        {
            ArtistList artists = m_memCollection->artistMap().values();
            if ( d->maxsize >= 0 && artists.count() > d->maxsize )
                artists = artists.mid( 0, d->maxsize );
            emitProperResult( ArtistPtr, artists );
            break;
        }
        case Private::COMPOSER :
        {
            ComposerList composers = m_memCollection->composerMap().values();
            if ( d->maxsize >= 0 && composers.count() > d->maxsize )
                composers = composers.mid( 0, d->maxsize );
            emitProperResult( ComposerPtr, m_memCollection->composerMap().values() );
            break;
        }
        case Private::GENRE :
        {
            GenreList genres = m_memCollection->genreMap().values();
            if ( d->maxsize >= 0 && genres.count() > d->maxsize )
                genres = genres.mid( 0, d->maxsize );
            emitProperResult( GenrePtr, genres );
            break;
        }
        case Private::YEAR :
        {
            YearList years = m_memCollection->yearMap().values();
            if ( d->maxsize >= 0 && years.count() > d->maxsize )
                years = years.mid( 0, d->maxsize );
            emitProperResult( YearPtr, years );
            break;
        }
        case Private::CUSTOM :
            //TODO stub, fix this
            break;
        case Private::NONE :
            //nothing to do
            break;
    }
}

void
MemoryQueryMaker::handleResult( const TrackList &tracks )
{
    switch( d->type )
    {
        case Private::TRACK :
            if ( d->maxsize < 0 || tracks.count() <= d->maxsize )
            {
                emitProperResult( TrackPtr, tracks );
            }
            else
            {
                TrackList newResult = tracks.mid( 0, d->maxsize );
                emitProperResult( TrackPtr, newResult );
            }
            break;
        case Private::ALBUM :
        {
            QSet<AlbumPtr> albumSet;
            foreach( TrackPtr track, tracks )
            {
                if ( d->maxsize >= 0 && albumSet.count() == d->maxsize )
                    break;
                albumSet.insert( track->album() );
            }
            emitProperResult( AlbumPtr, albumSet.toList() );
            break;
        }
        case Private::ARTIST :
        {
            QSet<ArtistPtr> artistSet;
            foreach( TrackPtr track, tracks )
            {
                if ( d->maxsize >= 0 && artistSet.count() == d->maxsize )
                    break;
                artistSet.insert( track->artist() );
            }
            emitProperResult( ArtistPtr, artistSet.toList() );
            break;
        }
        case Private::GENRE :
        {
            QSet<GenrePtr> genreSet;
            foreach( TrackPtr track, tracks )
            {
                if ( d->maxsize >= 0 && genreSet.count() == d->maxsize )
                    break;
                genreSet.insert( track->genre() );
            }
            emitProperResult( GenrePtr, genreSet.toList() );
            break;
        }
        case Private::COMPOSER :
        {
            QSet<ComposerPtr> composerSet;
            foreach( TrackPtr track, tracks )
            {
                if ( d->maxsize >= 0 && composerSet.count() == d->maxsize )
                    break;
                composerSet.insert( track->composer() );
            }
            emitProperResult( ComposerPtr, composerSet.toList() );
            break;
        }
        case Private::YEAR :
        {
            QSet<YearPtr> yearSet;
            foreach( TrackPtr track, tracks )
            {
                if ( d->maxsize >= 0 && yearSet.count() == d->maxsize )
                    break;
                yearSet.insert( track->year() );
            }
            emitProperResult( YearPtr, yearSet.toList() );
            break;
        }
        case Private::CUSTOM :
            //hmm, not sure if this makes sense
            break;
        case Private::NONE:
            //should never happen, but handle error anyway
            break;
    }
}

QueryMaker*
MemoryQueryMaker::startTrackQuery()
{
    if ( d->type == Private::NONE )
        d->type = Private::TRACK;
    return this;
}

QueryMaker*
MemoryQueryMaker::startArtistQuery()
{
    if ( d->type == Private::NONE )
        d->type = Private::ARTIST;
    return this;
}

QueryMaker*
MemoryQueryMaker::startAlbumQuery()
{
    if ( d->type == Private::NONE )
        d->type = Private::ALBUM;
    return this;
}

QueryMaker*
MemoryQueryMaker::startComposerQuery()
{
    if ( d->type == Private::NONE )
        d->type = Private::COMPOSER;
    return this;
}

QueryMaker*
MemoryQueryMaker::startGenreQuery()
{
    if ( d->type == Private::NONE )
        d->type = Private::GENRE;
    return this;
}

QueryMaker*
MemoryQueryMaker::startYearQuery()
{
    if ( d->type == Private::NONE )
        d->type = Private::YEAR;
    return this;
}

QueryMaker*
MemoryQueryMaker::startCustomQuery()
{
    if ( d->type == Private::CUSTOM )
        d->type = Private::CUSTOM;
    return this;
}

QueryMaker*
MemoryQueryMaker::returnResultAsDataPtrs( bool resultAsDataPtrs )
{
    d->returnDataPtrs = resultAsDataPtrs;
    return this;
}

QueryMaker*
MemoryQueryMaker::addReturnValue( qint64 value )
{
    Q_UNUSED( value );
    //TODO stub
    return this;
}

QueryMaker*
MemoryQueryMaker::addReturnFunction( ReturnFunction function, qint64 value )
{
    Q_UNUSED( value )
    Q_UNUSED( function )
    //TODO stub
    return this;
}

QueryMaker*
MemoryQueryMaker::orderBy( qint64 value, bool descending )
{
    Q_UNUSED( value ); Q_UNUSED( descending );
    //TODO stub
    return this;
}

QueryMaker*
MemoryQueryMaker::includeCollection( const QString &collectionId )
{
    Q_UNUSED( collectionId );
    //TODO stub
    return this;
}

QueryMaker*
MemoryQueryMaker::excludeCollection( const QString &collectionId )
{
    Q_UNUSED( collectionId );
    //TODO stub
    return this;
}

QueryMaker*
MemoryQueryMaker::addMatch( const TrackPtr &track )
{
    MemoryMatcher *trackMatcher = new TrackMatcher( track );
    if ( d->matcher == 0 )
        d->matcher = trackMatcher;
    else
    {
        MemoryMatcher *tmp = d->matcher;
        while ( !tmp->isLast() )
            tmp = tmp->next();
        tmp->setNext( trackMatcher );
    }
    return this;
}

QueryMaker*
MemoryQueryMaker::addMatch( const ArtistPtr &artist )
{
    MemoryMatcher *artistMatcher = new ArtistMatcher( artist );
    if ( d->matcher == 0 )
        d->matcher = artistMatcher;
    else
    {
        MemoryMatcher *tmp = d->matcher;
        while ( !tmp->isLast() )
            tmp = tmp->next();
        tmp->setNext( artistMatcher );
    }
    return this;
}

QueryMaker*
MemoryQueryMaker::addMatch( const AlbumPtr &album )
{
    MemoryMatcher *albumMatcher = new AlbumMatcher( album );
    if ( d->matcher == 0 )
        d->matcher = albumMatcher;
    else
    {
        MemoryMatcher *tmp = d->matcher;
        while ( !tmp->isLast() )
            tmp = tmp->next();
        tmp->setNext( albumMatcher );
    }
    return this;
}

QueryMaker*
MemoryQueryMaker::addMatch( const GenrePtr &genre )
{
    MemoryMatcher *genreMatcher = new GenreMatcher( genre );
    if ( d->matcher == 0 )
        d->matcher = genreMatcher;
    else
    {
        MemoryMatcher *tmp = d->matcher;
        while ( !tmp->isLast() )
            tmp = tmp->next();
        tmp->setNext( genreMatcher );
    }
    return this;
}

QueryMaker*
MemoryQueryMaker::addMatch( const ComposerPtr &composer )
{
    MemoryMatcher *composerMatcher = new ComposerMatcher( composer );
    if ( d->matcher == 0 )
        d->matcher = composerMatcher;
    else
    {
        MemoryMatcher *tmp = d->matcher;
        while ( !tmp->isLast() )
            tmp = tmp->next();
        tmp->setNext( composerMatcher );
    }
    return this;
}

QueryMaker*
MemoryQueryMaker::addMatch( const YearPtr &year )
{
    MemoryMatcher *yearMatcher = new YearMatcher( year );
    if ( d->matcher == 0 )
        d->matcher = yearMatcher;
    else
    {
        MemoryMatcher *tmp = d->matcher;
        while ( !tmp->isLast() )
            tmp = tmp->next();
        tmp->setNext( yearMatcher );
    }
    return this;
}

QueryMaker*
MemoryQueryMaker::addMatch( const DataPtr &data )
{
    ( const_cast<DataPtr&>(data) )->addMatchTo( this );
    return this;
}

QueryMaker*
MemoryQueryMaker::addFilter( qint64 value, const QString &filter, bool matchBegin, bool matchEnd )
{
    d->containerFilters.top()->addFilter( FilterFactory::filter( value, filter, matchBegin, matchEnd ) );
    d->usingFilters = true;
    return this;
}

QueryMaker*
MemoryQueryMaker::excludeFilter( qint64 value, const QString &filter, bool matchBegin, bool matchEnd )
{
    MemoryFilter *tmp = FilterFactory::filter( value, filter, matchBegin, matchEnd );
    d->containerFilters.top()->addFilter( new NegateMemoryFilter( tmp ) );
    d->usingFilters = true;
    return this;
}

QueryMaker*
MemoryQueryMaker::addNumberFilter( qint64 value, qint64 filter, NumberComparison compare )
{
    d->containerFilters.top()->addFilter( FilterFactory::numberFilter( value, filter, compare ) );
    d->usingFilters = true;
    return this;
}

QueryMaker*
MemoryQueryMaker::excludeNumberFilter( qint64 value, qint64 filter, NumberComparison compare )
{
    MemoryFilter *tmp = FilterFactory::numberFilter( value, filter, compare );
    d->containerFilters.top()->addFilter( new NegateMemoryFilter( tmp ) );
    d->usingFilters = true;
    return this;
}

QueryMaker*
MemoryQueryMaker::limitMaxResultSize( int size )
{
    d->maxsize = size;
    return this;
}

QueryMaker*
MemoryQueryMaker::beginAnd()
{
    ContainerMemoryFilter *filter = new AndContainerMemoryFilter();
    d->containerFilters.top()->addFilter( filter );
    d->containerFilters.push( filter );
    return this;
}

QueryMaker*
MemoryQueryMaker::beginOr()
{
    ContainerMemoryFilter *filter = new OrContainerMemoryFilter();
    d->containerFilters.top()->addFilter( filter );
    d->containerFilters.push( filter );
    return this;
}

QueryMaker*
MemoryQueryMaker::endAndOr()
{
    d->containerFilters.pop();
    return this;
}

void
MemoryQueryMaker::done( ThreadWeaver::Job *job )
{
    ThreadWeaver::Weaver::instance()->dequeue( job );
    job->deleteLater();
    d->job = 0;
    emit queryDone();
}

#include "MemoryQueryMaker.moc"