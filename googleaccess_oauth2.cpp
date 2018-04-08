
#include "googleaccess.h"
#include "../Lib/supportfunctions.h"
#include "configuration.h"

#include <QMessageBox>
#include <QNetworkReply>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrlQuery>

#include "googleaccess-account.h"

//
// Useful Links
//
// The oauth2 playground allows you to walk through all submissions
// accesses manually, and gives a good overview of how the protocols
// actually work:
//
//     https://developers.google.com/oauthplayground/
//
// https://developers.google.com/google-apps/contacts/v3/?hl=ja#updating_contacts
//


// OAuth2 / Google - Network Access Helper Functions
//
// setupRToken          - Sets the refresh token, previously generated with Authorise
// Authorise            - allows user to log in and authorise use.  Returns refresh token/email
// googleGetAccessToken - Retrieve the access token for the session
//
//


//=====================================================================================================
//
// Public - GoogleAccess - setupRToken
//
// Constructor, sets the refresh token which has been
// previously generated with Authorise.
//
void GoogleAccess::setupRToken(const QString& rt)
{
    QStringList rtparts = rt.split(" ") ;
    if (rtparts.size()==2) {
      refreshtoken=rtparts.at(0) ;
      username=rtparts.at(1) ;
    }
}


//=====================================================================================================
//
// Public: GoogleAccess - Authorise
//
//  Get the authorisation token, by popping up a dialog box to prompt the
//  user to visit an authorisation url, and enter the response code.
//
//  Returns a refresh_token, which is valid indefinately, and enables the app
//  to gain access again and again without logging in.
//
QString GoogleAccess::Authorise()
{
    QString resultstring  ;

    QString device_code ;
    QString user_code ;
    QString verification_url ;
    QString expires_in ;
    QString interval ;

    refreshtoken="" ;
    accesstoken="" ;
    username="" ;
    refreshtokenandusername="" ;

    // Get the authorisation url and user code

    {
      QNetworkReply *reply ;
      QEventLoop eventLoop ;
      QNetworkAccessManager manager ;
      QUrlQuery params ;
      QUrl url("https://accounts.google.com/o/oauth2/device/code") ;
      QNetworkRequest request(url) ;
      params.addQueryItem("client_id", CLIENTID);
      params.addQueryItem("scope", SCOPE);
      request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
      QObject::connect(&manager, SIGNAL(finished(QNetworkReply *)), &eventLoop, SLOT(quit()));
      reply = manager.post(request, params.query(QUrl::FullyEncoded).toUtf8());

      eventLoop.exec() ;
      QNetworkReply::NetworkError err = reply->error() ;
      QString errstring = reply->errorString() ;
      resultstring = reply->readAll() ;
      device_code = ExtractParameter(resultstring, "device_code") ;
      user_code = ExtractParameter(resultstring, "user_code") ;
      verification_url = ExtractParameter(resultstring, "verification_url") ;
      expires_in = ExtractParameter(resultstring, "expires_in") ;
      interval = ExtractParameter(resultstring, "interval") ;
    }

    if (user_code.isEmpty()) {
        addLog("Unable to authorise with Google.  Network error or missing ssleay32.dll and libeay32.dll") ;
        errorOkDialog(NULL, "Contact Manager Error", "Unable to connect.  Network error or missing ssleay32.dll and libeay32.dll") ;
        return refreshtokenandusername ;
    }

    // Prompt the user to authenticate, using the code

    // TODO: Check errstring / resultstring and report
    // particularly if SSL DLLs aren't working

    QMessageBox mb ;
    mb.setTextFormat(Qt::RichText) ;
    mb.setTextInteractionFlags(Qt::TextBrowserInteraction) ;
    QString str = QString("<p>1. Connect to <a href=\"") + verification_url + QString("\"><font size=\"+1\">") + verification_url + QString("</font></p>") +
                  QString("<p>2. Sign-in, and Enter the following code when prompted</p>") +
                  QString("<p align=\"center\"><font size=\"+2\" color=\"blue\"><b>") + user_code + QString("</b></font></p>") +
                  QString("<p>3. Select <i>Allow</i> to enable Contact Manager to access your contacts / calendars</p>") +
                  QString("<p>4. And then press OK <i>below</i> to continue when complete</p>") ;
    mb.setText(str) ;
    if (!mb.exec()) {
        return refreshtokenandusername ;
    }




    // TODO: UP TO HERE IN FLOW

    // Get the refresh and access tokens
    {
      QNetworkReply *reply ;
      QEventLoop eventLoop ;
      QNetworkAccessManager manager ;
      QUrlQuery params ;
      QUrl url("https://www.googleapis.com/oauth2/v4/token") ;
      QNetworkRequest request(url) ;
      params.addQueryItem("client_id", CLIENTID);
      params.addQueryItem("client_secret", SECRET);
      params.addQueryItem("code", device_code);
      params.addQueryItem("grant_type", "http://oauth.net/grant_type/device/1.0");
      request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
      QObject::connect(&manager, SIGNAL(finished(QNetworkReply *)), &eventLoop, SLOT(quit()));
      reply = manager.post(request, params.query(QUrl::FullyEncoded).toUtf8());
      eventLoop.exec() ;
      resultstring = reply->readAll() ;

      refreshtoken = ExtractParameter(resultstring, "refresh_token") ;
      accesstoken = ExtractParameter(resultstring, "access_token") ;

    }

    // Above should have been polled ....


    // Get the username (i.e. the login email address)
    {
        QString result ;
        result = googleGet("https://www.googleapis.com/oauth2/v3/userinfo") ;
        username = ExtractParameter(result, "email") ;
    }

    if (username.isEmpty()) {
        addLog("Unable to authorise with Google.  Network error or missing ssleay32.dll and libeay32.dll") ;
    }

    if (gConf->debugGoogleEnabled()) {
        writeToFile(gConf->getDatabasePath() + "/googleauthenticationresponse.txt", QString("authentication:\n") + resultstring + QString("\nemail: ") + username) ;
    }

    refreshtokenandusername = refreshtoken + " " + username ;
    return refreshtokenandusername ;
}


