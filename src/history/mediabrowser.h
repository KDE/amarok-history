// (c) 2004 Christian Muehlhaeuser <chris@chris.de>
// (c) 2005 Martin Aumueller <aumuell@reserv.at>
// (c) 2005 Seb Ruiz <ruiz@kde.org>  
// (c) 2006 T.R.Shashwath <trshash84@gmail.com>
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef AMAROK_MEDIABROWSER_H
#define AMAROK_MEDIABROWSER_H

#include "amarok_export.h"
#include "Amarok.h"
#include "browserToolBar.h"
#include "medium.h"
#include "metabundle.h"
#include "PluginManager.h"
#include "plugin/plugin.h"   //baseclass
#include "scrobbler.h"       //SubmitItem

#include <k3listview.h>       //baseclass
#include <KHBox>
#include <kio/global.h>      //filesize_t
#include <KUrl>            //stack allocated
#include <KVBox>           //baseclass

#include <Q3PtrList>
#include <QDateTime>
#include <QLabel>
#include <QMutex>
#include <QPixmap>
#include <QWidget>


class MediaBrowser;
class MediaDevice;
class MediaItem;
class MediaView;
class SpaceLabel;
class TransferDialog;
class SearchWidget;

class KAction;
class KComboBox;
class KPushButton;
class K3ShellProcess;

class QDropEvent;
class QKeyEvent;
class QPaintEvent;
class Q3DragObject;
class QLabel;
class QProgressBar;



class MediaQueue : public K3ListView
{
    Q_OBJECT

    public:
        MediaQueue(MediaBrowser *parent);
        MediaItem *findPath( QString path );

        KIO::filesize_t totalSize() const; // total size of items to transfer in KB
        void computeSize() const; // compute total size of items to transfer in KB
        void addItemToSize( const MediaItem *item ) const;
        void subtractItemFromSize( const MediaItem *item, bool unconditonally=false ) const;

        void removeSelected();
        void clearItems();

        void load( const QString &path );
        void save( const QString &path );
        void syncPlaylist( const QString &playlistName, const QString &sql, bool loading=false );
        void syncPlaylist( const QString &playlistName, const KUrl &url, bool loading=false );
        void addUrl( const KUrl& url, MetaBundle *bundle=NULL, const QString &playlistName=QString() );
        void addUrl( const KUrl& url, MediaItem *item );
        void addUrls( const KUrl::List urls, const QString &playlistName=QString() );

        void URLsAdded(); // call after finishing adding single urls

        void dropProxyEvent( QDropEvent *e );
        // Reimplemented from K3ListView
        bool acceptDrag( QDropEvent *e ) const;
        Q3DragObject *dragObject();

    public slots:
        void itemCountChanged();

    private slots:
        void selectAll() {Q3ListView::selectAll(true); }
        void slotShowContextMenu( Q3ListViewItem* item, const QPoint& point, int );
        void slotDropped (QDropEvent* e, Q3ListViewItem* parent, Q3ListViewItem* after);

    private:
        void keyPressEvent( QKeyEvent *e );
        MediaBrowser *m_parent;
        mutable KIO::filesize_t m_totalSize;
};


class MediaBrowser : public QWidget
{
    Q_OBJECT
    friend class DeviceConfigureDialog;
    friend class MediaDevice;
    friend class MediaView;
    friend class MediaQueue;
    friend class MediumPluginChooser;
    friend class MediaItem;

    public:
        static bool isAvailable();
        AMAROK_EXPORT static MediaBrowser *instance() { return s_instance; }
        AMAROK_EXPORT static MediaQueue *queue() { return s_instance ? s_instance->m_queue : 0; }

        MediaBrowser( const char *name );
        virtual ~MediaBrowser();
        bool blockQuit() const;
        MediaDevice *currentDevice() const { return m_currentDevice; }
        MediaDevice *deviceFromId( const QString &id ) const;
        QStringList deviceNames() const;
        bool deviceSwitch( const QString &name );

