/*
 *
 *  TODO:
 *
 *   Retrieve data into DOM structure from Google - OK, in function xxxx
 *   Create a new blank DOM structure - OK, just creates an entry tag
 *   Search a DOM structure for a named tag/label/rel - OK, but only returns first
 *   Modify the found element of DOM structure - No
 *   Insert an element in the DOM structure if not found - No
 *
 */


#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include "configuration.h"
#include "calendar.h"

//
// Public: GoogleAccess - Constructor
//
//  Initialises, and saves the refresh token to the internal data structures
//  if it has been provided.  If it is not provided, no function will work, and
//  a refresh token must be acquired by calling the Authorise function.
//
GoogleAccess::GoogleAccess()
{
    username="" ;
    refreshtoken="" ;
    errorstatus="" ;
    accesstoken="" ;
    refreshtokenandusername = "" ;
    contactsynctoken="" ;
    calendarsynctoken="" ;
}


//=====================================================================================================
//
// Public: setContactSyncToken
// Public: setCalendaSyncToken
// Public: getContactSyncToken
// Public: getCalendaSyncToken
//
//   Sets and retrieves the contact and calendar sync tokens
//

void GoogleAccess::setContactSyncToken(QString& token)
{
    contactsynctoken=token ;
}

void GoogleAccess::setCalendarSyncToken(QString& token)
{
    calendarsynctoken=token ;
}

QString& GoogleAccess::getContactSyncToken()
{
    return contactsynctoken ;
}

QString& GoogleAccess::getCalendarSyncToken()
{
    return calendarsynctoken ;
}

//=====================================================================================================
//
// Public: getUsername
//
//   Returns the username associated with the current google login
//
QString GoogleAccess::getUsername()
{
    return username ;
}


//=====================================================================================================
//
// Public: debugGetDataResponse & debugPutDataResponse
//
//  Debug functions
//
QString GoogleAccess::debugGetDataResponse()
{
    return googleGetResponse ;
}

QString GoogleAccess::debugPutPostDataResponse()
{
    return googlePutPostResponse ;
}
