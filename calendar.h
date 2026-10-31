#ifndef CALENDAR_H
#define CALENDAR_H

#include <QString>
#include <QList>

#include "appointment.h"

//using namespace std;

enum CalendarOverviewType {
    calendarAsText = 0,
    calendarAsHTML = 1,
};

enum CalendarSearchType {
    calendarSearchExact = 0,
    calendarSearchLast = 1,
    calendarSearchThis = 2,
    calendarSearchNearest = 3,
    calendarSearchNext = 4
};

class ContactDatabase ;

// The row whose start is nearest to wanted, or -1 when there are no rows (issue #92).  Used to put the focus on the nearest
// date when the list is rebuilt and the entry on screen is no longer listed.
int nearestRowToDate(const QList<QDateTime> &starts, const QDateTime &wanted) ;

class Calendar
{
private:
    // TODO: Calendar uses QList, others use std::list
      QList<Appointment> appointments ;
      Appointment nullentry ;
      QString idName ;
      bool isdirty ;
      QString getOverviewResponse ;
      QString selectedappointment ;
      QString selectedhint ;                  // The entry on screen: a repeat has its own hint, but the appointment it comes from is selected

public:
    Calendar();
    ~Calendar() ;

private:
    // Unused copy construct
    Calendar(const Calendar& other) ;
    Calendar& operator =(const Calendar &rhs) ;
    bool loadAppointment(QString ident) ;

public:
    void clear() ;
    void sort() ;
    int size() ;

    bool load() ;
    bool save() ;
    void dumpEntries(QString source, class ContactDatabase &db) ;
    int find(QString text, bool futureonly=false) ;

    bool appointmentIsInFuture(int n) ;
    bool appointmentIsInRange(int n, qint64 beforedays, qint64 afterdays) ;

    bool addAppointment(Appointment &appt, bool dosort=false) ;

    Appointment &getAppointment(enum CalendarSearchType method, int startindex) ;

    Appointment& getAppointment(int n) ;
    Appointment& getAppointmentBy(enum Appointment::AppointmentRecord type, QString id) ;

    Appointment &getAppointmentClash(Appointment &appt) ;
    Appointment &getLastAppointment() ;
    Appointment &getCurrentAppointment() ;
    Appointment &getNearestAppointment() ;
    Appointment &getNextAppointment() ;
    Appointment &getNull() ;

    QString& getAppointmentAsText(int n) ;
    QString& getAppointmentAsHTML(int n) ;
    QString getOverview(enum CalendarOverviewType overviewtype, QString contactid, bool futureonly=true) ;

    Appointment& selectAppointment(QString id) ;
    Appointment& selectAppointment(Appointment &appointment) ;

    // A repeat in the list has the hint <ID of its appointment>@<its start>.  The appointment is what is selected.
    static QString occurrenceHint(QString id, QDateTime start) ;
    static QString appointmentIdOf(QString hint) ;
    QString selectedHint() ;
    Appointment& getSelected() ;

    void purgeBirthdays() ;
    int tidyBirthdays(bool dryrun) ;

    // Archived appointments (issue #96).  An archived appointment is kept in the archive folder, which start-up does not read.
    // archiveExpired moves each expired appointment that is not changed since lastsync, and has nothing waiting to be
    // uploaded or downloaded, into the archive, and returns how many.  Its IDs are added to archived.
    int archiveExpired(QDateTime lower, QDateTime lastsync, QStringList &archived) ;
    static QList<Appointment> loadArchive() ;
    bool restoreFromArchive(Appointment &appt) ;
    bool updateArchived(Appointment &appt) ;
    bool removeFromArchive(QString id) ;
    int repointContact(QString fromid, QString toid) ;

    // Tidy Records, for the calendar.  A repeat with no occurrences left (every one removed) shows nothing, so it is marked
    // deleted, as deleting all of a repeat's occurrences does.  Returns how many, and describes each in removed.
    int tidyRepeatsWithNoOccurrences(QStringList &removed) ;

    // Tidy Records, for the calendar.  Deleted entries that are not on Google are moved to the deleted folder.  Those still
    // linked to Google are left for Google Sync to remove there.  Returns how many were moved, and how many were linked.
    int tidyDeleted(int &linked) ;

    // Entries whose contact is missing or deleted.  They are described, and are left as they are: which contact they belong
    // to is for the user to decide.
    QStringList orphanedEntries(ContactDatabase &db) ;
    void addBirthday(QString who, QString whoid, QString when) ;


};

bool Compare(const Appointment &A, const Appointment &B) ;

#endif // CALENDAR_H
