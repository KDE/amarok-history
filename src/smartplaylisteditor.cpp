// (c) Pierpaolo Di Panfilo 2004
// See COPYING file for licensing information

#include "collectiondb.h"
#include "metabundle.h"
#include "smartplaylisteditor.h"

#include <kcombobox.h>
#include <klineedit.h>
#include <klocale.h>
#include <knuminput.h>

#include <qcheckbox.h>
#include <qdatetime.h>
#include <qdatetimeedit.h>    //loadEditWidgets()
#include <qframe.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qobjectlist.h>
#include <qstringlist.h>
#include <qtoolbutton.h>
#include <qvbox.h>
#include <qvgroupbox.h>


QStringList m_fields;
QStringList m_dbFields;


SmartPlaylistEditor::SmartPlaylistEditor( QWidget *parent, QString defaultName, const char *name )
    : KDialogBase( parent, name, true, i18n("Create Smart Playlist"), Ok|Cancel)
{
    makeVBoxMainWidget();

    m_fields.clear();
    m_fields << "Artist" << "Album" << "Genre" << "Title" << "Track #" << "Year"
           << "Comment" << "Play Counter" << "Score" << "First Play" << "Last Play"
           << "Modified Date";

    m_dbFields.clear();
    m_dbFields << "artist.name" << "album.name" << "genre.name" << "tags.title"
               << "tags.track" << "year.name" << "tags.comment" << "statistics.playcounter"
               << "statistics.percentage" << "statistics.createdate" << "statistics.accessdate"
               << "tags.createdate";

    QHBox *hbox = new QHBox( mainWidget() );
    hbox->setSpacing( 5 );
    new QLabel( i18n("Playlist name:"), hbox );
    m_nameLineEdit = new KLineEdit( defaultName, hbox );

    QFrame *sep = new QFrame( mainWidget() );
    sep->setFrameStyle( QFrame::HLine | QFrame::Sunken );

    //match box
    QHBox *matchBox = new QHBox( mainWidget() );
    m_matchCheck = new QCheckBox( i18n("Match the following condition" ), matchBox );
    m_matchCheck->setChecked( true );
    m_matchCombo = new KComboBox( matchBox );
    m_matchCombo->insertItem( i18n("All") );
    m_matchCombo->insertItem( i18n("Any") );
    m_matchLabel = new QLabel( i18n(" of the following conditions"), matchBox );
    matchBox->setStretchFactor( new QWidget( matchBox ), 1 );

    //criteria box
    m_criteriaGroupBox = new QVGroupBox( QString::null, mainWidget() );
    addCriteria();

    //order box
    QHBox *hbox2 = new QHBox( mainWidget() );
    m_orderCheck = new QCheckBox( i18n("Order by"), hbox2 );
    QHBox *orderBox = new QHBox( hbox2 );
    orderBox->setSpacing( 5 );
    //fields combo
    m_orderCombo = new KComboBox( orderBox );
    for ( QStringList::ConstIterator it = m_fields.begin(); it != m_fields.end(); ++it )
        m_orderCombo->insertItem( i18n( (*it).latin1() ) );
    //order type
    m_orderTypeCombo = new KComboBox( orderBox );
    m_orderTypeCombo->insertItem( i18n("Ascending") );
    m_orderTypeCombo->insertItem( i18n("Descending") );
    hbox2->setStretchFactor( new QWidget( hbox2 ), 1 );

    //limit box
    QHBox *hbox1 = new QHBox( mainWidget() );
    m_limitCheck = new QCheckBox( i18n("Limit to"), hbox1 );
    QHBox *limitBox = new QHBox( hbox1 );
    limitBox->setSpacing( 5 );
    m_limitSpin = new KIntSpinBox( limitBox );
    m_limitSpin->setMinValue( 1 );
    m_limitSpin->setMaxValue( 1000 );
    m_limitSpin->setValue( 15 );
    new QLabel( i18n("tracks"), limitBox );
    hbox1->setStretchFactor( new QWidget( hbox1 ), 1 );

    //add stretch
    static_cast<QHBox *>(mainWidget())->setStretchFactor(new QWidget(mainWidget()), 1);

    connect( m_matchCheck, SIGNAL( toggled(bool) ), m_criteriaGroupBox, SLOT( setEnabled(bool) ) );
    connect( m_orderCheck, SIGNAL( toggled(bool) ), orderBox, SLOT( setEnabled(bool) ) );
    connect( m_limitCheck, SIGNAL( toggled(bool) ), limitBox, SLOT(  setEnabled(bool) ) );

    orderBox->setEnabled( false );
    limitBox->setEnabled( false );

    m_nameLineEdit->setFocus();

    resize( 550, 200 );
}


