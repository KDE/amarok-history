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

#include "DynamicCategory.h"

#include "Amarok.h"
#include "Debug.h"
#include "DynamicModel.h"
#include "DynamicBiasDelegate.h"
#include "DynamicBiasModel.h"
#include "amarokconfig.h"
#include "playlist/PlaylistActions.h"
#include "playlist/PlaylistModel.h"
#include "PaletteHandler.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>

#include <KHBox>
#include <KIcon>
#include <KToolBar>


namespace PlaylistBrowserNS {


DynamicCategory::DynamicCategory( QWidget* parent )
    : Amarok::Widget( parent ), m_biasListView(0), m_biasModel(0), m_biasDelegate(0)
{
    bool enabled = AmarokConfig::dynamicMode();

    setContentsMargins(0,0,0,0);

    m_vLayout = new QVBoxLayout( this );

    //m_vLayout->setSizePolicy( 
            //QSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored ) );

    m_onoffButton = new QPushButton( this );
    m_onoffButton->setIcon( KIcon( "dynamic-amarok" ) );
    m_onoffButton->setText( i18n( "On" ) );
    m_onoffButton->setToolTip( i18n( "Turn dynamic mode on." ) );
    m_onoffButton->setCheckable( true );
    m_onoffButton->setSizePolicy( 
            QSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred ) );
    QObject::connect( m_onoffButton, SIGNAL(toggled(bool)), this, SLOT(OnOff(bool)) );

    QObject::connect( (const QObject*)Amarok::actionCollection()->action( "playlist_clear" ),  SIGNAL( triggered( bool ) ),  this, SLOT( playlistCleared() ) );
    
    m_repopulateButton = new QPushButton( this );
    m_repopulateButton->setText( i18n("Repopulate") );
    m_repopulateButton->setToolTip( i18n("Replace the upcoming tracks with fresh ones.") );
    m_repopulateButton->setIcon( KIcon( "view-refresh-amarok" ) );
    m_repopulateButton->setEnabled( enabled );
    m_repopulateButton->setSizePolicy( 
            QSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred ) );
    QObject::connect( m_repopulateButton, SIGNAL(clicked(bool)), The::playlistActions(), SLOT(repopulateDynamicPlaylist()) );
            

    KHBox* presetLayout = new KHBox( this );


    QLabel* presetLabel = new QLabel( i18n( "Playlist:" ), presetLayout );

    m_presetComboBox = new KComboBox( presetLayout );
    m_presetComboBox->setPalette( QApplication::palette() );
    DynamicModel::instance()->loadPlaylists();
    m_presetComboBox->setModel( DynamicModel::instance() );
    
    connect( DynamicModel::instance(), SIGNAL(changeActive(int)),
            m_presetComboBox, SLOT(setCurrentIndex(int)) );

    connect( DynamicModel::instance(), SIGNAL(enableDynamicMode(bool)),
            SLOT(enableDynamicMode(bool)) );

    connect( m_presetComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(playlistSelectionChanged(int) ) );

    presetLabel->setBuddy( m_presetComboBox );

    presetLayout->setStretchFactor( m_presetComboBox, 1 );



    KToolBar* presetToolbar = new KToolBar( presetLayout );
    presetToolbar->setToolButtonStyle( Qt::ToolButtonIconOnly );
    presetToolbar->setMovable( false );
    presetToolbar->setFloatable( false );
    presetToolbar->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Preferred );
    m_saveButton   = new QToolButton( presetToolbar );
    //m_saveButton->setText( i18n("Save") );
    m_saveButton->setIcon( KIcon( "document-save-amarok" ) );
    m_saveButton->setToolTip( i18n( "Save the preset." ) );
    presetToolbar->addWidget( m_saveButton );

    connect( m_saveButton, SIGNAL(clicked(bool)), SLOT(save()) );


    m_deleteButton = new QToolButton( presetToolbar );
    //m_deleteButton->setText( i18n("Delete") );
    m_deleteButton->setEnabled( false );
    m_deleteButton->setIcon( KIcon( "edit-delete-amarok" ) );
    m_deleteButton->setToolTip( i18n( "Delete the preset.") );
    presetToolbar->addWidget( m_deleteButton );

    connect( m_deleteButton, SIGNAL(clicked(bool)),
            DynamicModel::instance(), SLOT(removeActive()) );
        

    m_biasListView = new QListView( this );
    m_biasListView->setFrameShape( QFrame::NoFrame );


    m_biasListView->setAlternatingRowColors( true );
    The::paletteHandler()->updateItemView( m_biasListView );
    connect( The::paletteHandler(), SIGNAL( newPalette( const QPalette & ) ), SLOT( newPalette( const QPalette & ) ) );

    m_biasModel = new DynamicBiasModel( m_biasListView );
    m_biasListView->setModel( m_biasModel );

    connect( m_biasModel, SIGNAL(playlistModified(Dynamic::BiasedPlaylistPtr)),
             DynamicModel::instance(), SLOT(playlistModified(Dynamic::BiasedPlaylistPtr)) );

    m_biasDelegate = new DynamicBiasDelegate( m_biasListView );
    m_biasListView->setItemDelegate( m_biasDelegate );

    m_vLayout->addWidget( m_onoffButton );
    m_vLayout->addWidget( m_repopulateButton );
    m_vLayout->addWidget( presetLayout );
    m_vLayout->addWidget( m_biasListView );

    this->setLayout( m_vLayout );


    int index = DynamicModel::instance()->playlistIndex( 
            AmarokConfig::lastDynamicMode() );

    debug() << "Setting index: " << index;
    if( index >= 0 )
    {
        m_presetComboBox->setCurrentIndex( index );
        playlistSelectionChanged( index );
    }
    else
    {
        m_presetComboBox->setCurrentIndex( 0 );
        playlistSelectionChanged( 0 );
    }

    m_onoffButton->setChecked( AmarokConfig::dynamicMode() );
}


