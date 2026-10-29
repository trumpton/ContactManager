#ifndef EMAIL_H
#define EMAIL_H

#include <QString>

// Sends email through the embedded SMTP client.  The server, username, password and port come from the settings.
class Email
{
public:
    Email(QString server, QString username, QString password, int port) ;

    // Returns: -1 - Not Started Sending
    //          -2 - Network Error
    //          -3 - Error during transaction
    //          -4 - Unable to Send / Possibly Sent
    //           1 - Send OK
    //           0 - Not Sent (email not configured, or no connection)
    int send(QString from, QString to, QString subject, QString body) ;

    QString& getErrorMessage() ;

private:
    QString server ;
    QString username ;
    QString password ;
    int port ;
    QString errormessage ;
};

#endif // EMAIL_H
