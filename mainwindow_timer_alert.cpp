
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

// What the messages show about an appointment: its start, end and repeat.  A change to these is sent as an update.
// The other fields (summary, location, description, the repeat's end and its removed occurrences) are not shown in the
// messages, so a change to them does not send one.  A removed occurrence is sent as a cancellation instead.
static QString messageSignature(Appointment &appt)
{
    const Appointment::AppointmentRecord fields[] = {
        Appointment::From, Appointment::To, Appointment::Repeat, Appointment::RepeatInterval } ;

    QStringList parts ;
    for (Appointment::AppointmentRecord field : fields) parts.append(appt.getField(field)) ;
    return parts.join(QChar(0x1f)) ;
}

// An occurrence as it is stored in the reminder and cancellation records
static QString occurrenceKey(const QDateTime &occurrence)
{
    return occurrence.toUTC().toString(Qt::ISODate) ;
}

// Whether the reminder for this occurrence has gone.  ReminderFor names the occurrence reminded of.  An appointment
// reminded before ReminderFor was kept is taken to have been reminded for its own start.
static bool reminderSentFor(Appointment &appt, const QDateTime &occurrence)
{
    QString reminded = appt.getField(Appointment::ReminderFor) ;
    if (!reminded.isEmpty()) return reminded==occurrenceKey(occurrence) ;
    return appt.getDate(Appointment::ReminderMessageSent).isValid() && occurrence==appt.getDate(Appointment::From) ;
}

// The occurrences already notified as cancelled
static QStringList cancelledOccurrences(Appointment &appt)
{
    return appt.getField(Appointment::CancelledOccurrences).split(',', Qt::SkipEmptyParts) ;
}

// The day's on-the-day reminder is recorded on the appointment, so that it is not sent again after a restart.  The record
// is the day, and the channels sent that day, e.g. "2026-10-30 email sms".
static bool dayChannelSent(const QString &record, const QString &day, const QString &channel)
{
    QStringList parts = record.split(' ', Qt::SkipEmptyParts) ;
    return !parts.isEmpty() && parts.at(0)==day && parts.contains(channel) ;
}

static QString dayChannelRecord(const QString &record, const QString &day, const QString &channel)
{
    QStringList parts = record.split(' ', Qt::SkipEmptyParts) ;
    if (parts.isEmpty() || parts.at(0)!=day) parts = QStringList() << day ;
    if (!parts.contains(channel)) parts.append(channel) ;
    return parts.join(' ') ;
}

// A message that has failed to send, in this session.  It waits SENDRETRYMINS minutes before it is tried again, and is
// given up after MAXMESSAGERETRIES failures.  Kept per appointment and kind of message, so that a failed confirmation
// does not use up the tries for a later cancellation.
static const int SENDRETRYMINS = 5 ;
static QHash<QString, QDateTime> sendretryafter ;
static QHash<QString, int> sendfailures ;

// Appointments whose sent state could not be read from Google, and the time it is tried again
static QHash<QString, QDateTime> sentstateretryafter ;

// Appointments already reported as having no address to send to, and contacts already reported as at a limit (once an hour
// for each contact), so that each report is made once
static QSet<QString> noaddressreported ;
static QSet<QString> limitreported ;

// Set when the calendar could not be saved after a send.  Nothing more is sent until it can be saved, so that a
// message is not sent again because its record was lost.
static bool saveFailed = false ;

// Result from sendMailSMS when the message file could not be loaded.  Other results are those of Email and SMS.
static const int RESULT_NO_TEMPLATE = -6 ;

// Whether a message with this key may be tried now (it has not been given up on, and its wait is over)
static bool maySend(const QString &key, const QDateTime &now)
{
    if (sendfailures.value(key) >= MAXMESSAGERETRIES) return false ;
    return !(sendretryafter.contains(key) && now<sendretryafter.value(key)) ;
}

