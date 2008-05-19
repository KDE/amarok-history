/***************************************************************************
 *   Copyright (C) 2004-2007 by Mark Kretschmann <markey@web.de>           *
 *                 2005-2007 by Seb Ruiz <ruiz@kde.org>                    *
 *                      2006 by Alexandre Oliveira <aleprj@gmail.com>      *
 *                      2006 by Martin Ellis <martin.ellis@kdemail.net>    *
 *                      2007 by Leonardo Franchi <lfranchi@gmail.com>      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.          *
 ***************************************************************************/

#define DEBUG_PREFIX "ScriptManager"

#include "scriptmanager.h"

#include "Amarok.h"
#include "amarokconfig.h"
#include "Debug.h"
#include "EngineController.h"
#include "AmarokProcess.h"
#include "ContextStatusBar.h"
#include "TheInstances.h"
#include "servicebrowser/scriptableservice/ScriptableServiceManager.h"

#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KApplication>
#include <KFileDialog>
#include <KIO/NetAccess>
#include <KLocale>
#include <KMenu>
#include <KMessageBox>
#include <knewstuff2/engine.h>
#include <KProtocolManager>
#include <KPushButton>
#include <KRun>
#include <KStandardDirs>
#include <KTar>
#include <KTextEdit>
#include <KWindowSystem>

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QPixmap>
#include <QSettings>
#include <QTextCodec>
#include <QTimer>

#include <sys/stat.h>
#include <sys/types.h>

namespace Amarok {

    QString
    proxyForUrl(const QString& url)
    {
        KUrl kurl( url );

        QString proxy;

        if ( KProtocolManager::proxyForUrl( kurl ) !=
                QString::fromLatin1( "DIRECT" ) ) {
            KProtocolManager::slaveProtocol ( kurl, proxy );
        }

        return proxy;
    }

    QString
    proxyForProtocol(const QString& protocol)
    {
        return KProtocolManager::proxyFor( protocol );
    }


}

////////////////////////////////////////////////////////////////////////////////
// class ScriptManager
////////////////////////////////////////////////////////////////////////////////

ScriptManager* ScriptManager::s_instance = 0;


