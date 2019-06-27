
#include "mainwindow.h"
#include "../Lib/alertsound.h"

void MainWindow::on_actionIncome_triggered()
{
    class TransactionForm *frm ;
    qint64 ret ;

    // TODO: check that an account is selected
    Contact &dr = db.getSelected() ;
    if (dr.isNull()) {
        play(Disabled) ; return ;
    }

    QString &me = gConf->getMe() ;

    if (!me.isEmpty() && dr.getField(Contact::ID).compare(me)==0) {
        if (!warningYesNoDialog(this, "Income Warning",
                  "Warning: You are about to add some income from yourself.  Is this what you really intend?\n"
                  "Select Yes/Enter to add the income from yourself or No/Escape to abort.")) {
            return ;
        }
    }

    frm = new TransactionForm(this) ;

    frm->setupForm( QString("Income Form"),
                    QString("Income From"),
                    QString("Description of Income"),
                    QString("Income Amount"),
                    dr.getFormattedName(false, false),
                    QString("Appointment"),
                    QString(""),
                    QString("Income From"),
                    QString("Description of Income From ") + dr.getFormattedName(false, false),
                    QString("Income Amount From ") + dr.getFormattedName(false, false)) ;
    play(Query) ;
    ret = frm->exec() ;
    if (ret!=0) {
        QString msg = "Income Received: " + frm->getValue() + " - " + frm->getDescription() ;
        appendHistory(dr, msg) ;
    }
    delete frm ;
}

void MainWindow::on_actionPayment_triggered()
{
    class TransactionForm *frm ;
    qint64 ret ;

    // TODO: check that an account is selected

    Contact *rec= &db.getSelected() ;
    QString& me = gConf->getMe() ;

    if (me.isEmpty()) {
        errorOkDialog(this, "Error, Not Configured", "Me! not configured, so I can't find my list") ;
        return ;
    }

    if (rec->isNull()) {
        play(Disabled) ; return ;
    }

    if (rec->getField(Contact::ID).compare(me)!=0) {
        if (warningYesNoDialog(this, "Expense Warning",
                  "Warning: You are about to add an expense for " + rec->getFormattedName(false, false) +
                  ". Do you want to add this as your expense instead?\n"
                  "Select Yes/Enter to add the Expense to your record or No/Escape to continue adding the expense to " +
                  rec->getFormattedName(false, false))) {
               rec=&db.getContactBy(Contact::ID, me) ;
        }
    }

    frm = new TransactionForm(this) ;
    frm->setupForm( QString("Expense Form"),
                    QString("Expense Payment For"),
                    QString("Expense Payee / Description"),
                    QString("Expense Amount"),
                    rec->getFormattedName(false, false),
                    QString(""),
                    QString(""),
                    QString("Expense Payment For"),
                    QString("Payee and Description of Expense For ") + rec->getFormattedName(false, false),
                    QString("Expense Amount For ") + rec->getFormattedName(false, false)) ;
    play(Query) ;
    ret = frm->exec() ;
    if (ret!=0) {
        QString msg = "Payment Expense: " + frm->getValue() + " - " + frm->getDescription() ;
        appendHistory(*rec, msg) ;
    }
    delete frm ;
}

void MainWindow::on_actionTransactionReport_triggered()
{
    // TODO: Expenses Form and Report
}

