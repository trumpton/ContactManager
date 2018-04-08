
#include "mainwindow.h"


void MainWindow::LoadOverviewTab()
{
    QString overview ;
    Contact &c = db.getSelected();
    Todo &todo = c.getTodo() ;
    History &h = c.getHistory() ;

    // TODO: set up a stylesheet here
    // Define Headings, titles:, contactdata, journaldata, and appointmentdata

    overview = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\"><html><head></head><body>\n" ;
    overview += "<span style=\" font-size:18pt; font-weight:600; color:#000080;\">" + c.getFormattedName(false, false) + "</span><br/>\n" ;
    overview += c.getOverview(Contact::contactAsHTML) ;

    // TODO: Create a calendar->getOverview, and journal->getOverview, and use as has been done with contacts

    overview += "<br/><span style=\" font-size:18pt; font-weight:600; color:#000080;\">Future Appointments</span><br/>"  ;
    overview += calendar.getOverview(calendarAsHTML, c.getField(Contact::ID), true) ;

    overview += "<br/><span style=\" font-size:18pt; font-weight:600; color:#000080;\">To Do List</span><br/>"  ;
    overview += todo.getOverview(todoAsHTML) ;

    overview += "<br/><span style=\" font-size:18pt; font-weight:600; color:#000080;\">History</span><br/>"  ;
    overview += h.getOverview(historyAsHTML) ;

    overview += "</body></html>\n" ;

    ui->editOverview->setText(overview) ;
}