ScriptManager::ScriptManager( QWidget *parent, const char *name )
        : KDialog( parent )
        , EngineObserver( The::engineController() )
        , m_gui( new Ui::ScriptManagerBase() )
{
    DEBUG_BLOCK
    setObjectName( name );
    setModal( false );
    setButtons( Close );
    setDefaultButton( Close );
    showButtonSeparator( true );


    s_instance = this;

    kapp->setTopWidget( this );
    setCaption( KDialog::makeStandardCaption( i18n( "Script Manager" ) ) );

    // Gives the window a small title bar, and skips a taskbar entry
#ifdef Q_WS_X11
    KWindowSystem::setType( winId(), NET::Utility );
    KWindowSystem::setState( winId(), NET::SkipTaskbar );
#endif

    QWidget* main = new QWidget( this );
    m_gui->setupUi( main );

    setMainWidget( main );

    /// Category items
    m_generalCategory    = new QTreeWidgetItem( m_gui->treeWidget );
    m_lyricsCategory     = new QTreeWidgetItem( m_gui->treeWidget );
    m_scoreCategory      = new QTreeWidgetItem( m_gui->treeWidget );
    m_transcodeCategory  = new QTreeWidgetItem( m_gui->treeWidget );
    m_contextCategory    = new QTreeWidgetItem( m_gui->treeWidget );
    m_servicesCategory   = new QTreeWidgetItem( m_gui->treeWidget );

    m_generalCategory  ->setText( 0, i18n( "General" ) );
    m_lyricsCategory   ->setText( 0, i18n( "Lyrics" ) );
    m_scoreCategory    ->setText( 0, i18n( "Score" ) );
    m_transcodeCategory->setText( 0, i18n( "Transcoding" ) );
    m_contextCategory  ->setText( 0, i18n( "Context Browser" ) );
    m_servicesCategory ->setText( 0, i18n( "Scripted Services" ) );

    m_generalCategory  ->setFlags( Qt::ItemIsEnabled );
    m_lyricsCategory   ->setFlags( Qt::ItemIsEnabled );
    m_scoreCategory    ->setFlags( Qt::ItemIsEnabled );
    m_transcodeCategory->setFlags( Qt::ItemIsEnabled );
    m_contextCategory  ->setFlags( Qt::ItemIsEnabled );
    m_servicesCategory ->setFlags( Qt::ItemIsEnabled );

    m_generalCategory  ->setIcon( 0, SmallIcon( "folder-amarok" ) );
    m_lyricsCategory   ->setIcon( 0, SmallIcon( "folder-amarok" ) );
    m_scoreCategory    ->setIcon( 0, SmallIcon( "folder-amarok" ) );
    m_transcodeCategory->setIcon( 0, SmallIcon( "folder-amarok" ) );
    m_contextCategory  ->setIcon( 0, SmallIcon( "folder-amarok" ) );
    m_servicesCategory  ->setIcon( 0, SmallIcon( "folder-amarok" ) );


    // Restore the open/closed state of the category items
    KConfigGroup config = Amarok::config( "ScriptManager" );
    m_generalCategory  ->setExpanded( config.readEntry( "General category open", false ) );
    m_lyricsCategory   ->setExpanded( config.readEntry( "Lyrics category open", false ) );
    m_scoreCategory    ->setExpanded( config.readEntry( "Score category State", false ) );
    m_transcodeCategory->setExpanded( config.readEntry( "Transcode category open", false ) );
    m_contextCategory  ->setExpanded( config.readEntry( "Context category open", false ) );
    m_servicesCategory ->setExpanded( config.readEntry( "Service category open", false ) );

    connect( m_gui->treeWidget, SIGNAL( currentItemChanged( QTreeWidgetItem*, QTreeWidgetItem* ) ), SLOT( slotCurrentChanged( QTreeWidgetItem* ) ) );
    connect( m_gui->treeWidget, SIGNAL( itemDoubleClicked( QTreeWidgetItem*, int ) ), SLOT( slotRunScript() ) );
    connect( m_gui->treeWidget, SIGNAL( customContextMenuRequested ( const QPoint& ) ), SLOT( slotShowContextMenu( const QPoint& ) ) );

    connect( m_gui->installButton,   SIGNAL( clicked() ), SLOT( slotInstallScript() ) );
    connect( m_gui->retrieveButton,  SIGNAL( clicked() ), SLOT( slotRetrieveScript() ) );
    connect( m_gui->uninstallButton, SIGNAL( clicked() ), SLOT( slotUninstallScript() ) );
    connect( m_gui->runButton,       SIGNAL( clicked() ), SLOT( slotRunScript() ) );
    connect( m_gui->stopButton,      SIGNAL( clicked() ), SLOT( slotStopScript() ) );
    connect( m_gui->configureButton, SIGNAL( clicked() ), SLOT( slotConfigureScript() ) );
    connect( m_gui->aboutButton,     SIGNAL( clicked() ), SLOT( slotAboutScript() ) );

    m_gui->installButton  ->setIcon( KIcon( "folder-amarok" ) );
    m_gui->retrieveButton ->setIcon( KIcon( "get-hot-new-stuff-amarok" ) );
    m_gui->uninstallButton->setIcon( KIcon( "edit-delete-amarok" ) );
    m_gui->runButton      ->setIcon( KIcon( "media-playback-start-amarok" ) );
    m_gui->stopButton     ->setIcon( KIcon( "media-playback-stop-amarok" ) );
    m_gui->configureButton->setIcon( KIcon( "configure-amarok" ) );
    m_gui->aboutButton    ->setIcon( KIcon( "help-about-amarok" ) );

    QSize sz = sizeHint();
    setMinimumSize( qMax( 350, sz.width() ), qMax( 250, sz.height() ) );
    resize( sizeHint() );

//FIXME: contex tbrowser changes
//     connect( this, SIGNAL(lyricsScriptChanged()), ContextBrowser::instance(), SLOT( lyricsScriptChanged() ) );

    // Delay this call via eventloop, because it's a bit slow and would block
    QTimer::singleShot( 0, this, SLOT( findScripts() ) );
}


