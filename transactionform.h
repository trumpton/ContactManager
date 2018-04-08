#ifndef TRANSACTIONFORM_H
#define TRANSACTIONFORM_H

#include <QDialog>

namespace Ui {
class TransactionForm;
}

class TransactionForm : public QDialog
{
    Q_OBJECT

public:
    explicit TransactionForm(QWidget *parent = 0);
    ~TransactionForm();

    void setupForm(QString formTitle,
                   QString transactionNameTitle,
                   QString transactionDescriptionTitle,
                   QString transactionValueTitle,
                   QString defaultName,
                   QString defaultTitle,
                   QString defaultAmount,
                   QString accessibleNameText,
                   QString accessibleDescriptionText,
                   QString accessibleValueText) ;

    QString& getValue() ;
    QString& getDescription() ;

private:
    Ui::TransactionForm *ui;
};

#endif // TRANSACTIONFORM_H
