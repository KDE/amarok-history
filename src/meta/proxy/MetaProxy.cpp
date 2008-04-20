/*
   Copyright (C) 2007 Maximilian Kossick <maximilian.kossick@googlemail.com>

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

#include "MetaProxy.h"
#include "MetaProxy_p.h"
#include "MetaProxy_p.moc"

#include "debug.h"
#include "EditCapability.h"

#include "CollectionManager.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <KSharedPtr>

using namespace MetaProxy;

class ProxyArtist;
class ProxyFmAlbum;
class ProxyGenre;
class ProxyComposer;
class ProxyYear;

class EditCapabilityProxy : public Meta::EditCapability
{
    public:
        EditCapabilityProxy( MetaProxy::Track *track )
            : Meta::EditCapability()
            , m_track( track ) {}

        virtual bool isEditable() const { return true; }
        virtual void setTitle( const QString &title ) { m_track->setName( title ); }
        virtual void setAlbum( const QString &newAlbum ) { m_track->setAlbum( newAlbum ); }
        virtual void setArtist( const QString &newArtist ) { m_track->setArtist( newArtist ); }
        virtual void setComposer( const QString &newComposer ) { m_track->setComposer( newComposer ); }
        virtual void setGenre( const QString &newGenre ) { m_track->setGenre( newGenre ); }
        virtual void setYear( const QString &newYear ) { m_track->setYear( newYear ); }
        virtual void setComment( const QString &newComment ) { Q_UNUSED( newComment ); /*m_track->setComment( newComment );*/ } // Do we want to support this?
        virtual void setTrackNumber( int newTrackNumber ) { m_track->setTrackNumber( newTrackNumber ); }
        virtual void setDiscNumber( int newDiscNumber ) { m_track->setDiscNumber( newDiscNumber ); }

        virtual void beginMetaDataUpdate() {}  // Nothing to do, we cache everything
        virtual void endMetaDataUpdate() {}
        virtual void abortMetaDataUpdate() {}

    private:
        KSharedPtr<MetaProxy::Track> m_track;
};

MetaProxy::Track::Track( const KUrl &url )
    : Meta::Track()
    , d( new Private() )
{
    d->url = url;
    d->proxy = this;
    d->cachedLength = 0;

    QObject::connect( CollectionManager::instance(), SIGNAL( trackProviderAdded( TrackProvider* ) ), d, SLOT( slotNewTrackProvider( TrackProvider* ) ) );

    d->albumPtr = Meta::AlbumPtr( new ProxyAlbum( QPointer<Track::Private>( d ) ) );
    d->artistPtr = Meta::ArtistPtr( new ProxyArtist( QPointer<Track::Private>( d ) ) );
    d->genrePtr = Meta::GenrePtr( new ProxyGenre( QPointer<Track::Private>( d ) ) );
    d->composerPtr = Meta::ComposerPtr( new ProxyComposer( QPointer<Track::Private>( d ) ) );
    d->yearPtr = Meta::YearPtr( new ProxyYear( QPointer<Track::Private>( d ) ) );

    QTimer::singleShot( 0, d, SLOT( slotCheckCollectionManager() ) );
}

MetaProxy::Track::~Track()
{
    delete d;
}

QString
MetaProxy::Track::name() const
{
    if( d->realTrack )
        return d->realTrack->name();
    else
        return d->cachedName;
}

void
MetaProxy::Track::setName( const QString &name )
{
    d->cachedName = name;
}

QString
MetaProxy::Track::prettyName() const
{
    if( d->realTrack )
        return d->realTrack->prettyName();
    else
        return d->cachedName;   //TODO maybe change this?
}

QString
MetaProxy::Track::fullPrettyName() const
{
    if( d->realTrack )
        return d->realTrack->fullPrettyName();
    else
        return d->cachedName;   //TODO maybe change this??
}

QString
MetaProxy::Track::sortableName() const
{
    if( d->realTrack )
        return d->realTrack->sortableName();
    else
        return d->cachedName;   //TODO change this?
}

KUrl
MetaProxy::Track::playableUrl() const
{
    if( d->realTrack )
        return d->realTrack->playableUrl();
    else
//         return KUrl();
        return d->url; // Maybe?
}

QString
MetaProxy::Track::prettyUrl() const
{
    if( d->realTrack )
        return d->realTrack->prettyUrl();
    else
        return d->url.url();
}

QString
MetaProxy::Track::url() const
{
    if( d->realTrack )
        return d->realTrack->url();
    else
        return d->url.url();
}

bool
MetaProxy::Track::isPlayable() const
{
    if( d->realTrack )
        return d->realTrack->isPlayable();
    else
        return false;
}

Meta::AlbumPtr
MetaProxy::Track::album() const
{
    return d->albumPtr;
}

void
MetaProxy::Track::setAlbum( const QString &album )
{
    d->cachedAlbum = album;
}

