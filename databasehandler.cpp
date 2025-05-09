#include "databasehandler.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QMessageBox>

DatabaseHandler::DatabaseHandler(const QString &host, int port,
                                 const QString &dbName,
                                 const QString &user,
                                 const QString &password,
                                 QObject *parent)
    : QObject(parent) {
    if (!QSqlDatabase::contains("main_connection")) {
        db = QSqlDatabase::addDatabase("QPSQL", "main_connection");
    } else {
        db = QSqlDatabase::database("main_connection");
    }

    db.setHostName(host);
    db.setPort(port);
    db.setDatabaseName(dbName);
    db.setUserName(user);
    db.setPassword(password);
    //db.setConnectOptions("requiressl=1");

    // Инициализация менеджеров
    projectManager  = new ProjectManager(db);
    categoryManager = new CategoryManager(db);
    templateManager = new TemplateManager(db);
    tableManager    = new TableManager(db);
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

bool DatabaseHandler::connectToDatabase() {

    if (!db.open()) {
        qDebug() << "Ошибка БД:" << db.lastError().text();
        QMessageBox::critical(nullptr, "Ошибка", "Не удалось подключиться к БД: " + db.lastError().text());
        return false;
    }
    return true;
}


