/***************************************************************************
                         playerapp.h  -  description
                            -------------------
   begin                : Mit Okt 23 14:35:18 CEST 2002
   copyright            : (C) 2002 by Mark Kretschmann
   email                :
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef AMAROK_PLAYERAPP_H
#define AMAROK_PLAYERAPP_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define APP_VERSION "0.6.91"

#include "amarokarts/amarokarts.h"

#include <kglobalaccel.h>
#include <kuniqueapplication.h>
#include <vector>
#include <arts/artsmodules.h>
#include <arts/kartsdispatcher.h>
#include <arts/kplayobjectfactory.h>

class QColor;
class QListView;
class QListViewItem;
class QString;
class QTimer;

class KConfig;

class BrowserWin;
class EffectWidget;
class PlaylistItem;
class PlayerWidget;

class PlayerApp;
extern PlayerApp *pApp;

class PlayerApp : public KUniqueApplication
{
        Q_OBJECT

    public:
        PlayerApp();
        virtual ~PlayerApp();

        virtual int newInstance();
        bool queryClose();

        void restore();

        // ATTRIBUTES ------
        KGlobalAccel *m_pGlobalAccel;

        PlayerWidget *m_pPlayerWidget;
        BrowserWin *m_pBrowserWin;

        QColor m_bgColor;
        QColor m_fgColor;

        QFont m_browserWindowFont;
        QFont m_playerWidgetFont;
        QFont m_playerWidgetScrollFont;

        // <option attributes>
        bool m_optSavePlaylist;
        bool m_optConfirmClear;
        bool m_optConfirmExit;
        bool m_optFollowSymlinks;
        bool m_optTimeDisplayRemaining;
        bool m_optReadMetaInfo;
        bool m_optRepeatTrack;
        bool m_optRepeatPlaylist;
        bool m_optRandomMode;
        bool m_optShowTrayIcon;
        bool m_optHidePlaylistWindow;
        QString m_optDropMode;
        bool m_optXFade;
        long m_optXFadeLength;
        QColor m_optBrowserFgColor;
        QColor m_optBrowserBgColor;
        unsigned int m_optUndoLevels;
        // </option attributes>

        int m_Volume;
        bool m_bSliderIsPressed;

        KDE::PlayObject *m_pPlayObject;
        KDE::PlayObject *m_pPlayObjectXFade;
        Arts::SoundServerV2 m_Server;
        Amarok::WinSkinFFT m_Scope;
        Arts::StereoEffectStack m_globalEffectStack;
        Arts::StereoEffectStack m_effectStack;
        Arts::StereoVolumeControl m_volumeControl;
        Arts::Synth_AMAN_PLAY m_amanPlay;
        Amarok::Synth_STEREO_XFADE m_XFade;

    public slots:
        void slotPrev();
        void slotPlay();
        void slotConnectPlayObj();
        void slotPause();
        void slotStop();
        void slotNext();
        void slotSavePlaylist();
        void slotClearPlaylistAsk();
        void slotAddLocation();
        void slotSliderPressed();
        void slotSliderReleased();
        void slotSliderChanged( int );
        void slotVolumeChanged( int value );
        void slotMainTimer();
        void slotAnimTimer();
        void slotItemDoubleClicked( QListViewItem *item );
        void slotShowAbout();
        void slotPlaylistToggle( bool b );
        void slotPlaylistHide();
        void slotEq( bool b );
        void slotShowOptions();
        void slotConfigEffects();
        void slotShowTip();
        void slotSetRepeatTrack();
        void slotSetRepeatPlaylist();
        void slotSetRandomMode();
        void slotShowHelp();
        void slotWidgetMinimized();
        void slotWidgetRestored();

    private slots:
        void saveSessionState();

    signals:
        void sigScope( std::vector<float> *s );
        void sigPlay();
        void sigShowTrayIcon( bool );
/*         void sigUpdateFonts(); */

    private:
        void initArts();
        void initPlayerWidget();
        void initMixer();
        bool initMixerHW();
        bool initScope();
        void initBrowserWin();
        void initColors();

        void saveConfig();
        void readConfig();

        void getTrackLength();
        void setupScrolltext();
        void startXFade();
        void stopXFade();

        QString convertDigit( const long &digit );

        // ATTRIBUTES ------
        KArtsDispatcher *m_pArtsDispatcher;
        bool m_usingMixerHW;
        KConfig *m_pConfig;
        QTimer *m_pMainTimer;
        QTimer *m_pAnimTimer;
        long m_scopeId;
        bool m_scopeActive;
        long m_length;
        int m_Mixer;
        int m_playRetryCounter;
        EffectWidget *m_pEffectWidget;
        bool m_bIsPlaying;
        bool m_bChangingSlider;

        bool m_XFadeRunning;
        float m_XFadeValue;
        QString m_XFadeCurrent;
};
#endif                                            // AMAROK_PLAYERAPP_H
