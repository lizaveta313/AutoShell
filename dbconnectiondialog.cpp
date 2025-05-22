#include "dbconnectiondialog.h"
#include <QFormLayout>
#include <QSettings>
#include <QMessageBox>
#include <QStringListModel>
#include <QComboBox>

DBConnectionDialog::DBConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Parameters connection to the database"));
    hostEdit    = new QLineEdit(this);
    portEdit    = new QLineEdit(this);
    dbNameEdit  = new QLineEdit(this);
    userEdit    = new QLineEdit(this);
    passEdit    = new QLineEdit(this);
    passEdit->setEchoMode(QLineEdit::Password);
    saveCheck   = new QCheckBox("Save");

    okButton     = new QPushButton(tr("Connect"), this);
    cancelButton = new QPushButton(tr("Cancel"), this);

    hostCompleter = new QCompleter(this);
    portCompleter = new QCompleter(this);
    dbCompleter   = new QCompleter(this);
    userCompleter = new QCompleter(this);


    hostCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    portCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    dbCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    userCompleter->setCaseSensitivity(Qt::CaseInsensitive);

    hostCompleter->setCompletionMode(QCompleter::PopupCompletion);
    portCompleter->setCompletionMode(QCompleter::PopupCompletion);
    dbCompleter->setCompletionMode(QCompleter::PopupCompletion);
    userCompleter->setCompletionMode(QCompleter::PopupCompletion);

    hostCompleter->setFilterMode(Qt::MatchContains);
    portCompleter->setFilterMode(Qt::MatchContains);
    dbCompleter->setFilterMode(Qt::MatchContains);
    userCompleter->setFilterMode(Qt::MatchContains);

    hostEdit->setCompleter(hostCompleter);
    portEdit->setCompleter(portCompleter);
    dbNameEdit->setCompleter(dbCompleter);
    userEdit->setCompleter(userCompleter);

    auto *form = new QFormLayout;
    form->addRow(tr("Host:"),        hostEdit);
    form->addRow(tr("Port:"),        portEdit);
    form->addRow(tr("Database Name:"),      dbNameEdit);
    form->addRow(tr("Username:"), userEdit);
    form->addRow(tr("Password:"),      passEdit);
    form->addRow("", saveCheck);

    auto *btnLay = new QHBoxLayout;
    btnLay->addStretch();
    btnLay->addWidget(okButton);
    btnLay->addWidget(cancelButton);

    auto *mainLay = new QVBoxLayout(this);
    mainLay->addLayout(form);
    mainLay->addLayout(btnLay);

    loadSettings();

    connect(okButton, &QPushButton::clicked, this, [this](){
        if (host().isEmpty() || databaseName().isEmpty() || userName().isEmpty()) {
            QMessageBox::warning(this, tr("Error"), tr("All fields must be filled in."));
            return;
        }
        if (saveCheck->isChecked())
            saveSettings();
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, &DBConnectionDialog::reject);
}

QString DBConnectionDialog::host()           const { return hostEdit->text(); }
QString DBConnectionDialog::port()           const { return portEdit->text(); }
QString DBConnectionDialog::databaseName()   const { return dbNameEdit->text(); }
QString DBConnectionDialog::userName()       const { return userEdit->text(); }
QString DBConnectionDialog::password()       const { return passEdit->text(); }

void DBConnectionDialog::loadSettings() {
    QSettings s;
    // поля по отдельности
    auto hosts = s.value("db/history/hosts").toStringList();
    auto ports = s.value("db/history/ports").toStringList();
    auto dbs   = s.value("db/history/dbnames").toStringList();
    auto users = s.value("db/history/users").toStringList();

    hostCompleter->setModel(new QStringListModel(hosts, hostCompleter));
    portCompleter->setModel(new QStringListModel(ports, portCompleter));
    dbCompleter->setModel(new QStringListModel(dbs, dbCompleter));
    userCompleter->setModel(new QStringListModel(users, userCompleter));
}

void DBConnectionDialog::saveSettings() {
    QSettings s;
    // загружаем старые списки и добавляем новый элемент (если его там нет)
    auto addUnique = [&](const QString &key, const QString &value){
        auto list = s.value(key).toStringList();
        if (!list.contains(value)) {
            list.prepend(value);
            while (list.size() > 10)
                list.removeLast();
            s.setValue(key, list);
        }
    };

    addUnique("db/history/hosts",     host());
    addUnique("db/history/ports",     port());
    addUnique("db/history/dbnames",   databaseName());
    addUnique("db/history/users",     userName());

    QString conn = host() + ";" + port() + ";" + databaseName() + ";" + userName();
    auto conns = s.value("db/history/connections").toStringList();
    if (!conns.contains(conn)) {
        conns.prepend(conn);
        while (conns.size() > 10) conns.removeLast();
        s.setValue("db/history/connections", conns);
    }
}
