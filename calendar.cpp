#include <QRegularExpression>
#include "calendar.h"
#include "appointment.h"
#include "configuration.h"
#include "contactdatabase.h"
#include "../Lib/supportfunctions.h"

#include <QDateTime>
#include <QList>
#include <QFile>
#include <QElapsedTimer>
#include <QTextStream>
#include <QtCore>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QtAlgorithms>

Calendar::Calendar()
{
    clear() ;
}

Calendar::~Calendar()
{
    clear() ;
}

void Calendar::clear()
{
    appointments.clear() ;
    nullentry.setNull() ;
    idName="" ;
    isdirty=false ;
    getOverviewResponse="" ;
    selectedappointment="" ;
}

bool Calendar::load()
{
    QElapsedTimer loadtimer ;
    loadtimer.start() ;

    Encryption *enc = gConf->encryption() ;
    if (!enc) return false;

    QStringList nameFilter;
    QFileInfoList list ;
    QStringList files ;

    clear() ;

    // Login if not already done so
    if (!enc->loggedIn()) {
        enc->login() ;
    }
    if (!enc->loggedIn()) {
        return false ;
    }

    // Load Calendar
    QDir dir(gCalendarSavePath) ;

    nameFilter << "*.appointment" ;
    dir.setNameFilters(nameFilter) ;
    list = dir.entryInfoList( nameFilter, QDir::Files );
    files = dir.entryList() ;

    for (int i=files.size()-1; i>=0; i--) {

        QString filename ;
        QString id ;
        Appointment appt ;

        filename = files.at(i) ;
        id = filename ;
        id.replace(".appointment","") ;
        appt.load(gCalendarSavePath, id) ;
        QString googleStatus = appt.getField(Appointment::GoogleStatus) ;
        if (appt.isSet(Appointment::Deleted) && (googleStatus.isEmpty() || googleStatus.compare("cancelled")==0)) {
            // TODO: Delete/move the file
            backupFile(gCalendarSavePath, filename) ;
        } else {
            appointments.push_back(appt) ;
        }

    }

    sort() ;
    dbg(QString("TIMING load calendar: %1 entries, %2 ms").arg(appointments.size()).arg(loadtimer.elapsed())) ;
    return true ;
}

bool Calendar::save()
{
    Encryption *enc = gConf->encryption() ;
    if (!enc) return false ;

    bool success=true ;

    if (size()>0) {

        // Login if not already done so
        if (!enc->loggedIn()) {
            enc->login() ;
        }
        if (!enc->loggedIn()) {
            return false ;
        }

        QDir cdir(gSavePath) ;
        cdir.mkpath("calendar") ;

        for (int i=0; i<size(); i++) {
            success |= getAppointment(i).save(gCalendarSavePath) ;
        }
    }

    return success ;
}

//=====================================================================================================
//
// Public: Calendar - dumpEntries
//
//  Writes the contents of every entry to the debug output, one line each, followed by a
//  summary line.  Only written when debug=yes is set in the [google] section of the ini file.
//  The lines hold personal details, so check them before sharing.
//
//  source - where the entries came from ("local" or "google"), shown on each line
//  db     - used to show whether the linked contact is live, hidden, deleted or missing
//

