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

        ID,                     // Unique Entry ID

        Deleted,                // Set if entry is deleted

        ContactId,
        For,
        Description,
        Location,
        From, To, Repeat,  RepeatInterval,
        Flags,

        InternetOwned,          // Internet flag (indicates this is a read-only entry)
        GoogleRecordId,
        GoogleSequence,
        GoogleCreated,
        GoogleStatus,

        // Timestamps
        Created,                // Set when created
        Updated,                // Updated when field upto LASTSYNCED changed
        FromUpdated,            // Updated when From changed (used to re-send SMS alerts)

        // Message Timestamps
        MessageSent,            // Updated when message sent
        ReminderMessageSent,    // Updated when reminder sent

        // Counters
        MessageCounter,         // Number of Message Sends

        // Flags
        Temporary,              // Temporary flag (used for local birthdays & repeats)
        ToBeUploaded,           // Used to track uploads
        ToBeDownloaded,         // Used to track downloads

        NumberOfRecords

        //------------------------
    };

    struct AppointmentRecordInfo {
        enum AppointmentRecord recordtype;
        bool isflag ;            // True if entry is a boolean
        bool issynced ;          // True if entry is synced (i.e. changes require uploading)
        bool updatesdirty ;      // True if entry affects dirty flag (and Updated)
        bool isdetails ;         // True if entry contains field details
        bool isgacct ;           // Tru if entry contains google account details
        bool issaved ;           // True if entry is saved / loaded
        char name[24] ;
    } ;

//    ContactRecordInfo const contactrecordinfo[NumberOfRecords][sizeof(ContactRecordInfo)] = {
    AppointmentRecordInfo const appointmentrecordinfo[NumberOfRecords] = {
        // type                flag   sync   dirty details gacct  saved name

        { ID,                  false,  true,  true, false, false, true, "id" },

        { Deleted,              true,  true,  true,  true, false, true, "deleted" },
        { ContactId,           false,  true,  true,  true, false, true, "contactid" },
        { For,                 false,  true,  true,  true, false, true, "for" },
        { Description,         false,  true,  true,  true, false, true, "description" },
        { Location,            false,  true,  true,  true, false, true, "location" },
        { From,                false,  true,  true,  true, false, true, "from" },
        { To,                  false,  true,  true,  true, false, true, "to" },
        { Repeat,              false,  true,  true,  true, false, true, "repeat" },
        { RepeatInterval,      false,  true,  true,  true, false, true, "repeatinterval" },
        { Flags,               false,  true,  true,  true, false, true, "flags" },

        { InternetOwned,        true,  true,  true, false,  true, true, "isreadonly" },
        { GoogleRecordId,      false,  true,  true, false,  true, true, "googlerecordId" },
        { GoogleSequence,      false,  true,  true, false,  true, true, "googlesequence" },
        { GoogleCreated,       false,  true,  true, false,  true, true, "googlecreated" },
        { GoogleStatus,        false,  true,  true, false,  true, true, "googlestatus" },

        { Created,             false,  true,  true, false, false, true, "createddate" },
        { Updated,             false, false,  true, false, false, true, "Updateddate" },
        { FromUpdated,         false,  true,  true, false, false, true, "fromupdateddate" },
        { MessageSent,         false,  true,  true, false, false, true, "messagesentdate" },
        { ReminderMessageSent, false,  true,  true, false, false, true, "remindermessagesentdate" },
        { MessageCounter,      false, false,  true, false, false, true, "messagecounter" },

        { Temporary,           false,  true,  true, false,  true, true, "istemporary" },
        { ToBeUploaded,         true, false, false, false, false, false, "tobeuploaded" },
        { ToBeDownloaded,       true, false, false, false, false, false, "tobedownloaded" }
    } ;

    // Match and Copy Masks (matype)
    static const int maId=0x01 ;            // ID
    static const int maGoogleId=0x02 ;      // Google ID
    static const int maDetailsNoUpdatedDate=0x04 ; // Data details (not google account info / updated)
    static const int maGoogleAcct=0x08 ;    // All Google account details
    static const int maUpdatedDate=0x10 ;   // Updated date Stamp
    static const int maFlag=0x20 ;
    static const int maSynced=0x40 ;
    static const int maControlFlags=0x100 ; // Uploaded / downloaded control flags
    static const int maSavedFields=0x200 ;  // All fields which are saved

    static const int maDetails = (maDetailsNoUpdatedDate | maUpdatedDate) ;

    bool appointmentrecordinfook ;

private:
    bool isnull ;
    bool isempty ;
    bool isdirty ;

    QString filedata[NumberOfRecords] ;

    QString shortTextResponse ;
    QString asTextResponse ;
    QString asHTMLResponse ;
    QDateTime getDateResponse ;
    QString asAccessibleDescriptionResponse ;

public:

    Appointment();
    Appointment(const Appointment& other) ;
    Appointment& operator=(const Appointment& rhs) ;

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

    void setField(enum AppointmentRecord field, QString data) ;
    void setFlag(enum AppointmentRecord field, bool flag) ;
    void setDate(enum AppointmentRecord field, QDateTime data) ;
    QString& getField(enum AppointmentRecord field) const ;
    QDateTime& getDate(enum AppointmentRecord field) const ;
    bool isSet(enum AppointmentRecord field) const ;

    // ID Shortcuts

    QString& getGoogleRecordId() ;
    QString& getId() ;
    QString& getContactId() ;

    QString& asText(QString name="", QString start="", QString mid="", QString end="") ;
    QString& asAccessibleDescription(QString name="") ;
    QString& shortText() ;
    QString& asHTML(QString name="") ;


    // Returns true if the appoinmentrecord 'i' is of type matype
    bool isAppointmentOfType(AppointmentRecord i, int matype) ;

    // Test current appointment against reference
    bool matches(Appointment &with, int matype) ;
    QString mismatch(Appointment &with, int matype, bool showboth=false) ;

    // Copy selected fields to new contact
    bool copyTo(Appointment &other, int matype) ;

    // Returns true if appointments clash
    bool clashes(Appointment& with) ;

    // Operators
    int operator==(const Appointment &rhs) const;
    int operator<(const Appointment &rhs) const;

};

#endif // APPOINTMENT_H