DynamicCategory::~DynamicCategory() 
{

}


void
DynamicCategory::enableDynamicMode( bool enable )
{
    if( AmarokConfig::dynamicMode() == enable )
        return;

    if( enable )
        On();
    else
        Off();
}

void
DynamicCategory::OnOff( bool checked )
{
    if( checked )
        On();
    else
        Off();
}


void
DynamicCategory::On()
{
    AmarokConfig::setDynamicMode( true );
    // TODO: turn off other incompatible modes
    AmarokConfig::self()->writeConfig();
    m_repopulateButton->setEnabled( true );
    The::playlistActions()->playlistModeChanged();

    //if the playlist is empty, repopulate while we are at it:

    if ( The::playlistModel()->rowCount() == 0 )
        The::playlistActions()->repopulateDynamicPlaylist();

}

void
DynamicCategory::Off()
{
    AmarokConfig::setDynamicMode( false );
    // TODO: should we restore the state of other modes?
    AmarokConfig::self()->writeConfig();
    m_repopulateButton->setEnabled( false );
    The::playlistActions()->playlistModeChanged();
}

void
DynamicCategory::playlistCleared() // SLOT
{
    // we have a whole method here b/c we  don't want to do any extra work on each clear press
    if( AmarokConfig::dynamicMode() ) // only do anything if dynamic mode was on
    {
        AmarokConfig::setDynamicMode( false );
        // TODO: should we restore the state of other modes?
        AmarokConfig::self()->writeConfig();
        m_repopulateButton->setEnabled( false );
        m_onoffButton->setChecked( false );
        The::playlistActions()->playlistModeChanged();
    }
}

void
DynamicCategory::playlistSelectionChanged( int index )
{
    Dynamic::DynamicPlaylistPtr playlist =
        DynamicModel::instance()->setActivePlaylist( index );

    if( DynamicModel::instance()->isActiveDefault() )
        m_deleteButton->setEnabled( false );
    else
        m_deleteButton->setEnabled( true );


    if( !DynamicModel::instance()->isActiveUnsaved() )
    {
        AmarokConfig::setLastDynamicMode( playlist->title() );
        AmarokConfig::self()->writeConfig();
    }


    m_biasModel->setPlaylist( playlist );

    debug() << "Changing biased playlist to: " << playlist->title();
}

void
DynamicCategory::save()
{
    bool ok;
    QString title =
        QInputDialog::getText( this, i18n("Playlist Name"),
                               i18n("Enter a name for the playlist:"),
                               QLineEdit::Normal,
                               DynamicModel::instance()->activePlaylist()->title(),
                               &ok );
    if( !ok ) return;

    // TODO: write a custom dialog to prevent this from happening in the first
    // place
    if( title == DynamicModel::instance()->defaultPlaylist()->title() )
    {
        QMessageBox::warning( this, i18n( "Warning!" ), i18n( "Can not overwrite the random playlist." ) );
        return;
    }

    DynamicModel::instance()->saveActive( title );
}

}

void PlaylistBrowserNS::DynamicCategory::newPalette(const QPalette & palette)
{
    Q_UNUSED( palette )
    The::paletteHandler()->updateItemView( m_biasListView );
    m_biasListView->reset();
}


#include "DynamicCategory.moc"

