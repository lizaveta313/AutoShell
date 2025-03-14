#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QVector>
#include <QString>

struct Project {
    int projectId;
    QString name;
};

class ProjectManager {

public:
    explicit ProjectManager(QSqlDatabase &db);
    ~ProjectManager();

    bool createProject(const QString &name);
    bool updateProject(int projectId, const QString &newName);
    bool deleteProject(int projectId);

    QVector<Project> getProjects() const;

private:
    QSqlDatabase &db;
};

#endif // PROJECTMANAGER_H
