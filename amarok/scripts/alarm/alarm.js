#!/usr/bin/env kjscmd
// (c) 2004 Mark Kretschmann <markey@web.de>
// See COPYING file for licensing information.


ScriptManager.connect( ScriptManager, 'configure(const QString&)', this, 'slotConfigure' );

// Load the demo gui
var gui = Factory.loadui( '/home/mark/mysource/kdecvs/amarok_1_1_branch/kdeextragear-1/amarok/amarok/scripts/alarm/setup.ui' );

var widget = new QWidget();
widget.show();

// application.exec();

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

this.slotConfigure = function( name )
{
    print( "slotConfigure()\n" );
    gui.show();
}
