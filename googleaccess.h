#ifndef GOOGLEACCESS_H
#define GOOGLEACCESS_H

#include <QJsonObject>
#include <QDomElement>
#include <QString>
#include <QList>

#include "contact.h"
#include "appointment.h"
#include "calendar.h"
#include "contactdatabase.h"

// Contact Details
// Requested parameters for person, from: addresses,ageRanges,biographies,birthdays,
// braggingRights,coverPhotos,emailAddresses,events,genders,imClients,interests,locales,
// memberships,metadata,names,nicknames,occupations,organizations,phoneNumbers,photos,
// relations,relationshipInterests,relationshipStatuses,residences,sipAddresses,skills,
// taglines,urls,userDefined
//#define ALLPERSONFIELDS "addresses,ageRanges,biographies,birthdays,,braggingRights,coverPhotos,emailAddresses,events,genders,imClients,interests,locales,memberships,metadata,names,nicknames,occupations,organizations,phoneNumbers,photos,relations,relationshipInterests,relationshipStatuses,residences,sipAddresses,skills,taglines,urls,userDefined"
#define PERSONFIELDS "addresses,biographies,birthdays,emailAddresses,events,memberships,metadata,names,organizations,phoneNumbers,sipAddresses,urls,userDefined"
#define GROUPPERSONFIELDS "memberships,userDefined"

class GoogleAccess
{

public:
    enum googleAction {
        Post = 0,      // Create New Contact / Calendar Entry
        Put = 1,       // Not Used / Update Calendar Entry
        Delete = 2,    // Delete Contact / Calendar Entry
        Patch = 3      // Update Contact

    };

private:

    // Details received following google authorisation
    // either from the authorisation, or retrieved from
    // storage and passed to the constructor. Used by:
    //    GoogleAccess(QString rtoken);
    //    QString googleAuthorise() ;
    //    QString getUsername() ;
    QString username, refreshtoken, refreshtokenandusername ;
    QString errorstatus ;
    bool connectionerror ;
    int errorcode ;
    int logsequencenumber ;

    // Groups update record
    QString addedstr, deletedstr ;

    // Group IDs
    QString gidbusiness, gidclient, gidfamily, gidfriend, gidother ;

    // Gets an access token, using the refresh token
    void googleGetAccessToken() ;
    QString accesstoken  ;

    // Parse JSon
    bool parseContact(QString &json, QString &tag, Contact &contact) ;
    bool parseContact(QJsonObject &item, Contact &contact) ;

    // Check metadata source of entry
    QString getMetadataField(QJsonValue object, bool primary=false) ;
    bool isMetadataFromContacts(QJsonValue object) ;
    bool isMetadataFromProfile(QJsonValue object) ;
    bool isMetadataPrimary(QJsonValue object) ;

    // Parse JSon
    bool parseAppointment(QString &json, Appointment &appt, QString account) ;
    bool parseAppointment(QJsonObject &item, Appointment &appt, QString account) ;

    // Extracts the specified parameter from the occurrenceth record in response
    QString ExtractParameter(QString Response, QString Parameter, int Occurrence=1) ;
    QString extracttokenresult ;

    // Fetch information via a http get
    QString googleGet(QString link, QString logfile = "") ;
    QString googleGetResponse ;

    // Fetch information via an http put or post
    QString googlePutPostDelete(QString link, enum googleAction action, QString data = "", QString logfile = "") ;
    QString googlePutPostResponse ;

    // Load the groups
    QStringList groupslist ;
    QString groupfindresult ;
    bool loadGroups() ;
    QString& findMyContactsId() ;
    QString& findGroupName(QString id) ;
    QString& findGroupId(QString name) ;

  public:

    // Constructor, sets the refresh token which has been
    // previously generated with googleAuthorise.
    GoogleAccess();


private:
    // Unused copy construct
    GoogleAccess(GoogleAccess& other) ;
    GoogleAccess& operator =(const GoogleAccess &rhs) ;

public:
    void setupRToken(const QString& rt) ;

    // Pop up authorisation dialog, and return a refresh_token
    // which must be saved for future use.
    QString Authorise() ;

    // Return the username associated with the current
    // google account
    QString getUsername() ;

    // Return the last network error string, or "" if OK
    QString getNetworkError() ;
    int getNetworkErrorCode() ;

    // Returns true if last network error was network-connection related
    bool isConnectionError() ;

    // Get all calendar from Google server
    bool getCalendar(Calendar &googlecalendar, int startdays, int enddays) ;

    // Download the appointment based on Google Details
    bool getAppointment(Appointment& appt) ;

    // Update Appointment to Google Server
    bool updateAppointment(Appointment &appt, googleAction action) ;

    // Update group IDs from Google server
    bool getGroupIds() ;
    bool createGroupIds() ;

    // Get Group ID Info
    QString getGroupId(Contact::ContactRecord rec, bool asverbosename=false) ;


    // Get all contacts from Google server
    bool getContacts(ContactDatabase &googlecontacts) ;

    // Update contact on Google Server
    bool updateContact(Contact &contact, googleAction action, QString etag = QString("")) ;

    // Update a group on Google with all contact changes
    bool updateSingleGoogleContactGroup(Contact::ContactRecord rec, ContactDatabase &db, ContactDatabase &googledb) ;
    QString& contactsDeletedFromGroup() ;
    QString& contactsAddedToGroup() ;

    // Get Contact, and validate against reference if provided
    bool getContact(Contact& googlecontact, bool getgrouponly=false) ;

    // Useful debug functions
    void resetLogFiles() ;
    QString debugGetDataResponse() ;
    QString debugPutPostDataResponse() ;

};

#endif // GOOGLEACCESS_H
