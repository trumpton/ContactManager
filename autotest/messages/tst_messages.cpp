// Tests for the messages ContactManager sends: the timing rules (the 24 hour reminder and the 36 hour confirm-only window,
// and the hold and send window), the messages decided for an appointment, the on-the-day messages, and the appointments
// that the calendar lists.  Run with run-messages-tests.sh, which links these tests with the application's own objects.

#include <QtTest>
#include <QTemporaryDir>
#include <QTextStream>
#include "messagedecision.h"
#include "appointment.h"
#include "calendar.h"

// A UTC time from a local date and time
static QDateTime localAt(QDate date, QTime time)
{
    return QDateTime(date, time, Qt::LocalTime).toUTC() ;
}

// The reference time for the tests: 30 Oct 2026, 10:00 local
static QDateTime nowRef()
{
    return localAt(QDate(2026, 10, 30), QTime(10, 0)) ;
}

// A message state for an appointment that is not yet messaged, with the given channels, and with nothing else set
static MessageState newState(bool email, bool text)
{
    MessageState s ;
    s.deleted = false ;
    s.messagesent = false ;
    s.changed = false ;
    s.emailme = email ;
    s.textme = text ;
    s.remindersent = false ;
    s.istime = false ;
    s.confirmonly = false ;
    s.isrepeating = false ;
    s.pendingcancel = QDateTime() ;
    s.onthisday = false ;
    s.createdbeforeday = false ;
    s.emaildone = false ;
    s.smsdone = false ;
    s.occurrence = localAt(QDate(2026, 11, 2), QTime(15, 0)) ;
    return s ;
}

// The same state, once the confirmation has gone
static MessageState sentState(bool email, bool text)
{
    MessageState s = newState(email, text) ;
    s.messagesent = true ;
    return s ;
}

// The kinds and channels of the steps, as text: "confirm:email, reminder:sms"
static QString describeSteps(const QList<MessageStep> &steps)
{
    QStringList parts ;
    for (const MessageStep &step : steps) {
        parts << step.kind + ":" + (step.email ? "email" : "sms") ;
    }
    return parts.join(", ") ;
}

class TestMessages : public QObject
{
    Q_OBJECT

private slots:

    // ---- The reminder: 24 hours before ----

    void reminderDueWithinTheReminderHours()
    {
        QDateTime now = nowRef() ;
        // One second inside 24 hours is due; exactly 24 hours is not yet; one second outside is not due
        QVERIFY(isReminderDue(now.addSecs(24 * 3600 - 1), now, 24)) ;
        QVERIFY(!isReminderDue(now.addSecs(24 * 3600), now, 24)) ;
        QVERIFY(!isReminderDue(now.addSecs(24 * 3600 + 1), now, 24)) ;
    }

    void reminderDueAtTheStartOfTheDayBefore()
    {
        // An appointment at 16:00 on the 31st: due from 16:00 on the 30th, and not before
        QDateTime occ = localAt(QDate(2026, 10, 31), QTime(16, 0)) ;
        QVERIFY(!isReminderDue(occ, localAt(QDate(2026, 10, 30), QTime(15, 59, 59)), 24)) ;
        QVERIFY(isReminderDue(occ, localAt(QDate(2026, 10, 30), QTime(16, 0, 1)), 24)) ;
    }

    void reminderDueForAnAppointmentThatHasStarted()
    {
        QDateTime now = nowRef() ;
        QVERIFY(isReminderDue(now.addSecs(-3600), now, 24)) ;
    }

    void reminderFollowsRemindHours()
    {
        QDateTime now = nowRef() ;
        QVERIFY(!isReminderDue(now.addSecs(12 * 3600), now, 12)) ;
        QVERIFY(isReminderDue(now.addSecs(12 * 3600 - 1), now, 12)) ;
    }

    // ---- The confirm-only window: 36 hours by default ----

    void confirmOnlyJustInside36Hours()
    {
        QDateTime now = nowRef() ;
        QVERIFY(isConfirmOnly(now.addSecs(36 * 3600 - 1), now, 36)) ;
    }

    void confirmOnlyExactly36HoursIsNot()
    {
        // Created exactly 36 hours before: the confirmation does not replace the reminder
        QDateTime now = nowRef() ;
        QVERIFY(!isConfirmOnly(now.addSecs(36 * 3600), now, 36)) ;
        QVERIFY(!isConfirmOnly(now.addSecs(36 * 3600 + 1), now, 36)) ;
    }