void Calendar::dumpEntries(QString source, ContactDatabase &db)
{
    if (!gConf->debugGoogleEnabled()) return ;

    // Keep line breaks out of the output, so each entry is one line
    auto oneLine = [](QString s) { return s.replace("\r", " ").replace("\n", " / ") ; } ;

    int total = appointments.size() ;
    int internet=0, birthdays=0, nocontact=0, livecontact=0, hiddencontact=0, deletedcontact=0, missingcontact=0 ;

    for (int i=0; i<total; i++) {
        Appointment &appt = appointments[i] ;

        QString contactid = appt.getField(Appointment::ContactId) ;
        QString contactstate = "none" ;
        if (contactid.isEmpty()) {
            nocontact++ ;
        } else {
            Contact &c = db.getContactById(contactid) ;
            if (c.isNull()) {
                contactstate = "missing" ;
                missingcontact++ ;
            } else if (c.isSet(Contact::Deleted)) {
                contactstate = "deleted" ;
                deletedcontact++ ;
            } else if (c.isSet(Contact::Hidden)) {
                contactstate = "hidden" ;
                hiddencontact++ ;
            } else {
                contactstate = "live" ;
                livecontact++ ;
            }
        }

        // Read-only entries (InternetOwned) come from a Google calendar that is not
        // this user's, such as public holidays or subscribed calendars
        bool isinternet = appt.isSet(Appointment::InternetOwned) ;
        bool isbirthday = appt.getField(Appointment::Summary).contains("birthday", Qt::CaseInsensitive) ;
        if (isinternet) internet++ ;
        if (isbirthday) birthdays++ ;

        dbg(QString("CALENDAR %1 | from %2 | to %3 | internet %4 | birthday %5 | deleted %6 | google status %7 | contact %8 | for %9 | summary %10 | location %11 | description %12 | id %13 | google id %14 | created %15 | updated %16")
            .arg(source)
            .arg(appt.getField(Appointment::From))
            .arg(appt.getField(Appointment::To))
            .arg(isinternet ? "yes" : "no")
            .arg(isbirthday ? "yes" : "no")
            .arg(appt.isSet(Appointment::Deleted) ? "yes" : "no")
            .arg(appt.getField(Appointment::GoogleStatus))
            .arg(contactstate)
            .arg(appt.getField(Appointment::For))
            .arg(oneLine(appt.getField(Appointment::Summary)))
            .arg(oneLine(appt.getField(Appointment::Location)))
            .arg(oneLine(appt.getField(Appointment::Description)))
            .arg(appt.getField(Appointment::ID))
            .arg(appt.getField(Appointment::GoogleRecordId))
            .arg(appt.getField(Appointment::Created))
            .arg(appt.getField(Appointment::Updated))) ;
    }

    dbg(QString("CALENDAR %1 SUMMARY: %2 entries, internet (read-only) %3, birthday in summary %4, "
                "contact none %5, live %6, hidden %7, deleted %8, missing %9")
        .arg(source).arg(total).arg(internet).arg(birthdays)
        .arg(nocontact).arg(livecontact).arg(hiddencontact).arg(deletedcontact).arg(missingcontact)) ;
}

Appointment &Calendar::getAppointmentBy(enum Appointment::AppointmentRecord type, QString id)
{
    // Read the list directly.  Going through getAppointment(int) builds a
    // temporary Appointment on every call, which made this search very slow
    // on large calendars (and addAppointment() calls it for every download).
    for (int i=0, sz=appointments.size(); i<sz; i++) {
        if (appointments[i].getField(type).compare(id)==0) {
            return appointments[i] ;
        }
    }
    return getNull() ;
}


bool Calendar::addAppointment(Appointment &appt, bool dosort)
{
    // Get a pointer to the google record for the appoontment
    Appointment& record = getAppointmentBy(Appointment::ID, appt.getField(Appointment::ID)) ;
    if (record.isNull()) {
        // Not found, so add it to the list
        appointments.push_back(appt) ;
    }  else {
        // Found, so update the entry
        record = appt ;
    }

    // Sort the appointments list
   if (dosort) sort() ;

   return true ;
}


void Calendar::sort()
{
    std::sort(appointments.begin(), appointments.end()) ;
}


int Calendar::size()
{
  return appointments.size() ;
}

bool Calendar::appointmentIsInFuture(int n)
{
    if (n<0 || n >= appointments.size()) {
      return false ;
    } else {
        Appointment appt = appointments.at(n) ;
        return appt.isInFuture() ;
    }
}

bool Calendar::appointmentIsInRange(int n, qint64 beforedays, qint64 afterdays)
{
    if (n<0 || n >= appointments.size()) {
      return false ;
    } else {
      Appointment appt = appointments.at(n) ;
      return appt.isInRange(beforedays, afterdays) ;
    }
}