// The reason no more messages can be sent to a contact now, or an empty string if they can.  The limits are rolling: the
// messages sent in the last hour, and in the last 24 hours (limitperhour and limitperday in [reminders]).
static QString contactLimitReason(const QString &contactid, const QDateTime &now)
{
    int hour = 0 ;
    QList<QDateTime> sent = gConf->getContactMessageTimes(contactid) ;   // the last 24 hours
    for (const QDateTime &when : sent) {
        if (when > now.addSecs(-3600)) hour++ ;
    }

    if (hour >= gConf->getLimitPerHour()) return QString("the limit of %1 messages an hour").arg(gConf->getLimitPerHour()) ;
    if (sent.size() >= gConf->getLimitPerDay()) return QString("the limit of %1 messages a day").arg(gConf->getLimitPerDay()) ;
    return QString() ;
}

void MainWindow::sendMailSMS()
{
    QString from, title, message ;
    static int mutex=0 ;
    QDateTime now = QDateTime::currentDateTimeUtc() ;
    QString today = now.toLocalTime().date().toString("yyyy-MM-dd") ;

    // Don't be anti-social: only send within the sending window, in local time
    QTime localtime = now.toLocalTime().time() ;
    if (localtime<gConf->getSendWindowStart() || localtime>=gConf->getSendWindowEnd()) return ;

    // Only the copy of ContactManager that holds the data folder's lock sends messages
    if (!remindersallowed) return ;

    // Nothing is sent until the calendar can be saved, so that a sent message is always recorded
    if (saveFailed) {
        if (!calendar.save()) return ;
        saveFailed = false ;
        addLog("The calendar has been saved again, so messages can be sent") ;
    }

    // Ensure non-reentrant
    if (mutex>0) return ;
    mutex++ ;

    // Sending can run an event loop, which lets the user change the calendar.  So the appointments are listed by ID
    // first, and each one is found again by ID after a send, rather than being held as a reference across it.
    QStringList ids ;
    for (int i=0, csz=calendar.size(); i<csz; i++) ids.append(calendar.getAppointment(i).getId()) ;

    bool saveneeded = false ;

    // Saves the calendar after a send, and records a failure so that nothing more is sent until it saves
    auto saveCalendar = [&](const QString &what) {
        if (!calendar.save()) {
            saveFailed = true ;
            addLog("ERROR: the calendar could not be saved after " + what + ". Messages are paused until it can be saved.") ;
        }
    } ;

    // The sent state on the Google event is read before a message is sent, so that a message another copy of ContactManager
    // on the same calendar has sent is not sent again.  Returns 1 to send, 0 when the sent state has changed (the message
    // is decided again on the next check), or -1 when it cannot be read, in which case nothing is sent for a minute.
    auto refreshSentState = [&](const QString &id) -> int {
        Appointment &local = calendar.getAppointmentBy(Appointment::ID, id) ;
        if (local.isNull() || local.getField(Appointment::GoogleRecordId).isEmpty() || local.getField(Appointment::ContactId).isEmpty()) return 1 ;

        if (sentstateretryafter.contains(id) && now<sentstateretryafter.value(id)) return -1 ;

        // Asked of a copy, as the calendar can change while the request runs
        Appointment asked = local ;
        google.setupRToken(gConf->getGoogleOauth2RefreshToken()) ;
        Appointment remote ;
        if (!google.getEventSentState(asked, remote)) {
            sentstateretryafter.insert(id, QDateTime::currentDateTimeUtc().addSecs(60)) ;
            addLog("Could not read the sent state of the appointment at " + asked.getDate(Appointment::From).toLocalTime().toString("dd-MMM-yyyy hh:mm") + " from Google, so it is not sent yet") ;
            return -1 ;
        }
        sentstateretryafter.remove(id) ;

        Appointment &after = calendar.getAppointmentBy(Appointment::ID, id) ;
        if (after.isNull()) return -1 ;
        if (after.mergeSentState(remote)) {
            saveCalendar("the sent state from Google") ;
            return 0 ;
        }
        return 1 ;
    } ;

    // The sent state is written to the Google event straight after a message, so the other copies see it.  If Google cannot
    // take it, it is left pending, and the next sync writes it.
    auto pushSentState = [&](const QString &id) {
        Appointment &written = calendar.getAppointmentBy(Appointment::ID, id) ;
        if (written.isNull() || written.getField(Appointment::GoogleRecordId).isEmpty()) return ;   // the first upload writes it

        Appointment asked = written ;
        google.setupRToken(gConf->getGoogleOauth2RefreshToken()) ;
        bool done = google.updateSentState(asked) ;

        Appointment &after = calendar.getAppointmentBy(Appointment::ID, id) ;
        if (after.isNull()) return ;
        if (done) {
            after.setSentField(Appointment::LedgerPending, QString()) ;
        } else {
            after.setSentField(Appointment::LedgerPending, "yes") ;
            addLog("Could not write the sent state of an appointment to Google, so the next sync writes it") ;
        }
    } ;

    // Sends one message through one channel, to the occurrence given.  The appointment and contact are found by ID, and
    // the message shows the occurrence (a repeat's stored start may be past).  Returns the send's result: 1 if sent,
    // -4 if it may have been sent, or otherwise a failure, which is retried later.  The caller records the outcome.
    auto deliver = [&](const QString &key, const QString &type, bool byemail, const QString &cfgfile,
                       const QString &apptid, const QString &contactid, const QString &mobile,
                       const QString &emailaddress, const QDateTime &occurrence) -> int {
        Appointment &stored = calendar.getAppointmentBy(Appointment::ID, apptid) ;
        QString cid = contactid ;
        Contact &contact = db.getContactById(cid) ;
        if (stored.isNull() || contact.isNull()) return 0 ;

        Appointment shown = stored ;
        qint64 length = stored.getDate(Appointment::From).secsTo(stored.getDate(Appointment::To)) ;
        shown.setDate(Appointment::From, occurrence) ;
        shown.setDate(Appointment::To, occurrence.addSecs(length)) ;
        QString apptdate = occurrence.toLocalTime().toString("dd-MMM-yyyy hh:mm") ;

        from.clear() ;
        title.clear() ;
        message.clear() ;

        int result = RESULT_NO_TEMPLATE ;
        QString errortext ;
        if (!loadMessage(cfgfile, from, title, message, shown, contact)) {
            errortext = "could not load " + cfgfile + " (it is missing, or has no sender or subject line)" ;
        } else if (byemail) {
            Email email(gConf->getSMTPServer(), gConf->getSMTPUsername(), gConf->getSMTPPassword(), gConf->getSMTPPort().toInt()) ;
            result = email.send(gConf->getEmailSender().isEmpty() ? from : gConf->getEmailSender(), emailaddress, title, message) ;
            errortext = email.getErrorMessage() ;
        } else {
            // Voodoo wants the number in international form, without a leading + or 0
            SMS sms(gConf->getSMSAgent(), gConf->getSMSUsername(), gConf->getSMSPassword()) ;
            result = sms.send(SMS::internationalNumber(mobile, gConf->getCountryDiallingCode()), from, title) ;
            errortext = sms.getErrorMessage() ;
        }

        if (result==1 || result==-4) {
            gConf->addContactMessage(contactid, now) ;
            sendfailures.remove(key) ;
            sendretryafter.remove(key) ;

            if (result==1) {
                addLog(QString("%1 - %2").arg(type,apptdate)) ;
                Contact &aftercontact = db.getContactById(cid) ;
                if (!aftercontact.isNull()) appendHistory(aftercontact, QString("%1: %2").arg(type,apptdate)) ;
                msg(type) ;
                play(Sent) ;
            } else {
                addLog(QString("WARNING: %1 - %2: no reply in time, so it may have been sent. It will not be sent again. %3")
                       .arg(type,apptdate,errortext)) ;
            }
            return result ;
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
        return result ;
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
        if (appt.isTemp()) continue ;

        bool deleted = appt.isSet(Appointment::Deleted) ;
        bool messagesent = appt.getDate(Appointment::MessageSent).isValid() ;
        if (deleted && !messagesent) continue ;

        Contact &contact = db.getContactById(contactid) ;
        bool validcontact = !contact.isNull() && !contact.isSet(Contact::Deleted) && !contact.isSet(Contact::Hidden);
        if (!validcontact) continue ;

        // The occurrence the messages are about: a repeat's first occurrence still to come (its stored start may be past).
        // Nothing is sent about an appointment with no occurrence to come.
        QDateTime occ = appt.nextOccurrence() ;
        if (!occ.isValid()) continue ;

        // The hold: a new or changed appointment waits waitbeforesendmins after its last change, so that a run of edits, or a
        // delete and re-create, is sent once, as its final details.  A cancellation is not held.
        qint64 sincechange = now.toSecsSinceEpoch() - appt.getDate(Appointment::Updated).toSecsSinceEpoch() ;
        if (!deleted && sincechange < (qint64)gConf->getWaitBeforeSendMinutes() * 60) continue ;

        // What the message will say.  A change to it since the last confirmation or update is sent as an update.
        QString signature = messageSignature(appt) ;
        if (messagesent && !deleted && appt.getField(Appointment::MessageSignature).isEmpty()) {
            // Confirmed before the signature was kept: start from the current details, so no update is sent for them
            appt.setSentField(Appointment::MessageSignature, signature) ;
            saveneeded = true ;
        }
        bool changed = messagesent && !deleted && appt.getField(Appointment::MessageSignature)!=signature ;

        QString mobile = contact.getField(Contact::Mobile) ;
        QString emailaddress = contact.getField(Contact::Email) ;

        bool textme = contact.isSet(Contact::TextMe)  && !mobile.isEmpty();
        bool emailme = contact.isSet(Contact::EmailMe) && !emailaddress.isEmpty() ;

        QString apptdate = occ.toLocalTime().toString("dd-MMM-yyyy hh:mm") ;
        bool remindermessagesent = reminderSentFor(appt, occ) ;

        // A reminder is due when the appointment starts within remindhoursbefore.  One that is confirmed or changed less than
        // confirmonlyhours before it starts gets its confirmation or update only: that message takes the reminder's place.
        QDateTime remindend = now.addSecs(gConf->getRemindHoursBefore() * 3600) ;
        QDateTime confirmonlyend = now.addSecs(gConf->getConfirmOnlyHours() * 3600) ;
        bool istime = occ < remindend ;
        bool confirmonly = occ < confirmonlyend ;

        QString kind ;
        QString cfgfile ;
        QString type ;
        bool sendsmtp = false ;
        bool setsignature = false ;
        bool deletemessagesent = false ;
        bool updatemessagesent = false ;
        bool updateremindermessagesent = false ;

        if (emailme && !deleted && !messagesent) {
            kind = "confirm" ;
            type = "Sending confirmation email to " + emailaddress ;
            cfgfile = gConf->getMessageFile(MSG_EMAIL_CONFIRMATION) ;
            sendsmtp = true ;
            updatemessagesent = true ;
            setsignature = true ;
            if (confirmonly) updateremindermessagesent = true ;
        }

        else if (textme && !emailme && !deleted && !messagesent) {
            kind = "confirm" ;
            type = "Sending confirmation SMS to " + mobile;
            cfgfile = gConf->getMessageFile(MSG_SMS_CONFIRMATION) ;
            updatemessagesent = true ;
            setsignature = true ;
            if (confirmonly) updateremindermessagesent = true ;
        }

        else if (emailme && !deleted && changed) {
            kind = "update" ;
            type = "Sending change email to " + emailaddress ;
            cfgfile = gConf->getMessageFile(MSG_EMAIL_UPDATE) ;
            sendsmtp = true ;
            updatemessagesent = true ;
            setsignature = true ;
            if (confirmonly) updateremindermessagesent = true ;
        }

        else if (textme && !emailme && !deleted && changed) {
            kind = "update" ;
            type = "Sending change sms to " + mobile ;
            cfgfile = gConf->getMessageFile(MSG_SMS_UPDATE) ;
            updatemessagesent = true ;
            setsignature = true ;
            if (confirmonly) updateremindermessagesent = true ;
        }

        else if (emailme && deleted && messagesent) {
            kind = "cancel" ;
            type = "Sending cancellation email to " + emailaddress ;
            cfgfile = gConf->getMessageFile(MSG_EMAIL_DELETION) ;
            sendsmtp = true ;
            deletemessagesent = true ;
        }

        else if (textme && !emailme && deleted && messagesent) {
            kind = "cancel" ;
            type = "Sending cancelled sms to " + mobile ;
            cfgfile = gConf->getMessageFile(MSG_SMS_DELETION) ;
            deletemessagesent = true ;
        }

        else if (textme && !emailme && !deleted && istime && !remindermessagesent) {
            kind = "reminder" ;
            type = "Sending reminder sms message to " + mobile ;
            cfgfile = gConf->getMessageFile(MSG_SMS_REMINDER) ;
            updateremindermessagesent = true ;
        }

        else if (emailme && !deleted && istime && !remindermessagesent) {
            kind = "reminder" ;
            type = "Sending reminder email to " + emailaddress ;
            cfgfile = gConf->getMessageFile(MSG_EMAIL_REMINDER) ;
            sendsmtp = true ;
            updateremindermessagesent = true ;
        }

        // An occurrence removed from a repeat (by "just this one") is cancelled, for the occurrence removed
        if (kind.isEmpty() && messagesent && !deleted && appt.isRepeating()) {
            QStringList notified = cancelledOccurrences(appt) ;
            for (const QDateTime &removed : appt.exceptions()) {
                if (removed<=now || notified.contains(occurrenceKey(removed))) continue ;
                kind = "cancelocc" ;
                occ = removed ;
                apptdate = occ.toLocalTime().toString("dd-MMM-yyyy hh:mm") ;
                if (emailme) {
                    type = "Sending cancellation email to " + emailaddress ;
                    cfgfile = gConf->getMessageFile(MSG_EMAIL_DELETION) ;
                    sendsmtp = true ;
                } else {
                    type = "Sending cancelled sms to " + mobile ;
                    cfgfile = gConf->getMessageFile(MSG_SMS_DELETION) ;
                }
                break ;
            }
        }

        // The on-the-day reminder: when both email and text are set, the reminder on the day of the appointment goes by
        // both.  Each channel is sent once, and recorded, so that it is not sent again after a restart.
        QString dayrecord = appt.getField(Appointment::ReminderOnDaySent) ;
        bool emaildone = dayChannelSent(dayrecord, today, "email") ;
        bool smsdone = dayChannelSent(dayrecord, today, "sms") ;

        if (kind.isEmpty() && emailme && textme && !deleted && !(emaildone && smsdone) &&
            occ.toLocalTime().date()==now.toLocalTime().date()) {

            QString emailkey = id + "|dayemail" ;
            QString textkey = id + "|daysms" ;

            if (!emaildone && maySend(emailkey, now) && contactLimitReason(contactid, now).isEmpty() && refreshSentState(id)==1) {
                int result = deliver(emailkey, "Sending on-the-day reminder email to " + emailaddress, true,
                                     gConf->getMessageFile(MSG_EMAIL_REMINDER), id, contactid, mobile, emailaddress, occ) ;
                if (result==1 || result==-4) {
                    Appointment &after = calendar.getAppointmentBy(Appointment::ID, id) ;
                    if (!after.isNull()) after.setSentField(Appointment::ReminderOnDaySent, dayChannelRecord(after.getField(Appointment::ReminderOnDaySent), today, "email")) ;
                    pushSentState(id) ;
                    saveCalendar("the on-the-day reminder") ;
                }
            }
            if (!smsdone && maySend(textkey, now) && contactLimitReason(contactid, now).isEmpty() && refreshSentState(id)==1) {
                int result = deliver(textkey, "Sending on-the-day reminder sms message to " + mobile, false,
                                     gConf->getMessageFile(MSG_SMS_REMINDER), id, contactid, mobile, emailaddress, occ) ;
                if (result==1 || result==-4) {
                    Appointment &after = calendar.getAppointmentBy(Appointment::ID, id) ;
                    if (!after.isNull()) after.setSentField(Appointment::ReminderOnDaySent, dayChannelRecord(after.getField(Appointment::ReminderOnDaySent), today, "sms")) ;
                    pushSentState(id) ;
                    saveCalendar("the on-the-day reminder") ;
                }
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
        QString key = id + "|" + kind + (kind=="cancelocc" ? "|" + occurrenceKey(occ) : QString()) ;
        if (!maySend(key, now)) continue ;

        // The limits on messages to one contact (per hour and per day) hold the message until they allow it
        QString limit = contactLimitReason(contactid, now) ;
        if (!limit.isEmpty()) {
            QString limitkey = contactid + "|" + QString::number(now.toSecsSinceEpoch()/3600) ;
            if (!limitreported.contains(limitkey)) {
                limitreported.insert(limitkey) ;
                addLog(QString("%1 has reached %2, so %3 was not sent for the appointment at %4")
                       .arg(contact.getFormattedName(), limit, type, apptdate)) ;
            }
            continue ;
        }

        // A cancellation is not held up by the sent state, as its appointment may already be gone from Google
        if (!deleted && refreshSentState(id)!=1) continue ;

        int result = deliver(key, type, sendsmtp, cfgfile, id, contactid, mobile, emailaddress, occ) ;

        // Recorded straight away, so that the message is not sent again after a restart
        if (result==1 || result==-4) {
            Appointment &after = calendar.getAppointmentBy(Appointment::ID, id) ;
            if (!after.isNull()) {
                if (deletemessagesent) {
                    after.setSentField(Appointment::CancelSent, nowToIsoString()) ;
                    after.setSentField(Appointment::MessageSent, "") ;
                    after.setSentField(Appointment::MessageSignature, "") ;
                    after.setSentField(Appointment::ReminderMessageSent, "") ;
                    after.setSentField(Appointment::ReminderFor, "") ;
                }
                if (updatemessagesent) {
                    after.setSentField(Appointment::MessageSent, nowToIsoString()) ;

                    // A confirmation or update given on the day of the appointment already gives the time, so the on-the-day
                    // reminders for that day count as sent
                    if (occ.toLocalTime().date()==now.toLocalTime().date()) {
                        QString record = dayChannelRecord(after.getField(Appointment::ReminderOnDaySent), today, "email") ;
                        after.setSentField(Appointment::ReminderOnDaySent, dayChannelRecord(record, today, "sms")) ;
                    }
                }
                if (setsignature) after.setSentField(Appointment::MessageSignature, signature) ;
                if (updateremindermessagesent) {
                    after.setSentField(Appointment::ReminderMessageSent, nowToIsoString()) ;
                    after.setSentField(Appointment::ReminderFor, occurrenceKey(occ)) ;
                }
                if (kind=="cancelocc") {
                    // Only cancellations still to come are kept, as a past one is never sent again, so the list stays short
                    QStringList notified = cancelledOccurrences(after) ;
                    notified.append(occurrenceKey(occ)) ;
                    QStringList kept ;
                    for (const QString &when : notified) {
                        if (QDateTime::fromString(when, Qt::ISODate) > now) kept.append(when) ;
                    }
                    after.setSentField(Appointment::CancelledOccurrences, kept.join(',')) ;
                }
            }
            pushSentState(id) ;
            saveCalendar(type) ;
        }
    }

    if (saveneeded) saveCalendar("the sync of change records") ;
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

    // A copy of the start, read before the list of upcoming appointments is made, which reads other dates
    QDateTime start = appt.getDate(Appointment::From) ;
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

// On startup, warns when the SMS API key expires within 30 days (the expiry date is set in Settings)
void MainWindow::checkSMSKeyExpiry()
{
    QDate expiry = gConf->getSMSKeyExpiry() ;
    if (!expiry.isValid()) return ;

    if (QDate::currentDate() >= expiry.addDays(-30)) {
        warningOkDialog(this, "SMS Key Expiring", "SMS Key Expiring on " + expiry.toString("dd/MM/yyyy")) ;
    }
}
