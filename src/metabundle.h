//Maintainer: Max Howell <max.howell@methylblue.com>, (C) 2004
//Copyright:  See COPYING file that comes with this distribution
//

#ifndef METABUNDLE_H
#define METABUNDLE_H

#include <kurl.h>    //inline functions
#include <klocale.h> //inline functions

class PlaylistItem;
class KFileMetaInfo;

namespace TagLib { class AudioProperties; class Tag; }

//#define PRETTY_TITLE_CACHE

/*
 * This class is not very complete, it fits our needs as they stand currently
 * If it doesn't work for you in some way, extend it sensibly :)
 */

class MetaBundle
{
public:

    //for the audioproperties
    static const int Undetermined = -2;
    static const int Irrelevant   = -1;
    static const int Unavailable  =  0;

    static const MetaBundle null; //use like you would QString::null

    /**
     * Creates an empty MetaBundle
     */
    MetaBundle() { init(); }

     /**
     * Creates a MetaBundle for url, tags will be obtained and set
     */
    MetaBundle( const KURL &u, bool readAudioProperties = true )
        : m_url( u )
    {
        readTags( readAudioProperties );
    }

    //TitleProxy:
    MetaBundle( const QString& title,
                const QString& streamUrl,
                const int      bitrate,
                const QString& genre,
                const QString& streamName,
                const KURL& );

    //PlaylistItems:
    MetaBundle( const PlaylistItem *item );

    //From tags:
    MetaBundle( const KURL &url, TagLib::Tag *tag, TagLib::AudioProperties *ap = 0 );

     /**
     * Test for an empty metabundle
     */
    bool isEmpty() const { return m_url.isEmpty(); }

    MetaBundle &readTags( bool readAudioProperties = true );


    int length()     const { return m_length > 0 ? m_length : 0; }
    int bitrate()    const { return m_bitrate; }
    int sampleRate() const { return m_sampleRate; }
    void setLength( int length )         { m_length = length; }
    void setBitrate( int bitrate )       { m_bitrate = bitrate; }
    void setSampleRate( int sampleRate ) { m_sampleRate = sampleRate; }

    const KURL    &url()     const { return m_url; }
    const QString &title()   const { return m_title; }
    const QString &artist()  const { return m_artist; }
    const QString &album()   const { return m_album; }
    const QString &year()    const { return m_year; }
    const QString &comment() const { return m_comment; }
    const QString &genre()   const { return m_genre; }
    const QString &track()   const { return m_track; }
    void setUrl( QString url ) { m_url = url; }
    void setTitle( QString title ) { m_title = title; }
    void setArtist( QString artist ) { m_artist = artist; }
    void setAlbum( QString album ) { m_album = album; }
    void setYear( QString year ) { m_year = year; }
    void setComment( QString comment ) { m_comment = comment; }
    void setGenre( QString genre ) { m_genre = genre; }
    void setTrack( QString track ) { m_track = track; }

    QString prettyTitle() const;
    QString prettyURL() const { return m_url.prettyURL(); }
    QString prettyBitrate() const { return prettyBitrate( m_bitrate ); }
    QString prettyLength() const { return prettyLength( m_length ); }
    QString prettySampleRate( bool shortened = false ) const { return prettyGeneric( i18n( "SampleRate", "%1 kHz" ), shortened ? m_sampleRate / 1000 : m_sampleRate ); }

    static /*inline */QString prettyBitrate( int );
    static QString prettyLength( int );
    static QString prettyTime( int, bool showHours = true );
    static QString zeroPad( uint i ) { return ( i < 10 ) ? QString( "0%1" ).arg( i ) : QString::number( i ); }
    static QString prettyTitle( QString );
    static QStringList genreList();

    friend class CollectionDB;

private:
    KURL    m_url;
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_year;
    QString m_comment;
    QString m_genre;
    QString m_track;

    #ifdef PRETTY_TITLE_CACHE
    mutable QString m_prettyTitleCache;
    #endif

    int m_bitrate;
    int m_length;
    int m_sampleRate;

    static /*inline */QString prettyGeneric( const QString&, int );

    void init( TagLib::AudioProperties *ap = 0 );
    void init( const KFileMetaInfo& info );
};

#endif
