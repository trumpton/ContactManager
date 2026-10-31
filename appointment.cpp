#include <algorithm>
#include "appointment.h"
#include <QDateTime>
#include <QUuid>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QTextDecoder>
#include <QRegularExpression>
#include "../Lib/supportfunctions.h"

// Bug Observation: QDateTime.SetDate() Only works if the QDateTime item is set to Utc.
// e.g. qDateTimeGmt.setDate(QDate::CurrentDate()) sets the qDateTimeGmt to "invalid"

Appointment::Appointment()
{
    // Check Params
    appointmentrecordinfook=true ;
    for (int i=0; i<NumberOfRecords; i++) {
        if (appointmentrecordinfo[i].recordtype!=(Appointment::AppointmentRecord)i) {
            appointmentrecordinfook=false ;
        }
    }

    isnull=false ;
    createNew() ;
    asTextResponse = "" ;
    asHTMLResponse = "" ;
    getDateResponse.setMSecsSinceEpoch(0);
}

Appointment::Appointment(const Appointment &rhs)
{
    isnull = false ;
    *this = rhs ;
}

Appointment& Appointment::operator=(const Appointment &rhs)
{
    if (this == &rhs || (isnull && rhs.isnull)) return *this;

    if (isnull) {
        qFatal("ERROR: Copying into NULL appointment entry not allowed") ;
        return *this ;
    }

    isnull = rhs.isnull ;
    isempty = rhs.isempty ;
    isdirty = rhs.isdirty ;
    istemp = rhs.istemp ;
    shortTextResponse = rhs.shortTextResponse ;
    asTextResponse = rhs.asTextResponse ;
    asHTMLResponse = rhs.asHTMLResponse ;
    getDateResponse = rhs.getDateResponse ;
    for (int i=0; i<NumberOfRecords; i++) {
        filedata[i] = rhs.filedata[i] ;
    }
    return *this ;
}

// forces record to be a read-only null one
void Appointment::setNull()
{
    createNew() ;
    filedata[ID] = "00000000-0000-0000-0000-000000000000" ;
    isnull=true ;
}

void Appointment::setId(QString newid)
{
    filedata[ID] = newid ;
}

Appointment::~Appointment()
{
    if (isdirty) {
        // TODO: Error - Save should be called before destructor
    }
}

bool Appointment::isNull()
{
    return isnull ;
}

bool Appointment::isEmpty()
{
    return isempty ;
}

bool Appointment::isDirty()
{
    return isdirty ;
}

bool Appointment::isTemp()
{
    return istemp ;
}

// Birthdays are shown from the contacts list, so a birthday entry is one whose summary says so
bool Appointment::isBirthday()
{
    return getField(Appointment::Summary).contains("birthday", Qt::CaseInsensitive) ;
}

void Appointment::markAsSaved()
{
    isdirty=false ;
}

void Appointment::markAsDirty()
{
    isdirty=true ;
}

void Appointment::markAsTemp()
{
    istemp=true ;
}

QString& Appointment::getId()
{
    return filedata[Appointment::ID] ;
}

QString& Appointment::getContactId()
{
    return filedata[Appointment::ContactId] ;
}

void Appointment::setContactId(QString id)
{
    setField(Appointment::ContactId, id) ;
}

QString& Appointment::getGoogleRecordId()
{
    return filedata[Appointment::GoogleRecordId] ;
}

