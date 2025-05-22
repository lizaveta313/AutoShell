#ifndef DBCONNECTIONDIALOG_H
#define DBCONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QCompleter>

class DBConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit DBConnectionDialog(QWidget *parent = nullptr);

    QString host() const;
    QString port() const;
    QString databaseName() const;
    QString userName() const;
    QString password() const;



private:
    QLineEdit   *hostEdit;
    QLineEdit    *portEdit;
    QLineEdit   *dbNameEdit;
    QLineEdit   *userEdit;
    QLineEdit   *passEdit;
    QPushButton *okButton;
    QPushButton *cancelButton;
    QCheckBox *saveCheck;

    QCompleter   *hostCompleter;
    QCompleter   *portCompleter;
    QCompleter   *dbCompleter;
    QCompleter   *userCompleter;

    void loadSettings();
    void saveSettings();
};

#endif // DBCONNECTIONDIALOG_H
