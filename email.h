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
    //           0 - Not Sent: email is not configured
    //          -2 - Not Sent: could not connect to the server
    //          (-4 is also used when the server stopped replying, as the message may have been accepted)
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
