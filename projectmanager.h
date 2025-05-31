#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QVector>
#include <QString>
#include <QMap>
#include <QDate>

struct Project {
    int projectId;
    QString name;
};

struct ProjectDetails {
    QString study;
    QString sponsor;
    QDate cutDate;
    QString version;
};

class ProjectManager {

public:
    explicit ProjectManager(QSqlDatabase &db);
    ~ProjectManager();

    int createProject(const QString &name);
    bool updateProject(int projectId, const QString &newName);
    bool deleteProject(int projectId);

    int copyProject(int oldProjectId, const QString &newProjectName);

    QString getProjectName(int projectId) const;
    QString getProjectStyle(int projectId) const;
    bool updateProjectStyle(int projectId, const QString &styleName);

    QVector<Project> getProjects() const;

    ProjectDetails getProjectDetails(int projectId) const;
    bool updateProjectDetails(int projectId, const ProjectDetails &details);

private:
    QSqlDatabase &db;

    // Вспомогательные методы:
    bool copyCategoriesRecursively(int oldProjectId, int newProjectId,
                                   const QVariant &oldParentId, const QVariant &newParentId,
                                   QMap<int,int> &categoryIdMap,
                                   QMap<int,int> &templateIdMap);

    bool copyTemplatesForCategory(int oldCategoryId, int newCategoryId,
                                  QMap<int,int> &templateIdMap);

    bool copySingleTemplate(int oldTemplateId, int newTemplateId, const QString &tmplType);

    // Пример копирования связанных данных шаблона:
    bool copyTableOrListing(int oldTemplateId, int newTemplateId);
    bool copyGraph(int oldTemplateId, int newTemplateId);
};

#endif // PROJECTMANAGER_H