SmartPlaylistEditor::~SmartPlaylistEditor()
{
}


SmartPlaylistEditor::Result SmartPlaylistEditor::exec()
{
    Result r;
    r.result = DialogCode(KDialogBase::exec());
    if( r.result == Accepted ) {
        r.query = getQuery();
        r.playlistName = m_nameLineEdit->text();
    }
    return r;
}


void SmartPlaylistEditor::addCriteria()
{
    CriteriaEditor *criteria = new CriteriaEditor( this, m_criteriaGroupBox );
    m_criteriaEditorList.append( criteria );
    updateMatchWidgets();
    m_criteriaEditorList.first()->enableRemove( m_criteriaEditorList.count() > 1 );
}


void SmartPlaylistEditor::removeCriteria( CriteriaEditor *criteria )
{
    m_criteriaEditorList.remove( criteria );
    criteria->deleteLater();
    updateMatchWidgets();
    resize( size().width(), sizeHint().height() );

    if( m_criteriaEditorList.count() == 1 )
        m_criteriaEditorList.first()->enableRemove( false );
}


QString SmartPlaylistEditor::getQuery()
{
    QString tables = "tags";
    QString whereStr;
    QString orderStr;

    //where expression
    if( m_matchCheck->isChecked() ) {
        whereStr += " WHERE ";

        CriteriaEditor *criteria = m_criteriaEditorList.first();
        for( int i=0; criteria; criteria = m_criteriaEditorList.next(), i++ ) {

            QString str = criteria->getSearchCriteria();
            //add the table used in the search expression to tables
            QString table = str.left( str.find('.') );
            if( !tables.contains( table ) )
                tables += ", " + table;

            if( i ) { //multiple conditions
                QString op = m_matchCombo->currentItem() == 0 ? "AND" : "OR";
                str.prepend( " " + op + " (" ).append( ")" );
            }
            whereStr += str;
        }
    }

    //order by expression
    if( m_orderCheck->isChecked() ) {
        QString field = m_dbFields[ m_orderCombo->currentItem() ];
        QString table = field.left( field.find('.') );
        if( !tables.contains( table ) ) {
            whereStr += whereStr.isEmpty() ? " WHERE " : " AND ";
            if( table == "statistics" )
                whereStr += "tags.url = statistics.url";
            else if( table != "tags" )
                whereStr += "tags." + table + " = " + table + ".id";
            tables += ", " + table;
        }

        QString orderType = m_orderTypeCombo->currentItem() == 1 ? " DESC" : " ASC";
        orderStr = " ORDER BY " +  field + orderType;
    }

    QString query = "SELECT tags.url FROM " + tables + whereStr + orderStr;

    if( m_limitCheck->isChecked() )
        query += " LIMIT 0," + QString::number( m_limitSpin->value() );

    query += ";";

    return query;
}


void SmartPlaylistEditor::updateMatchWidgets()
{
    int count = m_criteriaEditorList.count();
    if( count > 1 ) {
        m_matchCheck->setText( i18n("Match") );
        m_matchCombo->show();
        m_matchLabel->show();
    }
    else {
        m_matchCheck->setText( i18n("Match the following condition" ) );
        m_matchCombo->hide();
        m_matchLabel->hide();
    }
}


/////////////////////////////////////////////////////////////////////////////
//    CLASS CriteriaEditor
////////////////////////////////////////////////////////////////////////////

CriteriaEditor::CriteriaEditor( SmartPlaylistEditor *editor, QWidget *parent )
    : QHBox( parent )
    , m_playlistEditor( editor )
    , m_currentValueType( -1 )
{
    setSpacing( 5 );

    m_fieldCombo = new KComboBox( this );
    for ( QStringList::ConstIterator it = m_fields.begin(); it != m_fields.end(); ++it )
        m_fieldCombo->insertItem( i18n( (*it).latin1() ) );

    m_criteriaCombo = new KComboBox( this );

    m_editBox = new QHBox( this );
    m_editBox->setSpacing( 5 );
    setStretchFactor( m_editBox, 1 );

    m_addButton = new QToolButton( this );
    m_addButton->setUsesTextLabel( true );
    m_addButton->setTextLabel("+");
    m_removeButton = new QToolButton( this );
    m_removeButton->setUsesTextLabel( true );
    m_removeButton->setTextLabel("-");

    connect( m_fieldCombo,    SIGNAL( activated(int) ), SLOT( slotFieldSelected(int) ) );
    connect( m_criteriaCombo, SIGNAL( activated(int) ), SLOT( loadEditWidgets() ) );
    connect( m_addButton, SIGNAL( clicked() ), editor, SLOT( addCriteria() ) );
    connect( m_removeButton, SIGNAL( clicked() ), SLOT( slotRemoveCriteria() ) );

    slotFieldSelected( 0 );
    show();
}


