#include "calendar.h"
#include "appointment.h"
#include "configuration.h"
#include "../Lib/supportfunctions.h"

#include <QDateTime>
#include <QList>
#include <QFile>
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
}

void Calendar::clear()
{
    appointments.clear() ;
    nullentry.setNull() ;
    idName="" ;
    isdirty=false ;
    getOverviewResponse="" ;
}

bool Calendar::load()
{
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

Appointment &Calendar::getAppointmentBy(enum Appointment::AppointmentRecord type, QString id)
{
    for (int i=0, sz=size(); i<sz; i++) {
        if (getAppointment(i).getField(type).compare(id)==0) {
            return getAppointment(i) ;
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
    qSort(appointments.begin(), appointments.end()) ;
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
    QRegExp re(".*(" +text.toLower() +").*") ;
    QString overview = getOverview(calendarAsText, "", futureonly).toLower() ;
    if (re.exactMatch(overview)) return 1;
    else  return -1 ;
}

Appointment &Calendar::getAppointment(enum CalendarSearchType method, int startindex)
{
    Appointment *startappointment = &getNull() ;
    Appointment *thisappointment = &getNull() ;
    Appointment *nearestappointment = &getNull() ;
    Appointment *lastappointment = &getNull() ;
    Appointment *nextappointment = &getNull() ;
    Appointment testappointment ;

    if (startindex < 0 || startindex >= appointments.size()) {
      return getNull() ;
    }

    QDateTime now = QDateTime::currentDateTimeUtc() ;
    QDateTime ago = now.addSecs(-900) ;

    testappointment.setDate(Appointment::From, ago) ;
    testappointment.setDate(Appointment::To, now) ;

    if (method == calendarSearchExact) {
        startappointment = (Appointment *)&appointments.at(startindex) ;
    } else {

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
        if ((contactid.isEmpty() || getAppointment(i).getField(Appointment::ContactId).compare(contactid)==0) &&
                (appointmentIsInFuture(i) || !futureonly) &&
                !getAppointment(i).isSet(Appointment::Deleted)) {
            switch (overviewtype) {
            case calendarAsHTML:
                getOverviewResponse += getAppointmentAsHTML(i) ;
                getOverviewResponse += "<br/>" ;
                break ;
            default:
                getOverviewResponse += getAppointmentAsText(i) ;
                getOverviewResponse += "\n" ;
                break ;
            }
        }
    }
    return getOverviewResponse ;
}

Appointment& Calendar::getAppointmentClash(Appointment& appt)
{
    for (int i=0; i<size(); i++) {
        Appointment& test = getAppointment(i) ;
        if ( !test.isSet(Appointment::Deleted) &&
                appt.getField(Appointment::ID).compare(test.getField(Appointment::ID))!=0 &&
                appt.clashes(test)) return test ;
    }
    return getNull() ;
}
