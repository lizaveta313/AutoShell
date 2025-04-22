#include "databaseHandler.h"
#include <QSqlQuery>
#include <QSqlError>

DatabaseHandler::DatabaseHandler(QObject *parent)
    : QObject(parent) {
    if (!QSqlDatabase::contains("main_connection")) {
        db = QSqlDatabase::addDatabase("QPSQL", "main_connection");
    } else {
        db = QSqlDatabase::database("main_connection");
    }

    projectManager = new ProjectManager(db);
    categoryManager = new CategoryManager(db);
    templateManager = new TemplateManager(db);
    tableManager = new TableManager(db);
}


DatabaseHandler::DatabaseHandler(QSqlDatabase &db, QObject *parent)
    : QObject(parent), db(db) {

    projectManager = new ProjectManager(db);
    categoryManager = new CategoryManager(db);
    templateManager = new TemplateManager(db);
    tableManager = new TableManager(db);
}

DatabaseHandler::~DatabaseHandler() {
    delete projectManager;
    delete categoryManager;
    delete templateManager;
    delete tableManager;
    if (db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase("main_connection");
}

//
ProjectManager* DatabaseHandler::getProjectManager(){
    return projectManager;
}

CategoryManager* DatabaseHandler::getCategoryManager() {
    return categoryManager;
}

TemplateManager* DatabaseHandler::getTemplateManager() {
    return templateManager;
}

TableManager* DatabaseHandler::getTableManager() {
    return tableManager;
}

//
bool DatabaseHandler::connectToDatabase(const QString &dbName, const QString &user, const QString &password, const QString &host, int port) {

    db = QSqlDatabase::addDatabase("QPSQL");
    db.setDatabaseName(dbName);
    db.setUserName(user);
    db.setPassword(password);
    db.setHostName(host);
    db.setPort(port);

    if (!db.open()) {
        qDebug() << "Ошибка подключения к базе данных:" << db.lastError().text();
        return false;
    }

    return true;
}