ScriptManager::~ScriptManager()
{
    DEBUG_BLOCK

    QStringList runningScripts;
    foreach( const QString &key, m_scripts.keys() ) {
        if( m_scripts[key].process ) {
            terminateProcess( &m_scripts[key].process );
            runningScripts << key;
        }
    }
    // Save config
    KConfigGroup config = Amarok::config( "ScriptManager" );
    config.writeEntry( "Running Scripts", runningScripts );

    // Save the open/closed state of the category items
    config.writeEntry( "General category open", m_generalCategory->isExpanded() );
    config.writeEntry( "Lyrics category open", m_lyricsCategory->isExpanded() );
    config.writeEntry( "Score category open", m_scoreCategory->isExpanded() );
    config.writeEntry( "Transcode category open", m_transcodeCategory->isExpanded() );
    config.writeEntry( "Context category open", m_contextCategory->isExpanded() );
    config.writeEntry( "Service category open", m_servicesCategory->isExpanded() );
    s_instance = 0;
}


////////////////////////////////////////////////////////////////////////////////
// public
////////////////////////////////////////////////////////////////////////////////

bool
ScriptManager::runScript( const QString& name, bool silent )
{
    if( !m_scripts.contains( name ) )
        return false;

    m_gui->treeWidget->setCurrentItem( m_scripts[name].li );
    return slotRunScript( silent );
}


bool
ScriptManager::stopScript( const QString& name )
{
    if( !m_scripts.contains( name ) )
        return false;

    m_gui->treeWidget->setCurrentItem( m_scripts[name].li );
    slotStopScript();

    return true;
}


QStringList
ScriptManager::listRunningScripts()
{
    QStringList runningScripts;
    foreach( const QString &key, m_scripts.keys() )
        if( m_scripts[key].process )
            runningScripts << key;

    return runningScripts;
}


void
ScriptManager::customMenuClicked( const QString& message )
{
    notifyScripts( "customMenuClicked: " + message );
}


QString
ScriptManager::specForScript( const QString& name )
{
    if( !m_scripts.contains( name ) )
        return QString();
    QFileInfo info( m_scripts[name].url.path() );
    const QString specPath = info.path() + '/' + info.completeBaseName() + ".spec";

    return specPath;
}


void
ScriptManager::notifyFetchLyrics( const QString& artist, const QString& title )
{
    const QString args = QUrl::toPercentEncoding( artist ) + ' ' + QUrl::toPercentEncoding( title ); //krazy:exclude=qclasses
    notifyScripts( "fetchLyrics " + args );
}


void
ScriptManager::notifyFetchLyricsByUrl( const QString& url )
{
    notifyScripts( "fetchLyricsByUrl " + url );
}


void ScriptManager::notifyTranscode( const QString& srcUrl, const QString& filetype )
{
    notifyScripts( "transcode " + srcUrl + ' ' + filetype );
}


void
ScriptManager::requestNewScore( const QString &url, double prevscore, int playcount, int length, float percentage, const QString &reason )
{
    const QString script = ensureScoreScriptRunning();
    if( script.isNull() )
    {
        Amarok::ContextStatusBar::instance()->longMessage(
            i18n( "No score scripts were found, or none of them worked. Automatic scoring will be disabled. Sorry." ),
            KDE::StatusBar::Sorry );
        return;
    }

    m_scripts[script].process->writeStdin(
        QString( "requestNewScore %6 %1 %2 %3 %4 %5" )
        .arg( prevscore )
        .arg( playcount )
        .arg( length )
        .arg( percentage )
        .arg( reason )
        .arg( QString( QUrl::toPercentEncoding( url ) ) ) ); //last because it might have %s
}

