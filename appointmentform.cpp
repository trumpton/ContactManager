
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
        ui->contactName->addItem(" - Unknown Contact - ", "");
        ui->contactName->selectItem(" - Unknown Contact - ") ;
        ui->contactName->setReadOnly(false);
   } else {
       ui->contactName->addItem(contact.getFormattedName(false, true), contact.getField(Contact::ID));
       ui->contactName->selectItem(contactName) ;
       ui->contactName->setReadOnly(true);
   }

   if (contactid.isEmpty()) contactid = appt.getField(Appointment::ContactId) ;

   if (appt.isEmpty()) {

       if (createNew && !reference.isEmpty()) {

           now = reference.getDate(Appointment::To) ;

       } else {

           now = QDateTime::currentDateTimeUtc() ;
           now = now.addSecs(60*60) ;
           QTime t(now.toLocalTime().time().hour(), 0, 0) ;
           now.setTime(t);
       }

       then = now.addSecs(60*60) ;
       summary = "Appointment" ;

   } else {

       now = appt.getDate(Appointment::From);
       then = appt.getDate(Appointment::To) ;
       summary = appt.getField(Appointment::Summary) ;
       description = appt.getField(Appointment::Description).replace("\\n","\n") ;
   }

   int duration = (int)now.secsTo(then) / 60 ;

   ui->editWhenFrom->setDateTime(now, duration) ;
   ui->editWhenTo->setDateTime(then) ;
   ui->editSummary->setText(summary) ;
   ui->plaintexteditDescription->setPlainText(description);
}

AppointmentForm::~AppointmentForm()
{
    delete ui;
}

Appointment& AppointmentForm::getAppointmentDetails()
{
    QDateTime from, to ;
    QString summary, description ;

    QString contactName = ui->contactName->currentText() ;
    QString contactData = ui->contactName->currentData() ;
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
    description = description.replace("\n","\\n") ;
    appt.setField(Appointment::Description, description) ;

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
        qint64 newduration = (newto.toTime_t() - from.toTime_t()) / 60 ;
        ui->editWhenFrom->setDateTime(from, newduration) ;
    }
}
