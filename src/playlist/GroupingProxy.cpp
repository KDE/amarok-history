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

#define DEBUG_PREFIX "Playlist::GroupingProxy"

#include "Debug.h"
#include "GroupingProxy.h"
#include "meta/Meta.h"
#include "playlist/PlaylistModel.h"

#include <QAbstractProxyModel>
#include <QVariant>

Playlist::GroupingProxy* Playlist::GroupingProxy::s_instance = 0;

Playlist::GroupingProxy* Playlist::GroupingProxy::instance() {
    return (s_instance) ? s_instance : new GroupingProxy();
}

void
Playlist::GroupingProxy::destroy() {
    if (s_instance) {
        s_instance->deleteLater();
        s_instance = 0;
    }
}

Playlist::GroupingProxy::GroupingProxy() : QAbstractProxyModel(0) , m_model(Model::instance()) {
    DEBUG_BLOCK

    setSourceModel(m_model);
    // signal proxies
    connect(m_model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(modelDataChanged(const QModelIndex&, const QModelIndex&)));
    connect(m_model, SIGNAL(rowsInserted(const QModelIndex&, int, int)), this, SLOT(modelRowsInserted(const QModelIndex &, int, int)));
    connect(m_model, SIGNAL(rowsRemoved(const QModelIndex&, int, int)), this, SLOT(modelRowsRemoved(const QModelIndex&, int, int)));
    connect(m_model, SIGNAL(layoutChanged()), this, SLOT(regroupAll()));
    connect(m_model, SIGNAL(modelReset()), this, SLOT(regroupAll()));

    int max = m_model->rowCount();
    for (int i=0; i<max; i++) {
        m_rowGroupMode.append(None);
    }
    regroupRows(0,max-1);

    s_instance = this;
}

Playlist::GroupingProxy::~GroupingProxy() {
    DEBUG_BLOCK
}

QModelIndex
Playlist::GroupingProxy::index(int r, int c, const QModelIndex&) const {
    if (m_model->rowExists(r)) {
        return createIndex(r,c);
    } else {
        return QModelIndex();
    }
}

QModelIndex
Playlist::GroupingProxy::parent(const QModelIndex& i) const {
    return sourceModel()->parent(i);
}

int
Playlist::GroupingProxy::rowCount(const QModelIndex& i) const {
    return sourceModel()->rowCount(i);
}

int
Playlist::GroupingProxy::columnCount(const QModelIndex& i) const {
    return sourceModel()->columnCount(i);
}

QModelIndex
Playlist::GroupingProxy::mapToSource(const QModelIndex& i) const {
    return createIndex(i.row(), i.column());
}

QModelIndex
Playlist::GroupingProxy::mapFromSource(const QModelIndex& i) const {
    return m_model->index(i.row(), i.column());
}

QVariant
Playlist::GroupingProxy::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return QVariant();

    int row = index.row();

    if ( role == Playlist::GroupRole ) {
        return m_rowGroupMode.at(row);
    } else if ( role == Playlist::GroupedTracksRole ) {
        return groupRowCount(row);
    } else if ( role == Playlist::GroupedAlternateRole ) {
        return ( row % 2 == 1 );
    } else {
        return m_model->data(index, role);
    }
}

void
Playlist::GroupingProxy::setActiveRow(int row) const {
    DEBUG_BLOCK
    m_model->setActiveRow(row);
}

Meta::TrackPtr
Playlist::GroupingProxy::trackAt(int row) const {
    return m_model->trackAt(row);
}

Qt::DropActions
Playlist::GroupingProxy::supportedDropActions() const {
    return m_model->supportedDropActions();
}

Qt::ItemFlags
Playlist::GroupingProxy::flags(const QModelIndex &index) const {
    return m_model->flags(index);
}

QStringList
Playlist::GroupingProxy::mimeTypes() const {
    return m_model->mimeTypes();
}

QMimeData*
Playlist::GroupingProxy::mimeData(const QModelIndexList& indexes) const {
    return m_model->mimeData(indexes);
}

bool
Playlist::GroupingProxy::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) {
    return m_model->dropMimeData(data, action, row, column, parent);
}

void
Playlist::GroupingProxy::setCollapsed(int, bool) const {
    AMAROK_DEPRECATED
}

int
Playlist::GroupingProxy::firstInGroup(int row) const {
    AMAROK_DEPRECATED
    if (m_rowGroupMode.at(row) == None)
        return row;

    while (row >= 0) {
        if (m_rowGroupMode.at(row) == Head)
            return row;
        row--;
    }
    warning() << "No group head found for row" << row;
    return row;
}

int
Playlist::GroupingProxy::lastInGroup(int row) const {
    AMAROK_DEPRECATED
    if (m_rowGroupMode.at(row) == None)
        return row;

    while (row < rowCount()) {
        if (m_rowGroupMode.at(row) == Tail)
            return row;
        row++;
    }
    warning() << "No group tail found for row" << row;
    return row;
}

void
Playlist::GroupingProxy::modelDataChanged(const QModelIndex& start, const QModelIndex& end) {
    regroupRows(start.row(), end.row());
}

void
Playlist::GroupingProxy::modelRowsInserted(const QModelIndex& idx, int start, int end) {
    for (int i=start; i<=end; i++) {
        m_rowGroupMode.insert(i, None);
    }
    emit rowsInserted(mapToSource(idx), start, end);
}

void
Playlist::GroupingProxy::modelRowsRemoved(const QModelIndex& idx, int start, int end) {
    for (int i=start; i<=end; i++) {
        m_rowGroupMode.removeAt(start);
    }
    emit rowsRemoved(mapToSource(idx), start, end);
}

void
Playlist::GroupingProxy::regroupAll() {
    regroupRows(0, rowCount() - 1);
}

void
Playlist::GroupingProxy::regroupRows(int first, int last) {

    /* This function maps row numbers to one of the GroupMode enums, according
     * to the following truth matrix:
     *
     *                  Matches Preceding Row
     *
     *                     true      false
     *   Matches      true Body      Head
     * Following
     *       Row     false Tail      None
     *
     * Non-existent albums are non-matches
     */

    first = (first > 0) ? (first - 1) : first;
    last = (last < (m_model->rowCount() - 1)) ? (last + 1) : last;

    for (int row=first; row <= last; row++) {
        Meta::TrackPtr thisTrack = m_model->trackAt(row);

        if ((thisTrack == Meta::TrackPtr()) || (thisTrack->album() == Meta::AlbumPtr())) {
            m_rowGroupMode[row] = None;
            continue;
        }

        int beforeRow = row-1;
        bool matchBefore = false;
        Meta::TrackPtr beforeTrack = m_model->trackAt(beforeRow);
        if (beforeTrack != Meta::TrackPtr())
            matchBefore = (beforeTrack->album() == thisTrack->album());

        int afterRow = row+1;
        bool matchAfter = false;
        Meta::TrackPtr afterTrack = m_model->trackAt(afterRow);
        if (afterTrack != Meta::TrackPtr())
            matchAfter = (afterTrack->album() == thisTrack->album());

        if (matchBefore && matchAfter)
            m_rowGroupMode[row] = Body;
        else if (!matchBefore && matchAfter)
            m_rowGroupMode[row] = Head;
        else if (matchBefore && !matchAfter)
            m_rowGroupMode[row] = Tail;
        else
            m_rowGroupMode[row] = None;
    }

    emit layoutChanged();
}

int 
Playlist::GroupingProxy::groupRowCount(int row) const {
    AMAROK_DEPRECATED
    return lastInGroup(row) - firstInGroup(row) + 1;
}