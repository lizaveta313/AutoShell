#include "projectmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

ProjectManager::ProjectManager(QSqlDatabase &db) : db(db) {}
ProjectManager::~ProjectManager() {}

bool ProjectManager::createProject(const QString &name) {
    QSqlQuery query(db);
    query.prepare("INSERT INTO project (name) VALUES (:name)");
    query.bindValue(":name", name);
    if (!query.exec()) {
        qDebug() << "Ошибка создания проекта:" << query.lastError().text();
        return false;
    }
    return true;
}

bool ProjectManager::updateProject(int projectId, const QString &newName) {
    QSqlQuery query(db);
    query.prepare("UPDATE project SET name = :name WHERE project_id = :projectId");
    query.bindValue(":name", newName);
    query.bindValue(":projectId", projectId);

    if (!query.exec()) {
        qDebug() << "Ошибка обновления проекта:" << query.lastError().text();
        return false;
    }
    return true;
}

bool ProjectManager::deleteProject(int projectId) {
    QSqlQuery query(db);
    query.prepare("DELETE FROM project WHERE project_id = :projectId");
    query.bindValue(":projectId", projectId);

    if (!query.exec()) {
        qDebug() << "Ошибка удаления проекта:" << query.lastError().text();
        return false;
    }
    return true;
}


QVector<Project> ProjectManager::getProjects() const {
    QVector<Project> projects;
    QSqlQuery query(db);
    query.exec("SELECT project_id, name FROM project");

    while (query.next()) {
        Project project;
        project.projectId = query.value("project_id").toInt();
        project.name = query.value("name").toString();
        projects.append(project);
    }
    return projects;
}
