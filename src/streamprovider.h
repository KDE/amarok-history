/***************************************************************************
                     streamprovider.h - shoutcast streaming client
                        -------------------
begin                : Nov 20 14:35:18 CEST 2003
copyright            : (C) 2003 by Mark Kretschmann
email                : markey@web.de
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef AMAROK_STREAMPROVIDER_H
#define AMAROK_STREAMPROVIDER_H

#include <kurl.h>             //stack allocated

#include <qobject.h>
#include <qserversocket.h>    //baseclass
#include <qsocket.h>          //stack allocated

class QString;
class MetaBundle;

namespace amaroK {
    /**
     * Stream radio client, receives shoutcast streams and extracts metadata. 
     *
     * Provider Concept:
     * 1. Connect to streamserver
     * 2. Write GET request to streamserver, containing Icy-MetaData:1 token
     * 3. Read MetaInt token from streamserver (==metadata offset)
     *
     * 4. Read stream data (mp3 + metadata) from streamserver
     * 5. Filter out metadata, send to app
     * 6. Send stream data to application
     * 7. Goto 4
     *
     * Some info on the shoutcast metadata protocol can be found at:
     * @see http://www.smackfu.com/stuff/programming/shoutcast.html
     *
     * @short A class for handling Shoutcast streams and metadata.
     */
    class StreamProvider : public QObject
    {
            Q_OBJECT
        public:
            /**
             * Constructs a StreamProvider.
             * @param url URL of stream server
             * @param streamingMode The class has two modes of transferring stream data to the application:
             *                      Signal: Transfer the data directly via Qt SIGNAL.
             *                              (this mode is to be preferred).
             *                      Socket: Sets up proxy server and writes the data to the proxy.
             */
            StreamProvider( KURL url, int streamingMode );
            ~StreamProvider();

            /** Returns true if initialisation was successful */
            bool initSuccess() { return m_initSuccess; }
            
            /** Returns URL for proxy server (only relevant in Socket mode) */
            KURL proxyUrl();

        signals:
            /** Transmits new metadata bundle */
            void metaData( const MetaBundle& );
            
            /** Transmits chunk of audio data */
            void streamData( char*, int size );
            
            /** Signals an error: StreamProvider cannot operate on this stream. */
            void sigError();
            
        private slots:
            void accept( int socket );
            void connectToHost();
            void sendRequest();
            void readRemote();
            void connectError();
            
        private:
            bool processHeader( Q_LONG &index, Q_LONG bytesRead );
            void transmitData( const QString &data );
            void error();
            
            /** 
             * Find key/value pair in string and return value.
             * @param str String to search through.
             * @param key Key to find.
             * @return The value, QString:null if key not found.
             */
            QString extractStr( const QString &str, const QString &key );

        //ATTRIBUTES:
            KURL m_url;
            int m_streamingMode;
            bool m_initSuccess;
            bool m_connectSuccess;
            
            int m_metaInt;
            int m_bitRate;
            int m_byteCount;
            uint m_metaLen;
            
            QString m_metaData;
            bool m_headerFinished;
            QString m_headerStr;
            int m_usedPort;
            QString m_lastMetadata;
            bool m_icyMode;
            
            QString m_streamName;
            QString m_streamGenre;
            QString m_streamUrl;

            char *m_pBuf;

            QSocket m_sockRemote;
            QSocket m_sockProxy;
    };


    /**
     * Stream proxy server. 
     */
    class StreamProxy : public QServerSocket
    {
            Q_OBJECT

        public:
            StreamProxy( Q_UINT16 port, QObject* parent )
                : QServerSocket( port, 1, parent ) {};

        signals:
            void connected( int socket );

        private:
            void newConnection( int socket ) { emit connected( socket ); }
    };

} //namespace amaroK

#endif /*AMAROK_STREAMPROVIDER_H*/