bool Appointment::load(QString path, QString idname)
{
    if (isnull) {

        return false ;

    } else {

        QString Line ;
        QStringList ParsedLine;
        QRegularExpression sep("(\\=)");

        createNew() ;
        isdirty = false ;
        QString fullPath = path + idname + ".appointment" ;
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8) ;

        // The saved times are kept for after the loop: setField stamps the appointment with the time it is loaded
        QString loadedUpdated ;
        QString loadedDateUpdated ;

        while (!in.atEnd()) {
            Line = in.readLine();
            ParsedLine =  Line.split(sep);

            // TODO: Handle cases where there is an "=" in the middle of the string

            if (ParsedLine.count()==2) {

                for (int i=0; i<Appointment::NumberOfRecords; i++) {

                    if (ParsedLine.at(0).compare(appointmentrecordinfo[i].name)==0) {
                        QString entrytext = ParsedLine.at(1) ;
                        entrytext = entrytext.replace("\\n","\n").replace("\r","") ;
                        if (i==Appointment::Updated) loadedUpdated = entrytext.trimmed() ;
                        if (i==Appointment::DateUpdated) loadedDateUpdated = entrytext.trimmed() ;
                        setField((enum AppointmentRecord)i, entrytext) ;
                    }

                }
            }
        }
        file.close() ;

        // Loading is not an edit, so the times are the ones that were saved (empty for a file that has none)
        filedata[Appointment::Updated] = loadedUpdated.isEmpty() ? QString() : dateTimeStringToZulu(loadedUpdated) ;
        filedata[Appointment::DateUpdated] = loadedDateUpdated.isEmpty() ? QString() : dateTimeStringToZulu(loadedDateUpdated) ;

        isdirty = false ;
        return true ;

    }
}

bool Appointment::save(QString path)
{
    if (isdirty && !isempty && !isnull && !istemp) {

       QString fullPath = path + filedata[Appointment::ID] + ".appointment" ;
       QFile file(fullPath);
       if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
           return false;
       QTextStream out(&file);
       out.setEncoding(QStringConverter::Utf8) ;

       out << "[appointment]\n" ;

       for (int entry=0 ; entry < NumberOfRecords; entry++) {
           if (appointmentrecordinfo[entry].issaved) {
               QString entrytext = getField((enum Appointment::AppointmentRecord) entry).trimmed().replace("\n","\\n") ;
               out << appointmentrecordinfo[entry].name << "=" << entrytext.trimmed() << "\n" ;
           }
       }

       file.close() ;
       isdirty = false ;
    }
    return true ;
}

bool Appointment::isSet(enum Appointment::AppointmentRecord field) const
{
    if (field>=0 && field< NumberOfRecords &&
            filedata[field].compare("true")==0) return true ;
    return false;
}


void Appointment::setSentField(enum Appointment::AppointmentRecord field, QString data)
{
    QString updated = filedata[Appointment::Updated] ;
    QString dateupdated = filedata[Appointment::DateUpdated] ;
    setField(field, data) ;
    filedata[Appointment::Updated] = updated ;
    filedata[Appointment::DateUpdated] = dateupdated ;
}

void Appointment::setFlag(enum Appointment::AppointmentRecord field, bool flag)
{
    if (flag)  {
        setField(field, "true") ;
    }else {
        setField(field, "false") ;
    }
}


void Appointment::setDate(enum AppointmentRecord field, QDateTime data)
{
    QString datestr = dateTimeToIsoString(data) ;
    setField(field, datestr) ;
}

QDateTime Appointment::getDate(enum AppointmentRecord field) const
{
    // A copy is taken: isoStringToDateTime returns a shared value, which the next call overwrites
    return QDateTime(isoStringToDateTime(getField(field))) ;
}

QString& Appointment::getField(enum Appointment::AppointmentRecord field) const
{
    static QString nullstring ;
    if (field>=0 && field<NumberOfRecords) { return (QString&)filedata[field] ; }
    return nullstring ;
}


void Appointment::setField(enum Appointment::AppointmentRecord field, QString data)
{
    if (isnull) return ;

    QString dat ;
    dat = data.replace("\r"," ").trimmed() ;

    //
    // Safety checks for poor programming
    //

    if (field<0 || field>=NumberOfRecords) {
        warningOkDialog(0, "ERROR", "Appointment set FILE field invalid", true) ;
        return ;
    }

    //
    // Correct / normalise time entries
    //

    if (!dat.isEmpty() &&
        (  field == Appointment::From || field == Appointment::To ||
          field == Appointment::Created || field == Appointment::Updated ) )
    {
        dat = dateTimeStringToZulu(dat) ;
    }

    //
    // Set field data, and update dirty / empty flags as required
    //

    if (field>=0 && field<NumberOfRecords &&
            dat.compare(filedata[field])!=0) {

        // Set updated and fromupdated times
        if (appointmentrecordinfo[field].issaved) {
            QString now = nowToIsoString() ;
            filedata[Appointment::Updated] = now ;
            if (field==Appointment::From || field==Appointment::To) filedata[Appointment::DateUpdated] = now ;
            isdirty = true ;
        }

        // Now set the data
        filedata[field]=dat ;
        isempty = false ;

    }

    // TODO: This line is for debug purposes, so the summary can be seen in the debugger
    // asText() ;

    isnull = false ;
}


