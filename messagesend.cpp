#include "messagesend.h"

int sendChannelMessage(MessageTransport &transport, bool byemail, const QString &sender, const QString &address,
                       const QString &subject, const QString &body, QString &errortext)
{
    if (sender.trimmed().isEmpty()) {
        errortext = QString("not sent - sender missing from ContactManager.ini (set sender= in the [%1] section)")
                    .arg(byemail ? "email-reminder" : "sms-reminder") ;
        return MESSAGE_RESULT_NO_SENDER ;
    }

    if (byemail) return transport.sendEmail(sender, address, subject, body, errortext) ;
    return transport.sendSMS(address, sender, subject, errortext) ;
}

bool splitMessageTemplate(const QString &text, QString &subject, QString &body)
{
    QStringList lines = text.split('\n') ;
    subject = lines.isEmpty() ? QString() : lines.at(0).trimmed() ;

    body.clear() ;
    for (int i=1; i<lines.size(); i++) {
        body += lines.at(i).trimmed() + "\n" ;
    }
    body = body.trimmed() ;

    return !subject.isEmpty() ;
}

QStringList missingSenderWarnings(const QString &emailSender, const QString &smsSender)
{
    QStringList warnings ;
    if (emailSender.trimmed().isEmpty()) {
        warnings << "WARNING: sender= is blank in the [email-reminder] section of ContactManager.ini, so no email messages will be sent" ;
    }
    if (smsSender.trimmed().isEmpty()) {
        warnings << "WARNING: sender= is blank in the [sms-reminder] section of ContactManager.ini, so no SMS messages will be sent" ;
    }
    return warnings ;
}
