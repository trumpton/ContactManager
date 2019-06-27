/*
Copyright (c) 2013 Raivis Strogonovs
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
 
// Status: -1 - Not Started Sending
//         -2 - Network Error
//         -3 - Error during transaction
//         -4 - Unable to Send / Possibly Sent
//          1 - Send OK
 
#include "smtp.h"
 
Smtp::Smtp( const QString &user, const QString &pass, const QString &host, int port, int timeout )
{   
    socket = new QSslSocket(this);
 
    connect(socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
    connect(socket, SIGNAL(connected()), this, SLOT(connected() ) );
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this,SLOT(errorReceived(QAbstractSocket::SocketError)));  
    connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(stateChanged(QAbstractSocket::SocketState)));
    connect(socket, SIGNAL(disconnected()), this,SLOT(disconnected()));
 
 
    this->user = user;
    this->pass = pass;
 
    this->host = host;
    this->port = port;
    this->timeout = timeout;
 
    t = NULL ;
    state = Disconnected ;
}
 
QString& Smtp::errorMessage()
{
    return errmsg ;
}

int Smtp::success()
{
    return sendsuccess ;
}

void Smtp::sendMail(const QString &from, const QString &to, const QString &subject, const QString &body)
{
    sendsuccess= -1 ;
    errmsg = "Connecting" ;

    if (t!=NULL) { delete t ; t=NULL ; }
    message = "To: " + to + "\n";
    message.append("From: " + from + "\n");
    message.append("Subject: " + subject + "\n");
    message.append(body);
    message.replace( QString::fromLatin1( "\n" ), QString::fromLatin1( "\r\n" ) );
    message.replace( QString::fromLatin1( "\r\n.\r\n" ), QString::fromLatin1( "\r\n..\r\n" ) );
    this->from = from;
    rcpt = to;
    socket->connectToHostEncrypted(host, port); //"smtp.gmail.com" and 465 for gmail TLS
    if (!socket->waitForConnected(timeout)) {
        qDebug() << socket->errorString();
        state = ConnectFailure ;
        errmsg = QString("Connection Failure = ") + socket->errorString() ;
        sendsuccess = -2 ;
        emit status( socket->errorString() );
    } else {
        state = Init;
        sendsuccess = -3 ;
        errmsg = "Processing" ;
        t = new QTextStream( socket );
    }
}
 
Smtp::~Smtp()
{
    if (t!=NULL) delete t;
    delete socket;
}

void Smtp::stateChanged(QAbstractSocket::SocketState socketState)
{
 
    qDebug() <<"stateChanged " << socketState;
}
 
void Smtp::errorReceived(QAbstractSocket::SocketError socketError)
{
    qDebug() << "error " <<socketError;
}
 
void Smtp::disconnected()
{
 
    qDebug() <<"disconneted, error (if any) = "  << socket->errorString();
}
 
void Smtp::connected()
{   
    qDebug() << "Connected ";
}
 
bool Smtp::isConnected()
{
    return (state!=Disconnected && state!=ConnectFailure) ;
}

void Smtp::readyRead()
{
 
    response = "" ;
    if (state==Disconnected || state==ConnectFailure) return ;
    if (t==NULL) {
        qDebug() << "error Smtp::readyRead whilst t==NULL ";
        return ;
    }

     qDebug() <<"readyRead";
    // SMTP is line-oriented
 
    QString responseLine, responseCode;
    do
    {
        responseLine = socket->readLine();
        response += responseLine;
    }
    while ( socket->canReadLine() && responseLine[3] != ' ' );
 
    responseCode = responseLine ;
    responseCode.truncate( 3 );
 
    qDebug() << "Server response code:" <<  responseLine;
    qDebug() << "Server response: " << response;
 
    if ( state == Init && responseCode == "220" )
    {
        // banner was okay, let's go on
        qDebug() << "EHLO localhost" ;
        *t << "EHLO localhost" <<"\r\n";
        t->flush();
 
        state = HandShake;
    }

    else if (state == HandShake) {

        if (responseCode != "250") {

            errmsg = QString("EHLO Error ") + responseLine  ;
            state = Failed;

        } else {

            socket->startClientEncryption();
            if(!socket->waitForEncrypted(timeout))
            {
                qDebug() << socket->errorString();
                errmsg = QString("EHLO Timeout - ") + socket->errorString() ;
                state = Timeout;
            }

 
            //Send EHLO once again but now encrypted
 
            qDebug() << "EHLO localhost" ;
            *t << "EHLO localhost" << "\r\n";
            t->flush();
            state = Auth;

        }
    } else if (state == Auth) {

        if (responseCode != "250") {

            errmsg = QString("EHLO Error ") + responseLine  ;
            state = Failed;

        } else {

            // Trying AUTH
            qDebug() << "AUTH LOGIN";
            *t << "AUTH LOGIN" << "\r\n";
            t->flush();
            state = User;
        }

    } else if (state == User) {

        if (responseCode != "334") {

            errmsg = QString("AUTH Error ") + responseLine  ;
            state = Failed;

        } else {
            //Trying User
            qDebug() << user;
            //GMAIL is using XOAUTH2 protocol, which basically means that password and username has to be sent in base64 coding
            //https://developers.google.com/gmail/xoauth2_protocol
            *t << QByteArray().append(user).toBase64()  << "\r\n";
            t->flush();
            state = Pass;
        }

    } else if (state == Pass) {

        if (responseCode != "334") {

            errmsg = QString("USER Error ") + responseLine  ;
            state = Failed;

        } else {

            //Trying pass
            qDebug() << "PA**WORD";
            *t << QByteArray().append(pass).toBase64() << "\r\n";
            t->flush();
            state = Mail;

        }

    } else if (state == Mail) {

        if (responseCode != "235") {

            errmsg = QString("PASS Error ") + responseLine  ;
            state = Failed;

        } else {

            // HELO response was okay (well, it has to be)
            //Apperantly for Google it is mandatory to have MAIL FROM and RCPT email formated the following way -> <email@gmail.com>
            qDebug() << "MAIL FROM:<" << from << ">";
            *t << "MAIL FROM:<" << from << ">\r\n";
            t->flush();
            state = Rcpt;

        }

    } else if ( state == Rcpt) {

        if (responseCode != "250" ) {

            errmsg = QString("MAIL FROM Error ") + responseLine  ;
            state = Failed;

        } else {

            //Apperantly for Google it is mandatory to have MAIL FROM and RCPT email formated the following way -> <email@gmail.com>
            qDebug() << QString("RCPT TO:<") + rcpt + QString(">") ;
            *t << "RCPT TO:<" << rcpt << ">\r\n"; //r
            t->flush();
            state = Data;
        }

    } else if (state == Data) {

        if (responseCode != "250" ) {

            errmsg = QString("RCPT TO Error ") + responseLine  ;
            state = Failed;

        } else {
 
            qDebug() << "DATA" ;
            *t << "DATA\r\n";
            t->flush();
            sendsuccess = -4 ;  // Maybe complete / sent
            state = Body;
        }

    } else if (state == Body) {

        if (responseCode != "354" ) {

            errmsg = QString("DATA Error ") + responseLine  ;
            state = Failed;

        } else {
 
            qDebug() << message ;
            *t << message << "\r\n.\r\n";
            t->flush();
            state = Quit;
        }

    } else if (state == Quit) {

        if (responseCode != "250" ) {

            errmsg = QString("DATA Error ") + responseLine  ;
            state = Failed;

        } else {

            qDebug() << "QUIT" ;
            *t << "QUIT\r\n";
            t->flush();
            // here, we just close.
            state = Success;
            sendsuccess = 1 ;
            errmsg = "" ;
            emit status( tr( "Message sent" ) );

        }

    } else if (state == Success || state == Timeout || state == Failed) {

        deleteLater();
        return;

    } else {

        // something broke - this code should never be reached
        state = Failed;
        errmsg = QString("FATAL Error ") + responseLine  ;
        emit status( tr( "FAILED to send message: " ) + response );

    }
    response = "";
}
