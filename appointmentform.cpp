
#include "appointmentform.h"
#include "ui_appointmentform.h"
#include "../Lib/supportfunctions.h"

#include <QDateTime>
#include <QDate>
#include <QTime>

#include <stdio.h>

AppointmentForm::AppointmentForm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AppointmentForm)
{
    mutex=false ;
    database=NULL ;
    cal=NULL;
    appt.createNew() ;

    ui->setupUi(this);
    ui->contactName->setFocus() ;
    ui->contactName->setReadOnly(true) ;
}

// Set appointment time equal to reference time if updating
// Set appointment time following reference time if creating new and reference given
// Otherwise set appointent time based on current time (rounded to next hour)
void AppointmentForm::setupForm(ContactDatabase &db, QString contactid, Appointment &editing, Appointment &reference, bool createNew)
{
   QDateTime now, then ;
   QString summary ;
   QString description ;
   Contact& contact = db.getContactById(contactid) ;
   QString contactName ;

   appt = editing ;

   contactName = contact.getFormattedName(false, true) ;
   if (contact.isNull()) {
       for (int i=0; i<db.size(); i++) {
           ui->contactName->addItem(db.getContact(i).getFormattedName(false, true), db.getContact(i).getField(Contact::ID)) ;
       }
        ui->contactName->addItem(" - Unnamed Contact - ", "UNNAMED");
        ui->contactName->selectItem(" - Unnamed Contact - ") ;
        ui->contactName->setReadOnly(false);
   } else {
       ui->contactName->addItem(contact.getFormattedName(false, true), contact.getField(Contact::ID));
       ui->contactName->selectItem(contactName) ;
       ui->contactName->setReadOnly(true);
   }

   if (contactid.isEmpty()) contactid = appt.getField(Appointment::ContactId) ;

   if (createNew) {

       if (!reference.isEmpty()) {

           now = reference.getDate(Appointment::To) ;

       } else {

           now = QDateTime::currentDateTimeUtc() ;
           now = now.addSecs(60*60) ;
           QTime t(now.toLocalTime().time().hour(), 0, 0) ;
           now.setTime(t);
       }

       then = now.addSecs(60*60) ;

   } else {

       now = appt.getDate(Appointment::From);
       then = appt.getDate(Appointment::To) ;
       description = appt.getField(Appointment::Description) ;

   }

   int duration = (int)now.secsTo(then) / 60 ;
   summary = appt.getField(Appointment::Summary) ;

   ui->editWhenFrom->setDateTime(now, duration) ;
   ui->editWhenTo->setDateTime(then) ;
   ui->editSummary->setText(summary) ;
   ui->plaintexteditDescription->setPlainText(description);

   // The repeat: its type, and how often it repeats.  The unit after the number follows the type.
   int repeat = appt.getField(Appointment::Repeat).toInt() ;
   ui->comboRepeat->setCurrentIndex((repeat>=0 && repeat<=4) ? repeat : 0) ;
   ui->spinRepeatEvery->setValue(qMax(1, appt.getField(Appointment::RepeatInterval).toInt())) ;
   auto updateRepeat = [this]() {
       static const char *units[5] = { "", "day(s)", "week(s)", "month(s)", "year(s)" } ;
       int type = ui->comboRepeat->currentIndex() ;
       ui->spinRepeatEvery->setEnabled(type>0) ;
       ui->labelRepeatUnit->setText(units[type]) ;
   } ;
   updateRepeat() ;
   connect(ui->comboRepeat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [updateRepeat](int) { updateRepeat() ; }) ;
}

AppointmentForm::~AppointmentForm()
{
    delete ui;
}

Appointment& AppointmentForm::getAppointmentDetails()
{
    QDateTime from, to ;
    QString summary, description ;
    QString contactName, contactData ;

    contactName = ui->contactName->currentText() ;
    contactData = ui->contactName->currentData() ;
    if (contactData.compare("UNNAMED")==0) {
        contactName = "Unnamed Contact" ;
        contactData = "" ;
    }
    appt.setField(Appointment::For, contactName) ;
    appt.setField(Appointment::ContactId, contactData) ;

    summary = ui->editSummary->text() ;
    if (summary.isEmpty()) summary="Appointment" ;
    appt.setField(Appointment::Summary, summary) ;

    from=ui->editWhenFrom->getDateTime() ;

    // mins = durationstringToInt(ui->editDuration->text()) ;
    from = ui->editWhenFrom->getDateTime() ;
    to = ui->editWhenTo->getDateTime() ;

    appt.setDate(Appointment::From, from) ;
    appt.setDate(Appointment::To, to) ;

    description = ui->plaintexteditDescription->document()->toPlainText() ;
    appt.setField(Appointment::Description, description) ;

    // The repeat: 0 for none, or the type, and how often it repeats
    appt.setField(Appointment::Repeat, QString::number(ui->comboRepeat->currentIndex())) ;
    appt.setField(Appointment::RepeatInterval, QString::number(ui->spinRepeatEvery->value())) ;

    return appt ;
}




void AppointmentForm::on_editWhenFrom_valueChanged(const QDateTime &newfrom, int duration)
{
    QDateTime to = newfrom.addSecs(duration*60) ;
    if (ui->editWhenTo->getDateTime()!=to) ui->editWhenTo->setDateTime(to) ;
}



void AppointmentForm::on_editWhenTo_valueChanged(const QDateTime &newto, int dur)
{
    Q_UNUSED(dur) ;
    QDateTime from = ui->editWhenFrom->getDateTime() ;
    qint64 duration = ui->editWhenFrom->getDuration() ;

    if (from.addSecs(duration*60)!=newto) {
        if (from>newto) from=newto ;
        qint64 newduration = (newto.toSecsSinceEpoch() - from.toSecsSinceEpoch()) / 60 ;
        ui->editWhenFrom->setDateTime(from, newduration) ;
    }
}
