/***************************************************************************
 * copyright        : (C) 2007-2008 Ian Monroe <ian@monroe.nu>
 *                    (C) 2007 Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>
 *                    (C) 2008 Seb Ruiz <ruiz@kde.org>
 *                    (C) 2008 Soren Harward <stharward@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **************************************************************************/

#include "PlaylistController.h"

#define DEBUG_PREFIX "Playlist::Controller"

#include "Debug.h"
#include "DirectoryLoader.h"
#include "EngineController.h"
#include "collection/QueryMaker.h"
#include "playlist/PlaylistActions.h"
#include "playlist/PlaylistModel.h"
#include "playlistmanager/PlaylistManager.h"

#include <KActionCollection>
#include <KIcon>

#include <QAction>

Playlist::Controller* Playlist::Controller::s_instance = 0;

Playlist::Controller* Playlist::Controller::instance() {
    return (s_instance) ? s_instance : new Controller();
}

void
Playlist::Controller::destroy() {
    if (s_instance) {
        delete s_instance;
        s_instance = 0;
    }
}

Playlist::Controller::Controller(QObject* parent)
        : QObject(parent)
        , m_model(Model::instance())
        , m_undoStack(new QUndoStack(this))
{
    s_instance = this;

    m_undoStack->setUndoLimit(20);
    connect(m_undoStack, SIGNAL(canRedoChanged(bool)), this, SIGNAL(canRedoChanged(bool)));
    connect(m_undoStack, SIGNAL(canUndoChanged(bool)), this, SIGNAL(canUndoChanged(bool)));
}

Playlist::Controller::~Controller() {}

void
Playlist::Controller::insertOptioned( Meta::TrackPtr track, int options ) {
    DEBUG_BLOCK
    if (track == Meta::TrackPtr())
        return;

    Meta::TrackList list;
    list.append(track);
    insertOptioned(list, options);
}

void
Playlist::Controller::insertOptioned( Meta::TrackList list, int options ) {
    DEBUG_BLOCK
    if( list.isEmpty() )
        return;

    if (options & Unique) {
        QMutableListIterator<Meta::TrackPtr> i(list);
        while (i.hasNext()) {
            i.next();
            if (m_model->containsTrack(i.value()))
                i.remove();
        }
    }

    int firstItemAdded = -1;
    if (options & Replace) {
        m_undoStack->beginMacro( "Replace playlist" ); // TODO: does this need to be internationalized?
        clear();
        insertionHelper(-1, list);
        m_undoStack->endMacro();
        firstItemAdded = 0;
    } else if( options & Queue ) {
        firstItemAdded = m_model->activeRow() + 1;
        insertionHelper(firstItemAdded, list);
    } else {
        firstItemAdded = m_model->rowCount();
        insertionHelper(firstItemAdded, list);
    }

    const Phonon::State engineState = The::engineController()->state();
    debug() << "engine state: " << engineState;

    if( options & DirectPlay ) {
        Actions::instance()->play(firstItemAdded);
    } else if ((options & StartPlay) && (m_model->rowCount() > 0)
               && ((engineState == Phonon::StoppedState) || (engineState == Phonon::LoadingState)))
        Actions::instance()->play(firstItemAdded);
}

void
Playlist::Controller::insertOptioned( Meta::PlaylistPtr playlist, int options ) {
    DEBUG_BLOCK
    if (!playlist)
        return;

    insertOptioned( playlist->tracks(), options );
}

void
Playlist::Controller::insertOptioned( Meta::PlaylistList list, int options ) {
    DEBUG_BLOCK
    if (list.isEmpty())
        return;

    foreach( Meta::PlaylistPtr playlist, list ) {
        insertOptioned( playlist, options );
    }
}

void
Playlist::Controller::insertOptioned(QueryMaker *qm, int options) {
    DEBUG_BLOCK
    qm->setQueryType( QueryMaker::Track );
    connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ) );
    connect( qm, SIGNAL( newResultReady( QString, Meta::TrackList ) ), SLOT( newResultReady( QString, Meta::TrackList ) ) );
    m_optionedQueryMap.insert( qm, options );
    qm->run();
}
 
