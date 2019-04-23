#-------------------------------------------------
#
# Project created by QtCreator 2014-02-01T19:53:14
#
#-------------------------------------------------

# QT 5.6 += webenginewidgets

QT       += core gui network xml multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_LFLAGS += 

win32:RC_FILE=icon.rc
win32:LIBS += -lUser32

#CONFIG += console

DEFINES += QWIDGET_EXTRA_DEBUG

# Include Git Hash / Revision Details
GITHASH = "$$system(git --git-dir \"$$PWD/.git\" describe --always --tags)"
LIBHASH = "$$system(git --git-dir \"$$PWD/../Lib/.git\" describe --always --tags)"
DEFINES += GITHASH=\\\"$$GITHASH\\\"
DEFINES += LIBHASH=\\\"$$LIBHASH\\\"

TARGET = ContactManager
TEMPLATE = app

INCLUDE += $$PWD

SOURCES += main.cpp\
    mainwindow.cpp \
    history.cpp \
    contact.cpp \
    search.cpp \
    about.cpp \
    calendar.cpp \
    appointment.cpp \
    appointmentform.cpp \
    configuration.cpp \
    listviewstrings.cpp \
    googleaccess.cpp \
    googleupdatedialog.cpp \
    ../Lib/supportfunctions.cpp \
    transactionform.cpp \
    ../Lib/warningyesno.cpp \
    ../Lib/warningok.cpp \
    contactdatabase.cpp \
    googleaccess_appointments.cpp \
    googleaccess_contacts.cpp \
    googleaccess_oauth2.cpp \
    googleaccess_network.cpp \
    googleaccess_xmlprocessing.cpp \
    accessibledatetimeedit.cpp \
    logform.cpp \
    accessibledaterangeedit.cpp \
    sms.cpp \
    smtp.cpp \
    mainwindow_menu_transaction.cpp \
    mainwindow_tab_calendar.cpp \
    mainwindow_timer_backup.cpp \
    mainwindow_timer_alert.cpp \
    mainwindow_menu_navigation.cpp \
    mainwindow_tab_journal.cpp \
    mainwindow_tab_overview.cpp \
    mainwindow_tab_todo.cpp \
    mainwindow_tab_contactdetails.cpp \
    mainwindow_menu_search.cpp \
    mainwindow_menu_help.cpp \
    ../Lib/lineeditnavigator.cpp \
    todo.cpp \
    ../Lib/safelineedit.cpp \
    ../Lib/safetextedit.cpp \
    ../Lib/itemselect.cpp \
    ../Lib/safelistview.cpp \
    ../Lib/alertsound.cpp \
    help.cpp \
    ../Lib/iniconfig.cpp \
    sendsmsform.cpp \
    googleupdatedialog_appointments.cpp \
    googleupdatedialog_contacts.cpp \
    ../Lib/encryption.cpp \
    ../Lib/aes.cpp \
    googleaccess_parsecontact.cpp \
    googleaccess_updatecontact.cpp \
    googleaccess_getcontacts.cpp \
    googleaccess_updatecontactgroups.cpp \
    googleaccess_getcontact.cpp \
    advancedfind.cpp

HEADERS  += mainwindow.h \
    history.h \
    contact.h \
    search.h \
    about.h \
    calendar.h \
    appointment.h \
    appointmentform.h \
    configuration.h \
    listviewstrings.h \
    googleaccess.h \
    googleupdatedialog.h \
    ../Lib/supportfunctions.h \
    transactionform.h \
    ../Lib/warningyesno.h \
    ../Lib/warningok.h \
    contactdatabase.h \
    accessibledatetimeedit.h \
    logform.h \
    accessibledaterangeedit.h \
    sms.h \
    smtp.h \
    ../Lib/lineeditnavigator.h \
    todo.h \
    ../Lib/safelineedit.h \
    ../Lib/safetextedit.h \
    ../Lib/itemselect.h \
    ../Lib/safelistview.h \
    ../Lib/alertsound.h \
    help.h \
    ../Lib/iniconfig.h \
    sendsmsform.h \
    googleaccess-account.h \
    version.h \
    ../Lib/encryption.h \
    ../Lib/aes.h \
    advancedfind.h \
    ../Lib/version.h

FORMS    += mainwindow.ui \
    search.ui \
    about.ui \
    appointmentform.ui \
    configuration.ui \
    googleupdatedialog.ui \
    transactionform.ui \
    ../Lib/warningyesno.ui \
    ../Lib/warningok.ui \
    logform.ui \
    ../Lib/itemselect.ui \
    help.ui \
    sendsmsform.ui \
    ../Lib/encryption.ui \
    advancedfind.ui

RESOURCES += \
    Icon.qrc \
    ../Lib/sounds.qrc

OTHER_FILES += \
    icon.rc

DISTFILES += \
    ___TODO___.txt
