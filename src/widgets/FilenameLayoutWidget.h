/******************************************************************************
 * Copyright (C) 2008 Teo Mrnjavac <teo.mrnjavac@gmail.com>                   *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License as             *
 * published by the Free Software Foundation; either version 2 of             *
 * the License, or (at your option) any later version.                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.      *
 ******************************************************************************/
#ifndef FILENAMELAYOUTWIDGET_H
#define FILENAMELAYOUTWIDGET_H

#include "Token.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>

class Token;

class FilenameLayoutWidget
    : public QFrame
{
    Q_OBJECT

    public:
        FilenameLayoutWidget( QWidget *parent = 0 );
        void addToken( QString text, int index = 0);

        unsigned int getTokenCount();   //access for uint m_tokenCount
        QString getParsableScheme();    //access for QString m_parsableScheme

    protected:
        void mouseMoveEvent( QMouseEvent *event );
        void mousePressEvent( QMouseEvent *event );
        void dragEnterEvent( QDragEnterEvent *event );
        void dragMoveEvent( QDragMoveEvent *event );
        void dropEvent( QDropEvent *event );

    private:
        //void performDrag();
        void performDrag( QMouseEvent *event );
        void insertOverChild( Token *childUnder, QString &textFromMimeData, QDropEvent *event );
        void generateParsableScheme();

        QList< Token * > *tokenList;
        QLabel *backText;
        QHBoxLayout *layout;
        
        QPoint m_startPos;
        unsigned int m_tokenCount;
        QString m_parsableScheme;
};

#endif    //FILENAMELAYOUTWIDGET_H
