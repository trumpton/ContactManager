
#include "mainwindow.h"
#include "configuration.h"
#include "../Lib/alertsound.h"

#include <algorithm>

// The upcoming appointments of a contact, one per line in the form "10:00 on Friday 30 October", soonest first, up to
// ten.  Each occurrence of a repeat is listed.  Joined with the separator.  Used for $APPOINTMENTS in the messages.
static QString upcomingAppointments(Calendar &calendar, QString contactid, QString separator)
{
    QDateTime now = QDateTime::currentDateTimeUtc() ;
    QList<QDateTime> starts ;

    for (int i=0, csz=calendar.size(); i<csz; i++) {
        Appointment &appt = calendar.getAppointment(i) ;
        if (appt.isTemp() || appt.isSet(Appointment::Deleted) || appt.getContactId()!=contactid) continue ;

        // A single appointment has one occurrence; a repeat has an occurrence for each step, until it ends
        bool repeating = appt.isRepeating() ;
        int found=0 ;
        for (int n=0; found<10 && (repeating || n==0) && !appt.occurrenceEnded(n); n++) {
            if (!appt.occurrenceExists(n)) continue ;
            QDateTime start = appt.occurrenceFrom(n) ;
            if (start>now) {
                starts.append(start) ;
                found++ ;
            }
        }
    }

    std::sort(starts.begin(), starts.end()) ;

    QStringList lines ;
    for (int i=0; i<starts.size() && i<10; i++) {
        lines.append(starts.at(i).toLocalTime().toString("hh:mm 'on' dddd d MMMM")) ;
    }
    return lines.join(separator) ;
}

