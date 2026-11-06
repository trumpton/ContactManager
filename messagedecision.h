#ifndef MESSAGEDECISION_H
#define MESSAGEDECISION_H

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTime>

// What is known about an appointment at one check of the send loop.  The decision does not read the clock or the settings:
// the caller gives it the times, so that the boundaries can be tested.
struct MessageState {
    bool deleted ;              // the appointment is deleted
    bool messagesent ;          // a confirmation has been sent for it
    bool changed ;              // its details differ from those last messaged, and the hold has passed
    bool emailme ;              // the contact is to be messaged by email
    bool textme ;               // the contact is to be messaged by text
    bool remindersent ;         // the reminder for this occurrence has gone
    bool istime ;               // the occurrence starts within remindhoursbefore
    bool confirmonly ;          // the occurrence starts within confirmonlyhours: a confirmation or update replaces the reminder
    bool isrepeating ;          // the appointment is a repeat
    QDateTime pendingcancel ;   // the first occurrence removed from a repeat, still to come and not yet cancelled (or invalid)
    bool onthisday ;            // the occurrence is on today's date, in local time
    bool createdbeforeday ;     // the appointment was made before today, so a first message due today is the on-the-day one
    bool emaildone ;            // the on-the-day email has gone today, or a confirmation or update by email has given it
    bool smsdone ;              // the on-the-day text has gone today, or a confirmation or update by text has given it
    QDateTime occurrence ;      // the occurrence the messages are about
} ;

// One message to send.  kind is confirm, update, cancel, reminder, cancelocc (a removed occurrence) or onday.
struct MessageStep {
    QString kind ;
    bool email ;                // true for email, false for text
    QDateTime occurrence ;      // the occurrence the message is about
    bool recordreminder ;       // the message counts as the reminder for this occurrence, so no reminder is sent later
} ;

// The messages to send now for an appointment.  At most one kind is sent, except on the day, when each channel the contact
// has gets its on-the-day message, each once (email first, then text).  An empty list means nothing is to be sent yet.
QList<MessageStep> decideMessages(const MessageState &s) ;

// The timing rules.  Times are UTC unless said otherwise; hours and minutes come from the settings.

// The reminder is due when the occurrence starts within remindhours of now (strictly within: exactly remindhours is not due)
bool isReminderDue(const QDateTime &occurrence, const QDateTime &now, int remindhours) ;

// A confirmation or update made less than confirmhours before the occurrence takes the reminder's place (strictly less)
bool isConfirmOnly(const QDateTime &occurrence, const QDateTime &now, int confirmhours) ;

// The occurrence is on the same date as now, in local time
bool isOnTheDay(const QDateTime &occurrence, const QDateTime &now) ;

// A change is held while it is less than waitminutes old (secondsSinceChange is the age of the change, in seconds)
bool isWithinHold(qint64 secondsSinceChange, int waitminutes) ;

// Messages are sent from start, up to but not including end, in local time
bool inSendWindow(QTime localtime, QTime start, QTime end) ;

// The ini values, as read: remindhoursbefore is at least 1 and defaults to 24; confirmonlyhours defaults to
// remindhoursbefore x 1.5 (rounded), and a value that is not a number, or is less than remindhoursbefore, gives the default
int resolveRemindHours(const QString &text) ;
int resolveConfirmOnlyHours(const QString &text, int remindhours) ;

// The reason no more messages can be sent to a contact, given the messages sent in the last hour and the last 24 hours, or an
// empty string if they can.  The hour is checked first (limitperhour, then limitperday).
QString messageLimitReason(int sentLastHour, int sentLastDay, int perHour, int perDay) ;

// Whether a message that has failed may be tried now: it has not been given up on (maxretries failures), and its wait is over
// (a retryAfter that is not valid gives no wait)
bool sendMayBeTried(int failures, int maxretries, const QDateTime &retryAfter, const QDateTime &now) ;

// What a message shows about an appointment: its start, end and repeat.  A title or location is not shown, so is not part of it
QString messageSignatureOf(const QString &from, const QString &to, const QString &repeat, const QString &repeatinterval) ;

// A channel is used only if the contact has it ticked and has an address for it.  There is no fallback to the other channel.
bool channelUsable(bool ticked, const QString &address) ;

// The on-the-day template, or the reminder's when the on-the-day template is not set
QString onTheDayTemplate(const QString &ontheday, const QString &reminder) ;

// The SMS key is due for renewal when today is within 30 days of its expiry.  A date that is not valid gives no warning.
bool smsKeyDueSoon(const QDate &expiry, const QDate &today) ;

// Whether a send counts as made, so that it is recorded and not sent again: 1 is sent, and -4 (no reply in time) may have been
// sent, so is treated as sent.  Any other result is a refusal or an error: nothing is recorded, and the message is tried again.
bool sendWasMade(int result) ;

// The on-the-day record of an appointment: the day, then the channels sent that day, e.g. "2026-10-30 email sms"
bool dayChannelSent(const QString &record, const QString &day, const QString &channel) ;
QString dayChannelRecord(const QString &record, const QString &day, const QString &channel) ;

// Test builds only (see loadMessage, and CM_TEST_MESSAGE_TAGS).  The test code at the start of an appointment's summary,
// "AN-017 ..." gives "AN-017"; empty when the summary does not start with one.
QString testCodeOf(const QString &summary) ;
// Puts the test code in place of $TESTTAG in a message.  Other text is unchanged.
QString replaceTestTag(const QString &text, const QString &code) ;

#endif // MESSAGEDECISION_H
