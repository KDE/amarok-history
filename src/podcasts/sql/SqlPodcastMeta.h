/* This file is part of the KDE project
   Copyright (C) 2008 Bart Cerneels <bart.cerneels@kde.org>

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

#ifndef SQLPODCASTMETA_H
#define SQLPODCASTMETA_H

#include "PodcastMeta.h"

namespace Meta
{

class SqlPodcastEpisode;
class SqlPodcastChannel;

typedef KSharedPtr<SqlPodcastEpisode> SqlPodcastEpisodePtr;
typedef KSharedPtr<SqlPodcastChannel> SqlPodcastChannelPtr;

typedef QList<SqlPodcastEpisodePtr> SqlPodcastEpisodeList;
typedef QList<SqlPodcastChannelPtr> SqlPodcastChannelList;

class SqlPodcastEpisode : public PodcastEpisode
{
    public:
//         static SqlPodcastEpisodePtr getEpisode( SqlPodcastProvider *provider );
        
        SqlPodcastEpisode( const QStringList &queryResult );

        /** Copy from another PodcastEpisode
        */
        SqlPodcastEpisode( PodcastEpisodePtr episode );

        ~SqlPodcastEpisode() {};

        //Track Methods
        QString type() const { return i18n("SQL Podcast"); };

        //SqlPodcastEpisode specific methods
        int id() const { return m_id; };

    private:
        void updateInDb();

        bool m_batchUpdate;

        int m_id; //database ID
        SqlPodcastChannelPtr m_sqlChannel; //the parent of this episode
};

class SqlPodcastChannel : public PodcastChannel
{
    public:
        SqlPodcastChannel( const QStringList &queryResult );

        /** Copy a PodcastChannel
        */
        SqlPodcastChannel( PodcastChannelPtr channel );

        ~SqlPodcastChannel() {};

        //SqlPodcastChannel specific methods
        int id() const { return m_id; };
        virtual void addEpisode( SqlPodcastEpisodePtr episode ) { m_sqlEpisodes << episode; };

    private:
        void updateInDb();

        int m_id; //database ID

        SqlPodcastEpisodeList m_sqlEpisodes;
};

}

Q_DECLARE_METATYPE( Meta::SqlPodcastEpisodePtr )
Q_DECLARE_METATYPE( Meta::SqlPodcastEpisodeList )
Q_DECLARE_METATYPE( Meta::SqlPodcastChannelPtr )
Q_DECLARE_METATYPE( Meta::SqlPodcastChannelList )

#endif