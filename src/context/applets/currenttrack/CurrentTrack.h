/***************************************************************************
 * copyright            : (C) 2007 Leo Franchi <lfranchi@gmail.com>        *
 **************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CURRENT_TRACK_APPLET_H
#define CURRENT_TRACK_APPLET_H

#include <context/Applet.h>
#include <context/DataEngine.h>
#include <context/Svg.h>
#include <context/widgets/TextWidget.h>

#include <KDialog>

#include <QList>
#include <QAction>

class QGraphicsPixmapItem;
class QLabel;
class QHBoxLayout;
class QSpinBox;
class QCheckBox;

class CurrentTrack : public Context::Applet
{
    Q_OBJECT
public:
    CurrentTrack( QObject* parent, const QVariantList& args );
    ~CurrentTrack();

    void init();

    void paintInterface( QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect &contentsRect );

    void constraintsUpdated( Plasma::Constraints constraints = Plasma::AllConstraints);
    bool hasHeightForWidth() const;
    qreal heightForWidth( qreal width ) const;

public slots:
    void dataUpdated( const QString& name, const Plasma::DataEngine::Data &data );
    void showConfigurationInterface();

private slots:
    void configAccepted();

private:
    void createMenu();
    QList<QAction*> contextActions() { return m_contextActions; }
    QList<QAction*> m_contextActions;

    QHBoxLayout* m_configLayout;
    QSpinBox* m_spinWidth;
    int m_width;

    qreal m_aspectRatio;

    Context::Svg* m_theme;

    QGraphicsSimpleTextItem* m_titleLabel;
    QGraphicsSimpleTextItem* m_artistLabel;
    QGraphicsSimpleTextItem* m_albumLabel;
    QGraphicsPixmapItem* m_scoreLabel;
    QGraphicsPixmapItem* m_numPlayedLabel;
    QGraphicsPixmapItem* m_playedLastLabel;
    
    QGraphicsSimpleTextItem* m_title;
    QGraphicsSimpleTextItem* m_artist;
    QGraphicsSimpleTextItem* m_album;
    QGraphicsSimpleTextItem* m_score;
    QGraphicsSimpleTextItem* m_numPlayed;
    QGraphicsSimpleTextItem* m_playedLast;

    int m_rating;
    int m_trackLength;

    QGraphicsPixmapItem* m_albumCover;
    QGraphicsPixmapItem* m_sourceEmblem;
    QPixmap m_bigCover;
    QPixmap m_sourceEmblemPixmap;

    bool resizeCover(QPixmap cover);


    //keep this safe as we might need it when resizing
    QVariantMap m_currentInfo;

};

K_EXPORT_AMAROK_APPLET( currenttrack, CurrentTrack )

#endif