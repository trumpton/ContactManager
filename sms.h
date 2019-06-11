#ifndef SMS_H
#define SMS_H

#include <QString>

class SMS
{
public:
    enum Agent {
        Clockwork=1,
        DEFAULT=1
    };

public:
    SMS(QString &agent, QString &username, QString& password);

private:
    // Unused copy construct
    SMS(const SMS& other) ;
    SMS& operator =(const SMS &rhs) ;

public:

    // Returns: -1 - Not Started Sending
    //          -2 - Network Error
    //          -3 - Error during transaction
    //          -4 - Unable to Send / Possibly Sent
    //           1 - Send OK

    int send(QString number, QString from, QString message) ;
    QString& getErrorMessage() ;
    int getBalance() ;
    bool checkCredits() ;


private:
    enum Agent agent ;
    QString username ;
    QString password ;
    QString smsErrorData ;
    int sendClockwork(QString number, QString from, QString message) ;
    int getClockworkBalance() ;
};

#endif // SMS_H