void
Playlist::Controller::insertOptioned(QList<KUrl>& urls, int options) {
    DirectoryLoader* dl = new DirectoryLoader(); //dl handles memory management
    dl->setProperty("options", QVariant( options ) );
    connect( dl, SIGNAL( finished( const Meta::TrackList& ) ), this, SLOT( slotFinishDirectoryLoader( const Meta::TrackList& ) ) );

    dl->init( urls );
}
 
void
Playlist::Controller::insertTrack( int row, Meta::TrackPtr track ) {
    DEBUG_BLOCK
    if (track == Meta::TrackPtr())
        return;

    Meta::TrackList tl;
    tl.append(track);
    insertionHelper(row, tl);
}

void
Playlist::Controller::insertTracks( int row, Meta::TrackList tl ) {
    DEBUG_BLOCK

    // expand any tracks that are actually playlists
    QMutableListIterator<Meta::TrackPtr> i(tl);
    while (i.hasNext()) {
        i.next();
        Meta::TrackPtr track = i.value();
        if (The::playlistManager()->canExpand(track)) {
            i.remove();
            Meta::TrackList newtracks = The::playlistManager()->expand(track)->tracks();
            foreach (Meta::TrackPtr t, newtracks) {
                i.insert(t);
            }
        }
    }

    insertionHelper(row, tl);
}

void
Playlist::Controller::insertPlaylist( int row, Meta::PlaylistPtr playlist ) {
    DEBUG_BLOCK
    Meta::TrackList tl(playlist->tracks());
    insertionHelper(row, tl);
}

void
Playlist::Controller::insertPlaylists( int row, Meta::PlaylistList playlists ) {
    DEBUG_BLOCK
    Meta::TrackList tl;
    foreach( Meta::PlaylistPtr playlist, playlists ) {
        tl += playlist->tracks();
    }
    insertionHelper(row, tl);
}

void
Playlist::Controller::insertTracks( int row, QueryMaker *qm ) {
    DEBUG_BLOCK
    qm->setQueryType( QueryMaker::Track );
    connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ) );
    connect( qm, SIGNAL( newResultReady( QString, Meta::TrackList ) ), SLOT( newResultReady( QString, Meta::TrackList ) ) );
    m_queryMap.insert( qm, row );
    qm->run();
}

void
Playlist::Controller::insertUrls(int row, const QList<KUrl>& urls) {
    // FIXME: figure out some way to have this insert at the appropriate row, rather than always at end
    const int options = Append | DirectPlay;
    DirectoryLoader* dl = new DirectoryLoader(); //dl handles memory management
    dl->setProperty("options", QVariant( options ) );
    connect( dl, SIGNAL( finished( const Meta::TrackList& ) ), this, SLOT( slotFinishDirectoryLoader( const Meta::TrackList& ) ) );

    dl->init( urls );
}

void
Playlist::Controller::removeRow(int row) {
    DEBUG_BLOCK
    QList<int> rl;
    rl.append(row);
    removeRows(rl);
}

void
Playlist::Controller::removeRows(int row, int count) {
    DEBUG_BLOCK
    QList<int> rl;
    for (int i=0; i<count; i++) {
        rl.append(row++);
    }
    removeRows(rl);
}

void
Playlist::Controller::removeRows(QList<int>& rows) {
    DEBUG_BLOCK
    RemoveCmdList cmds;
    foreach (int r, rows) {
        if ((r >= 0) && (r < m_model->rowCount()))
            cmds.append(RemoveCmd(m_model->trackAt(r), r));
        else
            warning() << "received command to remove non-existent row" << r;
    }
    m_undoStack->push(new RemoveTracksCmd(0, cmds));
}

void
Playlist::Controller::moveRow(int from, int to) {
    DEBUG_BLOCK
    if (from == to)
        return;

    QList<int> source;
    QList<int> target;
    source.append(from);
    source.append(to);

    // shift all the rows between
    if (from < to) {
        for (int i=from+1; i<=to; i++) {
            source.append(i);
            target.append(i-1);
        }
    } else {
        for (int i=from-1; i>=to; i--) {
            source.append(i);
            target.append(i+1);
        }
    }

    moveRows(source, target);
}