    void confirmOnlyForShortNotice()
    {
        QDateTime now = nowRef() ;
        QVERIFY(isConfirmOnly(now.addSecs(25 * 3600), now, 36)) ;
        QVERIFY(isConfirmOnly(now.addSecs(3600), now, 36)) ;
    }

    void confirmOnlyAndReminderMeetAtTheBoundary()
    {
        // An appointment 36 hours away: created now, the confirmation is not confirm-only, and the reminder falls 12 hours
        // later, at 24 hours before.  One second earlier and the confirmation covers it.
        QDateTime now = nowRef() ;
        QDateTime occ = now.addSecs(36 * 3600) ;
        QVERIFY(!isConfirmOnly(occ, now, 36)) ;
        QVERIFY(!isReminderDue(occ, now, 24)) ;
        QDateTime later = occ.addSecs(-24 * 3600 + 1) ;
        QVERIFY(isReminderDue(occ, later, 24)) ;
    }

    void confirmOnlyDefaultIs1Point5TimesRemind()
    {
        QCOMPARE(resolveConfirmOnlyHours(QString(), 24), 36) ;
        QCOMPARE(resolveConfirmOnlyHours(QString(), 12), 18) ;
        QCOMPARE(resolveConfirmOnlyHours(QString(), 5), 8) ;   // 7.5 rounds to 8
    }

    void confirmOnlyUsesAValidValue()
    {
        QCOMPARE(resolveConfirmOnlyHours("48", 24), 48) ;
        QCOMPARE(resolveConfirmOnlyHours("36", 24), 36) ;
        QCOMPARE(resolveConfirmOnlyHours(" 18 ", 12), 18) ;
    }

    void confirmOnlyLessThanRemindUsesTheDefault()
    {
        QCOMPARE(resolveConfirmOnlyHours("20", 24), 36) ;
        QCOMPARE(resolveConfirmOnlyHours("11", 12), 18) ;
        QCOMPARE(resolveConfirmOnlyHours("-5", 24), 36) ;
    }

    void confirmOnlyNotANumberUsesTheDefault()
    {
        QCOMPARE(resolveConfirmOnlyHours("soon", 24), 36) ;
        QCOMPARE(resolveConfirmOnlyHours("36.5", 24), 36) ;
    }

    void remindHoursDefaultsAndLimits()
    {
        QCOMPARE(resolveRemindHours(QString()), 24) ;
        QCOMPARE(resolveRemindHours("12"), 12) ;
        QCOMPARE(resolveRemindHours("1"), 1) ;
        QCOMPARE(resolveRemindHours("0"), 24) ;
        QCOMPARE(resolveRemindHours("-3"), 24) ;
        QCOMPARE(resolveRemindHours("soon"), 24) ;
    }

    // ---- The hold, the send window and the same day ----

    void holdLastsWaitMinutes()
    {
        QVERIFY(isWithinHold(0, 5)) ;
        QVERIFY(isWithinHold(299, 5)) ;
        QVERIFY(!isWithinHold(300, 5)) ;
        QVERIFY(!isWithinHold(0, 0)) ;
    }

    void sendWindowIsStartInclusiveEndExclusive()
    {
        QTime start(7, 0) ;
        QTime end(22, 0) ;
        QVERIFY(!inSendWindow(QTime(6, 59, 59), start, end)) ;
        QVERIFY(inSendWindow(QTime(7, 0), start, end)) ;
        QVERIFY(inSendWindow(QTime(21, 59, 59), start, end)) ;
        QVERIFY(!inSendWindow(QTime(22, 0), start, end)) ;
    }

    void onTheDayUsesLocalDate()
    {
        QDateTime now = localAt(QDate(2026, 10, 30), QTime(10, 0)) ;
        QVERIFY(isOnTheDay(localAt(QDate(2026, 10, 30), QTime(23, 59)), now)) ;
        QVERIFY(!isOnTheDay(localAt(QDate(2026, 10, 31), QTime(0, 0)), now)) ;
        QVERIFY(!isOnTheDay(localAt(QDate(2026, 10, 29), QTime(23, 59)), now)) ;
    }