void Appointment::createNew()
{
    if (isnull) return ;

    for (int i=(int)0; i<(int)NumberOfRecords; i++) {
        if (appointmentrecordinfo[i].isflag) {
            setFlag((enum Appointment::AppointmentRecord)i, false) ;
        } else {
            setField((enum Appointment::AppointmentRecord)i, "") ;
        }
    }

    QUuid qid = QUuid::createUuid() ;
    if (qid.isNull()) {
        // Error
    }
    QString id=qid.toString();
    id.replace("{","") ;
    id.replace("}","") ;
    setField(Appointment::ID, id) ;

    // Set Repeat
    setField(Appointment::Repeat, "0") ;
    setField(Appointment::RepeatInterval, "0") ;

    // Set a default date/time to 1st January 1970
    QDateTime t = QDateTime::fromSecsSinceEpoch(0) ;
    t.setTimeSpec(Qt::UTC) ;
    setDate(Appointment::From, t) ;
    setDate(Appointment::To, t) ;
    setDate(Appointment::Updated, t) ;

    // Set default dates
    setField(Appointment::Created, nowToIsoString()) ;

    isempty = true ;
    isnull = false ;
    isdirty = false ;
    istemp = false ;
}

QString& Appointment::asAccessibleText(QString name)
{
    asAccessibleTextResponse = asText(name, "", "", "", true) ;
    return asAccessibleTextResponse ;
}

// The accessible version leaves out the year for dates in the current year, which keeps the
// text a screen reader reads for each calendar entry short
QString& Appointment::asText(QString name, QString start, QString mid, QString end, bool accessible)
{
    Q_UNUSED(mid) ;
    QDateTime from = getDate(Appointment::From) ;
    QDateTime to = getDate(Appointment::To) ;
    int thisyear = QDate::currentDate().year() ;
    QString fromformat = (accessible && from.date().year()==thisyear) ? "dd MMM " : "dd MMM yyyy " ;
    QString tostampformat = (accessible && to.date().year()==thisyear) ? "dd MMM hh:mm" : "dd MMM yyyy hh:mm" ;

    if (getField(Appointment::From).isEmpty()) {

        asTextResponse = start + "(purged record)" + end ;

    } else {

        asTextResponse = start ;
        asTextResponse += from.toString(fromformat) ;

        if (from == to) {

            int h = from.time().hour() ;
            int m = from.time().minute() ;
            int s = from.time().second() ;

            if (h==0 && m==0 && s==0) {
                // Midnight - assume whole day event
            } else {
                // Reminder at specific time
                asTextResponse+= from.toLocalTime().toString("hh:mm") ;
            }

        } else if (from.date() == to.date()) {
            // Appointment all on same day
            asTextResponse+= from.toLocalTime().toString("hh:mm") + " - " + to.toLocalTime().toString("hh:mm") ;
        } else {
            // Appointment crossing two days
            asTextResponse += from.toLocalTime().toString("hh:mm") + " - " + to.toLocalTime().toString(tostampformat) ;
        }

        asTextResponse += ": " ;

        if (!name.isEmpty()) {
            asTextResponse = asTextResponse + name + " - " ;
        }
        asTextResponse += getField(Appointment::Summary).replace("\n", ". ") ;

        if (isSet(Appointment::Deleted)) {
            asTextResponse = asTextResponse + " (deleted)" ;
        }
        asTextResponse += end ;

    }

    return asTextResponse ;
}

