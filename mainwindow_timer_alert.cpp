
#include "mainwindow.h"
#include "configuration.h"

// TODO: handle retries
// TODO: read messages from files
void MainWindow::sendMailSMS()
{
    QString from, title, message ;
    static int mutex=0 ;
    QDateTime now = QDateTime::currentDateTimeUtc() ;

    // Don't be anti-social
    int hour = now.toLocalTime().time().hour() ;
    if (hour<8 || hour>22) return ;

    // Ensure non-reentrant
    if (mutex>0) return ;
    mutex++ ;

    // Set a test window (now+1h -> now+25h)
    QDateTime fromt, tot ;
    fromt = now ;
    fromt = fromt.addSecs(3600) ;
    tot = fromt.addDays(1) ;

    Appointment testappt ;
    testappt.setDate(Appointment::From, fromt) ;
    testappt.setDate(Appointment::To, tot) ;

    bool skipmessage=false ;

    for (int i=0, csz=calendar.size(); i<csz; i++) {

        Appointment &appt = calendar.getAppointment(i) ;
        bool isinfuture = appt.isInFuture() ;
        bool deleted = appt.isSet(Appointment::Deleted) ;
        QString apptdate = appt.getDate(Appointment::From).toLocalTime().toString("dd-MMM-yyyy hh:mm") ;

        QString id = appt.getContactId() ;
        Contact& contact = db.getContactById(id) ;
        bool validcontact = !contact.isNull() && !contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden);

        QDateTime now = QDateTime::currentDateTimeUtc() ;
        QDateTime created = appt.getDate(Appointment::Created) ;
        unsigned long int elapsed = ( now.toTime_t() - created.toTime_t() ) ;
        bool elapsed10min = (elapsed > 600) ;

        if (elapsed10min && isinfuture && validcontact) {

            QString mobile = contact.getField(Contact::Mobile) ;
            QString emailaddress = contact.getField(Contact::Email) ;

            bool textme = contact.isSet(Contact::TextMe)  && !mobile.isEmpty();
            bool emailme = contact.isSet(Contact::EmailMe) && !emailaddress.isEmpty() ;


            QDateTime& changedate = appt.getDate(Appointment::FromUpdated) ;
            QDateTime& messagesentdate = appt.getDate(Appointment::MessageSent) ;
            QDateTime& remindermessagesentdate = appt.getDate(Appointment::ReminderMessageSent) ;

            bool messagesent = messagesentdate.isValid()  ;
            bool remindermessagesent = remindermessagesentdate.isValid() ;
            bool changed = (messagesent && (changedate > messagesentdate)) ||
                    (remindermessagesent && (changedate > remindermessagesentdate)) ;

            bool istime = appt.clashes(testappt) ;

            QString nowdate = nowToIsoString() ;

            //
            // TODO:
            //
            // If emailme && textme
            //       Send confirmations, changes, cancellations by email
            //       Send reminder by SMS
            //
            // If emailme
            //       Send everything by email
            //
            // If textme
            //       Send everything by SMS
            //

            // Send Confirmation
            //

// TODO: if delete or change, unset the messagesent and remindermessagesent flag

            if (emailme && !deleted && !changed && !messagesent) {
                QString cfgfile = gConf->getMessageFile(MSG_EMAIL_CONFIRMATION) ;
                msg("Sending Email message to " + emailaddress) ;
               if (loadMessage(cfgfile, from, title, message, appt, contact)) {
                   if (SendMail(from, emailaddress, title, message)) {
                     addLog("Sent Email confirmation to " + emailaddress) ;
                     appt.setField(Appointment::MessageSent, nowdate) ;
                     calendar.save() ;
                     contact.getHistory().addEntry("Sending confirmation email for appointment: " + apptdate)  ;
                   } else {
                       addLog("Error Sending Email confirmation to " + emailaddress + ": ") ;                      
                   }
               } else {
                   addLog(QString("Error loading: ") + cfgfile) ;
               }
            }

            //
            // Send Change
            //

            if (emailme && !deleted && changed && !messagesent) {
               QString cfgfile = gConf->getMessageFile(MSG_EMAIL_UPDATE) ;
               msg("Sending Email change message to " + emailaddress) ;
               if (loadMessage(cfgfile, from, title, message, appt, contact)) {
                   if (SendMail(from, emailaddress, title, message)) {
                       addLog("Sent Email change message to " + emailaddress) ;
                       appt.setField(Appointment::MessageSent, nowdate) ;
                       contact.getHistory().addEntry("Sending change email for appointment: " + apptdate)  ;
                       calendar.save() ;
                   } else {
                       addLog("Error Sending Change Confirmation Email to " + emailaddress + ": ") ;
                   }
               } else {
                   addLog(QString("Error loading: ") + cfgfile) ;
               }
            }

            //
            // Send Deleted / Cancelled
            //

            if (emailme && deleted && !messagesent) {
               QString cfgfile = gConf->getMessageFile(MSG_EMAIL_DELETION) ;
               msg("Sending Email cancelled message to " + emailaddress) ;
               if (loadMessage(cfgfile, from, title, message, appt, contact)) {
                   if (SendMail(from, emailaddress, title, message)) {
                       addLog("Sent Email cancelled message to " + emailaddress) ;
                       appt.setField(Appointment::MessageSent, nowdate) ;
                       contact.getHistory().addEntry("Sending cancellation email for appointment: " + apptdate)  ;
                       calendar.save() ;
                   } else {
                       addLog("Sending Cancelled Confirmation Email to " + emailaddress + " failed: ") ;
                   }
               } else {
                   addLog(QString("Error loading: ") + cfgfile) ;
               }
            }

            //
            // Send SMS Reminder if we have a mobile number
            //

            if (istime && textme && !deleted && !remindermessagesent) {
                QString cfgfile = gConf->getMessageFile(MSG_SMS_REMINDER) ;
                msg("Sending Reminder SMS message to " + mobile) ;
                if (loadMessage(cfgfile, from, title, message, appt, contact)) {
                    // Note: 'message' is not used
                    if (SendSMS(from, mobile, title)) {
                        addLog("Sent SMS reminder to " + mobile) ;
                        appt.setField(Appointment::ReminderMessageSent, nowdate) ;
                        contact.getHistory().addEntry("Sending reminderSMS for appointment: " + apptdate)  ;
                        calendar.save() ;
                    } else {
                        addLog("Sending SMS to " + mobile + " failed: " + GetSMSErrorMessage()) ;
                        if (!CheckSMSCredits() && !skipmessage) {
                            warningOkDialog(this,"Send SMS", "You have insufficient credit to send SMS messages") ;
                            skipmessage=true ; // Skip the rest
                        }
                    }
                } else {
                    addLog(QString("Error loading: ") + cfgfile) ;
                }
             }

            //
            // Else, send an email if we have an email address
            //

            else if (istime && !textme && emailme && !deleted && !remindermessagesent) {
                QString cfgfile = gConf->getMessageFile(MSG_EMAIL_REMINDER) ;
                msg("Sending Reminder Email message to " + emailaddress) ;
                if (loadMessage(cfgfile, from, title, message, appt, contact)) {
                    if (SendMail(from, emailaddress, title, message)) {
                        addLog("Sent Email reminder to " + emailaddress) ;
                        appt.setField(Appointment::ReminderMessageSent, nowdate) ;
                        contact.getHistory().addEntry("Sending reminder email for appointment: " + apptdate)  ;
                        calendar.save() ;
                    } else {
                        addLog("Sending Reminder Email to " + emailaddress + " failed: ") ;
                    }
                } else {
                    addLog(QString("Error loading: ") + cfgfile) ;
                }
            }

        }
    }
    mutex-- ;

}

