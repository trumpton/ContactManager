
#include "mainwindow.h"
#include "configuration.h"
#include "../Lib/alertsound.h"

void MainWindow::sendMailSMS()
{
    QString from, title, message ;
    static int mutex=0 ;
    QDateTime now = QDateTime::currentDateTimeUtc() ;

#ifndef DEBUGTIMER
    // Don't be anti-social
    int hour = now.toLocalTime().time().hour() ;
    if (hour<8 || hour>22) return ;
#endif

    // Ensure non-reentrant
    if (mutex>0) return ;
    mutex++ ;

    // Set a test window (now+2h -> now+36h)
    // Used so that reminders are sent within this window
    Appointment testappt ;
    QDateTime fromt, tot ;
    fromt = now ;
    fromt = fromt.addSecs(7200) ;
    tot = now ;
    tot = tot.addSecs(129600) ;
    testappt.setDate(Appointment::From, fromt) ;
    testappt.setDate(Appointment::To, tot) ;

    for (int i=0, csz=calendar.size(); i<csz; i++) {

        Appointment &appt = calendar.getAppointment(i) ;
        bool deleted = appt.isSet(Appointment::Deleted) ;
        QDateTime messagesentdate = appt.getDate(Appointment::MessageSent) ;
        bool messagesent = messagesentdate.isValid()  ;

        // Only process non-deleted or non-recently-deleted messages
        if (!deleted || (deleted && messagesent)) {

            bool isinfuture = appt.isInFuture() ;

            QString apptdate = appt.getDate(Appointment::From).toLocalTime().toString("dd-MMM-yyyy hh:mm") ;

            QString id = appt.getContactId() ;
            Contact& contact = db.getContactById(id) ;
            bool validcontact = !contact.isNull() && !contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden);

            QDateTime created = appt.getDate(Appointment::Created) ;
            unsigned long int elapsed = ( now.toTime_t() - created.toTime_t() ) ;

#ifdef DEBUGTIMER
            bool elapsed2min = (elapsed > 5) ;
#else
            bool elapsed2min = (elapsed > 120) ;
#endif

            if (elapsed2min && isinfuture && validcontact) {

                QString mobile = contact.getField(Contact::Mobile) ;
                QString emailaddress = contact.getField(Contact::Email) ;

                bool textme = contact.isSet(Contact::TextMe)  && !mobile.isEmpty();
                bool emailme = contact.isSet(Contact::EmailMe) && !emailaddress.isEmpty() ;

                QDateTime changedate = appt.getDate(Appointment::FromUpdated) ;
                QDateTime remindermessagesentdate = appt.getDate(Appointment::ReminderMessageSent) ;
                bool remindermessagesent = remindermessagesentdate.isValid() ;

                int messagecounter = appt.getField(Appointment::MessageCounter).toInt() ;

                bool changed = (messagesent && (changedate > messagesentdate)) ||
                        (remindermessagesent && (changedate > remindermessagesentdate)) ;

                bool istime = appt.clashes(testappt) ;

                //
                // Send first confirmation email / sms
                //

                if (emailme && !deleted && !messagesent && messagecounter<MAXMESSAGERETRIES) {

                    QString cfgfile = gConf->getMessageFile(MSG_EMAIL_CONFIRMATION) ;
                    msg("Sending confirmation email to " + emailaddress) ;
                   if (loadMessage(cfgfile, from, title, message, appt, contact)) {

                       switch (SendMail(from, emailaddress, title, message)) {
                       case 1:
                           addLog("Sent confirmation email to " + emailaddress) ;
                           appt.setField(Appointment::MessageSent, nowToIsoString()) ;
                           if (istime) {
                               addLog("Not sending confirmation email to " + emailaddress + " as appointment is near") ;
                               appt.setField(Appointment::ReminderMessageSent, nowToIsoString()) ;
                           } else {
                               appt.setField(Appointment::ReminderMessageSent, "") ;
                           }

                           contact.getHistory().addEntry("Sending confirmation email for appointment: " + apptdate)  ;
                           play(Sent) ;
                           calendar.save() ;
                           break ;
                       case -1: // -1 - Not Started Sending
                       case -2: // -2 - Network Error
                           break ;
                       case -3: // -3 - Error during transaction
                       case -4: // -4 - Unable to Send / Possibly Sent
                           appt.setField(Appointment::MessageCounter, QString::number(++messagecounter)) ;
                           addLog("Error Sending confirmation email to " + emailaddress + ": " + SMTPSMSErrorMessage) ;
                           break ;
                       }

                   } else {
                       addLog(QString("Error loading: ") + cfgfile) ;
                   }
                }

                else if (textme && !emailme && !deleted && !messagesent && messagecounter<MAXMESSAGERETRIES) {


                    QString cfgfile = gConf->getMessageFile(MSG_SMS_CONFIRMATION) ;
                    msg("Sending confirmation sms to " + mobile) ;

                   if (loadMessage(cfgfile, from, title, message, appt, contact)) {

                       switch (SendSMS(from, mobile, title)) {
                       case 1:
                           addLog("Sent confirmation sms to " + mobile) ;
                           appt.setField(Appointment::MessageSent, nowToIsoString()) ;
                           if (istime) {
                               addLog("Not sending confirmation sms to " + mobile + " as appointment is near") ;
                               appt.setField(Appointment::ReminderMessageSent, nowToIsoString()) ;
                           } else {
                               appt.setField(Appointment::ReminderMessageSent, "") ;
                           }

                           contact.getHistory().addEntry("Sending confirmation sms for appointment: " + apptdate)  ;
                           play(Sent) ;
                           calendar.save() ;
                           break ;
                       case -1: // -1 - Not Started Sending
                       case -2: // -2 - Network Error
                           break ;
                       case -3: // -3 - Error during transaction
                       case -4: // -4 - Unable to Send / Possibly Sent
                           appt.setField(Appointment::MessageCounter, QString::number(++messagecounter)) ;
                           addLog("Error Sending confirmation sms to " + mobile + ": " + SMTPSMSErrorMessage) ;
                           break ;
                       }

                   } else {
                       addLog(QString("Error loading: ") + cfgfile) ;
                   }

                }


                //
                // Send Change email / sms
                //

                else if (emailme && !deleted && changed && messagesent && messagecounter<MAXMESSAGERETRIES) {

                    QString cfgfile = gConf->getMessageFile(MSG_EMAIL_UPDATE) ;
                    msg("Sending change email to " + emailaddress) ;
                    if (loadMessage(cfgfile, from, title, message, appt, contact)) {

                        switch (SendMail(from, emailaddress, title, message)) {
                        case 1:
                            addLog("Sent change email to " + emailaddress) ;
                            appt.setField(Appointment::MessageSent, nowToIsoString()) ;
                            if (istime) {
                                addLog("Not sending change email to " + emailaddress + " as appointment is near") ;
                                appt.setField(Appointment::ReminderMessageSent, nowToIsoString()) ;
                            } else {
                                appt.setField(Appointment::ReminderMessageSent, "") ;
                            }

                            contact.getHistory().addEntry("Sending change email for appointment: " + apptdate)  ;
                            play(Sent) ;
                            calendar.save() ;
                            break ;
                        case -1: // -1 - Not Started Sending
                        case -2: // -2 - Network Error
                            break ;
                        case -3: // -3 - Error during transaction
                        case -4: // -4 - Unable to Send / Possibly Sent
                            appt.setField(Appointment::MessageCounter, QString::number(++messagecounter)) ;
                            addLog("Error Sending Change Confirmation Email to " + emailaddress + ": " + SMTPSMSErrorMessage) ;
                            break ;
                        }

                    } else {
                        addLog(QString("Error loading: ") + cfgfile) ;
                    }
                 }

                else if (textme && !emailme && !deleted && changed && messagesent && messagecounter<MAXMESSAGERETRIES) {


                        QString cfgfile = gConf->getMessageFile(MSG_SMS_UPDATE) ;
                        msg("Sending change sms to " + mobile) ;
                        if (loadMessage(cfgfile, from, title, message, appt, contact)) {

                            switch (SendSMS(from, mobile, title)) {
                            case 1:
                                addLog("Sent change sms to " + mobile) ;
                                appt.setField(Appointment::MessageSent, nowToIsoString()) ;
                                if (istime) {
                                    addLog("Not sending change sms to " + mobile + " as appointment is near") ;
                                    appt.setField(Appointment::ReminderMessageSent, nowToIsoString()) ;
                                } else {
                                    appt.setField(Appointment::ReminderMessageSent, "") ;
                                }

                                contact.getHistory().addEntry("Sending change SMS for appointment: " + apptdate)  ;
                                play(Sent) ;
                                calendar.save() ;
                                break ;
                            case -1: // -1 - Not Started Sending
                            case -2: // -2 - Network Error
                                break ;
                            case -3: // -3 - Error during transaction
                            case -4: // -4 - Unable to Send / Possibly Sent
                                appt.setField(Appointment::MessageCounter, QString::number(++messagecounter)) ;
                                addLog("Error Sending Change Confirmation sms to " + mobile + ": " + SMTPSMSErrorMessage) ;
                                break ;
                            }

                        } else {
                            addLog(QString("Error loading: ") + cfgfile) ;
                        }
                    }


                //
                // Send Deleted email/sms
                //

                else if (emailme && deleted && messagesent && messagecounter<MAXMESSAGERETRIES) {

                   QString cfgfile = gConf->getMessageFile(MSG_EMAIL_DELETION) ;
                   msg("Sending cancellation email to " + emailaddress) ;
                   if (loadMessage(cfgfile, from, title, message, appt, contact)) {

                       switch (SendMail(from, emailaddress, title, message)) {
                       case 1:
                           addLog("Sent cancellation email to " + emailaddress) ;
                           appt.setField(Appointment::MessageSent, "") ;
                           appt.setField(Appointment::ReminderMessageSent, "") ;
                           contact.getHistory().addEntry("Sending cancellation email for appointment: " + apptdate)  ;
                           play(Sent) ;
                           calendar.save() ;
                           break ;
                       case -1: // -1 - Not Started Sending
                       case -2: // -2 - Network Error
                           break ;
                       case -3: // -3 - Error during transaction
                       case -4: // -4 - Unable to Send / Possibly Sent
                           appt.setField(Appointment::MessageCounter, QString::number(++messagecounter)) ;
                           addLog("Error Sending cancellation email to " + emailaddress + ": " + SMTPSMSErrorMessage) ;
                           break ;
                       }

                   } else {
                       addLog(QString("Error loading: ") + cfgfile) ;
                   }
                }

                else if (textme && !emailme && deleted && messagesent && messagecounter<MAXMESSAGERETRIES) {

                   QString cfgfile = gConf->getMessageFile(MSG_SMS_DELETION) ;
                   msg("Sending cancelled sms to " + mobile) ;
                   if (loadMessage(cfgfile, from, title, message, appt, contact)) {

                       switch (SendSMS(from, mobile, title)) {
                       case 1:
                           addLog("Sent cancellation sms to " + mobile) ;
                           appt.setField(Appointment::MessageSent, "") ;
                           appt.setField(Appointment::ReminderMessageSent, "") ;
                           contact.getHistory().addEntry("Sending cancellation sms for appointment: " + apptdate)  ;
                           play(Sent) ;
                           calendar.save() ;
                           break ;
                       case -1: // -1 - Not Started Sending
                       case -2: // -2 - Network Error
                           break ;
                       case -3: // -3 - Error during transaction
                       case -4: // -4 - Unable to Send / Possibly Sent
                           appt.setField(Appointment::MessageCounter, QString::number(++messagecounter)) ;
                           addLog("Error Sending cancellation sms to " + mobile + ": " + SMTPSMSErrorMessage) ;
                           break ;
                       }

                   } else {
                       addLog(QString("Error loading: ") + cfgfile) ;
                   }
                }


                //
                // Send reminder sms/email
                // note: SMS preferred for reminders
                //

                else if (textme && !deleted && istime && !remindermessagesent && messagecounter<MAXMESSAGERETRIES) {

                    QString cfgfile = gConf->getMessageFile(MSG_SMS_REMINDER) ;
                    msg("Sending reminder sms message to " + mobile) ;
                    if (loadMessage(cfgfile, from, title, message, appt, contact)) {
                        // Note: 'message' is not used

                        switch(SendSMS(from, mobile, title)) {
                        case 1:
                            addLog("Sent reminder sms to " + mobile) ;
                            appt.setField(Appointment::ReminderMessageSent, nowToIsoString()) ;
                            contact.getHistory().addEntry("Sending reminder email for appointment: " + apptdate)  ;
                            play(Sent) ;
                            calendar.save() ;
                            break ;
                        case -1: // -1 - Not Started Sending
                        case -2: // -2 - Network Error
                            break ;
                        case -3: // -3 - Error during transaction
                        case -4: // -4 - Unable to Send / Possibly Sent
                            appt.setField(Appointment::MessageCounter, QString::number(++messagecounter)) ;
                            addLog("Error Sending reminder sms to " + mobile + ": " + SMTPSMSErrorMessage) ;
                            break ;
                        }

                    } else {
                        addLog(QString("Error loading: ") + cfgfile) ;
                    }
                }

                else if (!textme && emailme && !deleted && istime && !remindermessagesent && messagecounter<MAXMESSAGERETRIES) {

                    QString cfgfile = gConf->getMessageFile(MSG_EMAIL_REMINDER) ;
                    msg("Sending reminder email to " + emailaddress) ;
                    if (loadMessage(cfgfile, from, title, message, appt, contact)) {

                        switch (SendMail(from, emailaddress, title, message)) {
                        case 1:
                            addLog("Sent reminder email to " + emailaddress) ;
                            appt.setField(Appointment::ReminderMessageSent, nowToIsoString()) ;
                            contact.getHistory().addEntry("Sending reminder email for appointment: " + apptdate)  ;
                            play(Sent) ;
                            calendar.save() ;
                            break ;
                        case -1: // -1 - Not Started Sending
                        case -2: // -2 - Network Error
                            break ;
                        case -3: // -3 - Error during transaction
                        case -4: // -4 - Unable to Send / Possibly Sent
                            appt.setField(Appointment::MessageCounter, QString::number(++messagecounter)) ;
                            addLog("Error Sending reminder email to " + emailaddress + ": " + SMTPSMSErrorMessage) ;
                            break ;
                        }

                    } else {
                        addLog(QString("Error loading: ") + cfgfile) ;
                    }
                }
            }
        }

    }
    mutex-- ;

}