QString& Appointment::shortText()
{
    QDateTime from = getDate(Appointment::From) ;
    QDateTime to = getDate(Appointment::To) ;

    shortTextResponse = from.toLocalTime().toString("dd-MMM") ;
    shortTextResponse += " at " ;
    shortTextResponse += to.toLocalTime().toString("hh:mm") ;
    return shortTextResponse ;
}

QString& Appointment::asHTML(QString name)
{
    return asText(name,
          "<span style=\" font-weight:600;\">",
          "</span><span style=\" color:#0000ff;\">",
          "</span>") ;
}

bool Appointment::isNotFor(QString& id)
{
    QString& apptfor = getField(Appointment::ContactId) ;
    return (apptfor.compare(id)!=0) ;
}

bool Appointment::isInFuture()
{
    QDateTime now ;
    now = QDateTime::currentDateTimeUtc() ;
    return (getDate(Appointment::From) > now) ;
}

bool Appointment::isInRange(qint64 beforedays, qint64 afterdays)
{
    QDateTime now, before, after, to, from ;
    bool inrange ;

    now = QDateTime::currentDateTimeUtc() ;
    before = now.addDays(beforedays * -1) ;
    after = now.addDays(afterdays) ;
    to = getDate(Appointment::To) ;
    from = getDate(Appointment::From) ;
    inrange = (to > before && from < after) ;
    return inrange ;
}

bool Appointment::isRepeating()
{
    int type = getField(Appointment::Repeat).toInt() ;
    return (type>=1 && type<=4) ;
}

QTimeZone Appointment::repeatTimeZone()
{
    QString id = getField(Appointment::RepeatZone) ;
    if (id.isEmpty()) return QTimeZone::systemTimeZone() ;
    QTimeZone zone(id.toUtf8()) ;
    return zone.isValid() ? zone : QTimeZone::systemTimeZone() ;
}

QDateTime Appointment::wallClockToUtc(QDateTime local, QTimeZone zone)
{
    QDateTime when(local.date(), local.time(), zone) ;
    if (!when.isValid()) when = QDateTime(local.date(), local.time().addSecs(3600), zone) ;
    return when.toUTC() ;
}

// The start of occurrence n of this appointment, where occurrence 0 is this appointment.  Each occurrence is n steps of
// the repeat (every RepeatInterval days, weeks, months or years) after this one.  The steps are taken on the clock in the
// repeat's zone, so the occurrence keeps its clock time when the clocks change.
QDateTime Appointment::occurrenceFrom(int n)
{
    QDateTime start = getDate(Appointment::From) ;
    int every = qMax(1, getField(Appointment::RepeatInterval).toInt()) ;
    int steps = n * every ;
    QTimeZone zone = repeatTimeZone() ;
    QDateTime next = start.toTimeZone(zone) ;
    switch (getField(Appointment::Repeat).toInt()) {
        case 1: next = next.addDays(steps) ; break ;
        case 2: next = next.addDays(7*steps) ; break ;
        case 3: next = next.addMonths(steps) ; break ;
        case 4: next = next.addYears(steps) ; break ;
        default: return start ;
    }
    return wallClockToUtc(next, zone) ;
}

bool Appointment::occurrenceEnded(int n)
{
    int count = getField(Appointment::RepeatCount).toInt() ;
    if (count>0 && n>=count) return true ;

    QString until = getField(Appointment::RepeatUntil) ;
    if (!until.isEmpty()) {
        QDateTime last = QDateTime::fromString(until, Qt::ISODate) ;
        if (occurrenceFrom(n) > last) return true ;
    }
    return false ;
}

bool Appointment::occurrenceExists(int n)
{
    return !occurrenceEnded(n) && !isException(occurrenceFrom(n)) ;
}

bool Appointment::isException(QDateTime start)
{
    return exceptions().contains(start) ;    // QDateTime equality compares the moment, whatever its time spec
}

void Appointment::addException(QDateTime start)
{
    QList<QDateTime> list = exceptions() ;
    if (!list.contains(start)) {
        list.append(start) ;
        setExceptions(list) ;
    }
}

