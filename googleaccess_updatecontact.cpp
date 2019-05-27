//=====================================================================================================
//
// Public: GoogleAccess - updateContact
//
// DESCRIPTION:
//
//  Creates or updates the contact associated with the contact details supplied.
//
// PARAMETERS:
//
//    contact   -   The contact structure to use in the update
//    action    -   The required action (Create/Put, Update/Patch, Remove/Delete)
//    etag      -   The etag for the request (applicable for Updates)
//
// API CALLS:
//
//    https://people.googleapis.com/v1/ID:deleteContact (GoogleAccess::Delete)
//    https://people.googleapis.com/v1/ID:updateContact (GoogleAccess::Patch)
//    https://people.googleapis.com/v1/people:createContact (GoogleAccess::Put)
//

// TODO:
//    Should not call parseContact (this should be left to getcontacts or validatecontacts)
//

#include "googleaccess.h"
#include "../Lib/supportfunctions.h"

#include <QUrl>
#include <QJsonDocument>
#include <QJsonValueRef>
#include <QJsonObject>
#include <QJsonArray>

// Trap ID For Testing Breakpoint
#define DEBUGTRAPID "f9acd413-4f83-47e7-a62e-f653d59c6044"


bool GoogleAccess::updateContact(Contact &contact, googleAction action, QString etag)
{
    QString googlerecordid = contact.getField(Contact::GoogleRecordId) ;
    QString url ;
    bool success=true ;
    QString personFields="userDefined" ;

#ifdef DEBUGTRAPID
    if (contact.getField(Contact::ID).compare(DEBUGTRAPID)==-0) {
        QString id = DEBUGTRAPID ; // Debug Trap
    }
#endif

    // Request to Update a Deleted entry is actually invalid
    // TODO: ERROR TRAP RATHER THAN OVERRIDE
    if (contact.isSet(Contact::Deleted) || contact.isSet(Contact::Hidden))
        action=GoogleAccess::Delete ;

    // Calculate URL
    if (action==GoogleAccess::Delete) {
        url = "https://people.googleapis.com/v1/" + googlerecordid + ":deleteContact" ;
    } else if (action==GoogleAccess::Post) {
        // Create New Contact
        url = "https://people.googleapis.com/v1/people:createContact" ;
    } else if (action==GoogleAccess::Patch) {
        // Modify Contact
        QString googlerecordid = contact.getField(Contact::GoogleRecordId) ;
        url = "https://people.googleapis.com/v1/" + googlerecordid + ":updateContact" + "?updatePersonFields={{PFD}}"  ;
    } else {        
        // Error
        qFatal("Call to updateContact with invalid googleAction") ;
        return false ;
    }

    // Process Request
    if (action==GoogleAccess::Delete) {

        addLog(QString("GoogleAccess::updateContact: (") + contact.getFormattedName(false) + ", " + contact.getField(Contact::ID) + ", " + contact.getField(Contact::GoogleRecordId) + ") - Delete") ;

        googlePutPostDelete(url, action, QString(""), QString("updateContact-Delete-") + contact.getFormattedName(false,false).replace(" ","")) ;

        if (getNetworkError().isEmpty()) {
            // Deletion success
            addLog("    Delete OK") ;
            success=true ;
        } else {
            if (getNetworkErrorCode()==404) {
                // Record has already been purged (does not exist)
                addLog("    Already Deleted 404 OK") ;
                success=true ;
            } else {
                addLog("    Delete ERROR - " + getNetworkError()) ;
                success=false ;
            }
        }

     } else {

        // Put / Post / Patch

        QJsonObject entry ;

        // ETag
        if (action==googleAction::Patch) {
            addLog(QString("GoogleAccess::updateContact: (") + contact.getFormattedName(false) + ", " + contact.getField(Contact::ID) + ", " + contact.getField(Contact::GoogleRecordId) + ") - Patch") ;
            if (etag.isEmpty()) {
                success = false ;
            } else {
                entry.insert("etag", etag) ;
            }
        } else {
            addLog(QString("GoogleAccess::updateContact: (") + contact.getFormattedName(false) + ", " + contact.getField(Contact::ID) + ", " + contact.getField(Contact::GoogleRecordId) + ") - Put/Post") ;
        }

        // "names": [{
        //    "familyName": "SURNAME",
        //    "givenName": "FIRSTNAME",
        //    "middleName": "MIDDLENAME"
        // }],
        QJsonArray a_names ;
        QJsonObject o_name ;
        o_name.insert("familyName", contact.getField(Contact::Surname)) ;
        o_name.insert("givenName", contact.getField(Contact::Names)) ;
        // TODO: Split Contact::Names to givenName and middleName
        a_names.append(o_name) ;
        if (a_names.size()>0) {
            entry.insert("names", a_names) ;
        }
        personFields = personFields + ",emailAddresses" ;


        // "emailAddresses": [{
        //     "value": "EMAILADDRESS",
        //     "type": "Email/Other",
        // }],
        QJsonArray a_emails ;

        if (!contact.getField(Contact::Email).isEmpty()) {
            QJsonObject o_email1 ;
            o_email1.insert("type", "Email") ;
            o_email1.insert("value", contact.getField(Contact::Email)) ;
            QJsonObject metadata ;
            metadata.insert("primary", true) ;
            o_email1.insert("metadata", metadata) ;
            a_emails.append(o_email1) ;
        }

        if (!contact.getField(Contact::Email2).isEmpty()) {
            QJsonObject o_email2 ;
            o_email2.insert("type", "Other") ;
            o_email2.insert("value", contact.getField(Contact::Email2)) ;
            a_emails.append(o_email2) ;
        }
        if (a_emails.size()>0) {
            entry.insert("emailAddresses", a_emails) ;
        }
        personFields = personFields + ",emailAddresses" ;

        // "sipAddresses": [{
        //    "value": string,
        //    "type": string,
        // }],
        QString& sipaddress = contact.getField(Contact::Voip) ;
        if (!sipaddress.isEmpty()) {
            QJsonArray a_sipaddresses ;
            QJsonObject o_sipaddress ;
            o_sipaddress.insert("type", "sip") ;
            o_sipaddress.insert("value", sipaddress) ;
            a_sipaddresses.append(o_sipaddress) ;
            entry.insert("sipAddresses", a_sipaddresses) ;
        }
        personFields = personFields + ",sipAddresses" ;

        // "phoneNumbers": [{
        //     "value": "PHONENUMBERWITHSPACES",
        //     "canonicalForm": "PHONENUMBER",
        //     "type": "TYPE"
        // }],
        QJsonArray a_numbers ;
        for (int i=Contact::Phone; i<=Contact::Phone4; i++) {
            QString& phone = contact.getField((Contact::ContactRecord)i) ;
            if (!phone.isEmpty()) {
                QJsonObject o_phone ;
                o_phone.insert("value", phone) ;
                QString type ;
                switch (i) {
                  case Contact::Phone: type="home" ; break ;
                  case Contact::Work: type="work" ; break ;
                  case Contact::Mobile: type="mobile" ; break ;
                  case Contact::Phone2: type=contact.getField(Contact::Phone2Title) ; break ;
                  case Contact::Phone3: type=contact.getField(Contact::Phone3Title) ; break ;
                  case Contact::Phone4: type=contact.getField(Contact::Phone4Title) ; break ;
                }
                o_phone.insert("type", type) ;
                a_numbers.append(o_phone) ;
            }
        }
        if (a_numbers.size()>0) {
            entry.insert("phoneNumbers", a_numbers) ;
        }
        personFields = personFields + ",phoneNumbers" ;

        // "userDefined": [{
        //       "key": "CUSTOMLABEL1",
        //       "value": "CUSTOM1"
        // },{
        //       "key": "CUSTOMLABEL2",
        //       "value": "CUSTOM2"
        // }]

        // Contact ID & Hidden
        QJsonArray properties ;

        QString textme = contact.getField((Contact::TextMe)) ;
        QString emailme = contact.getField((Contact::EmailMe));
        QString hidden = contact.getField((Contact::Hidden)) ;

        QJsonObject textmeproperty ;
        textmeproperty.insert("key", "textme") ;
        textmeproperty.insert("value", textme) ;
        properties.append(textmeproperty) ;

        QJsonObject emailmeproperty ;
        emailmeproperty.insert("key", "emailme") ;
        emailmeproperty.insert("value", emailme) ;
        properties.append(emailmeproperty) ;

        QJsonObject idproperty ;
        idproperty.insert("key", "id") ;
        idproperty.insert("value", contact.getField(Contact::ID)) ;
        properties.append(idproperty) ;

        QJsonObject hiddenproperty ;
        hiddenproperty.insert("key", "hidden") ;
        hiddenproperty.insert("value", hidden) ;
        properties.append(hiddenproperty) ;

        entry.insert("userDefined", properties) ;


        // "organizations": [{
        //    "metadata": { "source": { "type": "CONTACT" }},
        //    "type": "Work",
        //    "name": "ORGANISATION"
        // }],
        QString& organisationstr = contact.getField(Contact::Organisation) ;
        if (!organisationstr.isEmpty()) {
            QJsonArray organisations ;
            QJsonObject organisation ;
            QJsonObject gdorgname ;
            organisation.insert("name", organisationstr) ;
            organisation.insert("type", "work") ;
            organisations.append(organisation) ;
            entry.insert("organizations", organisations) ;
        }
        personFields = personFields + ",organizations" ;

        // "urls": [ {
        //    "metadata": {
        //      "source": { "type": "CONTACT" }
        //    },
        //    "value": "http://mywebsite/",
        //    "type": "homePage"
        //   }],
        QString& websitevalue = contact.getField(Contact::Webaddress) ;
        if (!websitevalue.isEmpty()) {
            QJsonArray urls ;
            QJsonObject item ;
            item.insert("value", websitevalue) ;
            item.insert("type", "homePage") ;
            urls.append(item) ;
            entry.insert("urls", urls) ;
        }
        personFields = personFields + ",urls" ;

        // "birthdays": [{
        //    "metadata": { "source": { "type": "CONTACT" }},
        //    "date": {
        //        "year": YYYY,
        //        "month": MM,
        //        "day": DD
        //    }
        // }],
        QString& dob = contact.getField(Contact::Birthday) ;
        if (!dob.isEmpty()) {
            // yyyy-mm-dd
            QStringList dmy = dob.split('-') ;
            QJsonArray birthdays ;
            QJsonObject date ;
            QJsonObject datedetails ;
            int d=1, m=1, y=0 ;
            if (dmy.size()==3) {
                if (!dmy.at(0).isEmpty()) y = dmy.at(0).toInt() ;
                if (!dmy.at(1).isEmpty()) m = dmy.at(1).toInt() ;
                if (!dmy.at(2).isEmpty()) d = dmy.at(2).toInt() ;
            }
            if (y>0) datedetails.insert("year", y) ;
            datedetails.insert("month", m) ;
            datedetails.insert("day", d) ;
            date.insert("date", datedetails) ;
            birthdays.append(date) ;
            entry.insert("birthdays", birthdays) ;
        }
        personFields = personFields + ",birthdays" ;

        // "biographies": [{
        //    "metadata": { "source": { "type": "CONTACT" }},
        //      "value": "NOTES",
        //      "contentType": "TEXT_PLAIN"
        // }],
        QString& comments = contact.getField(Contact::Comments) ;
        if (!comments.isEmpty()) {
            QJsonArray biographies ;
            QJsonObject item ;
            item.insert("value", comments) ;
            item.insert("contentType", "TEXT_PLAIN") ;
            biographies.append(item) ;
            entry.insert("biographies", biographies) ;
            personFields = personFields + ",biographies" ;
        }

        // "addresses": [{
        //    "metadata": { "source": { "type": "CONTACT" }},
        //    "type": "Address/Other",
        //    "formattedValue": "house street, pobox, street line 2, town, POSTCODE, Country"
        // }],
        QJsonArray addresses ;
        for (int i=Contact::Address; i<=Contact::Address2; i++) {
            QString& address = contact.getField((Contact::ContactRecord)i) ;
            if (!address.isEmpty()) {
                QJsonObject item ;
                item.insert("formattedValue", address) ;
                switch (i) {
                case Contact::Address:
                    item.insert("type", "Address") ;
                    break ;
                case Contact::Address2:
                    item.insert("type", "Other") ;
                    break ;
                }
                addresses.append(item) ;
            }
        }
        if (addresses.size()>0) {
            entry.insert("addresses", addresses) ;
        }
        personFields = personFields + ",addresses" ;

        QString jsonresponse ;
        Contact googlecontact ;

        if (success) {
            // Encoding & Entry
            QJsonDocument doc ;

            doc.setObject(entry) ;
            QString jsontext = doc.toJson() ;

            // createnew = POST, Update = PUT
            jsonresponse = googlePutPostDelete(url.replace("{{PFD}}", personFields), action, jsontext, QString("updateContact-PutPostPatch-")+contact.getFormattedName(false,false).replace(" ","")) ;
            if (getNetworkError().isEmpty()) {
                addLog("    Put/Post/Patch OK") ;
            } else {
                addLog("    Put/Post/Patch ERROR - " + getNetworkError()) ;
                success = false ;
            }
        }

        if (success) {
            // Decode Response
            QString tag = "" ;
            if (parseContact(jsonresponse, tag, googlecontact)) {
                contact.setField(Contact::GoogleRecordId, googlecontact.getField(Contact::GoogleRecordId)) ;
                QString mismatch = contact.mismatch(googlecontact, Contact::mcDetails|Contact::mcId, true) ;
                if (mismatch.isEmpty()) {
                    addLog(QString("    parseContact OK ")) ;
                } else {
                    addLog(QString("    parseContact ERROR (local/google): ") + mismatch) ;
                    success=false ;
                }
            } else {
                addLog("    parseContact ERROR: Unable to decode") ;
                success = false ;
            }
        }

        if (success) {
        }
    }

    return success ;
}