////////////////////////////////////////////////////////////////////////////////
// private slots
////////////////////////////////////////////////////////////////////////////////

void
ScriptManager::findScripts() //SLOT
{
    const QStringList allFiles = KGlobal::dirs()->findAllResources( "data", "amarok/scripts/*",KStandardDirs::Recursive );

    // Add found scripts to treeWidget:
    foreach( const QString &str, allFiles )
        if( QFileInfo( str ).isExecutable() )
            loadScript( str );

    // Handle auto-run:

    KConfigGroup config = Amarok::config( "ScriptManager" );
    const QStringList runningScripts = config.readEntry( "Running Scripts", QStringList() );

    foreach( const QString &str, runningScripts )
        if( m_scripts.contains( str ) ) {
            m_gui->treeWidget->setCurrentItem( m_scripts[str].li );
            slotRunScript();
        }

//FIXME    m_gui->treeWidget->setCurrentItem( m_gui->treeWidget->firstChild() );
    slotCurrentChanged( m_gui->treeWidget->currentItem() );
}


void
ScriptManager::slotCurrentChanged( QTreeWidgetItem* item )
{
    const bool isCategory = item == m_generalCategory ||
                            item == m_lyricsCategory ||
                            item == m_scoreCategory ||
                            item == m_transcodeCategory ||
                            item == m_servicesCategory;

    if( item && !isCategory ) {
        const QString name = item->text( 0 );
        m_gui->uninstallButton->setEnabled( true );
        m_gui->runButton->setEnabled( !m_scripts[name].process );
        m_gui->stopButton->setEnabled( m_scripts[name].process );
        m_gui->configureButton->setEnabled( m_scripts[name].process );
        m_gui->aboutButton->setEnabled( true );
    }
    else {
        m_gui->uninstallButton->setEnabled( false );
        m_gui->runButton->setEnabled( false );
        m_gui->stopButton->setEnabled( false );
        m_gui->configureButton->setEnabled( false );
        m_gui->aboutButton->setEnabled( false );
    }
}


bool
ScriptManager::slotInstallScript( const QString& path )
{
    QString _path = path;

    if( path.isNull() ) {
        _path = KFileDialog::getOpenFileName( KUrl(),
            "*.amarokscript.tar *.amarokscript.tar.bz2 *.amarokscript.tar.gz|"
            + i18n( "Script Packages (*.amarokscript.tar, *.amarokscript.tar.bz2, *.amarokscript.tar.gz)" )
            , this );
        if( _path.isNull() ) return false;
    }

    KTar archive( _path );
    if( !archive.open( QIODevice::ReadOnly ) ) {
        KMessageBox::sorry( 0, i18n( "Could not read this package." ) );
        return false;
    }

    QString destination = Amarok::saveLocation( "scripts/" );
    const KArchiveDirectory* const archiveDir = archive.directory();

    // Prevent installing a script that's already installed
    const QString scriptFolder = destination + archiveDir->entries().first();
    if( QFile::exists( scriptFolder ) ) {
        KMessageBox::error( 0, i18n( "A script with the name '%1' is already installed. "
                                     "Please uninstall it first.", archiveDir->entries().first() ) );
        return false;
    }

    archiveDir->copyTo( destination );
    m_installSuccess = false;
    recurseInstall( archiveDir, destination );

    if( m_installSuccess ) {
        KMessageBox::information( 0, i18n( "Script successfully installed." ) );
        return true;
    }
    else {
        KMessageBox::sorry( 0, i18n( "<p>Script installation failed.</p>"
                                     "<p>The package did not contain an executable file. "
                                     "Please inform the package maintainer about this error.</p>" ) );

        // Delete directory recursively
        KIO::NetAccess::del( KUrl( scriptFolder ), 0 );
    }

    return false;
}