QList<QDateTime> Appointment::exceptions()
{
    QList<QDateTime> list ;
    foreach (const QString &text, getField(Appointment::RepeatExceptions).split(QChar(','), Qt::SkipEmptyParts)) {
        QDateTime when = QDateTime::fromString(text, Qt::ISODate) ;
        if (when.isValid()) list.append(when) ;
    }

    // Kept in date order, whatever order they were stored in, so an upload matches Google's order
    std::sort(list.begin(), list.end()) ;
    list.erase(std::unique(list.begin(), list.end()), list.end()) ;
    return list ;
}

void Appointment::setExceptions(QList<QDateTime> starts)
{
    // Kept in date order, as Google returns them, so that a copy of this appointment from Google matches it
    std::sort(starts.begin(), starts.end()) ;
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end()) ;

    QStringList texts ;
    foreach (const QDateTime &when, starts) texts.append(when.toUTC().toString(Qt::ISODate)) ;
    setField(Appointment::RepeatExceptions, texts.join(QChar(','))) ;
}

int Appointment::indexOfOccurrence(QDateTime start)
{
    if (!isRepeating()) return (occurrenceFrom(0)==start) ? 0 : -1 ;
    for (int n=0; !occurrenceEnded(n); n++) {
        QDateTime when = occurrenceFrom(n) ;
        if (when==start) return n ;
        if (when>start) break ;
    }
    return -1 ;
}

void Appointment::endBefore(int n)
{
    if (n<=0) return ;
    int count = getField(Appointment::RepeatCount).toInt() ;
    if (count>0) {
        if (n<count) setField(Appointment::RepeatCount, QString::number(n)) ;
        return ;
    }
    setField(Appointment::RepeatUntil, occurrenceFrom(n-1).toUTC().toString(Qt::ISODate)) ;
}

// A copy with a new ID, and without the Google record of this appointment, so that Google receives it as a new event
Appointment Appointment::copyAsNew()
{
    Appointment copy = *this ;
    copy.setId(QUuid::createUuid().toString()) ;
    for (int i=0; i<NumberOfRecords; i++) {
        if (!appointmentrecordinfo[i].isgacct) continue ;
        if (appointmentrecordinfo[i].isflag) copy.setFlag((enum AppointmentRecord)i, false) ;
        else copy.setField((enum AppointmentRecord)i, "") ;
    }
    return copy ;
}

// The first occurrence that is still to come.  A repeat is stepped from its start, so a repeat that began in the past is
// found by its later dates.
QDateTime Appointment::nextOccurrence()
{
    QDateTime now = QDateTime::currentDateTimeUtc() ;
    if (!isRepeating()) {
        QDateTime start = getDate(Appointment::From) ;
        return start>now ? start : QDateTime() ;
    }
    for (int n=0; !occurrenceEnded(n); n++) {
        QDateTime when = occurrenceFrom(n) ;
        if (when > now && !isException(when)) return when ;
    }
    return QDateTime() ;
}

bool Appointment::isCurrent()
{
    QDateTime from = getDate(Appointment::From) ;
    QDateTime to = getDate(Appointment::To) ;
    QDateTime now  = QDateTime::currentDateTimeUtc() ;
    return (now >= from && now <= to) ;
}


// The records that say what has been messaged, and when
static bool isSentStateRecord(int i)
{
    return i==(int)Appointment::MessageSent || i==(int)Appointment::MessageSignature ||
           i==(int)Appointment::ReminderMessageSent || i==(int)Appointment::ReminderFor ||
           i==(int)Appointment::ReminderOnDaySent || i==(int)Appointment::CancelledOccurrences ||
           i==(int)Appointment::CancelSent ;
}

