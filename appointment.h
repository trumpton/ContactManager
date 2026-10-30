#ifndef APPOINTMENT_H
#define APPOINTMENT_H

#include <QDateTime>
#include <QTimeZone>
#include <QList>
#include <QStringList>


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
        Summary,
        Description,
        Location,
        From, To, Repeat,  RepeatInterval,
        Flags,

        InternetOwned,          // Internet flag (indicates this is a read-only entry)
        GoogleRecordId,
        GoogleSequence,
        GoogleCreated,
        GoogleStatus,
        Uploaded,

        // Timestamps
        Created,                // Set when created
        Updated,                // Updated when field upto LASTSYNCED changed
        DateUpdated,            // Updated when To/From changed (used to re-send SMS alerts)

        // Message Timestamps
        MessageSent,            // Updated when message sent
        ReminderMessageSent,    // Updated when reminder sent

        // Counters
        MessageCounter,         // Number of Message Sends

        // What the last confirmation or update said, so that a change to it is sent as an update
        MessageSignature,

        // Set when the on-the-day reminder has gone by both email and text
        ReminderOnDaySent,

        // The occurrence (UTC start) the last reminder was for, so that each occurrence of a repeat gets its own
        ReminderFor,

        // The occurrences (UTC starts, comma separated) whose removal has been notified by a cancellation
        CancelledOccurrences,

        // Set when a cancellation has been sent, so that no copy of ContactManager sends it again
        CancelSent,

        // Set when the sent state could not be written to Google.  The Google copy is not read over the local one until
        // it has been written, and the next sync uploads it.
        LedgerPending,

        // Flags
        ToBeUploaded,           // Used to track uploads
        ToBeDownloaded,         // Used to track downloads

        // Repeat end: the number of times it repeats (counting this appointment), and the date it stops
        RepeatCount,
        RepeatUntil,

        // The time zone the repeat is worked out in (an IANA name, such as Europe/London), so it keeps its clock time
        RepeatZone,

        // The occurrences the repeat leaves out (deleted, or changed into another appointment), as UTC starts
        RepeatExceptions,

        NumberOfRecords

        //------------------------
    };

    struct AppointmentRecordInfo {
        enum AppointmentRecord recordtype;
        bool isflag ;            // True if entry is a boolean
        bool isdetails ;         // True if entry contains field details
        bool isgacct ;           // Tru if entry contains google account details
        bool issaved ;           // True if entry affects dirty flag (and Updated)
        char name[24] ;
    } ;

//    ContactRecordInfo const contactrecordinfo[NumberOfRecords][sizeof(ContactRecordInfo)] = {
    AppointmentRecordInfo const appointmentrecordinfo[NumberOfRecords] = {
        // type                flag  details gacct saved name

        { ID,                  false, false, false, true, "id" },

        { Deleted,              true,  true, false, true, "deleted" },
        { ContactId,           false,  true, false, true, "contactid" },
        { For,                 false,  true, false, true, "for" },
        { Summary,             false,  true, false, true, "summary"},
        { Description,         false,  true, false, true, "description" },
        { Location,            false,  true, false, true, "location" },
        { From,                false,  true, false, true, "from" },
        { To,                  false,  true, false, true, "to" },
        { Repeat,              false,  true, false, true, "repeat" },
        { RepeatInterval,      false,  true, false, true, "repeatinterval" },
        { Flags,               false,  true, false, true, "flags" },

        { InternetOwned,        true, false,  true, true, "isreadonly" },
        { GoogleRecordId,      false, false,  true, true, "googlerecordId" },
        { GoogleSequence,      false, false,  true, true, "googlesequence" },
        { GoogleCreated,       false, false,  true, true, "googlecreated" },
        { GoogleStatus,        false, false,  true, true, "googlestatus" },
        { Uploaded,             true, false,  true,  true, "uploaded" },

        { Created,             false, false, false, true, "createddate" },
        { Updated,             false, false, false, true, "updateddate" },
        { DateUpdated,         false, false, false, true, "dateupdateddate" },
        { MessageSent,         false, false, false, true, "messagesentdate" },
        { ReminderMessageSent, false, false, false, true, "remindermessagesentdate" },
        { MessageCounter,      false, false, false, true, "messagecounter" },
        { MessageSignature,    false, false, false, true, "messagesignature" },
        { ReminderOnDaySent,   false, false, false, true, "reminderondaysent" },
        { ReminderFor,         false, false, false, true, "reminderfor" },
        { CancelledOccurrences, false, false, false, true, "cancelledoccurrences" },
        { CancelSent,          false, false, false, true, "cancelsent" },
        { LedgerPending,       false, false, false, true, "ledgerpending" },

        { ToBeUploaded,         true, false, false, false, "tobeuploaded" },
        { ToBeDownloaded,       true, false, false, false, "tobedownloaded" },

        { RepeatCount,         false,  true, false, true, "repeatcount" },
        { RepeatUntil,         false,  true, false, true, "repeatuntil" },
        { RepeatZone,          false,  true, false, true, "repeatzone" },
        { RepeatExceptions,    false,  true, false, true, "repeatexceptions" }
    } ;

    // Match and Copy Masks (matype)
    static const int maId=0x01 ;            // ID
    static const int maGoogleId=0x02 ;      // Google ID
    static const int maDetails=0x04 ;       // Data details (not google account info / updated)
    static const int maGoogleAcct=0x08 ;    // All Google account details
    static const int maUpdatedDate=0x10 ;   // Updated date Stamp
    static const int maFlag=0x20 ;
    static const int maControlFlags=0x100 ; // Uploaded / downloaded control flags
    static const int maSavedFields=0x200 ;  // All fields which are saved
    static const int maSentState=0x400 ;    // The sent state of messages (see mergeSentState)

    bool appointmentrecordinfook ;