void
Playlist::Controller::moveRows(QList<int>& from, int to) {
    DEBUG_BLOCK
    if (from.size() <= 0)
        return;

    int min = to;
    int max = to;
    foreach (int i, from) {
        min = qMin(min, i);
        max = qMax(max, i);
    }

    qSort(from.begin(), from.end());

    foreach (int f, from) {
        if (f < to)
            to--;
    }

    QList<int> sourceSlots;
    QList<int> targetSlots;
    for (int i=min; i <= max; i++) {
        sourceSlots.append(i);
        targetSlots.append(i);
    }

    QList<int> sourceMoves;
    QList<int> targetMoves;
    foreach (int r, from) {
        if (!(sourceSlots.removeOne(r) && targetSlots.removeOne(to)))
            return;
        sourceMoves.append(r);
        targetMoves.append(to++);
    }

    sourceMoves += sourceSlots;
    targetMoves += targetSlots;

    moveRows(sourceMoves, targetMoves);
}

void
Playlist::Controller::moveRows(QList<int>& from, QList<int>& to) {
    DEBUG_BLOCK
    if (from.size() != to.size())
        return;

    // validity check
    foreach (int i, from) {
        if ((from.count(i) != 1) || (to.count(i) != 1)) {
            error() << "row move lists malformed:";
            error() << from;
            error() << to;
            return;
        }
    }

    MoveCmdList cmds;
    for (int i=0; i<from.size(); i++) {
        debug() << "moving rows:" << from.at(i) << to.at(i);
        if ((from.at(i) >=0) && (from.at(i) < m_model->rowCount()))
            cmds.append(MoveCmd(from.at(i), to.at(i)));
    }
    m_undoStack->push(new MoveTracksCmd(0, cmds));
}

void
Playlist::Controller::undo() {
    m_undoStack->undo();
}

void
Playlist::Controller::redo() {
    m_undoStack->redo();
}

void
Playlist::Controller::clear() {
    DEBUG_BLOCK
    removeRows(0, Model::instance()->rowCount());
}

/**************************************************
 * Private Functions
 **************************************************/

void
Playlist::Controller::newResultReady( const QString&, const Meta::TrackList& tracks ) {
    QueryMaker *qm = dynamic_cast<QueryMaker*>(sender());
    if (qm) {
        m_queryMakerTrackResults[qm] += tracks;
    }
}

void
Playlist::Controller::queryDone() {
    QueryMaker *qm = dynamic_cast<QueryMaker*>(sender());
    if (qm) {
        qStableSort(m_queryMakerTrackResults[qm].begin(), m_queryMakerTrackResults[qm].end(), Meta::Track::lessThan);
        if (m_queryMap.contains(qm)) {
            insertTracks(m_queryMap.value(qm), m_queryMakerTrackResults.value(qm));
            m_queryMap.remove(qm);
        } else if (m_optionedQueryMap.contains(qm)) {
            insertOptioned(m_queryMakerTrackResults.value(qm), m_optionedQueryMap.value(qm));
            m_optionedQueryMap.remove(qm);
        }
        m_queryMakerTrackResults.remove(qm);
        qm->deleteLater();
    }
}

void
Playlist::Controller::slotFinishDirectoryLoader( const Meta::TrackList& tracks ) {
    DEBUG_BLOCK
    if (!tracks.isEmpty()) {
        insertOptioned( tracks, sender()->property( "options" ).toInt() );
    }
}

Meta::TrackList
Playlist::Controller::filterEmpties(Meta::TrackList& list) const {
    QMutableListIterator<Meta::TrackPtr> i(list);
    while (i.hasNext()) {
        i.next();
        if (i.value() == Meta::TrackPtr())
            i.remove();
    }
    return list;
}

void
Playlist::Controller::insertionHelper(int row, Meta::TrackList& tracks) {
    InsertCmdList cmds;
    if (row < 0 || row > Model::instance()->rowCount()) {
        row = Model::instance()->rowCount();
    }
    foreach (Meta::TrackPtr t, filterEmpties(tracks)) {
        cmds.append(InsertCmd(t, row++));
    }
    m_undoStack->push(new InsertTracksCmd(0, cmds));
}

namespace The {
    AMAROK_EXPORT Playlist::Controller* playlistController() { return Playlist::Controller::instance(); }
}