void
ScriptManager::recurseInstall( const KArchiveDirectory* archiveDir, const QString& destination )
{
    const QStringList entries = archiveDir->entries();

    foreach( const QString &entry, entries ) {
        const KArchiveEntry* const archEntry = archiveDir->entry( entry );

        if( archEntry->isDirectory() ) {
            const KArchiveDirectory* const dir = static_cast<const KArchiveDirectory*>( archEntry );
            recurseInstall( dir, destination + entry + '/' );
        }
        else {
            ::chmod( QFile::encodeName( destination + entry ), archEntry->permissions() );

            if( QFileInfo( destination + entry ).isExecutable() ) {
                loadScript( destination + entry );
                m_installSuccess = true;
            }
        }
    }
}


void
ScriptManager::slotRetrieveScript()
{
    KNS::Engine *engine = new KNS::Engine();
    engine->init( "amarok.knsrc" );
    KNS::Entry::List entries = engine->downloadDialogModal();

    qDeleteAll( entries );
    delete engine;
}


void
ScriptManager::slotUninstallScript()
{
    const QString name = m_gui->treeWidget->currentItem()->text( 0 );

    if( KMessageBox::warningContinueCancel( this, i18n( "Are you sure you want to uninstall the script '%1'?", name ), i18n("Uninstall Script"), KGuiItem( i18n("Uninstall") ) ) == KMessageBox::Cancel )
        return;

    if( m_scripts.find( name ) == m_scripts.end() )
        return;

    const QString directory = m_scripts[name].url.directory();

    // Delete directory recursively
    const KUrl url = KUrl( directory );
    if( !KIO::NetAccess::del( url, 0 ) ) {
        KMessageBox::sorry( 0, i18n( "<p>Could not uninstall this script.</p><p>The ScriptManager can only uninstall scripts which have been installed as packages.</p>" ) );
        return;
    }

    QStringList keys;

    // Find all scripts that were in the uninstalled folder
    foreach( const QString &key, m_scripts.keys() )
        if( m_scripts[key].url.directory() == directory )
            keys << key;

    // Terminate script processes, remove entries from script list
    foreach( const QString &key, keys ) {
        delete m_scripts[key].li;
        terminateProcess( &m_scripts[key].process );
        m_scripts.remove( key );
    }
}


bool
ScriptManager::slotRunScript( bool silent )
{
    if( !m_gui->runButton->isEnabled() ) return false;

    QTreeWidgetItem* const li = m_gui->treeWidget->currentItem();
    const QString name = li->text( 0 );

    if( m_scripts[name].type == "lyrics" && lyricsScriptRunning() != QString::null ) {
        if( !silent )
            KMessageBox::sorry( 0, i18n( "Another lyrics script is already running. "
                                         "You may only run one lyrics script at a time." ) );
        return false;
    }

    if( m_scripts[name].type == "transcode" && transcodeScriptRunning() != QString::null ) {
        if( !silent )
            KMessageBox::sorry( 0, i18n( "Another transcode script is already running. "
                                         "You may only run one transcode script at a time." ) );
        return false;
    }

    // Don't start a script twice
    if( m_scripts[name].process ) return false;

    AmarokProcIO* script = new AmarokProcIO();
    script->setOutputChannelMode( AmarokProcIO::SeparateChannels );
    const KUrl url = m_scripts[name].url;
    *script << url.path();
    script->setWorkingDirectory( Amarok::saveLocation( "scripts-data/" ) );

    connect( script, SIGNAL( receivedStderr( AmarokProcess* ) ), SLOT( slotReceivedStderr( AmarokProcess* ) ) );
    connect( script, SIGNAL( receivedStdout( AmarokProcess* ) ), SLOT( slotReceivedStdout( AmarokProcess* ) ) );
    connect( script, SIGNAL( processExited( AmarokProcess* ) ), SLOT( scriptFinished( AmarokProcess* ) ) );

    script->start( );
    if( script->error() != AmarokProcIO::FailedToStart )
    {
        if( m_scripts[name].type == "score" && !scoreScriptRunning().isNull() )
        {
            stopScript( scoreScriptRunning() );
            m_gui->treeWidget->setCurrentItem( li );
        }
        AmarokConfig::setLastScoreScript( name );
    }
    else
    {
        if( !silent )
            KMessageBox::sorry( 0, i18n( "<p>Could not start the script <i>%1</i>.</p>"
                                         "<p>Please make sure that the file has execute (+x) permissions.</p>", name ) );
        delete script;
        return false;
    }

    li->setIcon( 0, SmallIcon( "media-playback-start-amarok" ) );

    m_scripts[name].process = script;
    slotCurrentChanged( m_gui->treeWidget->currentItem() );
    if( m_scripts[name].type == "lyrics" )
        emit lyricsScriptChanged();
    else if( m_scripts[name].type == "service" )
        The::scriptableServiceManager()->addRunningScript( name, script );

    return true;
}


