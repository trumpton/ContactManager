

#include <QApplication>
#include <QSharedMemory>
#include <QProcess>
#include "mainwindow.h"
#include "../Lib/supportfunctions.h"
#include "../Lib/alertsound.h"
#include <iostream>
#include <QMessageBox>

#ifdef WIN32
#include <windows.h>
#include <winuser.h>
#endif

qint64 masterPID(MainWindow *win, QSharedMemory *sharedMem) ;

int main(int argc, char *argv[])
{
    if (argc>1 && argv[1][0]=='-' && argv[1][1]!='\0') {

        switch (argv[1][1]) {
        case 'v':
            std::cout << appHash().replace("v","").toLatin1().data() << "\n" ;
            break ;
        case 'l':
            std::cout << libVersion().replace("v","").toLatin1().data() << "\n" ;
            break ;
        case 'd':
            std::cout << buildDate().toLatin1().data() << "\n" ;
            break ;
        case 'a':
            std::cout << appHash().replace("v","").toLatin1().data() << "\n" ;
            std::cout << libVersion().replace("v","").toLatin1().data() << "\n" ;
            std::cout << buildDate().toLatin1().data() << "\n" ;
            break ;
        default:
            std::cout << "ContactManager -adlv\n -a - Show all\n -d - Build date\n -l - Library version\n -v - Application version\n" ;
            break ;
        }
        return 1 ;

    } else {

        QApplication a(argc, argv);

        // Set Application Name
        a.setApplicationName("Contact Manager") ;
        a.setApplicationVersion(appHash().replace("v","")) ;

        MainWindow w;
        int result ;

        // Initialise the sound alerts
        initSound(NULL) ;

        QSharedMemory *sharedMem = new QSharedMemory("ContactManager");
        qint64 master = masterPID(&w, sharedMem) ;

        if (master>=0) {

            // TODO: Signal the PID to tell it to maximise, then exit quietly
            // If the signal doesn't get through, give the user a wait a minute message

            // Get HWND from PID
            // Call SetFocus() for HWND
    #ifdef WIN32
            WPARAM wp = SC_RESTORE ;
            LPARAM lp = 0 ;
            if (PostMessage((HWND)master, WM_SYSCOMMAND, wp, lp))
                SetForegroundWindow((HWND)master);
            else
                // Send QMessage
    #endif
                warningOkDialog(&w, "Contact Manager Already Running",
                        "Contact Manager is already Running.  Press Alt-Tab to find it") ;
            result=0 ;

        } else {

            w.show();
            result = a.exec();

        }

        sharedMem->detach() ;
        delete sharedMem ;

        return result ;

    }
}

// Get the Process ID of the first app
// If not found, return -1
qint64 masterPID(MainWindow *win, QSharedMemory *sharedMem)
{
    Q_UNUSED(win) ;
    qint64 PID, MYPID ;

#ifdef WIN32
    MYPID = win->winId() ;
#else
    MYPID = QCoreApplication::applicationPid() ;
#endif

    // Try to create twice.
    // This clears stale shared memory if a Unix application has aborted / crashed
    if (sharedMem->create(sizeof(qint64)) || sharedMem->create(sizeof(qint64)) ) {
        // Created so write PID
        void *ptr = sharedMem->data() ;
        qint64 *qptr = (qint64 *)ptr ;
        *qptr = MYPID ;
        PID=-1 ;
    } else {
        // Already exists, so read PID
        sharedMem->attach() ;
        void *ptr = sharedMem->data() ;
        qint64 *qptr = (qint64*) ptr ;
        PID = *qptr ;
        // TODO: If the process does not exist, don't detach, write our PID, and return -1
        sharedMem->detach() ;
    }
    return PID ;
}

