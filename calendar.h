#ifndef CALENDAR_H
#define CALENDAR_H

#include <QString>
#include <QList>

#include "appointment.h"
#include "../Lib/encryption.h"

using namespace std;

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

class Calendar
{
private:
    // TODO: Calendar uses QList, others use std::list
      QList<Appointment> appointments ;
      Appointment nullentry ;
      QString idName ;
      bool isdirty ;
      QString getOverviewResponse ;
      Encryption *enc ;


      bool loadAppointment(QString ident) ;

public:
    Calendar();
    ~Calendar() ;

    void setEncryption(Encryption *enc) ;
    void clear() ;
    void sort() ;
    int size() ;

    bool load() ;
    bool save() ;
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

};

bool Compare(const Appointment &A, const Appointment &B) ;

#endif // CALENDAR_H