// Return true if i is of type mctype
bool Appointment::isAppointmentOfType(Appointment::AppointmentRecord i, int matype)
{

    bool testid = ( (matype&Appointment::maId)!=0) ;
    bool testgoogleid = ( (matype&Appointment::maGoogleId)!=0) ;
    bool testflag = ( (matype&Appointment::maFlag)!=0) ;
    bool testcontrol = ( (matype&Appointment::maControlFlags) !=0) ;
    bool testupdateddate = ( (matype&Appointment::maUpdatedDate) !=0) ;
    bool testdetails = ( (matype&Appointment::maDetails) !=0) ;
    bool testgoogleacct = ( (matype&Appointment::maGoogleAcct) !=0) ;
    bool testsaved = ( (matype&Appointment::maSavedFields) !=0) ;
    bool testsentstate = ( (matype&Appointment::maSentState) !=0) ;

    return (
         (testid && i==(int)Appointment::ID) ||
         (testgoogleid && i==(int)Appointment::GoogleRecordId) ||
         (testflag && appointmentrecordinfo[i].isflag) ||
         (testcontrol && !appointmentrecordinfo[i].issaved) ||
         (testdetails && appointmentrecordinfo[i].isdetails) ||
         (testupdateddate && i==(int)Appointment::Updated) ||
         (testgoogleacct && appointmentrecordinfo[i].isgacct) ||
         (testsentstate && isSentStateRecord(i)) ||
         (testsaved && appointmentrecordinfo[i].issaved)) ;
}

// The later of two dates held as ISO strings.  An empty or invalid date is earlier than any other.
static QString laterDate(const QString &a, const QString &b)
{
    QDateTime da = QDateTime::fromString(a, Qt::ISODate) ;
    QDateTime db = QDateTime::fromString(b, Qt::ISODate) ;
    if (!db.isValid()) return a ;
    if (!da.isValid()) return b ;
    return db>da ? b : a ;
}

// The on-the-day record is "yyyy-MM-dd" followed by the channels sent that day.  The later day is kept, and on the same
// day the channels of both.
static QString laterDayRecord(const QString &a, const QString &b)
{
    QStringList pa = a.split(' ', Qt::SkipEmptyParts) ;
    QStringList pb = b.split(' ', Qt::SkipEmptyParts) ;
    if (pb.isEmpty()) return a ;
    if (pa.isEmpty() || pb.at(0)>pa.at(0)) return b ;
    if (pb.at(0)<pa.at(0)) return a ;
    for (const QString &channel : pb) {
        if (!pa.contains(channel)) pa.append(channel) ;
    }
    return pa.join(' ') ;
}

// A comma separated list with the items of both, once each
static QString unionList(const QString &a, const QString &b)
{
    QStringList items = a.split(',', Qt::SkipEmptyParts) ;
    for (const QString &item : b.split(',', Qt::SkipEmptyParts)) {
        if (!items.contains(item)) items.append(item) ;
    }
    return items.join(',') ;
}

bool Appointment::mergeSentState(Appointment &remote)
{
    if (getField(Appointment::LedgerPending).compare("yes")==0) return false ;

    bool changed=false ;

    // Sets the record to the value, when that is different
    auto take = [&](Appointment::AppointmentRecord field, QString value) {
        if (value.compare(getField(field))!=0) {
            setSentField(field, value) ;
            changed=true ;
        }
    } ;

    take(Appointment::MessageSent, laterDate(getField(Appointment::MessageSent), remote.getField(Appointment::MessageSent))) ;
    take(Appointment::ReminderMessageSent, laterDate(getField(Appointment::ReminderMessageSent), remote.getField(Appointment::ReminderMessageSent))) ;
    take(Appointment::ReminderFor, laterDate(getField(Appointment::ReminderFor), remote.getField(Appointment::ReminderFor))) ;
    take(Appointment::ReminderOnDaySent, laterDayRecord(getField(Appointment::ReminderOnDaySent), remote.getField(Appointment::ReminderOnDaySent))) ;
    take(Appointment::CancelledOccurrences, unionList(getField(Appointment::CancelledOccurrences), remote.getField(Appointment::CancelledOccurrences))) ;

    // The signature is that of the last message sent, so the other copy's is kept when it has one
    if (!remote.getField(Appointment::MessageSignature).isEmpty()) {
        take(Appointment::MessageSignature, remote.getField(Appointment::MessageSignature)) ;
    }

    // A cancellation sent elsewhere is not sent again from here, so the confirmation is no longer waiting to be cancelled
    if (!remote.getField(Appointment::CancelSent).isEmpty()) {
        take(Appointment::CancelSent, remote.getField(Appointment::CancelSent)) ;
        take(Appointment::MessageSent, QString()) ;
    }

    return changed ;
}