        QString getInternalPluginName ( const QString string ) { return m_pluginName[string]; }
        QString getDisplayPluginName ( const QString string ) { return m_pluginAmarokName[string]; }
        const KService::List &getPlugins() { return m_plugins; }
        void transcodingFinished( const QString &src, const QString &dst );
        bool isTranscoding() const { return m_waitForTranscode; }
        void updateStats();
        void updateButtons();
        void updateDevices();
        // return bundle for url if it is known to MediaBrowser
        bool getBundle( const KUrl &url, MetaBundle *bundle ) const;
        bool isQuitting() const { return m_quitting; }

        KUrl getProxyUrl( const KUrl& daapUrl ) const;
        KToolBar* getToolBar() const { return m_toolbar; }
        KAction *connectAction() const { return m_connectAction; }
        KAction *disconnectAction() const { return m_disconnectAction; }
        KAction *transferAction() const { return m_transferAction; }
        KAction *configAction() const { return m_configAction; }
        KAction *customAction() const { return m_customAction; }

    signals:
        void availabilityChanged( bool isAvailable );

    protected slots:
        void transferClicked();

    private slots:
        void slotSetFilterTimeout();
        void slotSetFilter();
        void slotSetFilter( const QString &filter );
        void slotEditFilter();
        void deviceAdded( const QString &udi );
        void deviceRemoved( const QString &udi );
        void activateDevice( const MediaDevice *device );
        void activateDevice( int index, bool skipDummy = true );
        void cancelClicked();
        void connectClicked();
        void disconnectClicked();
        void customClicked();
        bool config(); // false if canceled by user
        KUrl transcode( const KUrl &src, const QString &filetype );
        void tagsChanged( const MetaBundle &bundle );
        void prepareToQuit();

    private:
        MediaDevice *loadDevicePlugin( const QString &udi );
        void         unloadDevicePlugin( MediaDevice *device );

        QTimer *m_timer;
        AMAROK_EXPORT static MediaBrowser *s_instance;

        QList<MediaDevice *> m_devices;
        MediaDevice * m_currentDevice;

        QMap<QString, QString> m_pluginName;
        QMap<QString, QString> m_pluginAmarokName;
        void addDevice( MediaDevice *device );
        void removeDevice( MediaDevice *device );

        MediaQueue* m_queue;
        bool m_waitForTranscode;
        KUrl m_transcodedUrl;
        QString m_transcodeSrc;

        SpaceLabel*      m_stats;
        KHBox*           m_progressBox;
        QProgressBar*       m_progress;
        QWidget*           m_views;
        KPushButton*     m_cancelButton;
        //KPushButton*     m_playlistButton;
        KVBox*           m_configBox;
        KComboBox*       m_configPluginCombo;
        KComboBox*       m_deviceCombo;
        Browser::ToolBar*m_toolbar;
        typedef QMap<QString, MediaItem*> ItemMap;
        mutable QMutex   m_itemMapMutex;
        ItemMap          m_itemMap;
        KService::List m_plugins;
        bool             m_haveDevices;
        bool             m_quitting;
        KAction *m_connectAction;
        KAction *m_disconnectAction;
        KAction *m_customAction;
        KAction *m_configAction;
        KAction *m_transferAction;
        SearchWidget *m_searchWidget;
};

class MediaView : public K3ListView
{
    Q_OBJECT
    friend class MediaBrowser;
    friend class MediaDevice;

    public:
        enum Flags
        {
            None = 0,
            OnlySelected = 1,
            OnlyPlayed = 2
        };

        MediaView( QWidget *parent, MediaDevice *device );
        virtual ~MediaView();
        AMAROK_EXPORT KUrl::List nodeBuildDragList( MediaItem* item, int flags=OnlySelected );
        int getSelectedLeaves(MediaItem *parent, Q3PtrList<MediaItem> *list, int flags=OnlySelected );
        AMAROK_EXPORT MediaItem *newDirectory( MediaItem* parent );
        bool setFilter( const QString &filter, MediaItem *parent=NULL );

    private slots:
        void rmbPressed( Q3ListViewItem*, const QPoint&, int );
        void renameItem( Q3ListViewItem *item );
        void slotExpand( Q3ListViewItem* );
        void selectAll() { Q3ListView::selectAll(true); }
        void invokeItem( Q3ListViewItem*, const QPoint &, int column );
        void invokeItem( Q3ListViewItem* );

