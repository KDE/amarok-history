/***************************************************************************
                       amarokslider.h  -  description
                          -------------------
 begin                : Dec 15 2003
 copyright            : (C) 2003 by Mark Kretschmann
 email                :
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef AMAROKSLIDER_H
#define AMAROKSLIDER_H

#include <qslider.h>

namespace amaroK
{
    class Slider : public QSlider
    {
        Q_OBJECT

        public:
            Slider( Qt::Orientation, QWidget*, uint max = 0 );

            virtual void setValue( int );

            //WARNING non-virtual - and thus only really intended for internal use
            //this is a major flaw in the class presently, however it suits our
            //current needs fine
            int value() const { return adjustValue( QSlider::value() ); }

        signals:
            //we emit this when the user has specifically changed the slider
            //so connect to it if valueChanged() is too generic
            //Qt also emits valueChanged( int )
            void sliderReleased( int );

        protected:
            virtual void wheelEvent( QWheelEvent* );
            virtual void mouseMoveEvent( QMouseEvent* );
            virtual void mouseReleaseEvent( QMouseEvent* );
            virtual void mousePressEvent( QMouseEvent* );
            virtual void slideEvent( QMouseEvent* );

            bool m_sliding;

            int adjustValue( int v ) const { return orientation() == Vertical ? maxValue() - v : v; }

        private:
            bool m_outside;
            int  m_prevValue;

            Slider( const Slider& ); //undefined
            Slider &operator=( const Slider& ); //undefined
    };


    class PrettySlider : public Slider
    {
        public:
            PrettySlider( Qt::Orientation orientation, QWidget *parent, uint max = 0 );

        protected:
            virtual void paintEvent( QPaintEvent* );
            virtual void slideEvent( QMouseEvent* );
            virtual void mousePressEvent( QMouseEvent* );

        private:
            PrettySlider( const PrettySlider& ); //undefined
            PrettySlider &operator=( const PrettySlider& ); //undefined
    };
}

#endif