void
ScriptManager::slotStopScript()
{
    DEBUG_BLOCK

    QTreeWidgetItem* const li = m_gui->treeWidget->currentItem();
    const QString name = li->text( 0 );

    // Just a sanity check
    if( m_scripts.find( name ) == m_scripts.end() )
        return;

    terminateProcess( &m_scripts[name].process );
    m_scripts[name].log = QString::null;
    slotCurrentChanged( m_gui->treeWidget->currentItem() );

    li->setIcon( 0, QPixmap() );

    if( m_scripts.value( name ).type == "service" ) {
        The::scriptableServiceManager()->removeRunningScript( name );
    }
}


void
ScriptManager::slotConfigureScript()
{
    const QString name = m_gui->treeWidget->currentItem()->text( 0 );
    if( !m_scripts[name].process ) return;

    const KUrl url = m_scripts[name].url;
    QDir::setCurrent( url.directory() );

    m_scripts[name].process->writeStdin( QString("configure") );
}


void
ScriptManager::slotAboutScript()
{
    const QString name = m_gui->treeWidget->currentItem()->text( 0 );
    QFile readme( m_scripts[name].url.directory( KUrl::AppendTrailingSlash ) + "README" );
    QFile license( m_scripts[name].url.directory( KUrl::AppendTrailingSlash) + "COPYING" );

    if( !readme.open( QIODevice::ReadOnly ) ) {
        KMessageBox::sorry( 0, i18n( "There is no information available for this script." ) );
        return;
    }

    KAboutData aboutData( name.toLatin1(), 0, ki18n(name.toLatin1()), "1.0", ki18n(readme.readAll()) );

    KAboutApplicationDialog* about = new KAboutApplicationDialog( &aboutData, this );
    about->setButtons( KDialog::Ok );
    about->setDefaultButton( KDialog::Ok );

    kapp->setTopWidget( about );
    about->setCaption( KDialog::makeStandardCaption( i18n( "About %1", name ) ) );

    about->setInitialSize( QSize( 500, 350 ) );
    about->show();
}


