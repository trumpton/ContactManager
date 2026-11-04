#ifndef MESSAGESEND_H
#define MESSAGESEND_H

#include <QString>
#include <QStringList>

// Result from sendChannelMessage when the sender is blank in ContactManager.ini ([email-reminder] or [sms-reminder] sender=).
// Nothing is sent.  Other results are those of the transport (1 sent, -4 may have been sent, otherwise a failure).
#define MESSAGE_RESULT_NO_SENDER -7

// The sending of email and SMS, behind an interface so that the tests can stub it.  The application's implementation
// (RealMessageTransport, in mainwindow_timer_alert.cpp) uses Email and SMS.  Each returns the result of the send (1 sent,
// -4 may have been sent, otherwise a failure), and sets error to the reason.
class MessageTransport
{
public:
    virtual ~MessageTransport() {}
    virtual int sendEmail(const QString &from, const QString &to, const QString &subject, const QString &body, QString &error) = 0 ;
    virtual int sendSMS(const QString &number, const QString &from, const QString &text, QString &error) = 0 ;
};

// Sends one message on one channel, through the transport.  The sender is the one from ContactManager.ini for the channel,
// and is never taken from the message.  A blank sender sends nothing, and gives MESSAGE_RESULT_NO_SENDER with the reason in
// errortext.  For email, the subject and body are sent; for SMS, the text is the subject (the first line of the template).
int sendChannelMessage(MessageTransport &transport, bool byemail, const QString &sender, const QString &address,
                       const QString &subject, const QString &body, QString &errortext) ;

// Splits the text of a message template: the first line is the subject (email) or the text (SMS), and the rest, with each
// line trimmed, is the body.  The template has no sender line.  Returns false when there is no first line.
bool splitMessageTemplate(const QString &text, QString &subject, QString &body) ;

// The warnings for senders that are blank in ContactManager.ini, one for each channel (none when both are set)
QStringList missingSenderWarnings(const QString &emailSender, const QString &smsSender) ;

#endif // MESSAGESEND_H