    void dayRecordKeepsTheChannelsOfTheDay()
    {
        QString record = dayChannelRecord(QString(), "2026-10-30", "email") ;
        QVERIFY(dayChannelSent(record, "2026-10-30", "email")) ;
        QVERIFY(!dayChannelSent(record, "2026-10-30", "sms")) ;
        record = dayChannelRecord(record, "2026-10-30", "sms") ;
        QVERIFY(dayChannelSent(record, "2026-10-30", "sms")) ;
        QVERIFY(dayChannelSent(record, "2026-10-30", "email")) ;
        // A record for another day does not count
        QVERIFY(!dayChannelSent(record, "2026-10-31", "email")) ;
        // A new day replaces the old record
        QCOMPARE(dayChannelRecord(record, "2026-10-31", "sms"), QString("2026-10-31 sms")) ;
    }

    // ---- The messages decided for an appointment ----

    void newAppointmentByEmailGetsAConfirmation()
    {
        QList<MessageStep> steps = decideMessages(newState(true, false)) ;
        QCOMPARE(describeSteps(steps), QString("confirm:email")) ;
        QVERIFY(!steps.first().recordreminder) ;
    }

    void newAppointmentByTextGetsAConfirmation()
    {
        QCOMPARE(describeSteps(decideMessages(newState(false, true))), QString("confirm:sms")) ;
    }

    void newAppointmentWithBothGetsEmailOnly()
    {
        // With both ticked, a confirmation or update goes by email, and text is not sent
        QCOMPARE(describeSteps(decideMessages(newState(true, true))), QString("confirm:email")) ;
    }

    void newAppointmentWithNoChannelGetsNothing()
    {
        QVERIFY(decideMessages(newState(false, false)).isEmpty()) ;
    }

    void confirmOnlyFoldsTheReminderIntoTheConfirmation()
    {
        MessageState s = newState(true, false) ;
        s.confirmonly = true ;
        QList<MessageStep> steps = decideMessages(s) ;
        QCOMPARE(describeSteps(steps), QString("confirm:email")) ;
        QVERIFY(steps.first().recordreminder) ;
    }

    void changedAppointmentGetsAnUpdateOnItsChannel()
    {
        MessageState e = sentState(true, false) ;
        e.changed = true ;
        QCOMPARE(describeSteps(decideMessages(e)), QString("update:email")) ;

        MessageState t = sentState(false, true) ;
        t.changed = true ;
        QCOMPARE(describeSteps(decideMessages(t)), QString("update:sms")) ;
    }

    void unchangedSentAppointmentGetsNothing()
    {
        QVERIFY(decideMessages(sentState(true, false)).isEmpty()) ;
        QVERIFY(decideMessages(sentState(false, true)).isEmpty()) ;
    }

    void deletedSentAppointmentGetsACancellationOnItsChannel()
    {
        MessageState e = sentState(true, false) ;
        e.deleted = true ;
        QCOMPARE(describeSteps(decideMessages(e)), QString("cancel:email")) ;

        MessageState t = sentState(false, true) ;
        t.deleted = true ;
        QCOMPARE(describeSteps(decideMessages(t)), QString("cancel:sms")) ;
    }

    void deletedUnsentAppointmentGetsNothing()
    {
        MessageState s = newState(true, false) ;
        s.deleted = true ;
        QVERIFY(decideMessages(s).isEmpty()) ;
    }

    void reminderOnceByTheChannelOfTheContact()
    {
        MessageState e = sentState(true, false) ;
        e.istime = true ;
        QCOMPARE(describeSteps(decideMessages(e)), QString("reminder:email")) ;

        MessageState t = sentState(false, true) ;
        t.istime = true ;
        QCOMPARE(describeSteps(decideMessages(t)), QString("reminder:sms")) ;
    }

    void reminderIsNotSentTwice()
    {
        MessageState e = sentState(true, false) ;
        e.istime = true ;
        e.remindersent = true ;
        QVERIFY(decideMessages(e).isEmpty()) ;
    }

    void reminderNotDueIsNotSent()
    {
        MessageState e = sentState(true, false) ;
        QVERIFY(decideMessages(e).isEmpty()) ;
    }

    void removedOccurrenceIsCancelledOnItsChannel()
    {
        MessageState e = sentState(true, false) ;
        e.isrepeating = true ;
        e.pendingcancel = localAt(QDate(2026, 11, 2), QTime(15, 0)) ;
        QCOMPARE(describeSteps(decideMessages(e)), QString("cancelocc:email")) ;

        MessageState t = sentState(false, true) ;
        t.isrepeating = true ;
        t.pendingcancel = localAt(QDate(2026, 11, 2), QTime(15, 0)) ;
        QCOMPARE(describeSteps(decideMessages(t)), QString("cancelocc:sms")) ;
    }

