// Maintainer: Max Howell <max.howell@methylblue.com>, (C) 2004
// Copyright:  See COPYING file that comes with this distribution
//
// Description: a popupmenu to control various features of amaroK
//              also provides amaroK's helpMenu

#ifndef AMAROKMENU_H
#define AMAROKMENU_H

#include "engineobserver.h"
#include <kaction.h>
#include <kpopupmenu.h>

class KActionCollection;
class KHelpMenu;


namespace amaroK {

class Menu : public KPopupMenu
{
Q_OBJECT

public:
    enum MenuIds { ID_CONF_DECODER, ID_SHOW_VIS_SELECTOR, ID_SHOW_SCRIPT_SELECTOR }; 

    Menu( QWidget *parent );
    static KPopupMenu *helpMenu( QWidget *parent = 0 );

private slots:
    void slotAboutToShow();
    void slotActivated( int index );

private:
    static KHelpMenu *HelpMenu;
};


class MenuAction : public KAction
{
public:
    MenuAction( KActionCollection* );
    virtual int plug( QWidget*, int index = -1 );
};


class PlayPauseAction : public KAction, public EngineObserver
{
public:
    PlayPauseAction( KActionCollection* );
    ~PlayPauseAction();
    virtual void engineStateChanged( EngineBase::EngineState );
};


class AnalyzerAction : public KAction
{
public:
    AnalyzerAction( KActionCollection* );
    virtual int plug( QWidget *, int index = -1 );
};


class RandomAction : public KToggleAction
{
public:
    RandomAction( KActionCollection *ac );
    virtual void setChecked( bool on );
};


class RepeatTrackAction : public KToggleAction
{
public:
    RepeatTrackAction( KActionCollection *ac );
    virtual void setChecked( bool on );
};


class RepeatPlaylistAction : public KToggleAction
{
public:
    RepeatPlaylistAction( KActionCollection *ac );
    virtual void setChecked( bool on );
};

}

#endif