// Search for string, and return -1 on failure
int Calendar::find(QString text, bool futureonly)
{
    QRegularExpression re(".*(" + QRegularExpression::escape(text.toLower()) + ").*") ;
    QRegularExpressionMatch rem ;
    QString overview = getOverview(calendarAsText, "", futureonly).toLower() ;
    rem = re.match(overview) ;
    if (rem.hasMatch()) return 1;
    else  return -1 ;
}

Appointment &Calendar::getAppointment(enum CalendarSearchType method, int startindex)
{
    Appointment *startappointment = &getNull() ;
    Appointment *thisappointment = &getNull() ;
    Appointment *nearestappointment = &getNull() ;
    Appointment *lastappointment = &getNull() ;
    Appointment *nextappointment = &getNull() ;
    if (startindex < 0 || startindex >= appointments.size()) {
      return getNull() ;
    }

    if (method == calendarSearchExact) {
        startappointment = (Appointment *)&appointments.at(startindex) ;
    } else {

        // The test window is only needed by the searches, so it is built here
        Appointment testappointment ;
        QDateTime now = QDateTime::currentDateTimeUtc() ;
        QDateTime ago = now.addSecs(-900) ;

        testappointment.setDate(Appointment::From, ago) ;
        testappointment.setDate(Appointment::To, now) ;

        int n = appointments.size() ;

        if (n>0) {

            for (int i=startindex; i<n; i++) {
               Appointment *appt = (Appointment *)&appointments.at(i) ;

               if (!appt->isSet(Appointment::Deleted)) {

                   // Last Appointment
                   if (testappointment.clashes(*appt)) {
                       if (lastappointment->isNull()) lastappointment = appt ;
                       else if (*appt < *lastappointment) lastappointment = appt ;
                   }

                   // This Appointment (and only this one)
                   if (appt->isCurrent()) {
                       if (thisappointment->isNull()) thisappointment = appt ;
                       else if (*appt < *thisappointment) thisappointment = appt ;
                   }

                   // Nearest appointment to now
                   if (appt->isCurrent() || appt->isInFuture()) {
                       if (nearestappointment->isNull()) nearestappointment = appt ;
                       else if (*appt < *nearestappointment) nearestappointment = appt ;
                   }

                   if (appt->isInFuture() && !appt->isCurrent()) {
                       if (nextappointment->isNull()) nextappointment = appt ;
                       else if (*appt < *nextappointment) nextappointment = appt ;
                   }

               }
            }
        }
    }

    switch (method) {
    case calendarSearchExact:
        return *startappointment ;
    case calendarSearchLast:
        return *lastappointment ;
    case calendarSearchThis:
        return *thisappointment ;
    case calendarSearchNearest:
        return *nearestappointment ;
    case calendarSearchNext:
        return *nextappointment ;
    }

    return getNull() ;
}

Appointment &Calendar::getLastAppointment()
{
    return getAppointment(calendarSearchLast, 0);
}

Appointment &Calendar::getCurrentAppointment()
{
    return getAppointment(calendarSearchThis, 0);
}

Appointment &Calendar::getNearestAppointment()
{
    return getAppointment(calendarSearchNearest, 0);
}

Appointment &Calendar::getNextAppointment()
{
    return getAppointment(calendarSearchNext, 0);
}

Appointment& Calendar::getAppointment(int n)
{
    return getAppointment(calendarSearchExact, n) ;
}


Appointment& Calendar::getNull()
{
    return nullentry ;
}


QString& Calendar::getAppointmentAsText(int n)
{
   return getAppointment(n).asText() ;
}


QString& Calendar::getAppointmentAsHTML(int n)
{
    return getAppointment(n).asHTML() ;
}

// TODO: ...
// Append Calendar onto another one
// Sort Calendar
// Look for Conflicts with new appointment

