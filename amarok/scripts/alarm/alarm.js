#!/usr/bin/env kjscmd
// (c) 2004 Mark Kretschmann <markey@web.de>
// See COPYING file for licensing information.


ScriptManager.connect( ScriptManager, "stop(const QString&)", this, "slotStop" );
ScriptManager.connect( ScriptManager, "configure(const QString&)", this, "slotConfigure" );

// Load the demo gui
var gui = Factory.loadui( "setup.ui" );

ScriptManager.connect( gui.child( "pushButton2" ), "clicked()", this, "slotPlay" );


////////////////////////////////////////////////////////////////////////////////
// functions
////////////////////////////////////////////////////////////////////////////////

function slotStop( name )
{
    print( "slotStop()\n" );
}


function slotConfigure( name )
{
    print( "slotConfigure()\n" );
    gui.show();
}


function slotPlay( name )
{
    print( "slotPlay()\n" );
    DcopHandler.play();
}

