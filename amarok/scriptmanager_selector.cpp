// (c) 2003 Scott Wheeler <wheeler@kde.org>,
// (c) 2004 Mark Kretschmann <markey@web.de>
// See COPYING file for licensing information.

#include <qcheckbox.h>

#include <kfiledialog.h>
#include <klocale.h>
#include <klistview.h>
#include <kpushbutton.h>

#include "scriptmanagerbase.h"
#include "scriptmanager_selector.h"

////////////////////////////////////////////////////////////////////////////////
// public interface
////////////////////////////////////////////////////////////////////////////////


ScriptManager::Selector::Selector( const QStringList &directories, QWidget *parent, const char *name )
        : KDialogBase( parent, name, true, i18n( "Script List" ), Ok | Cancel, Ok, true )
        , m_dirList( directories )
{
    m_base = new ScriptManagerBase( this );
    setWFlags( Qt::WDestructiveClose );

    setMainWidget( m_base );

    m_base->directoryListView->setFullWidth( true );

    connect( m_base->addDirectoryButton, SIGNAL( clicked() ),
             SLOT( slotAddDirectory() ) );
    connect( m_base->removeDirectoryButton, SIGNAL( clicked() ),
             SLOT( slotRemoveDirectory() ) );

    QStringList::ConstIterator it = directories.begin();
    for ( ; it != directories.end(); ++it )
        new KListViewItem( m_base->directoryListView, *it );

    //     m_base->scanRecursivelyCheckBox->setChecked( scanRecursively );
    //     m_base->monitorChangesCheckBox->setChecked( monitorChanges );

    QSize sz = sizeHint();
    setMinimumSize( kMax( 350, sz.width() ), kMax( 250, sz.height() ) );
    resize( sizeHint() );
}


ScriptManager::Selector::~Selector()
{}


////////////////////////////////////////////////////////////////////////////////
// private interface
////////////////////////////////////////////////////////////////////////////////


ScriptManager::Selector::Result
ScriptManager::Selector::exec()
{
    m_result.status = static_cast<DialogCode>( KDialogBase::exec() );
    m_result.dirs = m_dirList;
    return m_result;
}


void
ScriptManager::Selector::slotAddDirectory()
{
    QString dir = KFileDialog::getExistingDirectory();

    if ( !dir.isEmpty() && m_dirList.find( dir ) == m_dirList.end() ) {
        m_dirList.append( dir );
        new KListViewItem( m_base->directoryListView, dir );
        m_result.addedDirs.append( dir );
    }
}


void
ScriptManager::Selector::slotRemoveDirectory()
{
    if ( !m_base->directoryListView->selectedItem() )
        return ;

    QString dir = m_base->directoryListView->selectedItem() ->text( 0 );
    m_dirList.remove( dir );
    m_result.removedDirs.append( dir );
    delete m_base->directoryListView->selectedItem();
}


#include "scriptmanager_selector.moc"

