#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QVector>
#include <QString>
#include <QMap>

struct Project {
    int projectId;
    QString name;
};

class ProjectManager {

public:
    explicit ProjectManager(QSqlDatabase &db);
    ~ProjectManager();

    int createProject(const QString &name);
    bool updateProject(int projectId, const QString &newName);
    bool deleteProject(int projectId);

    int copyProject(int oldProjectId, const QString &newProjectName);

    QString getProjectStyle(int projectId);
    bool updateProjectStyle(int projectId, const QString &styleName);

    QVector<Project> getProjects() const;

private:
    QSqlDatabase &db;

    // Вспомогательные методы:
    bool copyCategoriesRecursively(int oldProjectId, int newProjectId,
                                   int oldParentId, int newParentId,
                                   QMap<int,int> &categoryIdMap,
                                   QMap<int,int> &templateIdMap);

    bool copyTemplatesForCategory(int oldCategoryId, int newCategoryId,
                                  QMap<int,int> &templateIdMap);

    bool copySingleTemplate(int oldTemplateId, int newTemplateId);

    // Пример копирования связанных данных шаблона:
    bool copyTableRowsColumnsCells(int oldTemplateId, int newTemplateId);
    bool copyListing(int oldTemplateId, int newTemplateId);
    bool copyFigures(int oldTemplateId, int newTemplateId);
};

#endif // PROJECTMANAGER_H