    private:
        void keyPressEvent( QKeyEvent *e );
        // Reimplemented from K3ListView
        void contentsDropEvent( QDropEvent *e );
        void viewportPaintEvent( QPaintEvent* );
        bool acceptDrag( QDropEvent *e ) const;
        Q3DragObject *dragObject();

        QWidget *m_parent;
        MediaDevice *m_device;
};


/* at least the pure virtual functions have to be implemented by a media device,
   all items are stored in a hierarchy of MediaItems,
   when items are manipulated the MediaItems have to be updated accordingly */

class AMAROK_EXPORT MediaDevice : public QObject, public Amarok::Plugin
{
    Q_OBJECT
    friend class DeviceConfigureDialog;
    friend class TransferDialog;
    friend class MediaBrowser;
    friend class MediaView;
    friend class MediaQueue;

    public:
        enum Flags
        {
            None = 0,
            OnlyPlayed = 1,
            DeleteTrack = 2,
            Recursing = 4
        };

        MediaDevice();
        virtual void init( MediaBrowser* parent );
        virtual ~MediaDevice();

        MediaView *view();

        /**
         * @return a KAction that will be plugged into the media device browser toolbar
         */
        virtual KAction *customAction() { return 0; }

        virtual void rmbPressed( Q3ListViewItem *item, const QPoint &point, int ) { (void)item; (void) point; }

        /**
         * @return list of filetypes playable on this device
         *  (empty list is interpreted as all types are good)
         */
        virtual QStringList supportedFiletypes() { return QStringList(); }

        /**
         * @param bundle describes track that should be checked
         * @return true if the device is capable of playing the track referred to by bundle
         */
        virtual bool isPlayable( const MetaBundle &bundle );

        /**
         * @param bundle describes track that should be checked
         * @return true if the track is in the preferred (first in list) format of the device
         */
        virtual bool isPreferredFormat( const MetaBundle &bundle );

        /**
         * @return true if the device is connected
         */
        virtual bool       isConnected() = 0;

        /**
         * Adds particular tracks to a playlist
         * @param playlist parent playlist for tracks to be added to
         * @param after insert following this item
         * @param items tracks to add to playlist
         */
        virtual void       addToPlaylist(MediaItem *playlist, MediaItem *after, Q3PtrList<MediaItem> items) { Q_UNUSED(playlist); Q_UNUSED(after); Q_UNUSED(items); }

        /**
         * Create a new playlist
         * @param name playlist title
         * @param parent parent MediaItem of the new playlist
         * @param items tracks to add to the new playlist
         * @return the newly created playlist
         */
        virtual MediaItem *newPlaylist(const QString &name, MediaItem *parent, Q3PtrList<MediaItem> items) { Q_UNUSED(name); Q_UNUSED(parent); Q_UNUSED(items); return 0; }

        /**
         * Move items to a directory
         * @param directory new parent of dropped items
         * @param items tracks to add to the directory
         */
        virtual void      addToDirectory( MediaItem *directory, Q3PtrList<MediaItem> items ) { Q_UNUSED(directory); Q_UNUSED(items); }

        /**
         * Create a new directory
         * @param name directory title
         * @param parent parent MediaItem of the new directory
         * @param items tracks to add to the new directory
         * @return the newly created directory
         */
        virtual MediaItem *newDirectory( const QString &name, MediaItem *parent ) { Q_UNUSED(name); Q_UNUSED(parent); return 0; }

        /**
         * Notify device of changed tags
         * @param item item to be updated
         * @param changed bundle containing new tags
         * @return the changed MediaItem
         */
        virtual MediaItem *tagsChanged( MediaItem *item, const MetaBundle &changed ) { Q_UNUSED(item); Q_UNUSED(changed); return 0; }

        /**
         * Indicate whether the device has a custom transfer dialog
         * @return whether there is a custom dialog
         */
        virtual bool hasTransferDialog() { return false; }

        /**
         * Run the transfer dialog to be used when Transfer is clicked
         */
        virtual void runTransferDialog() {}

        /**
         * Get the transfer dialog, if any
         * @return the transfer dialog, if any, else NULL;
         */
        virtual TransferDialog *getTransferDialog() { return NULL; }

