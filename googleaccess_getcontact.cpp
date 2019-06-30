//=====================================================================================================
//
// Public: GoogleAccess - getContact
//
// DESCRIPTION:
//
//    Retrieves contact
//
// PARAMETERS:
//
//    googlecontact      -   The contact structure to store update
//
// API CALLS:
//
//    https://people.googleapis.com/v1/contactGroups/ID (Get)
//


#include "googleaccess.h"
#include "../Lib/supportfunctions.h"

#include <QUrl>
#include <QJsonDocument>
#include <QJsonValueRef>
#include <QJsonObject>
#include <QJsonArray>

bool GoogleAccess::getContact(Contact& googlecontact, bool getgrouponly)
{
    bool success=true ;
    QString verifyurl = "https://people.googleapis.com/v1/" + googlecontact.getField(Contact::GoogleRecordId) + "?personFields=" + (getgrouponly?GROUPPERSONFIELDS:READPERSONFIELDS) ;
    QString verifyjsonresponse ;

    verifyjsonresponse = googleGet(verifyurl, (getgrouponly?"getContact(Groups)":"getContact")) ;
    if (!getNetworkError().isEmpty()) { success=false ; }

    if (success) {
        QString tag="" ;
        if (!parseContact(verifyjsonresponse, tag, googlecontact)) { success=false ; }
    }

    return success ;
}
