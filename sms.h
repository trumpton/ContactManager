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
    bool send(QString number, QString from, QString message) ;
    QString& getErrorMessage() ;
    int getBalance() ;
    bool checkCredits() ;


private:
    enum Agent agent ;
    QString username ;
    QString password ;
    QString smsErrorData ;
    bool sendClockwork(QString number, QString from, QString message) ;
    int getClockworkBalance() ;
};

#endif // SMS_H