QString Calendar::getOverview(enum CalendarOverviewType overviewtype, QString contactid, bool futureonly)
{
    int ne=size() ;

    getOverviewResponse = "" ;

    for (int i=0; i<ne; i++) {
        if (!contactid.isEmpty() && getAppointment(i).getField(Appointment::ContactId).compare(contactid)!=0) continue ;
        if (getAppointment(i).isSet(Appointment::Deleted)) continue ;

        // Future appointments are listed at their next occurrence, so a repeat that began in the past is listed too
        Appointment shown = getAppointment(i) ;
        if (futureonly) {
            QDateTime next = shown.nextOccurrence() ;
            if (!next.isValid()) continue ;
            QDateTime from = shown.getDate(Appointment::From) ;
            QDateTime to = shown.getDate(Appointment::To) ;
            shown.setDate(Appointment::From, next) ;
            shown.setDate(Appointment::To, next.addSecs(from.secsTo(to))) ;
        }

        switch (overviewtype) {
            case calendarAsHTML:
                getOverviewResponse += "<p>" ;
                getOverviewResponse += shown.asHTML() ;
                getOverviewResponse += "</p>\n" ;
                break ;
            default:
                getOverviewResponse += shown.asText() ;
                getOverviewResponse += "\n" ;
                break ;
        }
    }
    return getOverviewResponse ;
}

// The appointment that clashes with appt: one whose listed occurrence overlaps appt's own time.  Removed occurrences of a
// repeat are not compared, so a repeat is only in a clash through an occurrence the calendar shows.
Appointment& Calendar::getAppointmentClash(Appointment& appt)
{
    if (appt.isTemp()) return getNull() ;

    QDateTime from = appt.getDate(Appointment::From) ;     // copies: getDate shares one value
    QDateTime to = appt.getDate(Appointment::To) ;

    for (int i=0; i<size(); i++) {
        Appointment& test = getAppointment(i) ;
        if ( test.isSet(Appointment::Deleted) || test.isTemp() ||
                appt.getField(Appointment::ID).compare(test.getField(Appointment::ID))==0 ) continue ;

        QList<QPair<QDateTime,QDateTime>> occurrences = test.listedOccurrences(from, to) ;
        for (const QPair<QDateTime,QDateTime> &occurrence : occurrences) {
            if (Appointment::periodsClash(from, to, occurrence.first, occurrence.second)) return test ;
        }
    }
    return getNull() ;
}

//
// Searching and Selection
//

Appointment& Calendar::selectAppointment(QString id)
{
    selectedhint = id ;
    selectedappointment = appointmentIdOf(id) ;
    return getSelected() ;
}

// The hint for a repeat: the start is in UTC, so the same repeat always has the same hint
QString Calendar::occurrenceHint(QString id, QDateTime start)
{
    return id + "@" + start.toUTC().toString(Qt::ISODate) ;
}

// The appointment a hint belongs to.  An appointment's own hint is its ID.
QString Calendar::appointmentIdOf(QString hint)
{
    return hint.section('@', 0, 0) ;
}

QString Calendar::selectedHint()
{
    return selectedhint ;
}

Appointment& Calendar::selectAppointment(Appointment &appointment)
{
    return selectAppointment(appointment.getField(Appointment::ID)) ;
}

Appointment& Calendar::getSelected()
{
    Appointment *appt = NULL ;
    for (int i=0, ne=appointments.size(); i<ne && !appt; i++) {
        QString& recordid = appointments[i].getField(Appointment::ID) ;
        if (selectedappointment.compare(recordid)==0) appt = (Appointment*)&appointments[i] ;
    }

    if (!appt) {
        appt=(Appointment *)&getNull() ;
        selectedappointment = "" ;
    }

    return *appt ;
}

void Calendar::purgeBirthdays()
{
    QList<Appointment>::iterator it = appointments.begin();
    while (it != appointments.end()) {
        if ((*it).isTemp()) {
            it = appointments.erase(it);
        } else {
            ++it;
        }
    }
}

