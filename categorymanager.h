#ifndef CATEGORYMANAGER_H
#define CATEGORYMANAGER_H

#include <QVector>
#include <QString>
#include <QSqlDatabase>

struct Category {
    int categoryId;
    QString name;
    int parentId;
    int position;
    int depth;
    int projectId;
};

class CategoryManager {
public:
    CategoryManager(QSqlDatabase &db);
    ~CategoryManager();

    bool createCategory(const QString &name, int parentId, int projectId);
    bool updateCategory(int categoryId, const QString &newName);
    bool deleteCategory(int categoryId, bool deleteAll);

    QVector<Category> getCategoriesByProject(int projectId) const;  // Получение списка категорий
    QVector<Category> getCategoriesByProjectAndParent(int projectId, const QVariant &parentId);

    bool updateNumerationDB(int itemId, int parentId, const QString &numeration, int depth);
    bool updateParentId(int itemId, int newParentId);

private:
    QSqlDatabase &db;
};

#endif // CATEGORYMANAGER_H