CriteriaEditor::~CriteriaEditor()
{
}


QString CriteriaEditor::getSearchCriteria()
{
    QString searchCriteria;
    QString field = m_dbFields[ m_fieldCombo->currentItem() ];
    QString criteria = m_criteriaCombo->currentText();

    if( field.isEmpty() )
        return QString::null;

    QString table = field.left( field.find('.') );
    if( table == "statistics" )
        searchCriteria += "statistics.url = tags.url AND ";
    else if( table != "tags" )
        searchCriteria += table + ".id = tags." + table + " AND ";

    searchCriteria += field;

    QString value;
    switch( getValueType( m_fieldCombo->currentItem() ) ) {
        case String:
        case AutoCompletionString:
            value = m_lineEdit->text();
            break;
        case Year:    //fall through
        case Number:
            value = QString::number( m_intSpinBox1->value() );
            if( criteria == i18n("is between")  )
                value += " AND " + QString::number( m_intSpinBox2->value() );
            break;
        case Date:
        {
            if( criteria == i18n("is in the last") ) {
                int n = m_intSpinBox1->value();
                QDateTime time;
                if( m_dateCombo->currentItem() == 0 ) //days
                    time.addDays( n );
                else if( m_dateCombo->currentItem() == 1 ) //months
                    time.addMonths( n );
                else time.addYears( n ); //years

                value += QString("strftime('now') - %1 AND strftime('now')").arg( time.toTime_t() );
            }
            else {
                QDateTime datetime1( m_dateEdit1->date() );
                value += QString::number( datetime1.toTime_t() );
                if( criteria == i18n("is between")  ) {
                    QDateTime datetime2( m_dateEdit2->date() );
                    value += " AND " + QString::number( datetime2.toTime_t() );
                }
                else
                    value += " AND " + QString::number( datetime1.addDays( 1 ).toTime_t() );
            }
            break;
        }
        default: ;
    };


    if( criteria == i18n("contains") )
        searchCriteria += " LIKE \"%" + value + "%\"";
    else if( criteria == i18n("does not contain") )
        searchCriteria += " NOT LIKE \"%" + value + "%\"";
    else if( criteria == i18n("is") ) {
        if( m_currentValueType == Date )
            searchCriteria += " BETWEEN ";
        else
            searchCriteria += " = ";
        if( m_currentValueType == String || m_currentValueType == AutoCompletionString )
            value.prepend("\"").append("\"");
        searchCriteria += value;
    }
    else if( criteria == i18n("is not") ) {
        if( m_currentValueType == Date )
            searchCriteria += " NOT BETWEEN ";
        else
            searchCriteria += " <> ";
        if( m_currentValueType == String || m_currentValueType == AutoCompletionString )
            value.prepend("\"").append("\"");
        searchCriteria += value;
    }
    else if( criteria == i18n("starts with") )
        searchCriteria += " LIKE \"%" + value + "\"";
    else if( criteria == i18n("ends with") )
        searchCriteria += " LIKE \"%" + value + "\"";
    else if( criteria == i18n("is greater than") || criteria == i18n("is after") )
        searchCriteria += " > " + value;
    else if( criteria == i18n("is smaller than") || criteria == i18n("is before" ) )
        searchCriteria += " < " + value;
    else if( criteria == i18n("is between") || criteria == i18n("is in the last") )
        searchCriteria += " BETWEEN " + value;


    return searchCriteria;
}


void CriteriaEditor::setSearchCriteria( const QString & )
{
    //TODO
}


void CriteriaEditor::enableRemove( bool enable )
{
    m_removeButton->setEnabled( enable );
}


void CriteriaEditor::slotRemoveCriteria()
{
    m_playlistEditor->removeCriteria( this );
}


void CriteriaEditor::slotFieldSelected( int field )
{
    int valueType = getValueType( field );
    loadCriteriaList( valueType );
    loadEditWidgets();
    m_currentValueType = valueType;

    //enable auto-completion for artist, album and genre
    if( valueType == AutoCompletionString ) { //Artist, Album, Genre
        QStringList items;
        m_comboBox->clear();
        m_comboBox->completionObject()->clear();

        int currentField = m_fieldCombo->currentItem();
        if( currentField == 0 ) //artist
           items = CollectionDB().artistList();
        else if( currentField == 1 ) //album
           items = CollectionDB().albumList();
        else  //genre
           items = MetaBundle::genreList();

        m_comboBox->insertStringList( items );
        m_comboBox->completionObject()->insertItems( items );
        m_comboBox->completionObject()->setIgnoreCase( true );
        m_comboBox->setCurrentText( "" );
        m_comboBox->setFocus();
    }
}


