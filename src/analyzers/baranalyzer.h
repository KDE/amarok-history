// Maintainer: Max Howell <max.howell@methylblue.com>
// Authors:    Mark Kretcshmann & Max Howell (C) 2003-4
// Copyright:  See COPYING file that comes with this distribution
//

#ifndef BARANALYZER_H
#define BARANALYZER_H

#include "analyzerbase.h"


using std::vector;
typedef vector<uint> aroofMemVec;

class BarAnalyzer : public Analyzer::Base2D
{
    public:
        BarAnalyzer( QWidget* );

        void init();
        virtual void analyze( const Scope& );
        //virtual void transform( Scope& );
        void   resizeEvent( QResizeEvent * e);

        uint BAND_COUNT;
        int MAX_DOWN;
        int MAX_UP;
        static const uint ROOF_HOLD_TIME = 48;
        static const int  ROOF_VELOCITY_REDUCTION_FACTOR = 32;
        static const uint NUM_ROOFS = 16;
        static const uint COLUMN_WIDTH = 4;

    protected:
        QPixmap m_pixRoof[NUM_ROOFS];
        //vector<uint> m_roofMem[BAND_COUNT];

        //Scope m_bands; //copy of the Scope to prevent creating/destroying a Scope every iteration
        uint  m_lvlMapper[256];
        vector<aroofMemVec> m_roofMem;
        vector<uint> barVector;          //positions of bars
        vector<int>  roofVector;         //positions of roofs
        vector<uint> roofVelocityVector; //speed that roofs falls

        const QPixmap *gradient() const { return &m_pixBarGradient; }

    private:
        QPixmap m_pixBarGradient;
        QPixmap m_pixCompose;
        Scope m_scope;             //so we don't create a vector every frame
        QColor bg;
};

#endif