//=====================================================================================================
//
// Public: Calendar - tidyBirthdays
//
//  Moves the stored birthday entries to the deleted folder.  Birthdays are shown from the
//  contacts list, so these stored copies are not needed.  This covers copies of Google's
//  birthday entries, and the placeholders the sync made for them.  Nothing is deleted, so the
//  files can be moved back from the deleted folder.
//
//  dryrun - when true, only counts the entries that would be moved
//
//  Returns the number of entries moved (or that would be moved)
//

int Calendar::tidyBirthdays(bool dryrun)
{
    int count=0 ;

    // Work backwards, so that removing an entry does not shift the ones still to check
    for (int i=appointments.size()-1; i>=0; i--) {
        Appointment &appt = appointments[i] ;

        bool placeholder = (appt.getField(Appointment::Summary).compare("--! Undefined !--")==0) ;

        // Entries linked to a contact are kept, as they were entered by hand
        if ((appt.isBirthday() || placeholder) && appt.getField(Appointment::ContactId).isEmpty()) {
            count++ ;
            if (!dryrun) {
                QString filename = appt.getField(Appointment::ID) + ".appointment" ;
                backupFile(gCalendarSavePath, filename) ;
                appointments.removeAt(i) ;
            }
        }
    }

    return count ;
}

//=====================================================================================================
//
// Public: Calendar - repointContact
//
//  Moves the appointments for one contact to another, when two contacts are merged.
//  Returns the number of appointments moved.
//

int Calendar::repointContact(QString fromid, QString toid)
{
    int count=0 ;
    for (int i=0; i<appointments.size(); i++) {
        Appointment &appt = appointments[i] ;
        if (appt.getField(Appointment::ContactId).compare(fromid)==0) {
            appt.setField(Appointment::ContactId, toid) ;
            appt.markAsDirty() ;
            count++ ;
        }
    }
    return count ;
}

void Calendar::addBirthday(QString who, QString whoid, QString when)
{
    QStringList de = when.split("-") ;

    int count = de.count() ;
    if (count==3) {

        int day = de.at(2).toInt() ;
        int month = de[1].toInt() ;
        QTime midnight(0,0,0) ;

        QDateTime last, now, next ;
        int year = last.currentDateTimeUtc().date().year() ;

        last = QDateTime( QDate(year-1, month, day), midnight, Qt::UTC) ;
        Appointment *appt1 = new Appointment() ;
        appt1->setField(Appointment::Summary, QString("Birthday")) ;
        appt1->setField(Appointment::For, who) ;
        appt1->setField(Appointment::ContactId, whoid) ;
        appt1->setDate(Appointment::From, last) ;
        appt1->setDate(Appointment::To, last) ;
        appt1->setId(QString("1-%1").arg(whoid)) ;
        appt1->markAsTemp();
        appointments.append(*appt1) ;

        now = QDateTime( QDate(year, month, day), midnight, Qt::UTC) ;
        Appointment *appt2 = new Appointment(*appt1) ;
        appt2->setDate(Appointment::From, now) ;
        appt2->setDate(Appointment::To, now) ;
        appt2->setId(QString("2-%1").arg(whoid)) ;
        appointments.append(*appt2) ;

        next = QDateTime( QDate(year+1, month, day), midnight, Qt::UTC) ;
        Appointment *appt3 = new Appointment(*appt1) ;
        appt3->setDate(Appointment::From, next) ;
        appt3->setDate(Appointment::To, next) ;
        appt3->setId(QString("3-%1").arg(whoid)) ;
        appointments.append(*appt3) ;

    }
}

