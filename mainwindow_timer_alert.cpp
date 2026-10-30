
#include "mainwindow.h"
#include "configuration.h"
#include "../Lib/alertsound.h"

#include <algorithm>
#include <QHash>
#include <QSet>

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

    // With no appointments to come, the list says so
    if (lines.isEmpty()) return "none" ;
    return lines.join(separator) ;
}

// What a confirmation or update says about an appointment.  If it changes after one was sent, an update is sent.
static QString messageSignature(Appointment &appt)
{
    const Appointment::AppointmentRecord fields[] = {
        Appointment::From, Appointment::To, Appointment::Summary, Appointment::Description, Appointment::Location,
        Appointment::Repeat, Appointment::RepeatInterval, Appointment::RepeatCount, Appointment::RepeatUntil,
        Appointment::RepeatExceptions } ;

    QStringList parts ;
    for (Appointment::AppointmentRecord field : fields) parts.append(appt.getField(field)) ;
    return parts.join(QChar(0x1f)) ;
}

// A message that has failed to send, in this session.  It waits SENDRETRYMINS minutes before it is tried again, and is
// given up after MAXMESSAGERETRIES failures.  Kept per appointment and kind of message, so that a failed confirmation
// does not use up the tries for a later cancellation.
static const int SENDRETRYMINS = 5 ;
static QHash<QString, QDateTime> sendretryafter ;
static QHash<QString, int> sendfailures ;

// Appointments already reported as having no address to send to, so that the report is made once
static QSet<QString> noaddressreported ;

// The on-the-day reminder is sent by email and by text.  A channel that has been sent is kept here until both have
// been, when the day's reminder is recorded on the appointment.
static QSet<QString> daychannelsent ;

// Whether a message with this key may be tried now (it has not been given up on, and its wait is over)
static bool maySend(const QString &key, const QDateTime &now)
{
    if (sendfailures.value(key) >= MAXMESSAGERETRIES) return false ;
    return !(sendretryafter.contains(key) && now<sendretryafter.value(key)) ;
}

