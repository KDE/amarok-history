// (c) 2004 Mark Kretschmann <markey@web.de>
// See COPYING file for licensing information.


#include "scriptmanager.h"

#include <kjsembed/jsconsolewidget.h>
#include <kjsembed/kjsembedpart.h>

using namespace KJSEmbed;

////////////////////////////////////////////////////////////////////////////////
// public 
////////////////////////////////////////////////////////////////////////////////

ScriptManager::Manager::Manager( QObject* object )
        : QObject( object )
{
    //KJSEmbed
    m_kjs = new KJSEmbedPart( this );
    JSConsoleWidget* console = m_kjs->view();
    console->show();
}


ScriptManager::Manager::~Manager()
{
    delete selector;
}


void
ScriptManager::Manager::showSelector() //static
{
    if ( !self->selector ) {
        self->selector = new Selector( self->m_list );
        self->selector->show();
    }
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
ScriptManager::Manager::self = 0;

   
#include "scriptmanager.moc"