private:
    bool isnull ;
    bool isempty ;
    bool isdirty ;
    bool istemp ;

    QString filedata[NumberOfRecords] ;

    QString shortTextResponse ;
    QString asTextResponse ;
    QString asHTMLResponse ;
    QDateTime getDateResponse ;
    QString asAccessibleTextResponse ;

public:

    Appointment();
    Appointment(const Appointment& other) ;
    Appointment& operator=(const Appointment& rhs) ;

    void setNull() ;

    ~Appointment();

    bool isNull() ;
    bool isEmpty() ;
    bool isDirty() ;
    bool isTemp() ;
    bool isBirthday() ;

    bool load(QString path, QString idname) ;
    void createNew() ;
    bool save(QString path) ;

    void markAsSaved() ;
    void markAsDirty() ;
    void markAsTemp() ;

    void setId(QString newid) ;
    void setContactId(QString contactid) ;

    bool isInFuture() ;
    bool isCurrent() ;
    bool isInRange(qint64 beforedays, qint64 afterdays) ;

    // Repeats: Repeat is 0 for none, 1 daily, 2 weekly, 3 monthly or 4 yearly, every RepeatInterval of them.  The
    // occurrences are worked out from this appointment, and are not stored.  Occurrence 0 is this appointment.
    bool isRepeating() ;
    QDateTime occurrenceFrom(int n) ;

    // The zone a repeat is worked out in: its RepeatZone, or this machine's zone when it has none (repeats made before
    // the zone was stored).  A repeat keeps its clock time across daylight saving changes, as Google's does.
    QTimeZone repeatTimeZone() ;

    // The UTC instant of a wall-clock time in a zone.  A time the clocks skip (in spring) moves forward by an hour.
    static QDateTime wallClockToUtc(QDateTime local, QTimeZone zone) ;

    // True if occurrence n exists: it is within the repeat count, starts no later than the repeat's until date, and is
    // not one of the repeat's exceptions
    bool occurrenceExists(int n) ;

    // True once occurrence n is past the repeat's count or until date (an exception does not end a repeat)
    bool occurrenceEnded(int n) ;

    // Exceptions: the occurrences the repeat leaves out, by their starts in UTC
    bool isException(QDateTime start) ;
    void addException(QDateTime start) ;
    QList<QDateTime> exceptions() ;
    void setExceptions(QList<QDateTime> starts) ;

    // The number of the occurrence that starts at this time, or -1 if there is none
    int indexOfOccurrence(QDateTime start) ;

    // Ends the repeat before occurrence n, so that n and the later occurrences are not in it
    void endBefore(int n) ;

    // A copy of this appointment as a new one: a new ID, and none of the Google record of this one
    Appointment copyAsNew() ;

    // The first occurrence that is still to come, or an invalid time when there is none
    QDateTime nextOccurrence() ;

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

    QString& asText(QString name="", QString start="", QString mid="", QString end="", bool accessible=false) ;
    QString& asAccessibleText(QString name="") ;
    QString& shortText() ;
    QString& asHTML(QString name="") ;


    // Returns true if the appoinmentrecord 'i' is of type matype
    bool isAppointmentOfType(AppointmentRecord i, int matype) ;

    // Test current appointment against reference
    bool matches(Appointment &with, int matype) ;
    QString mismatch(Appointment &with, int matype, bool showboth=false) ;

    // Copy selected fields to new contact
    bool copyTo(Appointment &other, int matype) ;

    // Takes in the sent state held for the same appointment elsewhere (on Google), so that a message sent there is not
    // sent again from here.  Each kind of state keeps the later of the two, and the lists keep both.  Returns true if
    // anything changed.  Nothing is taken in while a sent state is pending, as the local one is then the newer.
    bool mergeSentState(Appointment &remote) ;

    // Returns true if appointments clash
    bool clashes(Appointment& with) ;

    // Operators
    int operator==(const Appointment &rhs) const;
    int operator<(const Appointment &rhs) const;

};

#endif // APPOINTMENT_H
