// (c) Pierpaolo Di Panfilo 2004
// See COPYING file for licensing information

#ifndef SMARTPLAYLISTEDITOR_H
#define SMARTPLAYLISTEDITOR_H

#include <kdialogbase.h>
#include <qhbox.h>
#include <qmap.h>
#include <qptrlist.h>

class KComboBox;
class KIntSpinBox;
class KLineEdit;
class QCheckBox;
class QDateEdit;
class QLabel;
class QToolButton;
class QVGroupBox;


class SmartPlaylistEditor : public KDialogBase
{
Q_OBJECT
    friend class CriteriaEditor;

    public:
        struct Result
        {
            DialogCode result;
            QString query;
            QString playlistName;
        };

        SmartPlaylistEditor( QWidget *parent, QString name = QString::null, const char *name=0 );
        ~SmartPlaylistEditor();

    public slots:
        Result exec();
        void addCriteria();
        void removeCriteria( CriteriaEditor *criteria );

    private:
        QString getQuery();
        void updateMatchWidgets();

        KLineEdit *m_nameLineEdit;

        QCheckBox *m_matchCheck;
        KComboBox *m_matchCombo;
        QLabel *m_matchLabel;

        QVGroupBox *m_criteriaGroupBox;
        //limit widgets
        QCheckBox *m_limitCheck;
        KIntSpinBox *m_limitSpin;
        //order by widgets
        QCheckBox *m_orderCheck;
        KComboBox *m_orderCombo;
        KComboBox *m_orderTypeCombo;

        QPtrList<CriteriaEditor> m_criteriaEditorList;
};



class CriteriaEditor : public QHBox
{
Q_OBJECT
    public:
        CriteriaEditor( SmartPlaylistEditor *editor, QWidget *parent );
        ~CriteriaEditor();
        QString getSearchCriteria();
        void setSearchCriteria( const QString &str );
        void enableRemove( bool );

    private slots:
        void slotRemoveCriteria();
        void slotFieldSelected( int );
        void loadEditWidgets();

    private:
        enum ValueType { String, AutoCompletionString, Number, Year, Date };

        void loadCriteriaList( int valueType );
        int getValueType( int fieldIndex );

        SmartPlaylistEditor *m_playlistEditor;
        int m_currentValueType;

        KComboBox *m_fieldCombo;
        KComboBox *m_criteriaCombo;
        QToolButton *m_addButton;
        QToolButton *m_removeButton;

        //editing widgets
        QHBox *m_editBox;
        KLineEdit *m_lineEdit;
        KComboBox *m_comboBox;
        KIntSpinBox *m_intSpinBox1;
        KIntSpinBox *m_intSpinBox2;
        QDateEdit *m_dateEdit1;
        QDateEdit *m_dateEdit2;
        KComboBox *m_dateCombo;
        QLabel *m_rangeLabel;
};

#endif
