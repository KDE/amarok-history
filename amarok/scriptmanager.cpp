// (c) 2004 Mark Kretschmann <markey@web.de>
// See COPYING file for licensing information.


#include "scriptmanager.h"

#include <kdebug.h>
#include <kjsembed/jsconsolewidget.h>
#include <kjsembed/kjsembedpart.h>

using namespace KJSEmbed;

////////////////////////////////////////////////////////////////////////////////
// public 
////////////////////////////////////////////////////////////////////////////////

ScriptManager::Manager::Manager( QObject* object )
        : QObject( object )
{
    self = this;
    
    //KJSEmbed
    m_kjs = new KJSEmbedPart( this );
    JSConsoleWidget* console = m_kjs->view();
    console->show();
}


ScriptManager::Manager::~Manager()
{}


void
ScriptManager::Manager::showSelector() //static
{
    kdDebug() << "BEGIN " << k_funcinfo << endl;

    Selector dia( self->m_list );
    Selector::Result result = dia.exec();
    
    for ( int i = 0; i < result.dirs.count(); i++ ) {
        kdDebug() << "Running script: " << result.dirs[i] << endl;
        self->m_kjs->runFile( result.dirs[i] );
    }
    
    kdDebug() << "END " << k_funcinfo << endl;  
}


void
ScriptManager::Manager::addObject( QObject* object )
{
    m_kjs->addObject( object );
}


////////////////////////////////////////////////////////////////////////////////
// private 
////////////////////////////////////////////////////////////////////////////////

ScriptManager::Manager*
ScriptManager::Manager::self;

   
#include "scriptmanager.moc"


