/*
    Copyright (c) 2008 Daniel Jones <danielcjones@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DYNAMICCATEGORY_H
#define DYNAMICCATEGORY_H

#include "widgets/Widget.h"

#include <QComboBox>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace PlaylistBrowserNS {


/**
*/
class DynamicCategory : public Amarok::Widget
{
    Q_OBJECT
    public:
        DynamicCategory( QWidget* parent );
        ~DynamicCategory();

    private slots:
        // where is the best place for this to live ?
        void OnOff(bool);


    private:
        void On();
        void Off();


        // Do we need these all to be members ??
        QPushButton *m_onoffButton;
        QPushButton *m_repopulateButton;

        QComboBox *m_presetComboBox;
        
        QPushButton *m_saveButton;
        QPushButton *m_deleteButton;
        QTreeView   *m_dynamicTreeView;
};





}

#endif