//
// AmarokSystray
//
// Author: Stanislav Karchebny <berkus@users.sf.net>, (C) 2003
//
// Copyright: like rest of amaroK
//

#ifndef AMAROKSYSTRAY_H
#define AMAROKSYSTRAY_H

#include "engineobserver.h" //baseclass
#include <ksystemtray.h>
#include <qpixmap.h>

namespace amaroK {

class TrayIcon : public KSystemTray, public EngineObserver
{
public:
    TrayIcon( QWidget* );
    ~TrayIcon( );

protected:
    // reimpl from engineobserver
    virtual void engineStateChanged( Engine::State state );
    virtual void engineNewMetaData( const MetaBundle &bundle, bool trackChanged );
    virtual void engineTrackPositionChanged( long position );
    // get notified of 'highlight' color change
    virtual void paletteChange( const QPalette & oldPalette );

private:
    bool event( QEvent* );

    // repaints trayIcon showing progress (and overlay if present)
    void paintIcon( int mergePixels = -1, bool force = false );
    // blend an overlay icon over 'sourcePixmap' and repaint trayIcon
    void blendOverlay( QPixmap &sourcePixmap );

    long trackLength, mergeLevel;
    QPixmap baseIcon, grayedIcon, alternateIcon;
    QPixmap playOverlay, pauseOverlay, stopOverlay;
    QPixmap *overlay;   // the current overlay (may be NULL)
    int blinkTimerID;   // timer ID returned by QObject::startTimer()
    bool overlayVisible;// used for blinking / hiding overlay
};

}

#endif