void
ScriptManager::slotShowContextMenu( const QPoint& pos )
{
    QTreeWidgetItem* item = m_gui->treeWidget->itemAt( pos );

    const bool isCategory = item == m_generalCategory ||
                            item == m_lyricsCategory ||
                            item == m_scoreCategory ||
                            item == m_transcodeCategory ||
			                item == m_contextCategory ||
                            item == m_servicesCategory;

    if( !item || isCategory ) return;

    // Find the script entry in our map
    QString key;
    foreach( key, m_scripts.keys() )
        if( m_scripts[key].li == item ) break;

    enum { SHOW_LOG, EDIT };
    KMenu menu;
    menu.addTitle( i18n( "Debugging" ) );
    QAction* logAction = menu.addAction( KIcon( "view-history-amarok" ), i18n( "Show Output &Log" ) );
    QAction* editAction = menu.addAction( KIcon( "document-properties-amarok" ), i18n( "&Edit" ) );
    logAction->setData( SHOW_LOG );
    editAction->setData( EDIT );

    logAction->setEnabled( m_scripts[key].process != 0 );

    QAction* choice = menu.exec( mapToGlobal( pos ) );
    if( !choice ) return;
    const int id = choice->data().toInt();

    switch( id )
    {
        case EDIT:
            KRun::runCommand( "kwrite " + m_scripts[key].url.path(), 0 );
            break;

        case SHOW_LOG:
            QString line;
            while( m_scripts[key].process->readln( line ) != -1 )
                m_scripts[key].log += line;

            KTextEdit* editor = new KTextEdit( m_scripts[key].log );
            kapp->setTopWidget( editor );
            editor->setWindowTitle( KDialog::makeStandardCaption( i18n( "Output Log for %1", key ) ) );
            editor->setReadOnly( true );

            QFont font( "fixed" );
            font.setFixedPitch( true );
            font.setStyleHint( QFont::TypeWriter );
            editor->setFont( font );

            editor->resize( 500, 380 );
            editor->show();
            break;
    }
}


/* This is just a workaround, some scripts crash for some people if stdout is not handled. */
void
ScriptManager::slotReceivedStdout( AmarokProcess *process )
{
    debug() << QString::fromLatin1( process->readAllStandardOutput() );
}


void
ScriptManager::slotReceivedStderr( AmarokProcess* process )
{
    // Look up script entry in our map
    ScriptMap::Iterator it;
    ScriptMap::Iterator end( m_scripts.end() );
    for( it = m_scripts.begin(); it != end; ++it )
        if( it.value().process == process ) break;

    const QString text = QString::fromLatin1( process->readAllStandardError() );
    error() << it.key() << ":\n" << text;

    if( it.value().log.length() > 20000 )
        it.value().log = "==== LOG TRUNCATED HERE ====\n";
    it.value().log += text;
}


void
ScriptManager::scriptFinished( AmarokProcess* process ) //SLOT
{
    DEBUG_BLOCK

    // Look up script entry in our map
    ScriptMap::Iterator it;
    ScriptMap::Iterator end( m_scripts.end() );
    for( it = m_scripts.begin(); it != end; ++it )
        if( it.value().process == process ) break;

    // FIXME The logic in this check doesn't make sense
    // Check if there was an error on exit
    if( process->error() != AmarokProcess::Crashed && process->exitStatus() != 0 ) {
        KMessageBox::detailedError( 0, i18n( "The script '%1' exited with error code: %2", it.key(), process->exitStatus() ),it.value().log );
        warning() << "Script exited with error status: " << process->exitStatus();
    }

    // Destroy script process
    it.value().process->close();
    it.value().process->deleteLater();

    it.value().process = 0;
    it.value().log.clear();
    it.value().li->setIcon( 0, QPixmap() );
    slotCurrentChanged( m_gui->treeWidget->currentItem() );
}


////////////////////////////////////////////////////////////////////////////////
// private
////////////////////////////////////////////////////////////////////////////////

QStringList
ScriptManager::scriptsOfType( const QString &type ) const
{
    QStringList scripts;
    foreach( const QString &key, m_scripts.keys() )
        if( m_scripts[key].type == type )
            scripts += key;

    return scripts;
}


QString
ScriptManager::scriptRunningOfType( const QString &type ) const
{
    foreach( const QString &key, m_scripts.keys() )
        if( m_scripts[key].process && m_scripts[key].type == type )
            return key;

    return QString();
}


