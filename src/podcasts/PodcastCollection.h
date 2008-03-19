/* This file is part of the KDE project
   Copyright (C) 2007 Bart Cerneels <bart.cerneels@gmail.com>

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

#ifndef PODCASTCOLLECTION_H
#define PODCASTCOLLECTION_H

#include "Collection.h"
#include "support/MemoryCollection.h"
#include "PodcastMeta.h"
#include "playlistmanager/PlaylistManager.h"

#include <kio/jobclasses.h>
#include <klocale.h>

class KUrl;
class PodcastReader;
class PodcastChannelProvider;
class SqlStorage;

/**
	@author Bart Cerneels <bart.cerneels@gmail.com>
*/
class PodcastCollection : public Collection, public MemoryCollection
{
    Q_OBJECT
    public:
        PodcastCollection();
        ~PodcastCollection();

        virtual QueryMaker * queryMaker();
        virtual void startFullScan() { }

        virtual QString collectionId() const;

        virtual bool possiblyContainsTrack( const KUrl &url ) const;
        virtual Meta::TrackPtr trackForUrl( const KUrl &url );

        virtual CollectionLocation* location() const;

        virtual QString prettyName() const { return i18n("Local Podcasts"); };

        AMAROK_EXPORT virtual void addPodcast( const QString &url );

        virtual void addChannel( Meta::PodcastChannelPtr channel );
        virtual void addEpisode( Meta::PodcastEpisodePtr episode );

        virtual Meta::PodcastChannelList channels() { return m_channels; };

        virtual PodcastChannelProvider * channelProvider() { return m_channelProvider; };

    signals:
        void remove();

    public slots:
        virtual void slotUpdateAll();
        virtual void slotUpdate( Meta::PodcastChannelPtr channel );
        virtual void slotDownloadEpisode( Meta::PodcastEpisodePtr episode );

        virtual void slotReadResult( PodcastReader *podcastReader, bool result );

    private slots:
        void downloadResult( KJob * );
        void redirected( KIO::Job *, const KUrl& );

    private:
        static PodcastCollection *s_instance;

        /** creates all the necessary tables, indexes etc. for the database */
        void createTables() const;

        SqlStorage *m_sqlStorage;

        Meta::PodcastChannelList m_channels;
        PodcastChannelProvider *m_channelProvider;

        QMap<KJob *, Meta::PodcastEpisodePtr> m_jobMap;
        QMap<KJob *, QString> m_fileNameMap;

};


class PodcastChannelProvider: public PlaylistProvider
{
    Q_OBJECT
    public:
        PodcastChannelProvider( PodcastCollection * parent );

        void addPodcast( QString url ) { m_parent->addPodcast( url ); };

        void updateAll() { m_parent->slotUpdateAll() ; };
        void update( Meta::PodcastChannelPtr channel ) { m_parent->slotUpdate( channel ); };
        void downloadEpisode( Meta::PodcastEpisodePtr episode )
                { m_parent->slotDownloadEpisode( episode ); };

        // PlaylistProvider methods
        virtual QString prettyName() const { return i18n( "Local Podcasts" ); };
        virtual int category() const { return (int)PlaylistManager::PodcastChannel; };
        virtual QString typeName() const { return i18n( "Podcasts" ); };

        virtual Meta::PlaylistList playlists();

    public slots:
        void slotUpdated();

    signals:
        virtual void updated();

    private:
        PodcastCollection *m_parent;
};

#endif