// Maintainer: Max Howell <mac.howell@methylblue.com>, (C) 2003-4
// Copyright:  See COPYING file that comes with this distribution
//

#ifndef BLOCKANALYZER_H
#define BLOCKANALYZER_H

#include "analyzerbase.h"

/**
@author Max Howell
*/

class QResizeEvent;
class QMouseEvent;
class QPalette;

class BlockAnalyzer : public Analyzer::Base2D
{
public:
    BlockAnalyzer( QWidget* );
    ~BlockAnalyzer();

    static const uint HEIGHT      = 2;
    static const uint WIDTH       = 4;
    static const uint MIN_ROWS    = 7;   //arbituary
    static const uint MAX_ROWS    = 14;  //arbituary, at some point maybe remove this
    static const uint MIN_COLUMNS = 32;  //arbituary
    static const uint MAX_COLUMNS = 128; //must be 2**n

protected:
    void transform( Scope& );
    void analyze( const Scope& );
    void resizeEvent( QResizeEvent* );
    void mousePressEvent( QMouseEvent* );
    void paletteChange( const QPalette& );

private:
    QPixmap m_glow;
    QPixmap m_dark;

    QPixmap* const glow() { return &m_glow; }
    QPixmap* const dark() { return &m_dark; }

    std::vector<uint> m_store; //store previous values
    Scope m_scope;             //so we don't create a vector every frame
    uint m_columns, m_rows;    //current size values

    float yscale[MAX_ROWS+1];

    uint oy;
};

#endif
