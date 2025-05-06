#ifndef DBCONNECTIONDIALOG_H
#define DBCONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QCompleter>

class DBConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit DBConnectionDialog(QWidget *parent = nullptr);

    QString host() const;
    int port() const;
    QString databaseName() const;
    QString userName() const;
    QString password() const;
    bool remember() const;

private:
    QLineEdit   *hostEdit;
    QSpinBox    *portSpin;
    QLineEdit   *dbNameEdit;
    QLineEdit   *userEdit;
    QLineEdit   *passEdit;
    QCheckBox   *rememberBox;
    QPushButton *okButton;
    QPushButton *cancelButton;

    QCompleter   *hostCompleter;
    QCompleter   *dbCompleter;
    QCompleter   *userCompleter;

    void loadSettings();
    void saveSettings();
};

#endif // DBCONNECTIONDIALOG_H
