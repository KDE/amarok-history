#!/usr/bin/env kjscmd
// (c) 2004 Mark Kretschmann <markey@web.de>
// See COPYING file for licensing information.


ScriptManager.connect( ScriptManager, "stop(const QString&)", this, "slotStop" );
ScriptManager.connect( ScriptManager, "configure(const QString&)", this, "slotConfigure" );

var date = new Date();
print( date.getTime() );
print( "\n" );

// Load the demo gui
var gui = Factory.loadui( "setup.ui" );

// ScriptManager.connect( gui.child( "pushButton2" ), "clicked()", this, "slotPlay" );


////////////////////////////////////////////////////////////////////////////////
// functions
////////////////////////////////////////////////////////////////////////////////

function slotTimer()
{
    print( "slotTimer()\n" );

/*    if ( date.getMinutes() == alarmTime.minute() )
        print( "ALARM!\n" ); */
}


function slotStop( name )
{
    print( "slotStop()\n" );

    timer.deleteLater();
}


function slotConfigure( name )
{
    print( "slotConfigure()\n" );
    
    gui.exec();
    print( "after gui.exec()\n" );
    
    if ( gui.child( "activatedCheckBox" ).checked ) {
        print( "alarm activated.\n" );
        
        var alarmTime = gui.child( "alarmTimeEdit" ).time;
        print( alarmTime.toString() );
        print( "\n" );
        
        timer = new QTimer();
        ScriptManager.connect( timer, "timeout()", this, "slotTimer" );
        timer.start( 2000 );
    }
}


function slotPlay( name )
{
    print( "slotPlay()\n" );
    DcopHandler.play();
}