        /**
         * Can be used to explicitly indicate whether a device needs manual configuration
         * @return whether manual configuration is needed
         */
        virtual bool needsManualConfig() { return true; }

        virtual void addConfigElements( QWidget * /*parent*/ ) {}
        virtual void removeConfigElements( QWidget * /*parent*/ ) {}
        virtual void applyConfig() {}
        virtual void loadConfig();

        QString configString( const QString &name, const QString &defValue = QString() );
        void setConfigString( const QString &name, const QString &value );
        bool configBool( const QString &name, bool defValue=false );
        void setConfigBool( const QString &name, bool value );

        void         setRequireMount( const bool b ) { m_requireMount = b; }
        bool         hasMountPoint() { return m_hasMountPoint; }
        void         setDeviceType( const QString &type ) { m_type = type; }
        QString      deviceType() { return m_type; }
        virtual bool autoConnect() { return false; }
        virtual bool asynchronousTransfer() { return false; }
        bool         isTransferring() { return m_transferring; }
        bool         isDeleting() { return m_deleting; }
        bool         isCanceled() { return m_canceled; }
        void         setCanceled( const bool b ) { m_canceled = b; }

        int          progress() const;
        void         setProgress( const int progress, const int total = -1 /* leave total unchanged by default */ );
        void         hideProgress();


        /**
         * @return a unique identifier that is constant across sessions
         */
        QString uniqueId() const { return m_uid; }

        /**
         * @return the name for the device that should be presented to the user
         */
        QString name() const { return m_name; }

        /**
         * @return the device node
         */
        QString deviceNode() const { return m_medium.deviceNode(); }

        /*
         * @return the device mount point (or empty if non-applicable or unknown)
         */
        QString mountPoint() const { return m_medium.mountPoint(); }

        QString           getTransferDir() { return m_transferDir; }
        Medium           &getMedium() { return m_medium; }

        void              setSpacesToUnderscores( bool yesno ) { m_spacesToUnderscores = yesno;
            setConfigBool( "spacesToUnderscores", yesno); }
        bool              getSpacesToUnderscores() { return m_spacesToUnderscores; }

        void              setFirstSort( QString text ) { m_firstSort = text;
            setConfigString( "firstGrouping", text ); }
        void              setSecondSort( QString text ) { m_secondSort = text;
            setConfigString( "secondGrouping", text ); }
        void              setThirdSort( QString text ) { m_thirdSort = text;
            setConfigString( "thirdGrouping", text ); }

        virtual KUrl getProxyUrl( const KUrl& /*url*/) { return KUrl(); }
        virtual void customClicked() { return; }

        BundleList bundlesToSync( const QString &playlistName, const QString &sql );
        BundleList bundlesToSync( const QString &playlistName, const KUrl &url );
        void preparePlaylistForSync( const QString &playlistName, const BundleList &bundles );
        bool isOnOtherPlaylist( const QString &playlistToAvoid, const MetaBundle &bundle );
        bool isOnPlaylist( const MediaItem &playlist, const MetaBundle &bundle );
        bool isInBundleList( const BundleList &bundles, const MetaBundle &bundle );
        bool bundleMatch( const MetaBundle &b1, const MetaBundle &b2 );

    public slots:
        void abortTransfer();
        void transferFiles();
        virtual void renameItem( Q3ListViewItem *item ) {(void)item; }
        virtual void expandItem( Q3ListViewItem *item ) {(void)item; }
        bool connectDevice( bool silent=false );
        bool disconnectDevice( bool postdisconnecthook=true );
        void scheduleDisconnect() { m_scheduledDisconnect = true; }

    protected slots:
        void fileTransferred( KIO::Job *job );
        void fileTransferFinished();

    private:
        int  sysCall(const QString & command);
        int  runPreConnectCommand();
        int  runPostDisconnectCommand();
        QString replaceVariables( const QString &cmd ); // replace %m with mount point and %d with device node

        QString uid() { return m_uid; }
        void setUid( const QString &uid ) { m_uid = uid; }

