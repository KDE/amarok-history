// Maintainer: Max Howell <max.howell@methylblue.com>, (C) 2004
// Copyright: See COPYING file that comes with this distribution
//

#ifndef VIS_SOCKETSERVER_H
#define VIS_SOCKETSERVER_H

#include <qlistview.h>        //baseclass
#include <qserversocket.h>    //baseclass
#include <qsocketnotifier.h>  //baseclass
#include <vector>             //stack allocated

class QPoint;
class KProcess;

namespace amaroK {

class SocketServer : public QServerSocket
{
public:
    SocketServer( const QString&, QObject* );
    ~SocketServer();

protected:
    int m_sockfd;
};

} //namespace amaroK



class LoaderServer : public amaroK::SocketServer
{
    public:
        LoaderServer( QObject* parent );

    private:
        void newConnection( int socket );
};



namespace Vis {

class SocketServer : public amaroK::SocketServer
{
//Q_OBJECT

public:
    SocketServer( QObject* );
    void newConnection( int );

//private slots:
//    void request( int );
};


class SocketNotifier : public QSocketNotifier
{
Q_OBJECT

public:
    SocketNotifier( int sockfd );

private slots:
    void request( int );
};



class Selector : public QListView
{
Q_OBJECT

    Selector( QWidget* = 0 );
    static Selector *m_instance;

public:
    static Selector* instance();

    void mapPID( int, int ); //assigns pid/sockfd combo

    class Item : public QCheckListItem //TODO use stack allocated KProcess
    {
    public:
        Item( QListView *parent, const char *command, const QString &text )
          : QCheckListItem( parent, text, QCheckListItem::CheckBox )
          , m_proc( 0 )
          , m_sockfd( -1 )
          , m_command( command )
        {}
        ~Item();

        virtual void stateChange( bool state );

        KProcess *m_proc;
        int       m_sockfd;
        const char *m_command;
    };

private slots:
    void rightButton( QListViewItem*, const QPoint&, int );

public slots:
    void processExited( KProcess* );
};

} //namespace VIS

#endif
