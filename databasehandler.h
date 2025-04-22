#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QObject>
#include <QSqlDatabase>
#include "projectManager.h"
#include "categoryManager.h"
#include "templateManager.h"
#include "tableManager.h"

class DatabaseHandler : public QObject {
public:
    explicit DatabaseHandler(QObject *parent = nullptr);
    DatabaseHandler(QSqlDatabase &db, QObject *parent = nullptr);
    ~DatabaseHandler();

    // Методы для получения менеджеров
    ProjectManager* getProjectManager();
    CategoryManager* getCategoryManager();
    TemplateManager* getTemplateManager();
    TableManager* getTableManager();

    // Подключение к бд
    bool connectToDatabase(const QString &dbName, const QString &user, const QString &password, const QString &host, int port);

private:
    QSqlDatabase db;
    ProjectManager *projectManager;
    CategoryManager *categoryManager;
    TemplateManager *templateManager;
    TableManager *tableManager;
};

#endif // DATABASEHANDLER_H