QString
ScriptManager::ensureScoreScriptRunning()
{
    QString s = scoreScriptRunning();
    if( !s.isNull() )
        return s;

    if( runScript( AmarokConfig::lastScoreScript(), true /*silent*/ ) )
        return AmarokConfig::lastScoreScript();

    const QString def = i18n( "Score" ) + ": " + "Default";
    if( runScript( def, true ) )
        return def;

    const QStringList scripts = scoreScripts();
    QStringList::ConstIterator end = scripts.constEnd();
    for( QStringList::ConstIterator it = scripts.constBegin(); it != end; ++it )
        if( runScript( *it, true ) )
            return *it;

    return QString();
}


void
ScriptManager::terminateProcess( AmarokProcIO** proc )
{
    if( *proc ) {
        (*proc)->kill(); // Sends SIGTERM

        delete *proc;
        *proc = 0;
    }
}


void
ScriptManager::notifyScripts( const QString& message )
{
    foreach( const ScriptItem &item, m_scripts ) {
        AmarokProcIO* const proc = item.process;
        if( proc ) proc->writeStdin( message );
    }
}


void
ScriptManager::loadScript( const QString& path )
{
    if( !path.isEmpty() ) {
        const KUrl url = KUrl( path );
        QString name = url.fileName();
        QString type = "generic";

        // Read and parse .spec file, if exists
        QFileInfo info( path );
        QTreeWidgetItem* li = 0;
        const QString specPath = info.path() + '/' + info.completeBaseName() + ".spec";
        if( QFile::exists( specPath ) ) {
            QSettings spec( specPath, QSettings::IniFormat );
            if( spec.contains( "name" ) )
                name = spec.value( "name" ).toString();
            if( spec.contains( "type" ) ) {
                type = spec.value( "type" ).toString();
                if( type == "lyrics" ) {
                    li = new QTreeWidgetItem( m_lyricsCategory );
                    li->setText( 0, name );
                }
                if( type == "transcode" ) {
                    li = new QTreeWidgetItem( m_transcodeCategory );
                    li->setText( 0, name );
                }
                if( type == "score" ) {
                    li = new QTreeWidgetItem( m_scoreCategory );
                    li->setText( 0, name );
                }
                if( type == "service" ) {
                    li = new QTreeWidgetItem( m_servicesCategory );
                    li->setText( 0, name );
                }
                if( type == "context" ) {
                    li = new QTreeWidgetItem( m_contextCategory );
                        li->setText( 0, name );
                }
            }
        }

        if( !li ) {
            li = new QTreeWidgetItem( m_generalCategory );
            li->setText( 0, name );
        }

        li->setIcon( 0, QPixmap() );

        ScriptItem item;
        item.url = url;
        item.type = type;
        item.process = 0;
        item.li = li;

        m_scripts[name] = item;

        slotCurrentChanged( m_gui->treeWidget->currentItem() );
    }
}


void
ScriptManager::engineStateChanged( Phonon::State state, Phonon::State /*oldState*/ )
{
    switch( state )
    {
        case Phonon::StoppedState:
        case Phonon::LoadingState:
            notifyScripts( "engineStateChange: stopped" );
            break;

        case Phonon::ErrorState:
            notifyScripts( "engineStateChange: error" );
            break;

        case Phonon::PausedState:
            notifyScripts( "engineStateChange: paused" );
            break;

        case Phonon::PlayingState:
            notifyScripts( "engineStateChange: playing" );
            break;

        case Phonon::BufferingState:
            return;
    }
}


void
ScriptManager::engineNewMetaData( const QHash< qint64, QString >& /*newMetaData*/, bool trackChanged )
{
    if( trackChanged)
    {
        notifyScripts( "trackChange" );
    }
}


void
ScriptManager::engineVolumeChanged( int newVolume )
{
    notifyScripts( "volumeChange: " + QString::number( newVolume ) );
}


#include "scriptmanager.moc"