// Result from sendMailSMS when the message file could not be loaded.  Other results are those of Email and SMS.
static const int RESULT_NO_TEMPLATE = -6 ;

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
    QDateTime fromt = now.addSecs(gConf->getReminderWindowFromHours() * 3600) ;
    QDateTime tot = now.addSecs(gConf->getReminderWindowToHours() * 3600) ;
    testappt.setDate(Appointment::From, fromt) ;
    testappt.setDate(Appointment::To, tot) ;

    // Sending can run an event loop, which lets the user change the calendar.  So the appointments are listed by ID
    // first, and each one is found again by ID after a send, rather than being held as a reference across it.
    QStringList ids ;
    for (int i=0, csz=calendar.size(); i<csz; i++) ids.append(calendar.getAppointment(i).getId()) ;

    bool saveneeded = false ;

    // Sends one message through one channel, and records the outcome by key.  The appointment and contact are found by
    // ID, as the send may have run an event loop.  Returns true if the message was sent, or may have been.
    auto sendOnDay = [&](const QString &key, const QString &type, bool byemail, const QString &cfgfile,
                         const QString &apptid, const QString &contactid, const QString &mobile,
                         const QString &emailaddress) -> bool {
        Appointment &appt = calendar.getAppointmentBy(Appointment::ID, apptid) ;
        QString cid = contactid ;
        Contact &contact = db.getContactById(cid) ;
        if (appt.isNull() || contact.isNull()) return false ;

        QString apptdate = appt.getDate(Appointment::From).toLocalTime().toString("dd-MMM-yyyy hh:mm") ;

        from.clear() ;
        title.clear() ;
        message.clear() ;

        int result = RESULT_NO_TEMPLATE ;
        QString errortext ;
        if (!loadMessage(cfgfile, from, title, message, appt, contact)) {
            errortext = "could not load " + cfgfile + " (it is missing, or has no sender or subject line)" ;
        } else if (byemail) {
            Email email(gConf->getSMTPServer(), gConf->getSMTPUsername(), gConf->getSMTPPassword(), gConf->getSMTPPort().toInt()) ;
            result = email.send(gConf->getEmailSender().isEmpty() ? from : gConf->getEmailSender(), emailaddress, title, message) ;
            errortext = email.getErrorMessage() ;
        } else {
            SMS sms(gConf->getSMSAgent(), gConf->getSMSUsername(), gConf->getSMSPassword()) ;
            result = sms.send(SMS::internationalNumber(mobile, gConf->getCountryDiallingCode()), from, title) ;
            errortext = sms.getErrorMessage() ;
        }

        if (result==1 || result==-4) {
            sendfailures.remove(key) ;
            sendretryafter.remove(key) ;
            daychannelsent.insert(key) ;
            if (result==1) {
                addLog(QString("%1 - %2").arg(type,apptdate)) ;
                msg(type) ;
                play(Sent) ;
            } else {
                addLog(QString("WARNING: %1 - %2: no reply in time, so it may have been sent. It will not be sent again. %3")
                       .arg(type,apptdate,errortext)) ;
            }
            return true ;
        }

        int failures = sendfailures.value(key) + 1 ;
        sendfailures.insert(key, failures) ;
        sendretryafter.insert(key, QDateTime::currentDateTimeUtc().addSecs(SENDRETRYMINS * 60)) ;
        addLog(QString("Error %1 - %2: %3 (try %4 of %5, next try in %6 minutes)")
               .arg(type,apptdate,errortext)
               .arg(failures)
               .arg(MAXMESSAGERETRIES)
               .arg(SENDRETRYMINS)) ;
        if (failures>=MAXMESSAGERETRIES) {
            addLog(QString("Given up on %1 - %2 after %3 tries").arg(type,apptdate).arg(failures)) ;
        }
        return false ;
    } ;

    for (int i=0; i<ids.size(); i++) {

        // By position while the calendar is unchanged, otherwise by ID
        Appointment *found = NULL ;
        if (i<calendar.size() && calendar.getAppointment(i).getId()==ids.at(i)) {
            found = &calendar.getAppointment(i) ;
        } else {
            Appointment &byid = calendar.getAppointmentBy(Appointment::ID, ids.at(i)) ;
            if (!byid.isNull()) found = &byid ;
        }
        if (found==NULL) continue ;

        Appointment &appt = *found ;
        QString id = appt.getId() ;
        QString contactid = appt.getContactId() ;
        bool istemp = appt.isTemp() ;
        bool deleted = appt.isSet(Appointment::Deleted) ;
        bool messagesent = appt.getDate(Appointment::MessageSent).isValid() ;

        // Only process non-temp and non-deleted or non-recently-deleted messages
        if (istemp || (deleted && !messagesent)) continue ;

        Contact &contact = db.getContactById(contactid) ;
        bool validcontact = !contact.isNull() && !contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden);
        bool isinfuture = appt.isInFuture() ;
        QDateTime created = appt.getDate(Appointment::Created) ;
        unsigned long int elapsed = ( now.toSecsSinceEpoch() - created.toSecsSinceEpoch() ) ;

        // Wait before sending, so that a new appointment can be finished first (waitbeforesendmins in [reminders])
        bool waitedenough = (elapsed >= (unsigned long int)gConf->getWaitBeforeSendMinutes() * 60) ;

        if (!waitedenough || !isinfuture || !validcontact) continue ;

        // What the message will say.  A change to it since the last confirmation or update is sent as an update.
        QString signature = messageSignature(appt) ;
        if (messagesent && !deleted && appt.getField(Appointment::MessageSignature).isEmpty()) {
            // Confirmed before the signature was kept: start from the current details, so no update is sent for them
            appt.setField(Appointment::MessageSignature, signature) ;
            saveneeded = true ;
        }

        QString mobile = contact.getField(Contact::Mobile) ;
        QString emailaddress = contact.getField(Contact::Email) ;

        bool textme = contact.isSet(Contact::TextMe)  && !mobile.isEmpty();
        bool emailme = contact.isSet(Contact::EmailMe) && !emailaddress.isEmpty() ;

        QString apptdate = appt.getDate(Appointment::From).toLocalTime().toString("dd-MMM-yyyy hh:mm") ;
        bool remindermessagesent = appt.getDate(Appointment::ReminderMessageSent).isValid() ;
        bool changed = messagesent && !deleted && appt.getField(Appointment::MessageSignature)!=signature ;

        // An appointment that starts within the lead time is reminded of at once (by its confirmation), not never
        QDateTime start = appt.getDate(Appointment::From) ;
        bool shortnotice = (start>now && start<fromt) ;
        bool istime = appt.clashes(testappt) || shortnotice ;

        QString kind ;
        QString cfgfile ;
        QString type ;
        bool sendsms = false ;
        bool sendsmtp = false ;
        bool setsignature = false ;
        bool deletemessagesent = false ;
        bool deleteremindermessagesent = false ;
        bool updatemessagesent = false ;
        bool updateremindermessagesent = false ;

        if (emailme && !deleted && !messagesent) {
            kind = "confirm" ;
            type = "Sending confirmation email to " + emailaddress ;
            cfgfile = gConf->getMessageFile(MSG_EMAIL_CONFIRMATION) ;
            sendsmtp = true ;
            updatemessagesent = true ;
            setsignature = true ;
            if (istime) updateremindermessagesent = true ;
        }

        else if (textme && !emailme && !deleted && !messagesent) {
            kind = "confirm" ;
            type = "Sending confirmation SMS to " + mobile;
            cfgfile = gConf->getMessageFile(MSG_SMS_CONFIRMATION) ;
            sendsms = true ;
            updatemessagesent = true ;
            setsignature = true ;
            if (istime) updateremindermessagesent = true ;
        }

        else if (emailme && !deleted && changed) {
            kind = "update" ;
            type = "Sending change email to " + emailaddress ;
            cfgfile = gConf->getMessageFile(MSG_EMAIL_UPDATE) ;
            sendsmtp = true ;
            updatemessagesent = true ;
            setsignature = true ;
            if (istime) updateremindermessagesent = true ;
        }

        else if (textme && !emailme && !deleted && changed) {
            kind = "update" ;
            type = "Sending change sms to " + mobile ;
            cfgfile = gConf->getMessageFile(MSG_SMS_UPDATE) ;
            sendsms = true ;
            updatemessagesent = true ;
            setsignature = true ;
            if (istime) updateremindermessagesent = true ;
        }

        else if (emailme && deleted && messagesent) {
            kind = "cancel" ;
            type = "Sending cancellation email to " + emailaddress ;
            cfgfile = gConf->getMessageFile(MSG_EMAIL_DELETION) ;
            sendsmtp = true ;
            deletemessagesent = true ;
            deleteremindermessagesent = true ;
        }

        else if (textme && !emailme && deleted && messagesent) {
            kind = "cancel" ;
            type = "Sending cancelled sms to " + mobile ;
            cfgfile = gConf->getMessageFile(MSG_SMS_DELETION) ;
            sendsms = true ;
            deletemessagesent = true ;
            deleteremindermessagesent = true ;
        }

        else if (textme && !emailme && !deleted && istime && !remindermessagesent) {
            kind = "reminder" ;
            type = "Sending reminder sms message to " + mobile ;
            cfgfile = gConf->getMessageFile(MSG_SMS_REMINDER) ;
            sendsms = true ;
            updateremindermessagesent = true ;
        }

        else if (emailme && !deleted && istime && !remindermessagesent) {
            kind = "reminder" ;
            type = "Sending reminder email to " + emailaddress ;
            cfgfile = gConf->getMessageFile(MSG_EMAIL_REMINDER) ;
            sendsmtp = true ;
            updateremindermessagesent = true ;
        }

        // The on-the-day reminder: when both email and text are set, the reminder on the day of the appointment goes by
        // both.  It is sent once, after any other reminder for the appointment has gone.
        if (kind.isEmpty() && emailme && textme && !deleted && isinfuture &&
            !appt.getDate(Appointment::ReminderOnDaySent).isValid() &&
            appt.getDate(Appointment::From).toLocalTime().date()==now.toLocalTime().date()) {

            QString emailkey = id + "|dayemail" ;
            QString textkey = id + "|daysms" ;

            if (!daychannelsent.contains(emailkey) && maySend(emailkey, now)) {
                sendOnDay(emailkey, "Sending on-the-day reminder email to " + emailaddress, true,
                          gConf->getMessageFile(MSG_EMAIL_REMINDER), id, contactid, mobile, emailaddress) ;
            }
            if (!daychannelsent.contains(textkey) && maySend(textkey, now)) {
                sendOnDay(textkey, "Sending on-the-day reminder sms message to " + mobile, false,
                          gConf->getMessageFile(MSG_SMS_REMINDER), id, contactid, mobile, emailaddress) ;
            }

            if (daychannelsent.contains(emailkey) && daychannelsent.contains(textkey)) {
                Appointment &after = calendar.getAppointmentBy(Appointment::ID, id) ;
                if (!after.isNull()) after.setField(Appointment::ReminderOnDaySent, nowToIsoString()) ;
                daychannelsent.remove(emailkey) ;
                daychannelsent.remove(textkey) ;
                if (!calendar.save()) addLog("ERROR: the calendar could not be saved after the on-the-day reminder") ;
            }
            continue ;
        }

        if (kind.isEmpty()) {
            // A contact who wants messages but has no address for them is reported once, rather than skipped in silence.
            // This covers a confirmation that cannot be sent and a cancellation that cannot be sent.
            bool wantsmessages = contact.isSet(Contact::EmailMe) || contact.isSet(Contact::TextMe) ;
            bool needed = (!deleted && !messagesent) || (deleted && messagesent) ;
            if (wantsmessages && !emailme && !textme && needed && !noaddressreported.contains(id)) {
                noaddressreported.insert(id) ;
                QString what = deleted ? "no cancellation was sent" : "no confirmation was sent" ;
                addLog(QString("No email address or mobile number for %1, so %2 for the appointment at %3")
                       .arg(contact.getFormattedName(), what, apptdate)) ;
            }
            continue ;
        }

        // A message that has failed recently, or too often, is left until its wait is over (or for ever, if given up)
        QString key = id + "|" + kind ;
        if (sendfailures.value(key) >= MAXMESSAGERETRIES) continue ;
        if (sendretryafter.contains(key) && now<sendretryafter.value(key)) continue ;

        int result = RESULT_NO_TEMPLATE ;
        QString errortext ;

        from.clear() ;
        title.clear() ;
        message.clear() ;

        if (!loadMessage(cfgfile, from, title, message, appt, contact)) {
            errortext = "could not load " + cfgfile + " (it is missing, or has no sender or subject line)" ;
        } else {
            if (sendsmtp) {
                Email email(gConf->getSMTPServer(), gConf->getSMTPUsername(), gConf->getSMTPPassword(), gConf->getSMTPPort().toInt()) ;
                result = email.send(gConf->getEmailSender().isEmpty() ? from : gConf->getEmailSender(), emailaddress, title, message) ;
                errortext = email.getErrorMessage() ;
            } else if (sendsms) {
                // Voodoo wants the number in international form, without a leading + or 0
                QString dest = SMS::internationalNumber(mobile, gConf->getCountryDiallingCode()) ;
                SMS sms(gConf->getSMSAgent(), gConf->getSMSUsername(), gConf->getSMSPassword()) ;
                result = sms.send(dest, from, title) ;
                errortext = sms.getErrorMessage() ;
            } else {
                errortext = "no channel to send by" ;
            }
        }

        // The send may have run an event loop, so the appointment and contact are found again by ID
        Appointment &after = calendar.getAppointmentBy(Appointment::ID, id) ;
        Contact &aftercontact = db.getContactById(contactid) ;

        // A result of -4 means the message may have been sent, so it is recorded as sent, and is not sent again
        if ((result==1 || result==-4) && !after.isNull()) {
            if (deletemessagesent) after.setField(Appointment::MessageSent, "") ;
            if (deletemessagesent) after.setField(Appointment::MessageSignature, "") ;
            if (deleteremindermessagesent) after.setField(Appointment::ReminderMessageSent, "") ;
            if (updatemessagesent) after.setField(Appointment::MessageSent, nowToIsoString()) ;
            if (updateremindermessagesent) after.setField(Appointment::ReminderMessageSent, nowToIsoString()) ;
            if (setsignature) after.setField(Appointment::MessageSignature, signature) ;
            sendfailures.remove(key) ;
            sendretryafter.remove(key) ;

            // Saved straight away, so that a message is not sent again after a restart
            if (!calendar.save()) addLog("ERROR: the calendar could not be saved after sending: " + type) ;
            saveneeded = false ;
        }

        if (result==1) {
            addLog(QString("%1 - %2").arg(type,apptdate)) ;
            if (!aftercontact.isNull()) appendHistory(aftercontact, QString("%1: %2").arg(type,apptdate)) ;
            msg(type) ;
            play(Sent) ;

        } else if (result==-4) {
            addLog(QString("WARNING: %1 - %2: no reply from the service in time, so it may have been sent. It will not be sent again. %3")
                   .arg(type,apptdate,errortext)) ;

        } else {
            // The message was not sent: it is tried again after a wait, and given up after MAXMESSAGERETRIES tries
            int failures = sendfailures.value(key) + 1 ;
            sendfailures.insert(key, failures) ;
            sendretryafter.insert(key, now.addSecs(SENDRETRYMINS * 60)) ;
            addLog(QString("Error %1 - %2: %3 (try %4 of %5, next try in %6 minutes)")
                   .arg(type,apptdate,errortext)
                   .arg(failures)
                   .arg(MAXMESSAGERETRIES)
                   .arg(SENDRETRYMINS)) ;
            if (failures>=MAXMESSAGERETRIES) {
                addLog(QString("Given up on %1 - %2 after %3 tries").arg(type,apptdate).arg(failures)) ;
            }
        }
    }

    if (saveneeded && !calendar.save()) addLog("ERROR: the calendar could not be saved") ;
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