void CriteriaEditor::loadEditWidgets()
{
    int valueType = getValueType( m_fieldCombo->currentItem() );

    if( m_currentValueType == valueType && !(
        m_criteriaCombo->currentText() == i18n("is between") ||
        m_criteriaCombo->currentText() == i18n("is in the last" ) ) )
        return;

    QObjectList* list = m_editBox->queryList( "QWidget" );
    for( QObject *obj = list->first(); obj; obj = list->next()  )
        ((QWidget*)obj)->deleteLater();

    delete list;

    switch( valueType ) {

        case String:
        {
            m_lineEdit = new KLineEdit( m_editBox );
            m_lineEdit->setFocus();
            m_lineEdit->show();
            break;
        }

        case AutoCompletionString:    //artist, album, genre
        {
            m_comboBox = new KComboBox( true, m_editBox );
            m_lineEdit = (KLineEdit*)m_comboBox->lineEdit();
            m_lineEdit->setFocus();
            m_comboBox->show();
            break;
        }

        case Year:    //fall through
        case Number:
        {
            bool yearField = m_fieldCombo->currentText() == i18n("Year");

            m_intSpinBox1 = new KIntSpinBox( m_editBox );
            int maxValue = 1000;
            if( yearField ) {
                maxValue = QDate::currentDate().year();
                m_intSpinBox1->setValue( maxValue );
            }
            m_intSpinBox1->setMaxValue( maxValue );
            m_intSpinBox1->setFocus();
            m_intSpinBox1->show();

            if( m_criteriaCombo->currentText() == i18n("is between") ) {
                m_rangeLabel = new QLabel( i18n("and"), m_editBox );
                m_rangeLabel->setAlignment( AlignCenter );
                m_rangeLabel->show();
                m_intSpinBox2 = new KIntSpinBox( m_editBox );
                if( yearField ) {
                    maxValue = QDate::currentDate().year();
                    m_intSpinBox2->setValue( maxValue );
                }
                m_intSpinBox2->setMaxValue( maxValue );
                m_intSpinBox2->show();
            }
            break;
        }

        case Date:
        {
            if( m_criteriaCombo->currentText() == i18n("is in the last") ) {
                m_intSpinBox1 = new KIntSpinBox( m_editBox );
                m_intSpinBox1->setMinValue( 1 );
                m_intSpinBox1->show();
                m_dateCombo = new KComboBox( m_editBox );
                m_dateCombo->insertItem( i18n("days") );
                m_dateCombo->insertItem( i18n("months") );
                m_dateCombo->insertItem( i18n("years") );
                m_dateCombo->show();
            }
            else {
                m_dateEdit1 = new QDateEdit( QDate::currentDate(), m_editBox);
                m_dateEdit1->setFocus();
                m_dateEdit1->show();
                if( m_criteriaCombo->currentText() == i18n("is between") ) {
                    m_rangeLabel = new QLabel( i18n("and"), m_editBox );
                    m_rangeLabel->setAlignment( AlignCenter );
                    m_rangeLabel->show();
                    m_dateEdit2 = new QDateEdit( QDate::currentDate(), m_editBox);
                    m_dateEdit2->show();
                }
            }

            break;
        }

        default: ;
    };

}


void CriteriaEditor::loadCriteriaList( int valueType )
{
    if( m_currentValueType == valueType )
        return;

    QStringList items;

    switch( valueType ) {
        case String:
        case AutoCompletionString:
            items << "contains" << "does not contain" << "is" << "is not"
                  << "starts with" << "ends with";
            break;

        case Number:
            items << "is" << "is not" << "is greater than" << "is smaller than"
                  << "is between";
            break;

        case Year: //fall through
        case Date:
            items << "is" << "is not" << "is before" << "is after"
                  << "is in the last" << "is between";
            break;
        default: ;
    };

    m_criteriaCombo->clear();
    for ( QStringList::ConstIterator it = items.begin(); it != items.end(); ++it )
        m_criteriaCombo->insertItem( i18n( (*it).latin1() ) );
}


int CriteriaEditor::getValueType( int index )
{
    int valueType;

    switch( index ) {
        case 0:
        case 1:
        case 2:
            valueType = AutoCompletionString;
            break;
        case 3:
        case 6:
            valueType = String;
            break;
        case 4:
        case 7:
        case 8:
            valueType = Number;
            break;
        case 5:
            valueType = Year;
            break;
        default: valueType = Date;
    };

    return valueType;
}


#include "smartplaylisteditor.moc"
