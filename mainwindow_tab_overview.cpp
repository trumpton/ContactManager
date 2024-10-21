
#include "mainwindow.h"


void MainWindow::LoadOverviewTab()
{
    QString overview ;
    Contact &c = db.getSelected();
    Todo &todo = c.getTodo() ;
    History &h = c.getHistory() ;

    overview = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\"><html><head></head><body>\n" ;
    overview += "<p><span style=\" font-size:18pt; font-weight:600; color:#000080;\">" + c.getFormattedName(false, false) + "</span></p>\n" ;
    overview += c.getOverview(Contact::contactAsHTML) ;

    overview += "<p><span style=\" font-size:18pt; font-weight:600; color:#000080;\">Future Appointments</span></p>\n"  ;
    overview += calendar.getOverview(calendarAsHTML, c.getField(Contact::ID), true) ;

    overview += "<p><span style=\" font-size:18pt; font-weight:600; color:#000080;\">To Do List</span></p>\n"  ;
    overview += todo.getOverview(todoAsHTML) ;

    overview += "<p><span style=\" font-size:18pt; font-weight:600; color:#000080;\">Recent Change History</span></p>\n"  ;
    overview += h.getOverview(historyTopLinesAsHTML) ;

    overview += "</body></html>\n" ;

    ui->editOverview->setText(overview) ;
}

