#include "statusbar.h"
#include "amarokconfig.h"
#include "metabundle.h"
#include "playerapp.h"

#include <qcolor.h>
#include <qapplication.h>

#include <kactionclasses.h>

#include <enginecontroller.h>

amaroK::StatusBar::StatusBar( QWidget *parent, const char *name ) : KStatusBar( parent, name )
{
    EngineController::instance()->attach( this );

    // random
    ToggleLabel *rand = new ToggleLabel( i18n( "RAND" ), this );
    addWidget( rand, 0, true );
    KToggleAction *tAction = static_cast<KToggleAction *>(pApp->actionCollection()->action( "random_mode" ));
    connect( rand, SIGNAL( toggled( bool ) ), tAction, SLOT( setChecked( bool ) ) );
    connect( tAction, SIGNAL( toggled(bool) ), rand, SLOT( setOn(bool) ) );
    rand->setOn( tAction->isChecked() );

    // repeat playlist
    ToggleLabel *repeat = new ToggleLabel( i18n( "REP" ), this );
    addWidget( repeat, 0, true );
    tAction = static_cast<KToggleAction *>(pApp->actionCollection()->action( "repeat_playlist" ));
    connect( repeat, SIGNAL( toggled( bool ) ), tAction, SLOT( setChecked( bool ) ) );
    connect( tAction, SIGNAL( toggled(bool) ), repeat, SLOT( setOn(bool) ) );
    repeat->setOn( tAction->isChecked() );

    addWidget( (m_pTimeLabel = new ToggleLabel( "", this )), 0, true );
    m_pTimeLabel->setColorToggle( false );
    connect( m_pTimeLabel, SIGNAL( toggled( bool ) ), this, SLOT( slotToggleTime() ) );

    // make the time label show itself.
    engineTrackPositionChanged( 0 );
}


amaroK::StatusBar::~StatusBar()
{
    EngineController::instance()->detach( this );
}


void amaroK::StatusBar::engineStateChanged( EngineBase::EngineState state )
{
    switch( state )
    {
        case EngineBase::Idle:
        case EngineBase::Empty:
            engineTrackPositionChanged( 0 );
            break;
        case EngineBase::Playing: // gcc silense
        case EngineBase::Paused:
            break;
    }
}


void amaroK::StatusBar::engineNewMetaData( const MetaBundle &bundle, bool /*trackChanged*/ )
{
    message( bundle.prettyTitle() + "  (" + bundle.prettyLength() + ")" );
}

void amaroK::StatusBar::engineTrackPositionChanged( long position )
{
    // TODO: Don't duplicate code
    int seconds = position / 1000;
    int songLength = EngineController::instance()->trackLength() / 1000;
    bool remaining = AmarokConfig::timeDisplayRemaining() && songLength > 0;

    if( remaining ) seconds = songLength - seconds;

    QString
    str  = zeroPad( seconds /60/60%60 );
    str += ':';
    str += zeroPad( seconds /60%60 );
    str += ':';
    str += zeroPad( seconds %60 );
    m_pTimeLabel->setText( str );
}

void amaroK::StatusBar::slotToggleTime()
{
    AmarokConfig::setTimeDisplayRemaining( !AmarokConfig::timeDisplayRemaining() );
}

/********** ToggleLabel ****************/

amaroK::ToggleLabel::ToggleLabel( const QString &text, QWidget *parent, const char *name ) :
    QLabel( text, parent, name )
    , m_State( false )
    , m_ColorToggle( true )
{
}

amaroK::ToggleLabel::~ToggleLabel()
{
}

void amaroK::ToggleLabel::setColorToggle( bool on )
{
    m_ColorToggle = on;
    QColorGroup group = QApplication::palette().active();
    setPaletteForegroundColor( group.text() );
}

void amaroK::ToggleLabel::mouseDoubleClickEvent ( QMouseEvent */*e*/ )
{
    m_State = !m_State;
    if( m_ColorToggle )
    {
        QColorGroup group = QApplication::palette().active();
        if( m_State )
            setPaletteForegroundColor( group.text() );
        else
            setPaletteForegroundColor( group.mid() );
    }
    emit toggled( m_State );
}

void amaroK::ToggleLabel::setOn( bool on )
{
    if( m_ColorToggle )
    {
        QColorGroup group = QApplication::palette().active();
        if( on )
            setPaletteForegroundColor( group.text() );
        else
            setPaletteForegroundColor( group.mid() );

    }

    m_State = on;
}

#include "statusbar.moc"
