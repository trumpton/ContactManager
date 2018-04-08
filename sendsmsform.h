#ifndef SENDSMSFORM_H
#define SENDSMSFORM_H

#include <QDialog>

namespace Ui {
class SendSMSForm;
}

class SendSMSForm : public QDialog
{
    Q_OBJECT

public:
    explicit SendSMSForm(QWidget *parent = 0);
    ~SendSMSForm();
    QString getMessage() ;
    void setRecipient(QString recipient) ;

private:
    Ui::SendSMSForm *ui;
};

#endif // SENDSMSFORM_H