    void removedOccurrenceOnlyForARepeat()
    {
        MessageState e = sentState(true, false) ;
        e.pendingcancel = localAt(QDate(2026, 11, 2), QTime(15, 0)) ;
        QVERIFY(decideMessages(e).isEmpty()) ;
    }

    // ---- The on-the-day messages: both channels, once each, on the day ----

    void onTheDayBothChannelsAreSentOnTheDay()
    {
        MessageState s = sentState(true, true) ;
        s.onthisday = true ;
        s.remindersent = true ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("onday:email, onday:sms")) ;
    }

    void onTheDayTextIsSentWhenEmailHasGone()
    {
        MessageState s = sentState(true, true) ;
        s.onthisday = true ;
        s.remindersent = true ;
        s.emaildone = true ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("onday:sms")) ;
    }

    void onTheDayEmailIsSentWhenTextHasGone()
    {
        MessageState s = sentState(true, true) ;
        s.onthisday = true ;
        s.remindersent = true ;
        s.smsdone = true ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("onday:email")) ;
    }

    void onTheDayIsNotSentAgainOnceBothHaveGone()
    {
        MessageState s = sentState(true, true) ;
        s.onthisday = true ;
        s.remindersent = true ;
        s.emaildone = true ;
        s.smsdone = true ;
        QVERIFY(decideMessages(s).isEmpty()) ;
    }

    void onTheDayIsSentForASingleChannel()
    {
        // A contact with only one channel still gets its on-the-day message on that channel
        MessageState e = sentState(true, false) ;
        e.onthisday = true ;
        e.remindersent = true ;
        QCOMPARE(describeSteps(decideMessages(e)), QString("onday:email")) ;

        MessageState t = sentState(false, true) ;
        t.onthisday = true ;
        t.remindersent = true ;
        QCOMPARE(describeSteps(decideMessages(t)), QString("onday:sms")) ;
    }

    void onTheDayIsNotSentOnAnotherDay()
    {
        MessageState s = sentState(true, true) ;
        s.onthisday = false ;
        s.remindersent = true ;
        QVERIFY(decideMessages(s).isEmpty()) ;
    }

    void sameDayBookingGivesItsEmailAndThenTheOnTheDayText()
    {
        // Booked on the day, with both channels: the confirmation goes by email, which covers the email for that day.  The
        // on-the-day text is still sent, once, as the text was not given that day.
        QString record = dayChannelRecord(QString(), "2026-10-30", "email") ;
        MessageState booked = newState(true, true) ;
        booked.onthisday = true ;
        QCOMPARE(describeSteps(decideMessages(booked)), QString("confirm:email")) ;

        MessageState later = sentState(true, true) ;
        later.onthisday = true ;
        later.remindersent = true ;
        later.emaildone = dayChannelSent(record, "2026-10-30", "email") ;
        later.smsdone = dayChannelSent(record, "2026-10-30", "sms") ;
        QCOMPARE(describeSteps(decideMessages(later)), QString("onday:sms")) ;

        // Once the text has gone too, nothing more is sent that day
        later.smsdone = true ;
        QVERIFY(decideMessages(later).isEmpty()) ;
    }

    void sameDayTextBookingCoversTheDaysText()
    {
        // Booked on the day, by text only: the confirmation text gives the day's text, so no on-the-day text follows
        QString record = dayChannelRecord(QString(), "2026-10-30", "sms") ;
        MessageState booked = newState(false, true) ;
        booked.onthisday = true ;
        QCOMPARE(describeSteps(decideMessages(booked)), QString("confirm:sms")) ;

        MessageState later = sentState(false, true) ;
        later.onthisday = true ;
        later.remindersent = true ;
        later.smsdone = dayChannelSent(record, "2026-10-30", "sms") ;
        QVERIFY(decideMessages(later).isEmpty()) ;
    }

    void sameDaySmsIsSentWhenBookedEarlierAndTheDayComes()
    {
        // Booked the day before with both channels, so the confirmation and the reminder have gone.  On the day, the
        // on-the-day email and text both go, once each, email first.
        MessageState s = sentState(true, true) ;
        s.onthisday = true ;
        s.remindersent = true ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("onday:email, onday:sms")) ;
    }

    // ---- The cases in the messaging rules document, decided one message at a time ----

    void caseSevenEmailIsTheConfirmationWhenWindowOpensLate()
    {
        // Made at 23:30 the day before, for 12:00 on the day.  At 07:00 on the day, the email is the confirmation, and it
        // gives the day's email.
        MessageState s = newState(true, false) ;
        s.onthisday = true ;
        s.createdbeforeday = true ;
        s.istime = true ;
        s.confirmonly = true ;
        QList<MessageStep> steps = decideMessages(s) ;
        QCOMPARE(describeSteps(steps), QString("confirm:email")) ;
        QVERIFY(steps.first().recordreminder) ;

        s.messagesent = true ;
        s.emaildone = true ;
        s.remindersent = true ;
        QVERIFY(decideMessages(s).isEmpty()) ;
    }

    void caseSevenTextIsTheOnTheDayTextWhenWindowOpensLate()
    {
        // The same appointment, for a text-only contact: no confirmation text, but the on-the-day text at 07:00
        MessageState s = newState(false, true) ;
        s.onthisday = true ;
        s.createdbeforeday = true ;
        s.istime = true ;
        s.confirmonly = true ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("onday:sms")) ;
    }

    void caseSevenBothIsEmailThenTheOnTheDayText()
    {
        // The same appointment, with both channels: the confirmation email first, then the on-the-day text
        MessageState s = newState(true, true) ;
        s.onthisday = true ;
        s.createdbeforeday = true ;
        s.istime = true ;
        s.confirmonly = true ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("confirm:email")) ;

        s.messagesent = true ;
        s.emaildone = true ;
        s.remindersent = true ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("onday:sms")) ;
    }

    void caseEightNoReminderOnTheDay()
    {
        // The reminder that was due the day before, while ContactManager was off, is not sent on the day: the on-the-day
        // messages take its place
        MessageState s = sentState(true, true) ;
        s.onthisday = true ;
        s.istime = true ;
        s.remindersent = false ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("onday:email, onday:sms")) ;
    }

    void reminderOnTheDayIsReplacedByTheOnTheDayText()
    {
        MessageState s = sentState(false, true) ;
        s.onthisday = true ;
        s.istime = true ;
        s.remindersent = false ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("onday:sms")) ;
    }

    void channelSwitchedToTextSendsOnlyWhatIsPending()
    {
        // Confirmed by email, and then switched to text only: nothing is sent again, but the reminder by text is pending
        MessageState s = sentState(false, true) ;
        s.istime = true ;
        s.remindersent = false ;
        QCOMPARE(describeSteps(decideMessages(s)), QString("reminder:sms")) ;

        MessageState none = sentState(false, true) ;
        QVERIFY(decideMessages(none).isEmpty()) ;
    }

    // ---- The timing and the messages together, at the 24 and 36 hour boundaries ----

    void scenarioCreatedThirtySixHoursAheadBothChannels()
    {
        // Created at exactly 36 hours: no confirm-only, so the confirmation carries no reminder.  At 24 hours the reminder
        // is due, and it goes by email (both channels: email only), and the on-the-day email and text follow on the day.
        QDateTime occ = localAt(QDate(2026, 11, 1), QTime(22, 0)) ;
        QDateTime created = occ.addSecs(-36 * 3600) ;

        MessageState c = newState(true, true) ;
        c.confirmonly = isConfirmOnly(occ, created, 36) ;
        c.istime = isReminderDue(occ, created, 24) ;
        QCOMPARE(describeSteps(decideMessages(c)), QString("confirm:email")) ;
        QVERIFY(!decideMessages(c).first().recordreminder) ;

        QDateTime atReminder = occ.addSecs(-24 * 3600 + 1) ;
        MessageState r = sentState(true, true) ;
        r.istime = isReminderDue(occ, atReminder, 24) ;
        QCOMPARE(describeSteps(decideMessages(r)), QString("reminder:email")) ;
    }

    void scenarioCreatedTwentyFiveHoursAheadBothChannels()
    {
        // Created 25 hours ahead: confirm-only, so the confirmation is the reminder.  There is no separate reminder, and on
        // the day the on-the-day email and text go.
        QDateTime occ = localAt(QDate(2026, 10, 31), QTime(16, 0)) ;
        QDateTime created = occ.addSecs(-25 * 3600) ;
        MessageState c = newState(true, true) ;
        c.confirmonly = isConfirmOnly(occ, created, 36) ;
        QVERIFY(c.confirmonly) ;
        QCOMPARE(describeSteps(decideMessages(c)), QString("confirm:email")) ;
        QVERIFY(decideMessages(c).first().recordreminder) ;

        MessageState sent = sentState(true, true) ;
        sent.remindersent = true ;
        sent.istime = isReminderDue(occ, localAt(QDate(2026, 10, 30), QTime(16, 0, 1)), 24) ;
        sent.onthisday = isOnTheDay(occ, localAt(QDate(2026, 10, 31), QTime(7, 0))) ;
        QCOMPARE(describeSteps(decideMessages(sent)), QString("onday:email, onday:sms")) ;
    }

    void scenarioBookedOnTheDayBothChannels()
    {
        // Created on the day: one email, and no on-the-day messages after it
        QDateTime occ = localAt(QDate(2026, 10, 30), QTime(15, 0)) ;
        QDateTime created = localAt(QDate(2026, 10, 30), QTime(8, 0)) ;
        MessageState c = newState(true, true) ;
        c.confirmonly = isConfirmOnly(occ, created, 36) ;
        c.onthisday = isOnTheDay(occ, created) ;
        QCOMPARE(describeSteps(decideMessages(c)), QString("confirm:email")) ;
        QVERIFY(decideMessages(c).first().recordreminder) ;
    }

    // ---- Appointments: the occurrences the calendar lists ----

    static Appointment dailyRepeat(QString id, int count)
    {
        Appointment a ;
        a.setField(Appointment::ID, id) ;
        a.setField(Appointment::ContactId, "contact-1") ;
        a.setField(Appointment::Summary, "Repeat") ;
        a.setDate(Appointment::From, QDateTime(QDate(2026, 10, 30), QTime(16, 0), Qt::UTC)) ;
        a.setDate(Appointment::To, QDateTime(QDate(2026, 10, 30), QTime(17, 0), Qt::UTC)) ;
        a.setField(Appointment::Repeat, "1") ;
        a.setField(Appointment::RepeatInterval, "1") ;
        a.setField(Appointment::RepeatCount, QString::number(count)) ;
        return a ;
    }

    static Appointment oneOff(QString id, QDate day)
    {
        Appointment a ;
        a.setField(Appointment::ID, id) ;
        a.setField(Appointment::ContactId, "contact-1") ;
        a.setField(Appointment::Summary, "Single") ;
        a.setDate(Appointment::From, QDateTime(day, QTime(16, 0), Qt::UTC)) ;
        a.setDate(Appointment::To, QDateTime(day, QTime(17, 0), Qt::UTC)) ;
        return a ;
    }

    void listedOccurrencesOfARepeat()
    {
        Appointment r = dailyRepeat("r", 3) ;
        QList<QPair<QDateTime,QDateTime>> all = r.listedOccurrences(QDateTime(QDate(2026, 10, 29), QTime(0, 0), Qt::UTC),
                                                                    QDateTime(QDate(2026, 11, 10), QTime(0, 0), Qt::UTC)) ;
        QCOMPARE(all.size(), 3) ;
    }

    void removedOccurrenceIsNotListed()
    {
        Appointment r = dailyRepeat("r", 3) ;
        r.addException(QDateTime(QDate(2026, 10, 30), QTime(16, 0), Qt::UTC)) ;
        QList<QPair<QDateTime,QDateTime>> listed = r.listedOccurrences(QDateTime(QDate(2026, 10, 29), QTime(0, 0), Qt::UTC),
                                                                       QDateTime(QDate(2026, 11, 10), QTime(0, 0), Qt::UTC)) ;
        QCOMPARE(listed.size(), 2) ;
        QCOMPARE(listed.first().first.date(), QDate(2026, 10, 31)) ;
    }

    void repeatWithEveryOccurrenceRemovedListsNothing()
    {
        Appointment r = dailyRepeat("r", 2) ;
        r.addException(QDateTime(QDate(2026, 10, 30), QTime(16, 0), Qt::UTC)) ;
        r.addException(QDateTime(QDate(2026, 10, 31), QTime(16, 0), Qt::UTC)) ;
        QVERIFY(r.listedOccurrences(QDateTime(QDate(2026, 10, 29), QTime(0, 0), Qt::UTC),
                                    QDateTime(QDate(2026, 11, 10), QTime(0, 0), Qt::UTC)).isEmpty()) ;
    }

    void periodsTouchingAtAnEndDoNotClash()
    {
        // 17:00 to 18:00 against 16:00 to 17:00 meet at 17:00, which is not a clash
        QDateTime t16 = QDateTime(QDate(2026, 10, 30), QTime(16, 0), Qt::UTC) ;
        QDateTime t17 = QDateTime(QDate(2026, 10, 30), QTime(17, 0), Qt::UTC) ;
        QDateTime t18 = QDateTime(QDate(2026, 10, 30), QTime(18, 0), Qt::UTC) ;
        QVERIFY(!Appointment::periodsClash(t17, t18, t16, t17)) ;
        QVERIFY(Appointment::periodsClash(t16, t17, t16, t17)) ;
        QVERIFY(Appointment::periodsClash(QDateTime(QDate(2026, 10, 30), QTime(16, 30), Qt::UTC), t18, t16, t17)) ;
    }

    void clashIgnoresARemovedOccurrence()
    {
        Appointment r = dailyRepeat("rep", 3) ;
        r.addException(QDateTime(QDate(2026, 10, 30), QTime(16, 0), Qt::UTC)) ;
        Calendar cal ;
        Appointment rr = r ;
        cal.addAppointment(rr) ;
        Appointment single = oneOff("single", QDate(2026, 10, 30)) ;
        QVERIFY(cal.getAppointmentClash(single).isNull()) ;
    }

    void clashWithAListedOccurrence()
    {
        Appointment r = dailyRepeat("rep", 3) ;
        Calendar cal ;
        Appointment rr = r ;
        cal.addAppointment(rr) ;
        Appointment single = oneOff("single", QDate(2026, 10, 30)) ;
        QVERIFY(!cal.getAppointmentClash(single).isNull()) ;
    }

    void clashIsNotReportedForAnEmptyRepeat()
    {
        Appointment r = dailyRepeat("rep", 2) ;
        r.addException(QDateTime(QDate(2026, 10, 30), QTime(16, 0), Qt::UTC)) ;
        r.addException(QDateTime(QDate(2026, 10, 31), QTime(16, 0), Qt::UTC)) ;
        Calendar cal ;
        Appointment rr = r ;
        cal.addAppointment(rr) ;
        Appointment single = oneOff("single", QDate(2026, 10, 30)) ;   // a time the empty repeat used to have
        QVERIFY(cal.getAppointmentClash(single).isNull()) ;
    }

    void getDateGivesACopyThatStaysTheSame()
    {
        // Regression: getDate once returned a shared value, which a later read overwrote
        Appointment a = oneOff("a", QDate(2026, 10, 30)) ;
        Appointment b = oneOff("b", QDate(2026, 11, 2)) ;
        QDateTime first = a.getDate(Appointment::From) ;
        QDateTime second = b.getDate(Appointment::From) ;
        QCOMPARE(first, QDateTime(QDate(2026, 10, 30), QTime(16, 0), Qt::UTC)) ;
        QCOMPARE(second, QDateTime(QDate(2026, 11, 2), QTime(16, 0), Qt::UTC)) ;
    }

    void sentStateDoesNotCountAsAnEdit()
    {
        Appointment a = oneOff("a", QDate(2026, 10, 30)) ;
        a.setField(Appointment::Updated, "2026-10-30T09:00:00Z") ;
        a.setSentField(Appointment::MessageSent, "2026-10-30T10:00:00Z") ;
        QCOMPARE(a.getField(Appointment::Updated), QString("2026-10-30T09:00:00Z")) ;
        QCOMPARE(a.getField(Appointment::MessageSent), QString("2026-10-30T10:00:00Z")) ;
    }

    void googleRecordCopyDoesNotRestartTheWait()
    {
        // After an upload, the Google record ID is copied in.  That must not move Updated, or the hold after the last edit
        // would start again at the upload.  A plain copy does move it, which is what the test shows.
        Appointment source = oneOff("a", QDate(2026, 10, 30)) ;
        source.setField(Appointment::GoogleRecordId, "google-event-1") ;

        Appointment kept = oneOff("a", QDate(2026, 10, 30)) ;
        kept.setField(Appointment::Updated, "2026-10-30T09:00:00Z") ;
        kept.copyFromKeepingUpdated(source, Appointment::maDetails|Appointment::maGoogleAcct) ;
        QCOMPARE(kept.getField(Appointment::GoogleRecordId), QString("google-event-1")) ;
        QCOMPARE(kept.getField(Appointment::Updated), QString("2026-10-30T09:00:00Z")) ;

        Appointment plain = oneOff("a", QDate(2026, 10, 30)) ;
        plain.setField(Appointment::Updated, "2026-10-30T09:00:00Z") ;
        source.copyTo(plain, Appointment::maDetails|Appointment::maGoogleAcct) ;
        QVERIFY(plain.getField(Appointment::Updated) != QString("2026-10-30T09:00:00Z")) ;
    }

    void contactLinkIsKeptWhenTheContactExists()
    {
        // Issue #95: a link to a contact that exists (hidden or deleted included) is kept
        QSet<QString> ids ;
        ids << "contact-1" << "contact-2" ;
        QHash<QString, QStringList> byname ;
        QString resolved ;
        QCOMPARE(matchAppointmentContact("contact-2", "", ids, byname, resolved), ContactKept) ;
        QCOMPARE(resolved, QString("contact-2")) ;
    }

    void contactLinkIsSetByAnExactNameMatch()
    {
        // Issue #95: no contact has the link, and the For name matches exactly one contact, ignoring case
        QSet<QString> ids ;
        ids << "contact-1" ;
        QHash<QString, QStringList> byname ;
        byname["gitte lowther-dane"] = QStringList() << "contact-9" ;
        QString resolved ;
        QCOMPARE(matchAppointmentContact("gone", "Gitte Lowther-Dane", ids, byname, resolved), ContactNamed) ;
        QCOMPARE(resolved, QString("contact-9")) ;
    }

    void contactLinkIsRemovedWhenTheNameMatchesMoreThanOne()
    {
        // Issue #95: two contacts with that name, so the link is not guessed
        QSet<QString> ids ;
        QHash<QString, QStringList> byname ;
        byname["sam jones"] = QStringList() << "contact-3" << "contact-4" ;
        QString resolved ;
        QCOMPARE(matchAppointmentContact("gone", "Sam Jones", ids, byname, resolved), ContactUnnamed) ;
        QVERIFY(resolved.isEmpty()) ;
    }

    void contactLinkIsRemovedWhenNoNameMatches()
    {
        // Issue #95: no contact has the link or the name, including a blank For name
        QSet<QString> ids ;
        QHash<QString, QStringList> byname ;
        QString resolved ;
        QCOMPARE(matchAppointmentContact("gone", "Unnamed Contact", ids, byname, resolved), ContactUnnamed) ;
        QVERIFY(resolved.isEmpty()) ;
        QCOMPARE(matchAppointmentContact("gone", "", ids, byname, resolved), ContactUnnamed) ;
    }

    void loadKeepsTheSavedUpdatedDate()
    {
        // Loading an appointment is not an edit: its saved Updated and DateUpdated are kept, so the hold and the sync do not
        // treat every appointment as changed each time the program starts
        QTemporaryDir dir ;
        QVERIFY(dir.isValid()) ;
        QString path = dir.path() + "/" ;
        QFile file(path + "loaded.appointment") ;
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text)) ;
        QTextStream out(&file) ;
        out << "[appointment]\n"
            << "ID=loaded\n"
            << "ContactId=contact-1\n"
            << "Summary=Saved\n"
            << "From=2026-11-01T13:15:00Z\n"
            << "To=2026-11-01T14:15:00Z\n"
            << "createddate=2026-10-20T10:00:00Z\n"
            << "updateddate=2026-10-20T10:05:00Z\n"
            << "dateupdateddate=2026-10-20T10:00:00Z\n"
            << "messagesentdate=2026-10-20T10:06:00Z\n" ;
        file.close() ;

        Appointment appt ;
        QVERIFY(appt.load(path, "loaded")) ;
        QCOMPARE(appt.getDate(Appointment::Updated), QDateTime(QDate(2026, 10, 20), QTime(10, 5), Qt::UTC)) ;
        QCOMPARE(appt.getDate(Appointment::DateUpdated), QDateTime(QDate(2026, 10, 20), QTime(10, 0), Qt::UTC)) ;
    }

    void mergedSentStateDoesNotCountAsAnEdit()
    {
        Appointment a = oneOff("a", QDate(2026, 10, 30)) ;
        a.setField(Appointment::Updated, "2026-10-30T09:00:00Z") ;
        Appointment remote = oneOff("a", QDate(2026, 10, 30)) ;
        remote.setField(Appointment::ReminderFor, "2026-10-30T16:00:00Z") ;
        QVERIFY(a.mergeSentState(remote)) ;
        QCOMPARE(a.getField(Appointment::Updated), QString("2026-10-30T09:00:00Z")) ;
        QCOMPARE(a.getField(Appointment::ReminderFor), QString("2026-10-30T16:00:00Z")) ;
    }
} ;

QTEST_MAIN(TestMessages)
#include "tst_messages.moc"
