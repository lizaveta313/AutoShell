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

//
bool DatabaseHandler::updateNumerationDB(int itemId, int parentId, const QString &numeration, int depth) {
    QSqlQuery checkQuery(db);

    // Определяем, является ли элемент категорией
    checkQuery.prepare("SELECT 1 FROM category WHERE category_id = :itemId");
    checkQuery.bindValue(":itemId", itemId);

    bool isCategory = false;

    if (checkQuery.exec()) {
        if (checkQuery.next()) {
            isCategory = true; // Найдено в таблице `category`
        }
    } else {
        qDebug() << "Ошибка выполнения запроса проверки категории:" << checkQuery.lastError();
        return false;
    }

    // Подготовка запроса для обновления
    QSqlQuery query(db);
    QStringList numerationParts = numeration.split(".");
    int position = numerationParts.last().toInt();

    if (isCategory) {
        query.prepare("UPDATE category "
                      "SET position = :position, "
                      "depth = :depth, "
                      "parent_id = :parentId "
                      "WHERE category_id = :itemId");
        query.bindValue(":parentId", (parentId == -1) ? QVariant() : parentId);
        query.bindValue(":depth", depth);
    } else {
        query.prepare("UPDATE table_template "
                      "SET position = :position "
                      "WHERE template_id = :itemId");
    }

    query.bindValue(":itemId", itemId);
    query.bindValue(":position", position);

    if (!query.exec()) {
        qDebug() << "Ошибка обновления нумерации в базе данных:" << query.lastError();
        return false;
    }

    return true;
}

bool DatabaseHandler::updateParentId(int itemId, int newParentId) {
    QSqlQuery query(db);

    query.prepare("UPDATE category SET parent_id = :newParentId WHERE category_id = :itemId");
    query.bindValue(":newParentId", newParentId == NULL ? QVariant() : newParentId);
    query.bindValue(":itemId", itemId);

    if (!query.exec()) {
        qDebug() << "Ошибка обновления parent_id:" << query.lastError();
        return false;
    }

    return true;
}