//=====================================================================================================
//
// Public: Calendar - tidyRepeatsWithNoOccurrences
//
//  A repeat with no occurrence left (every one removed) shows nothing, but it still takes part in the clash checks.  It is
//  marked deleted, as deleting all of a repeat's occurrences does.  Returns how many, and describes each in removed.
//
int Calendar::tidyRepeatsWithNoOccurrences(QStringList &removed)
{
    int count=0 ;
    for (int i=0; i<size(); i++) {
        Appointment &appt = getAppointment(i) ;
        if (appt.isSet(Appointment::Deleted) || appt.isTemp() || !appt.isRepeating()) continue ;

        // An occurrence that exists ends the search.  The exceptions are finite, so the search always ends.
        bool anyleft = false ;
        for (int n=0; !anyleft && !appt.occurrenceEnded(n); n++) anyleft = appt.occurrenceExists(n) ;
        if (anyleft) continue ;

        removed << QString("%1, from %2").arg(appt.getField(Appointment::Summary),
                                              appt.getDate(Appointment::From).toLocalTime().toString("dd MMM yyyy HH:mm")) ;
        appt.setFlag(Appointment::Deleted, true) ;
        count++ ;
    }
    return count ;
}

//=====================================================================================================
//
// Public: Calendar - tidyDeleted
//
//  Moves the deleted entries that are not linked to Google to the deleted folder, as the contacts are.  An entry linked to
//  Google is kept until Google Sync removes it there, which clears the link, so that a later tidy can move it.  Only the ID
//  is logged.  Returns how many were moved, and how many are linked.
//
int Calendar::tidyDeleted(int &linked)
{
    int moved=0 ;
    linked=0 ;

    // Work backwards, so that removing an entry does not shift the ones still to check
    for (int i=appointments.size()-1; i>=0; i--) {
        Appointment &appt = appointments[i] ;
        if (!appt.isSet(Appointment::Deleted)) continue ;

        if (!appt.getField(Appointment::GoogleRecordId).isEmpty()) {
            linked++ ;
            continue ;
        }

        QString id = appt.getField(Appointment::ID) ;
        addLog("Tidy: moving appointment " + id + " to deleted folder") ;
        QString filename = id + ".appointment" ;
        backupFile(gCalendarSavePath, filename) ;
        appointments.removeAt(i) ;
        moved++ ;
    }

    return moved ;
}

//=====================================================================================================
//
// Public: Calendar - orphanedEntries
//
//  The entries for a contact that is missing or deleted.  They are described and left as they are.  An entry for Me, and an
//  entry with no contact (an event made on Google), is not an orphan.
//
QStringList Calendar::orphanedEntries(ContactDatabase &db)
{
    QStringList orphans ;
    QString me = gConf->getMe() ;
    for (int i=0; i<size(); i++) {
        Appointment &appt = getAppointment(i) ;
        if (appt.isSet(Appointment::Deleted) || appt.isTemp()) continue ;

        QString contactid = appt.getField(Appointment::ContactId) ;
        if (contactid.isEmpty() || contactid.compare(me)==0) continue ;

        Contact &contact = db.getContactById(contactid) ;
        QString reason ;
        if (contact.isNull()) reason = "the contact is not in the database" ;
        else if (contact.isSet(Contact::Deleted)) reason = "the contact is deleted" ;
        else continue ;

        orphans << QString("%1, from %2 (%3)").arg(appt.getField(Appointment::Summary),
                                                    appt.getDate(Appointment::From).toLocalTime().toString("dd MMM yyyy HH:mm"),
                                                    reason) ;
    }
    return orphans ;
}

//=====================================================================================================
//
// Public: Calendar - archive (issue #96)
//
//  An expired appointment is moved from the calendar folder to the archive folder, which is not read at start-up.  Its Google
//  ID is kept, so that a later sync does not make a second copy of it.  Only an appointment that is not changed since the last
//  sync, and is not waiting to be uploaded, is archived, so that no change is moved out of reach.
//

static QString calendarArchivePath()
{
    return gCalendarSavePath + "archive/" ;
}