        /**
         * Find a particular track
         * @param bundle The metabundle of the requested media item
         * @return The MediaItem of the item if found, otherwise NULL
         * @note This may not be worth implementing for non database driven devices, as it could be slow
         */
        virtual MediaItem *trackExists( const MetaBundle& bundle ) = 0;

    protected:
        /**
         * Get the capacity and freespace available on the device, in bytes
         * @return true if successful
         */
        virtual bool getCapacity( KIO::filesize_t *total, KIO::filesize_t *available ) { Q_UNUSED(total); Q_UNUSED(available); return false; }

        /**
         * Lock device for exclusive access if possible
         */
        virtual bool lockDevice( bool tryOnly = false ) = 0;

        /**
         * Unlock device
         */
        virtual void unlockDevice() = 0;

        /**
         * Connect to device, and populate m_view with MediaItems
         * @return true if successful
         */
        virtual bool openDevice( bool silent=false ) = 0;

        /**
         * Wrap up any loose ends and close the device
         * @return true if successful
         */
        virtual bool closeDevice() = 0;

        /**
         * Write any pending changes to the device, such as database changes
         */
        virtual void synchronizeDevice() = 0;

        /**
         * Copy a track to the device
         * @param bundle The MetaBundle of the item to transfer. Will move the item specified by bundle().url().path()
         * @return If successful, the created MediaItem in the media device view, else 0
         */
        virtual MediaItem *copyTrackToDevice(const MetaBundle& bundle) = 0;

        /**
         * Copy track from device to computer
         * @param item The MediaItem of the track to transfer.
         * @param url The URL to transfer the track to.
         * @return The MediaItem transfered.
         */
        virtual void copyTrackFromDevice(MediaItem *item);

        /**
         * Recursively remove MediaItem from the tracklist and the device
         * @param item MediaItem to remove
         * @param onlyPlayed True if item should be deleted only if it has been played
         * @return -1 on failure, number of files deleted otherwise
         */
        virtual int deleteItemFromDevice( MediaItem *item, int flags=DeleteTrack ) = 0;

        /**
         * Abort the currently active track transfer
         */
        virtual void cancelTransfer() { /* often checking m_cancel is enough */ }

        virtual void updateRootItems();

        virtual bool isSpecialItem( MediaItem *item );

        int deleteFromDevice( MediaItem *item=0, int flags=DeleteTrack );

        void purgeEmptyItems( MediaItem *root=0 );
        void syncStatsFromDevice( MediaItem *root=0 );
        void syncStatsToDevice( MediaItem *root=0 );

        bool kioCopyTrack( const KUrl &src, const KUrl &dst );

        QString     m_name;

        bool        m_hasMountPoint;

        QString     m_preconnectcmd;
        QString     m_postdisconnectcmd;
        bool        m_autoDeletePodcasts;
        bool        m_syncStats;

        bool        m_transcode;
        bool        m_transcodeAlways;
        bool        m_transcodeRemove;

        K3ShellProcess   *sysProc;
        MediaBrowser    *m_parent;
        MediaView       *m_view;
        Medium           m_medium;
        QString          m_transferDir;
        QString          m_firstSort;
        QString          m_secondSort;
        QString          m_thirdSort;
        QString          m_uid;
        bool             m_wait;
        bool             m_waitForDeletion;
        bool             m_copyFailed;
        bool             m_requireMount;
        bool             m_canceled;
        bool             m_transferring;
        bool             m_deleting;
        bool             m_deferredDisconnect;
        bool             m_scheduledDisconnect;
        bool             m_runDisconnectHook;
        bool             m_spacesToUnderscores;
        bool             m_transfer;
        bool             m_configure;
        bool             m_customButton;

        QString          m_type;

        // root listview items
        MediaItem *m_playlistItem;
        MediaItem *m_podcastItem;
        // items not on the master playlist and not on the podcast playlist are not visible on the ipod
        MediaItem *m_invisibleItem;
        // items in the database for which the file is missing
        MediaItem *m_staleItem;
        // files without database entry
        MediaItem *m_orphanedItem;

        // stow away all items below m_rootItems when device is not current
        Q3PtrList<Q3ListViewItem> m_rootItems;
};


#endif /* AMAROK_MEDIABROWSER_H */