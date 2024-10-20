#-------------------------------------------------
#
# Project created by QtCreator 2014-02-01T19:53:14
#
#-------------------------------------------------

# QT 5.6 += webenginewidgets

QT       += core gui network xml multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat

win32:LIBS += -lUser32

win32:RC_FILE=icon.rc

TARGET = ContactManager
TEMPLATE = app

# Start Common Versioning Includes

unix:BUILDDATE = $$system(date +\"%Y-%d-%m %H:%M:%S\")
win32:BUILDDATE = $$system(echo "%date:~6,4%-%date:~3,2%-%date:~0,2% %time:~0,8%")

GITHASH = $$system(git --git-dir=\"$$PWD/.git\" describe --always --tags)
GITHASH = $$replace(GITHASH,"v","")
GITHASH = $$replace(GITHASH,"-",".")
LIBHASH = $$system(git --git-dir=\"$$PWD/../Lib/.git\" describe --always --tags)
LIBHASH = $$replace(LIBHASH,"v","")
LIBHASH = $$replace(LIBHASH,"-",".")

GITHASHPARTS = $$split(GITHASH, .)
V1 = $$member(GITHASHPARTS,0)
V2 = $$member(GITHASHPARTS,1)
V3 = $$member(GITHASHPARTS,2)
VERSION = "$${V1}.$${V2}.$${V3}"

system("echo VERSION=\"$$VERSION\" > $$OUT_PWD/version.txt")
system("echo GITHASH=\"$$GITHASH\" >> $$OUT_PWD/version.txt")
system("echo LIBHASH=\"$$LIBHASH\" >> $$OUT_PWD/version.txt")
system("echo BUILDDATE=\"$$BUILDDATE\" >> $$OUT_PWD/version.txt")

DEFINES += GITHASH='"\\\"$$GITHASH\\\""'
DEFINES += LIBHASH='"\\\"$$LIBHASH\\\""'
DEFINES += BUILDDATE='"\\\"$$BUILDDATE\\\""'
DEFINES += PWD='"\\\"$$PWD\\\""'

# End Common Versioning Includes

DEFINES += QWIDGET_EXTRA_DEBUG

CONFIG(debug, debug|release) {
# When debugging, allow SMS and Email to be sent at night, and don't wait 2 mins
# DEFINES += DEBUGTIMER
}

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
    advancedfind.cpp \
    mainwindow_test.cpp \
    accessiblestringlistmodel.cpp

HEADERS  += mainwindow.h \
    account-googleaccess.h \
    account-googleaccess.h \
    history.h \
    contact.h \
    search.h \
    about.h \
    calendar.h \
    appointment.h \
    appointmentform.h \
    configuration.h \
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
    account-googleaccess.h \
    ../Lib/encryption.h \
    ../Lib/aes.h \
    advancedfind.h \
    accessiblestringlistmodel.h

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
