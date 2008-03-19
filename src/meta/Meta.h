/* This file is part of the KDE project
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

#ifndef AMAROK_META_H
#define AMAROK_META_H

#include "amarok_export.h"

#include "meta/Capability.h"

#include <QList>
#include <QMetaType>
#include <QPixmap>
#include <QSet>
#include <QSharedData>
#include <QString>

#include <ksharedptr.h>
#include <kurl.h>

class Collection;
class QueryMaker;

namespace Meta
{
    class MetaBase;
    class Track;
    class Artist;
    class Album;
    class Genre;
    class Composer;
    class Year;


    typedef KSharedPtr<MetaBase> DataPtr;
    typedef QList<DataPtr > DataList;

    typedef KSharedPtr<Track> TrackPtr;
    typedef QList<TrackPtr > TrackList;
    typedef KSharedPtr<Artist> ArtistPtr;
    typedef QList<ArtistPtr > ArtistList;
    typedef KSharedPtr<Album> AlbumPtr;
    typedef QList<AlbumPtr > AlbumList;
    typedef KSharedPtr<Composer> ComposerPtr;
    typedef QList<ComposerPtr> ComposerList;
    typedef KSharedPtr<Genre> GenrePtr;
    typedef QList<GenrePtr > GenreList;
    typedef KSharedPtr<Year> YearPtr;
    typedef QList<YearPtr > YearList;

    class AMAROK_EXPORT Observer
    {
        public:
            /** This method is called when the metadata of a track has changed.
                The called class may not cache the pointer */
            virtual void metadataChanged( Track *track );
            virtual void metadataChanged( Artist *artist );
            virtual void metadataChanged( Album *album );
            virtual void metadataChanged( Genre *genre );
            virtual void metadataChanged( Composer *composer );
            virtual void metadataChanged( Year *year );
            virtual ~Observer();
    };

    class AMAROK_EXPORT MetaBase : public QSharedData
    {
        public:
            MetaBase() {}
            virtual ~MetaBase() {}
            virtual QString name() const = 0;
            virtual QString prettyName() const = 0;
            virtual QString fullPrettyName() const { return prettyName(); }
            virtual QString sortableName() const { return prettyName(); }

            virtual void addMatchTo( QueryMaker *qm ) = 0;

            virtual void subscribe( Observer *observer );
            virtual void unsubscribe( Observer *observer );

            virtual bool hasCapabilityInterface( Meta::Capability::Type type ) const;

            virtual Meta::Capability* asCapabilityInterface( Meta::Capability::Type type );

            /**
             * Retrieves a specialized interface which represents a capability of this
             * MetaBase object.
             *
             * @returns a pointer to the capability interface if it exists, 0 otherwise
             */
            template <class CapIface> CapIface *as()
            {
                Meta::Capability::Type type = CapIface::capabilityInterfaceType();
                Meta::Capability *iface = asCapabilityInterface(type);
                return qobject_cast<CapIface *>(iface);
            }

            /**
             * Tests if a MetaBase object provides a given capability interface.
             *
             * @returns true if the interface is available, false otherwise
             */
            template <class CapIface> bool is() const
            {
                return hasCapabilityInterface( CapIface::capabilityInterfaceType() );
            }

        protected:
            virtual void notifyObservers() const = 0;

        protected:
            QSet<Meta::Observer*> m_observers;

        private: // no copy allowed, since it's not safe with observer list
            Q_DISABLE_COPY(MetaBase)
    };

    class AMAROK_EXPORT Track : public MetaBase
    {
        public:

            virtual ~Track() {}
            /** an url which can be played by the engine backends */
            virtual KUrl playableUrl() const = 0;
            /** an url for display purposes */
            virtual QString prettyUrl() const = 0;
            /** an url which is unique for this track. Use this if you need a key for the track */
            virtual QString url() const = 0;

            /** Returns whether playableUrl() will return a playable Url */
            virtual bool isPlayable() const = 0;
            /** Returns the album this track belongs to */
            virtual AlbumPtr album() const = 0;
            /** Returns the artist of this track */
            virtual ArtistPtr artist() const = 0;
            /** Returns the composer of this track */
            virtual ComposerPtr composer() const = 0;
            /** Returns the genre of this track */
            virtual GenrePtr genre() const = 0;
            /** Returns the year of this track */
            virtual YearPtr year() const = 0;

            /** Returns the comment of this track */
            virtual QString comment() const = 0;
            /** Returns the score of this track */
            virtual double score() const = 0;
            virtual void setScore( double newScore ) = 0;
            /** Returns the ratint of this track */
            virtual int rating() const = 0;
            virtual void setRating( int newRating ) = 0;
            /** Returns the length of this track in seconds, or 0 if unknown */
            virtual int length() const = 0;
            /** Returns the filesize of this track in bytes */
            virtual int filesize() const = 0;
            /** Returns the sample rate of this track */
            virtual int sampleRate() const = 0;
            /** Returns the bitrate o this track */
            virtual int bitrate() const = 0;
            /** Returns the track number of this track */
            virtual int trackNumber() const = 0;
            /** Returns the discnumber of this track */
            virtual int discNumber() const = 0;
            /** Returns the time the song was last played, or 0 if it has not been played yet */
            virtual uint lastPlayed() const = 0;
            /** Returns the time the song was first played, or 0 if it has not been played yet */
            virtual uint firstPlayed() const;
            /** Returns the number of times the track was played (what about unknown?)*/
            virtual int playCount() const = 0;

            /** Returns the type of this track, e.g. "ogg", "mp3", "Stream" */
            virtual QString type() const = 0;

            /** tell the track object that amarok finished playing it.
                The argument is the percentage of the track which was played, in the range 0 to 1*/
            virtual void finishedPlaying( double playedFraction );

            virtual void addMatchTo( QueryMaker* qm );

            /** returns true if the track is part of a collection false otherwise */
            virtual bool inCollection() const;
            /**
                returns the collection that the track is part of, or 0 iff
                inCollection() returns false */
            virtual Collection* collection() const;

            /** get the cached lyrics for the track. returns an empty string if
                no cached lyrics are available */
            virtual QString cachedLyrics() const;
            /**
                set the cached lyrics for the track
            */
            virtual void setCachedLyrics( const QString &lyrics );

            virtual bool operator==( const Meta::Track &track ) const;

        protected:
            virtual void notifyObservers() const;

    };

    class AMAROK_EXPORT Artist : public MetaBase
    {
        public:

            virtual ~Artist() {}
            /** returns all tracks by this artist */
            virtual TrackList tracks() = 0;

            /** returns all albums by this artist */
            virtual AlbumList albums() = 0;

            virtual void addMatchTo( QueryMaker* qm );

            virtual bool operator==( const Meta::Artist &artist ) const;

        protected:
            virtual void notifyObservers() const;
    };

    class AMAROK_EXPORT Album : public MetaBase
    {
        public:

            virtual ~Album() {}
            virtual bool isCompilation() const = 0;

            /** Returns true if this album has an album artist */
            virtual bool hasAlbumArtist() const = 0;
            /** Returns a pointer to the album's artist */
            virtual ArtistPtr albumArtist() const = 0;
            /** returns all tracks on this album */
            virtual TrackList tracks() = 0;

            /** returns true if the album has a cover set */
            virtual bool hasImage( int size = 1) const { Q_UNUSED( size ); return false; }
            /** returns the cover of the album */
            virtual QPixmap image( int size = 1, bool withShadow = false );
            /** Returns true if it is possible to update the cover of the album */
            virtual bool canUpdateImage() const { return false; }
            /** updates the cover of the album */
            virtual void setImage( const QImage &image) { Q_UNUSED( image ); } //TODO: choose parameter
            /** removes the album art */
            virtual void removeImage() { }

            virtual void addMatchTo( QueryMaker* qm );

            virtual bool operator==( const Meta::Album &album ) const;

        protected:
            virtual void notifyObservers() const;
    };

    class AMAROK_EXPORT Composer : public MetaBase
    {
        public:

            virtual ~Composer() {}
            /** returns all tracks by this composer */
            virtual TrackList tracks() = 0;

            virtual void addMatchTo( QueryMaker* qm );

            virtual bool operator==( const Meta::Composer &composer ) const;

        protected:
            virtual void notifyObservers() const;
    };

    class AMAROK_EXPORT Genre : public MetaBase
    {
        public:

            virtual ~Genre() {}
            /** returns all tracks which belong to the genre */
            virtual TrackList tracks() = 0;

            virtual void addMatchTo( QueryMaker* qm );

            virtual bool operator==( const Meta::Genre &genre ) const;

        protected:
            virtual void notifyObservers() const;
    };

    class AMAROK_EXPORT Year : public MetaBase
    {
        public:

            virtual ~Year() {}
            /** returns all tracks which are tagged with this year */
            virtual TrackList tracks() = 0;

            virtual void addMatchTo( QueryMaker* qm );

            virtual bool operator==( const Meta::Year &year ) const;

        protected:
            virtual void notifyObservers() const;
    };
}

Q_DECLARE_METATYPE( Meta::DataPtr )
Q_DECLARE_METATYPE( Meta::DataList )
Q_DECLARE_METATYPE( Meta::TrackPtr )
Q_DECLARE_METATYPE( Meta::TrackList )
Q_DECLARE_METATYPE( Meta::ArtistPtr )
Q_DECLARE_METATYPE( Meta::ArtistList )
Q_DECLARE_METATYPE( Meta::AlbumPtr )
Q_DECLARE_METATYPE( Meta::AlbumList )
Q_DECLARE_METATYPE( Meta::ComposerPtr )
Q_DECLARE_METATYPE( Meta::ComposerList )
Q_DECLARE_METATYPE( Meta::GenrePtr )
Q_DECLARE_METATYPE( Meta::GenreList )
Q_DECLARE_METATYPE( Meta::YearPtr )
Q_DECLARE_METATYPE( Meta::YearList )



#endif /* AMAROK_META_H */