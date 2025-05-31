#include "mainwindow.h"
#include "dbconnectiondialog.h"
#include <QApplication>
#include <QFile>
#include <QDateTime>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("MyAutoShell");
    QCoreApplication::setOrganizationDomain("AutoShell.com");
    QCoreApplication::setApplicationName("AutoShell");

    // Настройка логгирования в файл
    QFile logFile("AutoTLG_log.txt");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &msg) {
            QFile file("AutoTLG_log.txt");
            if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                QTextStream stream(&file);
                stream << QDateTime::currentDateTime().toString() << ": " << msg << "\n";
                file.close();
            }
        });
    }

    int exitCode = 0;
    do {
        DBConnectionDialog dlg;
        if (dlg.exec() != QDialog::Accepted)
            return 0;   // Отмена в диалоге — выходим совсем

        DatabaseHandler dbh(
            dlg.host(),
            dlg.port().toInt(),
            dlg.databaseName(),
            dlg.userName(),
            dlg.password()
            );

        if (!dbh.connectToDatabase())
            continue;   // при ошибке — повторить ввод

        MainWindow w(&dbh);
        w.showMaximized();
        exitCode = a.exec();

        // если exitCode == 42 — пользователь выбрал «Сменить БД» в меню
    } while (exitCode == 42);

    return exitCode;
}
