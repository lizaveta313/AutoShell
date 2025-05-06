#include "dbconnectiondialog.h"
#include <QFormLayout>
#include <QSettings>
#include <QMessageBox>
#include <QStringListModel>

DBConnectionDialog::DBConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Параметры подключения к БД"));
    hostEdit    = new QLineEdit(this);
    portSpin    = new QSpinBox(this);
    portSpin->setRange(1, 65535);
    dbNameEdit  = new QLineEdit(this);
    userEdit    = new QLineEdit(this);
    passEdit    = new QLineEdit(this);
    passEdit->setEchoMode(QLineEdit::Password);
    rememberBox = new QCheckBox(tr("Сохранить параметры"), this);

    okButton     = new QPushButton(tr("Подключиться"), this);
    cancelButton = new QPushButton(tr("Отмена"), this);

    hostCompleter = new QCompleter(this);
    dbCompleter   = new QCompleter(this);
    userCompleter = new QCompleter(this);

    hostCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    dbCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    userCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    hostCompleter->setCompletionMode(QCompleter::PopupCompletion);
    dbCompleter->setCompletionMode(QCompleter::PopupCompletion);
    userCompleter->setCompletionMode(QCompleter::PopupCompletion);
    hostCompleter->setFilterMode(Qt::MatchContains);
    dbCompleter->setFilterMode(Qt::MatchContains);
    userCompleter->setFilterMode(Qt::MatchContains);

    hostEdit->setCompleter(hostCompleter);
    dbNameEdit->setCompleter(dbCompleter);
    userEdit->setCompleter(userCompleter);

    auto *form = new QFormLayout;
    form->addRow(tr("Хост:"),        hostEdit);
    form->addRow(tr("Порт:"),        portSpin);
    form->addRow(tr("Имя БД:"),      dbNameEdit);
    form->addRow(tr("Пользователь:"), userEdit);
    form->addRow(tr("Пароль:"),      passEdit);
    form->addRow("",                  rememberBox);

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
            QMessageBox::warning(this, tr("Ошибка"), tr("Все поля, кроме пароля, должны быть заполнены"));
            return;
        }
        if (rememberBox->isChecked())
            saveSettings();
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, &DBConnectionDialog::reject);
}

QString DBConnectionDialog::host()           const { return hostEdit->text(); }
int     DBConnectionDialog::port()           const { return portSpin->value(); }
QString DBConnectionDialog::databaseName()   const { return dbNameEdit->text(); }
QString DBConnectionDialog::userName()       const { return userEdit->text(); }
QString DBConnectionDialog::password()       const { return passEdit->text(); }
bool    DBConnectionDialog::remember()       const { return rememberBox->isChecked(); }

void DBConnectionDialog::loadSettings() {
    QSettings s;
    // читаем списки (или пустой QStringList)
    auto hosts = s.value("db/history/hosts").toStringList();
    auto dbs   = s.value("db/history/dbnames").toStringList();
    auto users = s.value("db/history/users").toStringList();

    hostCompleter->setModel(new QStringListModel(hosts, hostCompleter));
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
            while (list.size() > 10) // лимит, скажем, 10 последних
                list.removeLast();
            s.setValue(key, list);
        }
    };

    addUnique("db/history/hosts",     host());
    addUnique("db/history/dbnames",   databaseName());
    addUnique("db/history/users",     userName());
}