int Calendar::archiveExpired(QDateTime lower, QDateTime lastsync, QStringList &archived)
{
    int count=0 ;
    QString path = calendarArchivePath() ;
    QDir().mkpath(path) ;

    // Work backwards, so that removing an entry does not shift the ones still to check
    for (int i=appointments.size()-1; i>=0; i--) {
        Appointment &appt = appointments[i] ;
        if (appt.isTemp() || appt.isSet(Appointment::Deleted)) continue ;
        // Waiting to be uploaded: it stays until it is.  (Unchanged entries are always flagged to be downloaded, so that is not checked.)
        if (appt.isSet(Appointment::ToBeUploaded)) continue ;

        // Changed since the last sync: it waits for the upload
        QDateTime updated = appt.getDate(Appointment::Updated) ;
        if (!lastsync.isValid() || updated >= lastsync) continue ;

        if (!appt.isExpiredBefore(lower)) continue ;

        QString id = appt.getField(Appointment::ID) ;
        appt.markAsDirty() ;
        if (!appt.save(path)) continue ;
        QFile::remove(gCalendarSavePath + id + ".appointment") ;
        archived << id ;
        appointments.removeAt(i) ;
        count++ ;
    }
    return count ;
}

QList<Appointment> Calendar::loadArchive()
{
    QList<Appointment> list ;
    QString path = calendarArchivePath() ;
    QDir dir(path) ;
    dir.setNameFilters(QStringList() << "*.appointment") ;
    foreach (const QString &file, dir.entryList(QDir::Files)) {
        QString id = file ;
        id.chop(QString(".appointment").size()) ;
        Appointment appt ;
        if (appt.load(path, id)) list << appt ;
    }
    return list ;
}

// Restores an archived appointment to the calendar.  Its archived file is removed.
bool Calendar::restoreFromArchive(Appointment &appt)
{
    QString id = appt.getField(Appointment::ID) ;
    appt.markAsDirty() ;
    if (!appt.save(gCalendarSavePath)) return false ;
    QFile::remove(calendarArchivePath() + id + ".appointment") ;
    addAppointment(appt) ;
    return true ;
}

// Brings the archived copy of an appointment up to date, without restoring it
bool Calendar::updateArchived(Appointment &appt)
{
    appt.markAsDirty() ;
    return appt.save(calendarArchivePath()) ;
}

// Removes an archived appointment, which Google has cancelled.  It is moved to the archive's deleted folder, not deleted.
bool Calendar::removeFromArchive(QString id)
{
    QString path = calendarArchivePath() ;
    QString filename = id + ".appointment" ;
    return backupFile(path, filename) ;
}

int nearestRowToDate(const QList<QDateTime> &starts, const QDateTime &wanted)
{
    int nearest = -1 ;
    qint64 gap = 0 ;
    for (int i=0; i<starts.size(); i++) {
        qint64 g = qAbs(starts.at(i).secsTo(wanted)) ;
        if (nearest<0 || g<gap) {
            nearest = i ;
            gap = g ;
        }
    }
    return nearest ;
}

bool parseCalendarDate(const QString &text, QDate today, QDate *result)
{
    // A day and a month, then an optional year, as 2/12, 2-12, 2-12-26, 2/12/2026 or 2/12/26.  Two digits of a year take the
    // century of this year.
    QRegularExpressionMatch numbers = QRegularExpression("^(\\d{1,2})[/-](\\d{1,2})(?:[/-](\\d{4}|\\d{2}))?$").match(text.trimmed()) ;
    if (!numbers.hasMatch()) return false ;

    int day = numbers.captured(1).toInt() ;
    int month = numbers.captured(2).toInt() ;

    if (numbers.hasCaptured(3)) {
        int year = numbers.captured(3).toInt() ;
        if (numbers.captured(3).size()==2) year = (today.year()/100)*100 + year ;
        QDate date(year, month, day) ;
        if (!date.isValid()) return false ;
        *result = date ;
        return true ;
    }

    // No year: the next date with that day and month, on or after today.  A date such as 29 February is found in a later year
    // (within eight years), and 31 November is never found.
    for (int y=today.year(); y<=today.year()+8; y++) {
        QDate date(y, month, day) ;
        if (date.isValid() && date>=today) {
            *result = date ;
            return true ;
        }
    }
    return false ;
}