//
// Sends SMTP / SMS Messages
//
// Status: -1 - Not Started Sending
//         -2 - Network Error
//         -3 - Error during transaction
//         -4 - Unable to Send / Possibly Sent
//          1 - Send OK
//

int MainWindow::SendMail(QString from, QString emailaddress, QString subject, QString message)
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
        SMTPSMSErrorMessage = smtp.errorMessage() ;
        return smtp.success() ;
    }
}

int MainWindow::SendSMS(QString from, QString mobile, QString message)
{
    SMS sms(gConf->getSMSAgent(), gConf->getSMSUsername(), gConf->getSMSPassword()) ;
    int success = sms.send(mobile, from, message) ;
    SMTPSMSErrorMessage =  sms.getErrorMessage() ;
    return success ;
}

int MainWindow::GetSMSBalance()
{
    SMS sms(gConf->getSMSAgent(), gConf->getSMSUsername(), gConf->getSMSPassword()) ;
    return sms.getBalance() ;
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
    message.replace("$SURNAME", contact.getField(Contact::Surname)) ;
    message.replace("$TIME", start.toLocalTime().toString("hh:mm")) ;
    message.replace("$DOW", start.toLocalTime().toString("dddd")) ;
    message.replace("$DATE", start.toLocalTime().toString("dd")) ;
    message.replace("$MONTH", start.toLocalTime().toString("MMMM")) ;
    message.replace("$YEAR", start.toLocalTime().toString("yyyy")) ;
    message.replace("$FDATE", start.toLocalTime().toString("dd-MMM-yyyy")) ;

    title.replace("$NAME", contact.getField(Contact::Names)) ;
    title.replace("$SURNAME", contact.getField(Contact::Surname)) ;
    title.replace("$TIME", start.toLocalTime().toString("hh:mm")) ;
    title.replace("$DOW", start.toLocalTime().toString("dddd")) ;
    title.replace("$DATE", start.toLocalTime().toString("dd")) ;
    title.replace("$MONTH", start.toLocalTime().toString("MMMM")) ;
    title.replace("$YEAR", start.toLocalTime().toString("yyyy")) ;
    title.replace("$FDATE", start.toLocalTime().toString("dd-MMM-yyyy")) ;

    return !title.isEmpty() && !from.isEmpty();
}