// Copy Fields to other
bool Appointment::copyTo(Appointment &other, int matype)
{
    bool copied=false ;
    for (int i=0; i<Appointment::NumberOfRecords; i++) {
        if ( isAppointmentOfType((Appointment::AppointmentRecord)i, matype)) {
            other.setField((Appointment::AppointmentRecord)i, getField((Appointment::AppointmentRecord)i)) ;
            copied=true ;
        }
    }
    return copied ;
}

// Copy from source, keeping Updated and DateUpdated.  DateUpdated is set first, as setting it also sets Updated.
void Appointment::copyFromKeepingUpdated(Appointment &source, int matype)
{
    QString updated = getField(Appointment::Updated) ;
    QString dateupdated = getField(Appointment::DateUpdated) ;
    source.copyTo(*this, matype) ;
    setField(Appointment::DateUpdated, dateupdated) ;
    setField(Appointment::Updated, updated) ;
}

// Compare this with 'with' and return true/false
bool Appointment::matches(Appointment &with, int matype)
{
    return mismatch(with, matype).isEmpty() ;
}

// Compare this with 'with' and return description of differences
QString Appointment::mismatch(Appointment &with, int matype, bool showboth)
{
    QString result = "" ;
    for (int i=0; i<Appointment::NumberOfRecords; i++) {
        if ( isAppointmentOfType((Appointment::AppointmentRecord)i, matype)) {
            QString thisfield = getField((Appointment::AppointmentRecord)i) ;
            QString thatfield = with.getField((Appointment::AppointmentRecord)i) ;
            if (thisfield.compare(thatfield)!=0) {
                if (!result.isEmpty()) result = result + QString(", ") ;
                result = result + appointmentrecordinfo[i].name + QString(": ") ;
                result = result + thisfield ;
                if (showboth) result = result + QString(" / ") + thatfield ;
            }
        }
    }
    return result ;
}

bool Appointment::clashes(Appointment& with)
{
    // Temporary entries can never clash
    if (isTemp() || with.isTemp()) return false ;

    //
    // Appointments clash if they are identical, or overlap
    //
    //         <***** with *****>
    // 1 ------------>                         =  This overlaps with the start of RHS
    // 2                   <----------------   = This overlaps with the end of RHS
    // 3           <--------->                 = This is within, or is identical to RHS
    // 4   <------------------------->         = RHS is within this
    //

    // getDate shares one value, so each date is copied before the next is read
    QDateTime dFrom = getDate(Appointment::From) ;
    QDateTime dTo = getDate(Appointment::To) ;
    QDateTime wFrom = with.getDate(Appointment::From) ;
    QDateTime wTo = with.getDate(Appointment::To) ;
    return periodsClash(dFrom, dTo, wFrom, wTo) ;
}

// The clash test of clashes, on periods
bool Appointment::periodsClash(QDateTime dFrom, QDateTime dTo, QDateTime wFrom, QDateTime wTo)
{
    return (dTo > wFrom && dTo <= wTo) ||        // 1
           (dFrom > wFrom  && dFrom < wTo) ||    // 2
           (dFrom >= wFrom && dTo <= wTo) ||     // 3
           (dFrom < wFrom && dTo > wTo) ;        // 4
}

