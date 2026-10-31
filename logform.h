#ifndef LOGFORM_H
#define LOGFORM_H

#include <QDialog>
#include <QString>

class QTimer ;

namespace Ui {
class LogForm;
}

// The log, shown as it is written: while the form is open, new entries are added as they happen
class LogForm : public QDialog
{
    Q_OBJECT

public:
    explicit LogForm(QWidget *parent = 0);
    ~LogForm();
    void setText(QString& text) ;

private slots:
    void refreshLog() ;

private:
    Ui::LogForm *ui;
    QTimer *timer ;
    QString shown ;             // the text the log shows now, so that a change is only looked for when it is there
};

#endif // LOGFORM_H