void MainWindow::sendMailSMS()
{
    QString from, title, message ;
    static int mutex=0 ;
    QDateTime now = QDateTime::currentDateTimeUtc() ;

    // Don't be anti-social: only send within the sending window, in local time
    QTime localtime = now.toLocalTime().time() ;
    if (localtime<gConf->getSendWindowStart() || localtime>=gConf->getSendWindowEnd()) return ;

    // Ensure non-reentrant
    if (mutex>0) return ;
    mutex++ ;

    // Set a test window (now+2h -> now+24h by default, from the ini file)
    // Used so that reminders are sent within this window
    Appointment testappt ;
    QDateTime fromt, tot ;
    fromt = now ;
    fromt = fromt.addSecs(gConf->getReminderWindowFromHours() * 3600) ;
    tot = now ;
    tot = tot.addSecs(gConf->getReminderWindowToHours() * 3600) ;
    testappt.setDate(Appointment::From, fromt) ;
    testappt.setDate(Appointment::To, tot) ;

    for (int i=0, csz=calendar.size(); i<csz; i++) {

        Appointment &appt = calendar.getAppointment(i) ;
        bool istemp = appt.isTemp() ;
        bool deleted = appt.isSet(Appointment::Deleted) ;
        QDateTime messagesentdate = appt.getDate(Appointment::MessageSent) ;
        bool messagesent = messagesentdate.isValid()  ;

        // Only process non-temp and non-deleted or non-recently-deleted messages
        if (!istemp && (!deleted || (deleted && messagesent))) {

            bool isinfuture = appt.isInFuture() ;
            Contact& contact = db.getContactById(appt.getContactId()) ;
            bool validcontact = !contact.isNull() && !contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden);
            QDateTime created = appt.getDate(Appointment::Created) ;
            unsigned long int elapsed = ( now.toSecsSinceEpoch() - created.toSecsSinceEpoch() ) ;
            int messagecounter = appt.getField(Appointment::MessageCounter).toInt() ;

            // Wait before sending, so that a new appointment can be finished first (waitbeforesendmins in [reminders])
            bool waitedenough = (elapsed >= (unsigned long int)gConf->getWaitBeforeSendMinutes() * 60) ;

            if (waitedenough && isinfuture && validcontact && messagecounter<MAXMESSAGERETRIES) {

                QString mobile = contact.getField(Contact::Mobile) ;
                QString emailaddress = contact.getField(Contact::Email) ;

                bool textme = contact.isSet(Contact::TextMe)  && !mobile.isEmpty();
                bool emailme = contact.isSet(Contact::EmailMe) && !emailaddress.isEmpty() ;

                QString apptdate = appt.getDate(Appointment::From).toLocalTime().toString("dd-MMM-yyyy hh:mm") ;
                QDateTime changedate = appt.getDate(Appointment::DateUpdated) ;
                QDateTime remindermessagesentdate = appt.getDate(Appointment::ReminderMessageSent) ;
                bool remindermessagesent = remindermessagesentdate.isValid() ;

                bool changed = (messagesent && (changedate > messagesentdate)) ||
                        (remindermessagesent && (changedate > remindermessagesentdate)) ;

                bool istime = appt.clashes(testappt) ;

                QString cfgfile ;
                QString type ;
                bool sendsms = false ;
                bool sendsmtp = false ;
                bool deletemessagesent = false ;
                bool deleteremindermessagesent = false ;
                bool updatemessagesent = false ;
                bool updateremindermessagesent = false ;

                if (emailme && !deleted && !messagesent) {
                    type = "Sending confirmation email to " + emailaddress ;
                    cfgfile = gConf->getMessageFile(MSG_EMAIL_CONFIRMATION) ;
                    sendsmtp = true ;
                    updatemessagesent = true ;
                    if (istime) updateremindermessagesent = true ;
                }

                else if (textme && !emailme && !deleted && !messagesent) {
                    type = "Sending confirmation SMS to " + mobile;
                    cfgfile = gConf->getMessageFile(MSG_SMS_CONFIRMATION) ;
                    sendsms = true ;
                    updatemessagesent = true ;
                    if (istime) updateremindermessagesent = true ;
                }

                else if (emailme && !deleted && changed && messagesent) {
                    type = "Sending change email to " + emailaddress ;
                    cfgfile = gConf->getMessageFile(MSG_EMAIL_UPDATE) ;
                    sendsmtp = true ;
                    updatemessagesent = true ;
                    if (istime) updateremindermessagesent = true ;
                }

                else if (textme && !emailme && !deleted && changed && messagesent) {
                    type = "Sending change sms to " + mobile ;
                    cfgfile = gConf->getMessageFile(MSG_SMS_UPDATE) ;
                    sendsms = true ;
                    updatemessagesent = true ;
                    if (istime) updateremindermessagesent = true ;
                }

                else if (emailme && deleted && messagesent) {
                    type = "Sending cancellation email to " + emailaddress ;
                    cfgfile = gConf->getMessageFile(MSG_EMAIL_DELETION) ;
                    sendsmtp = true ;
                    deletemessagesent = true ;
                    deleteremindermessagesent = true ;
                }

                else if (textme && !emailme && deleted && messagesent) {
                    type = "Sending cancelled sms to " + mobile ;
                    cfgfile = gConf->getMessageFile(MSG_SMS_DELETION) ;
                    sendsms = true ;
                    deletemessagesent = true ;
                    deleteremindermessagesent = true ;
                }

                else if (textme && !deleted && istime && !remindermessagesent) {
                    type = "Sending reminder sms message to " + mobile ;
                    cfgfile = gConf->getMessageFile(MSG_SMS_REMINDER) ;
                    sendsms = true ;
                    updateremindermessagesent = true ;
                }

                else if (!textme && emailme && !deleted && istime && !remindermessagesent) {
                    type = "Sending reminder email to " + emailaddress ;
                    cfgfile = gConf->getMessageFile(MSG_EMAIL_REMINDER) ;
                    sendsmtp = true ;
                    updateremindermessagesent = true ;
                }


                if (sendsmtp || sendsms) {

                    if (!loadMessage(cfgfile, from, title, message, appt, contact)) {

                        addLog(QString("Error loading: ") + cfgfile) ;

                    } else {

                        int result ;
                        if (sendsmtp) {
                            Email email(gConf->getSMTPServer(), gConf->getSMTPUsername(), gConf->getSMTPPassword(), gConf->getSMTPPort().toInt()) ;
                            result = email.send(from, emailaddress, title, message) ;
                            SMTPSMSErrorMessage = email.getErrorMessage() ;
                        } else {
                            SMS sms(gConf->getSMSAgent(), gConf->getSMSUsername(), gConf->getSMSPassword()) ;
                            result = sms.send(mobile, from, title) ;
                            SMTPSMSErrorMessage = sms.getErrorMessage() ;
                        }

                        switch (result) {
                        case 1:
                            if (deletemessagesent) appt.setField(Appointment::MessageSent, "") ;
                            if (deleteremindermessagesent) appt.setField(Appointment::ReminderMessageSent, "") ;
                            if (updatemessagesent) appt.setField(Appointment::MessageSent, nowToIsoString()) ;
                            if (updateremindermessagesent) appt.setField(Appointment::ReminderMessageSent, nowToIsoString()) ;
                            addLog(QString("%1 - %2").arg(type,apptdate)) ;
                            appendHistory(contact, QString("%1: %2").arg(type,apptdate)) ;
                            msg(type) ;
                            play(Sent) ;
                            calendar.save() ;
                            break ;
                        case -1: // -1 - Not Started Sending
                        case -2: // -2 - Network Error
                            break ;
                        case -3: // -3 - Error during transaction
                        case -4: // -4 - Unable to Send / Possibly Sent
                            appt.setField(Appointment::MessageCounter, QString::number(++messagecounter)) ;
                            addLog(QString("Error %1: %2").arg(type,SMTPSMSErrorMessage)) ;
                            break ;
                        }

                    }
                }

            }
        }
    }
    mutex-- ;

}

bool MainWindow::loadMessage(QString filename,
       QString& from, QString& title,QString& message,
       Appointment& appt, Contact& contact)
{

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8) ;

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

    // One appointment per line in an email; SMS and subject lines get them on one line
    message.replace("$APPOINTMENTS", upcomingAppointments(calendar, contact.getField(Contact::ID), "\n")) ;

    title.replace("$NAME", contact.getField(Contact::Names)) ;
    title.replace("$SURNAME", contact.getField(Contact::Surname)) ;
    title.replace("$TIME", start.toLocalTime().toString("hh:mm")) ;
    title.replace("$DOW", start.toLocalTime().toString("dddd")) ;
    title.replace("$DATE", start.toLocalTime().toString("dd")) ;
    title.replace("$MONTH", start.toLocalTime().toString("MMMM")) ;
    title.replace("$YEAR", start.toLocalTime().toString("yyyy")) ;
    title.replace("$FDATE", start.toLocalTime().toString("dd-MMM-yyyy")) ;
    title.replace("$APPOINTMENTS", upcomingAppointments(calendar, contact.getField(Contact::ID), "; ")) ;

    return !title.isEmpty() && !from.isEmpty();
}
