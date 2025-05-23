#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QObject>
#include <QSqlDatabase>
#include "projectmanager.h"
#include "categorymanager.h"
#include "templatemanager.h"
#include "tablemanager.h"

class DatabaseHandler : public QObject {
    Q_OBJECT
public:
    explicit DatabaseHandler(const QString &host, int port,
                             const QString &dbName,
                             const QString &user,
                             const QString &password,
                             QObject *parent = nullptr);
    DatabaseHandler(QSqlDatabase &db, QObject *parent = nullptr);
    ~DatabaseHandler();

    // Методы для получения менеджеров
    ProjectManager* getProjectManager();
    CategoryManager* getCategoryManager();
    TemplateManager* getTemplateManager();
    TableManager* getTableManager();

    // Подключение к бд
    bool connectToDatabase();

    // Отключение от бд
    void disconnectFromDatabase();

private:
    QSqlDatabase db;
    ProjectManager *projectManager;
    CategoryManager *categoryManager;
    TemplateManager *templateManager;
    TableManager *tableManager;
};

#endif // DATABASEHANDLER_H
