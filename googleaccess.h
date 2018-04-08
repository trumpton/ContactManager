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

class GoogleAccess
{

public:
    enum googleAction {
        Post = 0,       // Create New Contact
        Create = 0,
        Put = 1,        // Update / Change Contact
        Update = 1,
        Delete = 2      // Delete Contact
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

    // Contact and Calendar Sync Tokens
    QString contactsynctoken, calendarsynctoken ;

    // Gets an access token, using the refresh token
    void googleGetAccessToken() ;
    QString accesstoken  ;

    // Get all contacts from server, and save in QString and list of Contacts
/*    QList<Contact> *convertXmlToContacts() ;
    QString googleContactsXml ;
    QList<Contact> contactlist ;
*/
    // Parse JSon
    bool parseContact(QString &json, QString &tag, Contact &contact) ;
    bool parseContact(QJsonObject &item, Contact &contact) ;

    // Get all appointments from server, and save in QString and list of Appointments
//    QList<Appointment> &convertJsonToAppointments(QString &json) ;
//    QString googleAppointmentJson ;
//    QList<Appointment> appointmentlist ;

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

/*    QString googleGroupsXml ;
    QString getGroupsXml() ;
    QString getContactsXml() ;
*/

    // XML Helper functions
    QDomElement domSearchChild(QDomElement &node, QString tag, QString rellabel=QString(), int occurrence=1) ;
    bool updateElementTextAttribute(QDomDocument doc, QDomElement documentroot, char *thistagname, QString value, char *rellabel, char *attribute=(char *)0) ;
    bool updateElementAttribute(QDomDocument doc, QDomElement documentroot, char *thistagname, char *attribute, QString value, char *rellabel=(char *)0) ;
    bool updateElementText(QDomDocument doc, QDomElement documentroot, char *thistagname, QString value, char *rellabel=(char *)0) ;
    bool updateElementText2(QDomDocument doc, QDomElement documentroot, char *parenttagname, char *thistagname, QString value, char *rellabel=(char *)0) ;
    bool updateElementHrefFlag(QDomDocument doc, QDomElement documentroot, QString thistagname, QString attributevalue, bool set) ;

  public:

    // Constructor, sets the refresh token which has been
    // previously generated with googleAuthorise.
    GoogleAccess();
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
    bool getCalendar(Calendar &googlecalendar) ;

    // Download the appointment based on Google Details
    bool getAppointment(Appointment& appt) ;

    // Update Appointment to Google Server
    bool updateAppointment(Appointment &appt, googleAction action) ;

    // Get all contacts from Google server
    bool getContacts(ContactDatabase &googlecontacts, int pass=0) ;

    // Download the contact based on Google Details
    bool getContact(Contact& contact) ;

    // Update contact on Google Server
    bool updateContact(Contact &contact, googleAction action) ;

    // Sets and retrieves the sync tokens
    void setContactSyncToken(QString& token) ;
    void setCalendarSyncToken(QString& token) ;
    QString& getContactSyncToken() ;
    QString& getCalendarSyncToken() ;

    // Useful debug functions
    QString debugGetDataResponse() ;
    QString debugPutPostDataResponse() ;

};

#endif // GOOGLEACCESS_H
