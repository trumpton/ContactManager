//=====================================================================================================
//
// Public: GoogleAccess - getContacts
//
//  Retrieves entire contact list
//
// contact        - The contact structure to use in the update
//

#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include <QUrl>

#include <QUrl>
#include <QJsonDocument>
#include <QJsonValueRef>
#include <QJsonObject>
#include <QJsonArray>

bool GoogleAccess::getContacts(ContactDatabase &googlecontacts)
{
    QJsonParseError err;
    QJsonDocument doc ;
    QString json ;
    bool success=true ;

    googlecontacts.clear() ;

    if (refreshtoken.isEmpty()) return false ;

    QString url = "https://people.googleapis.com/v1/people/me/connections" ;
    url = url + "?pageSize=1000" ;
    url = url + "&sortOrder=LAST_NAME_ASCENDING" ;
    url = url + QString("&personFields=") + QString(READPERSONFIELDS) ;

    // Page through the list, following nextPageToken
    QString pagetoken ;
    do {
        QString pageurl = url ;
        if (!pagetoken.isEmpty()) pageurl = url + "&pageToken=" + QString(QUrl::toPercentEncoding(pagetoken)) ;

        // If this returns 403, check that the API is enabled on google
        json = googleGet(pageurl, QString("getContacts")) ;
        if (!errorstatus.isEmpty()) return false ;

        doc = QJsonDocument::fromJson(json.toUtf8(), &err) ;

        if (err.error != QJsonParseError::NoError) return false ;
        if (doc.isNull()) return false ;
        if (!doc.isObject()) return false ;

        QJsonObject obj = doc.object() ;

        // Transfer the contacts to the ContactDatabase
        QJsonArray items = obj["connections"].toArray() ;
        foreach (const QJsonValue & value, items) {

            if (value.isObject()){
                QJsonObject item = value.toObject() ;
                Contact contact ;

                // Add the changes to the contacts list if the parse was
                // successful, even if the contact was deleted
                if (parseContact(item, contact)) {
                    Contact& search = googlecontacts.getContactByGoogleId(contact.getField(Contact::GoogleRecordId)) ;
                    if (search.isEmpty()) {
                        // Add new contact to google list
                        Contact& newcontact = googlecontacts.addContact() ;
                        contact.copyTo(newcontact, Contact::mcDetailsGroupProfile|Contact::mcId|Contact::mcGoogleId|Contact::mcEtag) ;
                    } else {
                        // Update contact google list, so just update it
                        contact.copyTo(search, Contact::mcDetailsGroupProfile|Contact::mcId|Contact::mcGoogleId|Contact::mcEtag) ;
                    }
                } else {
                    success=false ;
                }
            }
        }

        pagetoken = obj["nextPageToken"].toString() ;
        dbg(QString("TIMING getContacts: page of %1, more pages: %2").arg(items.size()).arg(!pagetoken.isEmpty())) ;

    } while (!pagetoken.isEmpty()) ;

    return success ;
}
