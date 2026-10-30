#include "messagedecision.h"
#include <QtGlobal>
#include <QStringList>

QList<MessageStep> decideMessages(const MessageState &s)
{
    QList<MessageStep> steps ;

    // A step of the given kind, on one channel
    auto add = [&](QString kind, bool email, bool recordreminder, QDateTime occurrence) {
        MessageStep step ;
        step.kind = kind ;
        step.email = email ;
        step.occurrence = occurrence ;
        step.recordreminder = recordreminder ;
        steps << step ;
    } ;

    // A confirmation or update, a cancellation, or a reminder.  A contact with both email and text is sent email for these.
    if (s.emailme && !s.deleted && !s.messagesent) {
        add("confirm", true, s.confirmonly, s.occurrence) ;
    }
    else if (s.textme && !s.emailme && !s.deleted && !s.messagesent) {
        add("confirm", false, s.confirmonly, s.occurrence) ;
    }
    else if (s.emailme && !s.deleted && s.changed) {
        add("update", true, s.confirmonly, s.occurrence) ;
    }
    else if (s.textme && !s.emailme && !s.deleted && s.changed) {
        add("update", false, s.confirmonly, s.occurrence) ;
    }
    else if (s.emailme && s.deleted && s.messagesent) {
        add("cancel", true, false, s.occurrence) ;
    }
    else if (s.textme && !s.emailme && s.deleted && s.messagesent) {
        add("cancel", false, false, s.occurrence) ;
    }
    else if (s.textme && !s.emailme && !s.deleted && s.istime && !s.remindersent) {
        add("reminder", false, true, s.occurrence) ;
    }
    else if (s.emailme && !s.deleted && s.istime && !s.remindersent) {
        add("reminder", true, true, s.occurrence) ;
    }

    // An occurrence removed from a repeat is cancelled, once, for that occurrence
    if (steps.isEmpty() && s.messagesent && !s.deleted && s.isrepeating && s.pendingcancel.isValid()) {
        add("cancelocc", s.emailme, false, s.pendingcancel) ;
    }

    // The on-the-day reminder: a contact with both email and text gets both, on the day of the appointment, each once
    if (steps.isEmpty() && s.emailme && s.textme && !s.deleted && !(s.emaildone && s.smsdone) && s.onthisday) {
        if (!s.emaildone) add("onday", true, false, s.occurrence) ;
        if (!s.smsdone) add("onday", false, false, s.occurrence) ;
    }

    return steps ;
}

bool isReminderDue(const QDateTime &occurrence, const QDateTime &now, int remindhours)
{
    return occurrence < now.addSecs((qint64)remindhours * 3600) ;
}

bool isConfirmOnly(const QDateTime &occurrence, const QDateTime &now, int confirmhours)
{
    return occurrence < now.addSecs((qint64)confirmhours * 3600) ;
}

bool isOnTheDay(const QDateTime &occurrence, const QDateTime &now)
{
    return occurrence.toLocalTime().date()==now.toLocalTime().date() ;
}

bool isWithinHold(qint64 secondsSinceChange, int waitminutes)
{
    return secondsSinceChange < (qint64)waitminutes * 60 ;
}

bool inSendWindow(QTime localtime, QTime start, QTime end)
{
    return !(localtime<start || localtime>=end) ;
}

// A whole number of hours from the text, or the default if it is missing or not a number
static bool wholeNumber(const QString &text, int &number)
{
    bool ok = false ;
    number = text.trimmed().toInt(&ok) ;
    return ok ;
}

int resolveRemindHours(const QString &text)
{
    int hours = 24 ;
    if (wholeNumber(text, hours)) return hours>=1 ? hours : 24 ;
    return 24 ;
}

int resolveConfirmOnlyHours(const QString &text, int remindhours)
{
    int defaulthours = qRound(remindhours * 1.5) ;
    if (text.trimmed().isEmpty()) return defaulthours ;

    int hours = 0 ;
    if (!wholeNumber(text, hours) || hours<remindhours) return defaulthours ;
    return hours ;
}

bool dayChannelSent(const QString &record, const QString &day, const QString &channel)
{
    QStringList parts = record.split(' ', Qt::SkipEmptyParts) ;
    return !parts.isEmpty() && parts.at(0)==day && parts.contains(channel) ;
}

QString dayChannelRecord(const QString &record, const QString &day, const QString &channel)
{
    QStringList parts = record.split(' ', Qt::SkipEmptyParts) ;
    if (parts.isEmpty() || parts.at(0)!=day) parts = QStringList() << day ;
    if (!parts.contains(channel)) parts.append(channel) ;
    return parts.join(' ') ;
}
