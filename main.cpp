#include "mainwindow.h"
#include "dbconnectiondialog.h"
#include <QApplication>
#include <QFile>
#include <QDateTime>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

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

    // —————— Запрос параметров подключения ——————
    DBConnectionDialog dlg;
    bool connected = false;
    while (!connected) {
        if (dlg.exec() != QDialog::Accepted) {
            return 0; // пользователь нажал Отмена
        }
        // создаём хендлер с введёнными параметрами
        DatabaseHandler dbh(dlg.host(), dlg.port(),
                            dlg.databaseName(),
                            dlg.userName(), dlg.password());
        if (dbh.connectToDatabase()) {
            connected = true;
            // передаём ownership главному окну
            MainWindow w(&dbh);
            w.show();
            return a.exec();
        }
        // иначе цикл повторится, в диалоге снова можно править параметры
    }
    return 0;
}