Meta::ArtistPtr
MetaProxy::Track::artist() const
{
    return d->artistPtr;
}

void
MetaProxy::Track::setArtist( const QString &artist )
{
    d->cachedArtist = artist;
}

Meta::GenrePtr
MetaProxy::Track::genre() const
{
    return d->genrePtr;
}

void
MetaProxy::Track::setGenre( const QString &genre )
{
    d->cachedGenre = genre;
}

Meta::ComposerPtr
MetaProxy::Track::composer() const
{
    return d->composerPtr;
}

void
MetaProxy::Track::setComposer( const QString &composer )
{
    d->cachedComposer = composer;
}
Meta::YearPtr
MetaProxy::Track::year() const
{
    return d->yearPtr;
}

void
MetaProxy::Track::setYear( const QString &year )
{
    d->cachedYear = year;
}

QString
MetaProxy::Track::comment() const
{
    if( d->realTrack )
        return d->realTrack->comment();
    else
        return QString();       //do we cache the comment??
}

double
MetaProxy::Track::score() const
{
    if( d->realTrack )
        return d->realTrack->score();
    else
        return 0.0;     //do we cache the score
}

void
MetaProxy::Track::setScore( double newScore )
{
    if( d->realTrack )
        d->realTrack->setScore( newScore );
}

int
MetaProxy::Track::rating() const
{
    if( d->realTrack )
        return d->realTrack->rating();
    else
        return 0;
}

void
MetaProxy::Track::setRating( int newRating )
{
    if( d->realTrack )
        d->realTrack->setRating( newRating );
}

int
MetaProxy::Track::trackNumber() const
{
    if( d->realTrack )
        return d->realTrack->trackNumber();
    else
        return d->cachedTrackNumber;
}

void
MetaProxy::Track::setTrackNumber( int number )
{
    d->cachedTrackNumber = number;
}

int
MetaProxy::Track::discNumber() const
{
    if( d->realTrack )
        return d->realTrack->discNumber();
    else
        return d->cachedDiscNumber;
}

void
MetaProxy::Track::setDiscNumber( int discNumber )
{
    d->cachedDiscNumber = discNumber;
}

int
MetaProxy::Track::length() const
{
    if( d->realTrack )
        return d->realTrack->length();
    else
        return d->cachedLength;
}

int
MetaProxy::Track::filesize() const
{
    if( d->realTrack )
        return d->realTrack->filesize();
    else
        return 0;
}

int
MetaProxy::Track::sampleRate() const
{
    if( d->realTrack )
        return d->realTrack->sampleRate();
    else
        return 0;
}

int
MetaProxy::Track::bitrate() const
{
    if( d->realTrack )
        return d->realTrack->bitrate();
    else
        return 0;
}

uint
MetaProxy::Track::lastPlayed() const
{
    if( d->realTrack )
        return d->realTrack->lastPlayed();
    else
        return 0;
}

int
MetaProxy::Track::playCount() const
{
    if( d->realTrack )
        return d->realTrack->playCount();
    else
        return 0;
}

QString
MetaProxy::Track::type() const
{
    if( d->realTrack )
        return d->realTrack->type();
    else
        return QString();       //TODO cache type??
}

void
MetaProxy::Track::finishedPlaying( double playedFraction )
{
    if( d->realTrack )
        d->realTrack->finishedPlaying( playedFraction );
}

bool
MetaProxy::Track::inCollection() const
{
    if( d->realTrack )
        return d->realTrack->inCollection();
    else
        return false;
}

Collection*
MetaProxy::Track::collection() const
{
    if( d->realTrack )
        return d->realTrack->collection();
    else
        return 0;
}

void
MetaProxy::Track::subscribe( Meta::Observer *observer )
{
    if( observer && !d->observers.contains( observer ) )
        d->observers.append( observer );
}

void
MetaProxy::Track::unsubscribe( Meta::Observer *observer )
{
    if( observer )
        d->observers.removeAll( observer );
}

bool
MetaProxy::Track::hasCapabilityInterface( Meta::Capability::Type type ) const
{
    if( d->realTrack )
        return d->realTrack->hasCapabilityInterface( type );
    else
        if( type == Meta::Capability::Editable )
            return true;
    return false;
}

Meta::Capability*
MetaProxy::Track::asCapabilityInterface( Meta::Capability::Type type )
{
    if( d->realTrack )
        return d->realTrack->asCapabilityInterface( type );
    else
        if( type == Meta::Capability::Editable )
            return new EditCapabilityProxy( this );
    return 0;
}

bool
MetaProxy::Track::operator==( const Meta::Track &track ) const
{
    const MetaProxy::Track *proxy = dynamic_cast<const MetaProxy::Track*>( &track );
    if( proxy && d->realTrack )
    {
        return d->realTrack == proxy->d->realTrack;
    }
    else if( proxy )
    {
        return d->url == proxy->d->url;
    }
    else
    {
        return d->realTrack && d->realTrack.data() == &track;
    }
}