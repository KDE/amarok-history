//
// Amarok BarAnalyzer 3 - Jet Turbine: Symmetric version of analyzer 1
//
// Author: Stanislav Karchebny <berkus@users.sf.net>, (C) 2003
//
// Copyright: like rest of amaroK
//

#ifndef ANALYZER_TURBINE_H
#define ANALYZER_TURBINE_H

#include "boomanalyzer.h"

class TurbineAnalyzer : public BoomAnalyzer
{
    public:
        TurbineAnalyzer( QWidget *parent ) : BoomAnalyzer( parent ) {}

        void analyze( const Scope& );
};

#endif
