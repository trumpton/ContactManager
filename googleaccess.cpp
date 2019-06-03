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
#include <QDir>
#include <QFile>


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
    logsequencenumber = 1 ;
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
// Public: resetLogFiles & debugGetDataResponse & debugPutDataResponse
//
//  Debug functions
//

void GoogleAccess::resetLogFiles()
{
    QStringList nameFilter, files ;

    QDir logdir(gConf->getDatabasePath()) ;
    nameFilter << QString("????-*.cmlog") ;
    logdir.setNameFilters(nameFilter) ;
    files = logdir.entryList() ;
    for (int i=files.size()-1; i>=0; i--) {

        QString filename = gConf->getDatabasePath() + "/" + files.at(i) ;
        QFile::remove(filename) ;
    }

    logsequencenumber=1 ;
}


QString GoogleAccess::debugGetDataResponse()
{
    return googleGetResponse ;
}

QString GoogleAccess::debugPutPostDataResponse()
{
    return googlePutPostResponse ;
}