//=====================================================================================================
//
// Private: GoogleAccess - googleGetAccessToken
//
//  Updates the access token, based on the authorisation token
//

void GoogleAccess::googleGetAccessToken()
{
    QNetworkReply *reply ;
    QEventLoop eventLoop ;
    QNetworkAccessManager manager ;
    QUrlQuery params ;

    accesstoken="" ;

    if (refreshtoken.isEmpty()) {
        errorstatus="Google account not set up in File/Setup (invalid refresh token)" ;
        return ;
    }

    QUrl url("https://accounts.google.com/o/oauth2/token") ;
    QNetworkRequest request(url) ;
    params.addQueryItem("client_id", CLIENTID);
    params.addQueryItem("client_secret", SECRET);
    params.addQueryItem("refresh_token", refreshtoken);
    params.addQueryItem("grant_type", "refresh_token");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QObject::connect(&manager, SIGNAL(finished(QNetworkReply *)), &eventLoop, SLOT(quit()));
    reply = manager.post(request, params.query(QUrl::FullyEncoded).toUtf8());
    eventLoop.exec() ;

    switch (reply->error()) {
    case QNetworkReply::ConnectionRefusedError:
        errorstatus="Connection Refused" ;
        break ;
    case QNetworkReply::RemoteHostClosedError:
        errorstatus="Remote Host Closed Connection" ;
        break ;
    case QNetworkReply::HostNotFoundError:
        errorstatus="Host accounts.google.com Not Found" ;
        break ;
    case QNetworkReply::UnknownServerError:
        errorstatus="Unknown Server Error" ;
        break ;
    default:
        QVariant replycode=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) ;
        if (replycode>=200 && replycode<=299) {
            QString resultstring = reply->readAll() ;
            accesstoken = ExtractParameter(resultstring, "access_token") ;
            if (accesstoken.isEmpty()) {
                errorstatus="Google access token not found." ;
            }
        } else {
            errorstatus="Error " + replycode.toString() ;
        }
        break ;
    }


}

//=====================================================================================================
//
// Private: GoogleAccess - ExtractParameter
//
// Parse the supplied response, and extract the JSON parameter identified
//

QString GoogleAccess::ExtractParameter(QString Response, QString Parameter, int Occurrence)
{
    QRegExp rx ;
    QStringList records;
    QString record ;
    QString pattern ;

    extracttokenresult="" ;
    if (Response.isEmpty()) return extracttokenresult ;

    // Remove \n and
    // Extract the Occurrenceth set of {}
    records = Response.replace("\n","").split("{") ;
    int numrecords = records.size() ;
    if (Occurrence>=(numrecords) || Occurrence<1) return extracttokenresult ;
    record=records[Occurrence] ;

    // Find "parameter" : "xxxx",
    // Or "parameter" : "xxxx"}
    pattern = "\"" + Parameter + "\" *: *\"(.*)\"" ;
    rx.setPattern(pattern) ;
    rx.setMinimal(true) ;
    if (rx.indexIn(record)>=0) extracttokenresult = rx.cap(1) ;
    return extracttokenresult ;
}