bool MainWindow::SendMail(QString from, QString emailaddress, QString subject, QString message)
{
    int port ;
    QString& user = gConf->getSMTPUsername() ;
    QString& pass = gConf->getSMTPPassword() ;
    QString& serv = gConf->getSMTPServer() ;

    if (user.isEmpty() || pass.isEmpty() || serv.isEmpty()) return false ;

    port = gConf->getSMTPPort().toInt() ;
    if (port<25) port=25 ;

    Smtp smtp(user, pass, serv, port) ;
    QEventLoop eventLoop ;

    QObject::connect(&smtp, SIGNAL(status(QString)), &eventLoop, SLOT(quit()));
    smtp.sendMail(from, emailaddress, subject, message) ;
    if (smtp.isConnected()) {
        eventLoop.exec() ;
        return smtp.success() ;
    } else {
        return false ;
    }
}


static bool SMSCredits ;
static QString SMSErrorMessage ;

bool MainWindow::SendSMS(QString from, QString mobile, QString message)
{
    SMS sms(gConf->getSMSAgent(), gConf->getSMSUsername(), gConf->getSMSPassword()) ;
    bool success = sms.send(mobile, from, message) ;
    SMSCredits = sms.checkCredits() ;
    SMSErrorMessage =  sms.getErrorMessage() ;
    return success ;
}

bool MainWindow::CheckSMSCredits()
{
    return SMSCredits ;
}

QString MainWindow::GetSMSErrorMessage()
{
    return SMSErrorMessage ;
}

bool MainWindow::loadMessage(QString filename,
       QString& from, QString& title,QString& message,
       Appointment& appt, Contact& contact)
{

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream in(&file);
    in.setCodec("UTF-8") ;

    if (!in.atEnd()) from = in.readLine().trimmed() ;
    if (!in.atEnd()) title = in.readLine().trimmed() ;

    message="" ;

    while (!in.atEnd()) {
        message = message + in.readLine().trimmed() + "\n" ;
    }

    file.close() ;
    message = message.trimmed() ;

    QDateTime &start = appt.getDate(Appointment::From) ;
    message.replace("$NAME", contact.getField(Contact::Names)) ;
    message.replace("$TIME", start.toLocalTime().toString("hh:mm")) ;
    message.replace("$DOW", start.toLocalTime().toString("dddd")) ;
    message.replace("$DATE", start.toLocalTime().toString("dd")) ;
    message.replace("$MONTH", start.toLocalTime().toString("MMMM")) ;
    message.replace("$YEAR", start.toLocalTime().toString("yyyy")) ;
    message.replace("$FDATE", start.toLocalTime().toString("dd-MMM-yyyy")) ;

    title.replace("$NAME", contact.getField(Contact::Names)) ;
    title.replace("$TIME", start.toLocalTime().toString("hh:mm")) ;
    title.replace("$DOW", start.toLocalTime().toString("dddd")) ;
    title.replace("$DATE", start.toLocalTime().toString("dd")) ;
    title.replace("$MONTH", start.toLocalTime().toString("MMMM")) ;
    title.replace("$YEAR", start.toLocalTime().toString("yyyy")) ;
    title.replace("$FDATE", start.toLocalTime().toString("dd-MMM-yyyy")) ;

    return !title.isEmpty() && !from.isEmpty();
}
