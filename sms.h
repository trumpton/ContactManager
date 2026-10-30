#ifndef SMS_H
#define SMS_H

#include <QString>

class SMS
{
public:
    // The SMS services that can send messages.  The agent name in the ini file selects one (see the constructor).
    enum Agent {
        Unknown=0,
        Voodoo=1,
        Esendex=2
    };

    // Returned by send and getBalance when the agent is not known
    enum Status {
        BadAgent=-5
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
    //          -5 - Bad Agent: the agent named in the ini file is not known, or is blank
    //           1 - Send OK

    int send(QString number, QString from, QString message) ;
    QString& getErrorMessage() ;

    // Returns the balance as an integer in PENNIES, so 1250 is £12.50.  Keep it in pennies: the reminder code
    // warns of low credit when it is under 100.  Returns a negative code (as for send) if the balance cannot be read.
    // Voodoo's balance is decimal, and is converted to pennies here.
    int getBalance() ;

    bool checkCredits() ;

    // A mobile number as Voodoo wants it: digits only, with the country code and no leading 0 or +.  A national number
    // (starting with 0) gets the country code, e.g. 07700900000 with countrycode 0044 gives 447700900000.
    static QString internationalNumber(QString number, QString countrycode) ;


private:
    enum Agent agent ;
    QString username ;
    QString password ;
    QString smsErrorData ;
    int sendVoodoo(QString number, QString from, QString message) ;
    int getVoodooBalance() ;
    int sendEsendex(QString number, QString from, QString message) ;
    int getEsendexBalance() ;
};

#endif // SMS_H
