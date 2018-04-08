#ifndef APPOINTMENTFORM_H
#define APPOINTMENTFORM_H

#include <QDialog>
#include "appointment.h"
#include "calendar.h"
#include "contactdatabase.h"
#include "accessibledatetimeedit.h"


namespace Ui {
class AppointmentForm;
}

class AppointmentForm : public QDialog
{
    Q_OBJECT

public:
    explicit AppointmentForm(QWidget *parent = 0);
    ~AppointmentForm();
    void setName(QString name) ;
    void setupForm(ContactDatabase &db, QString contactid, Appointment &editing, Appointment &reference, bool createNew) ;
    Appointment& getAppointmentDetails() ;

private slots:
    void on_editWhenFrom_valueChanged(const QDateTime &, int );
    void on_editWhenTo_valueChanged(const QDateTime &, int );

private:
    bool mutex ;
    void setupForm() ;
    ContactDatabase *database ;
    Ui::AppointmentForm *ui;
    Calendar *cal;
    Appointment appt ;
    void updateTimes() ;

};

#endif // APPOINTMENTFORM_H
