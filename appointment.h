#ifndef APPOINTMENT_H
#define APPOINTMENT_H

#include <QDateTime>

// Range of dates considered valid for any appointment: -3 months, + 5 years
#define APPOINTMENTSTARTWINDOW 90
#define APPOINTMENTENDWINDOW 1825

class Appointment
{
public:

    enum AppointmentRecord {

        //------------------------
        // Appointment Data

        FIRSTRECORD = 0,
        FIRSTSYNCEDRECORD = 0,
        ContactId = 0,
        For = 1,
        Description = 2,
        Location = 3,
        From = 4,
        To = 5,
        Repeat = 6,
        RepeatInterval = 7,
        Flags = 8,
        Deleted = 9,
        LASTSYNCEDRECORD = 9,

        ID = 10,
        Updated = 11,                // Updated when field upto LASTSYNCED changed
        MessageSent = 12,            // Updated when message sent
        ReminderMessageSent = 13,    // Updated when reminder sent
        Created = 14,                // Set when created
        FromUpdated = 15,            // Updated when From changed
        Temporary = 16,              // Temporary flag (used for local birthdays & repeats)

        GOOGLEFIRSTRECORD = 17,
        InternetOwned = 17,          // Internet flag (indicates this is a read-only entry)
        GoogleAccount = 18,
        GoogleRecordId = 19,
        GoogleIcalUid = 20,
        GoogleSequence = 21,
        GoogleCreated = 22,
        GoogleStatus = 23,
        GOOGLELASTRECORD = 23,

        LASTRECORD = 23

        //------------------------
    };


private:
    bool isnull ;
    bool isempty ;
    bool isdirty ;

    QString filedata[LASTRECORD+1] ;

    QString shortTextResponse ;
    QString asTextResponse ;
    QString asHTMLResponse ;
    QDateTime getDateResponse ;

public:

    Appointment();
    void setNull() ;

    ~Appointment();

    bool isNull() ;
    bool isEmpty() ;
    bool isDirty() ;

    bool load(QString path, QString idname) ;
    void createNew() ;
    bool save(QString path) ;

    void markAsSaved() ;
    void markAsDirty() ;

    void setId(QString newid) ;
    void setContactId(QString contactid) ;
    void setDescription(QString desc) ;

    bool isInFuture() ;
    bool isCurrent() ;
    bool isInRange(qint64 beforedays, qint64 afterdays) ;

    bool isNotFor(QString& id) ;

    bool isForAccount(QString googleaccount) ;

    void setField(enum AppointmentRecord field, QString data) ;
    void setFlag(enum AppointmentRecord field, bool flag) ;
    void setDate(enum AppointmentRecord field, QDateTime data) ;
    QString& getField(enum AppointmentRecord field) const ;
    QDateTime& getDate(enum AppointmentRecord field) const ;
    bool isSet(enum AppointmentRecord field) const ;

    char *getAppointmentRecordName(enum AppointmentRecord field)  ;

    // ID Shortcuts

    QString& getGoogleRecordId() ;
    QString& getId() ;
    QString& getContactId() ;

    QString& asText(QString name="", QString start="", QString mid="", QString end="") ;
    QString& shortText() ;
    QString& asHTML(QString name="") ;

    bool matches(Appointment &with) ;
    bool clashes(Appointment& with) ;

    Appointment& operator=(const Appointment &rhs);
    Appointment& copyGoogleAccountFieldsTo(Appointment& dest) ;
    Appointment& copySyncedFieldsTo(Appointment& dest) ;


    int operator==(const Appointment &rhs) const;
    int operator<(const Appointment &rhs) const;

};

#endif // APPOINTMENT_H