// The occurrences that are listed, between from and to inclusive.  A single appointment has one.  A repeat has an
// occurrence for each step that is not an exception and has not ended (see occurrenceExists).
QList<QPair<QDateTime,QDateTime>> Appointment::listedOccurrences(QDateTime from, QDateTime to)
{
    QList<QPair<QDateTime,QDateTime>> listed ;
    QDateTime start = getDate(Appointment::From) ;     // copies: getDate shares one value
    QDateTime end = getDate(Appointment::To) ;
    qint64 length = start.secsTo(end) ;

    if (!isRepeating()) {
        if (start <= to && end >= from) listed.append(qMakePair(start, end)) ;
        return listed ;
    }

    for (int n=0; !occurrenceEnded(n); n++) {
        QDateTime when = occurrenceFrom(n) ;
        if (when > to) break ;
        if (!occurrenceExists(n)) continue ;
        QDateTime whenend = when.addSecs(length) ;
        if (whenend >= from) listed.append(qMakePair(when, whenend)) ;
    }
    return listed ;
}

int Appointment::operator==(const Appointment& rhs) const
{
    // Supporting date search, but push deleted ones to the end
    if (rhs.isSet(Appointment::Deleted)) {
        return 1;
    }

    QDateTime f1 = getDate(Appointment::From) ;
    QDateTime f2 = rhs.getDate(Appointment::From) ;
    QDateTime t1 = getDate(Appointment::To) ;
    QDateTime t2 = rhs.getDate(Appointment::To) ;
    QString s1 = filedata[Appointment::For] + filedata[Appointment::Summary] ;
    QString s2 = rhs.filedata[Appointment::For] + rhs.filedata[Appointment::Summary] ;
    int c = s1.compare(s2) ;
    int i = filedata[Appointment::ID].compare(rhs.filedata[Appointment::ID]) ;

    if ( ( f1==f2 ) && ( t1==t2 ) && ( c==0 ) && ( i==0 ) ) { // Date/Time/Name/Summary/ID match

        return 1 ;

    } else {

        return 0 ;

    }

}


int Appointment::operator<(const Appointment &rhs) const
{
    // Supporting date search, but push deleted ones to the end.
    // Must be a strict weak ordering for std::sort: live entries come before
    // deleted ones, and entries of the same kind are then ordered by date.
    bool lhsdeleted = isSet(Appointment::Deleted) ;
    bool rhsdeleted = rhs.isSet(Appointment::Deleted) ;
    if (lhsdeleted != rhsdeleted) {
        return rhsdeleted ? 1 : 0 ;
    }

    QDateTime f1 = getDate(Appointment::From) ;
    QDateTime f2 = rhs.getDate(Appointment::From) ;
    QDateTime t1 = getDate(Appointment::To) ;
    QDateTime t2 = rhs.getDate(Appointment::To) ;
    QString s1 = filedata[Appointment::For] + filedata[Appointment::Summary] ;
    QString s2 = rhs.filedata[Appointment::For] + rhs.filedata[Appointment::Summary] ;
    int c = s1.compare(s2) ;
    int i = filedata[Appointment::ID].compare(rhs.filedata[Appointment::ID]) ;

    if ( ( f1<f2 ) ||                                            // Date less than
         ( ( f1==f2 ) && ( t1<t2 ) ) ||                          // Date match, and time less than
         ( ( f1==f2 ) && ( t1==t2 ) && ( c<0 ) ) ||              // Date/Time match & Name/Summary less than
         ( ( f1==f2 ) && ( t1==t2 ) && ( c==0 ) && ( i<0 ) ) ) { // Date/Time/Name/Summary match & ID less than

        return 1 ;

    } else {

        return 0 ;

    }
}

// The rule for an appointment's linked contact (issue #95).  A link to a contact that exists is kept.  Otherwise a For name
// that matches exactly one contact (case-insensitive) sets the link to that contact.  Anything else removes the link.
AppointmentContactRule matchAppointmentContact(const QString &contactid, const QString &forname, const QSet<QString> &ids,
                                               const QHash<QString, QStringList> &idsbyname, QString &resolvedid)
{
    if (ids.contains(contactid)) {
        resolvedid = contactid ;
        return ContactKept ;
    }

    QString name = forname.trimmed().toLower() ;
    QStringList matches = name.isEmpty() ? QStringList() : idsbyname.value(name) ;
    if (matches.size()==1) {
        resolvedid = matches.first() ;
        return ContactNamed ;
    }

    resolvedid.clear() ;
    return ContactUnnamed ;
}

