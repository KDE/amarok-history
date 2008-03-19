/***************************************************************************
 * copyright     : (C) 2004 Mark Kretschmann <markey@web.de>               *
                   (C) 2007 Dan Meltzer <hydrogen@notyetimplemented.com>   *
 **************************************************************************/

 /***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "volumewidget.h"

#include "amarok.h"
#include "amarokconfig.h"
#include "enginecontroller.h"
#include "sliderwidget.h"

#include <klocale.h>

VolumeWidget::VolumeWidget( QWidget *parent )
    : KHBox( parent ),
      EngineObserver( EngineController::instance() ),
      m_slider( 0 )
{
    m_slider = new Amarok::VolumeSlider( this, Amarok::VOLUME_MAX );
    m_slider->setObjectName( "ToolBarVolume" );
    m_slider->setValue( AmarokConfig::masterVolume() );
    setContentsMargins(0,0,0,0);

    m_slider->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum );
    //m_slider->setFixedSize( width(), height() );

    m_slider->setToolTip( i18n( "Volume control" ) );

    EngineController* const ec = EngineController::instance();
    connect( m_slider, SIGNAL(sliderMoved( int )), ec, SLOT(setVolume( int )) );
    connect( m_slider, SIGNAL(sliderReleased( int )), ec, SLOT(setVolume( int )) );
}

void
VolumeWidget::engineVolumeChanged( int value )
{
    if( m_slider ) m_slider->setValue( value );
}

#include "volumewidget.moc"