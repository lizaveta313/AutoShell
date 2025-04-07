#include "categorymanager.h"
#include <QSqlQuery>
#include <QSqlError>

CategoryManager::CategoryManager(QSqlDatabase &db) : db(db) {}
CategoryManager::~CategoryManager() {}

bool CategoryManager::createCategory(const QString &name, int parentId, int projectId) {
    QSqlQuery query(db);

    int depth = 0;
    int position = 0;

    if (parentId != -1) {
        query.prepare("SELECT depth FROM category WHERE category_id = :parentId");
        query.bindValue(":parentId", parentId);

        if (!query.exec() || !query.next()) {
            qDebug() << "Ошибка получения глубины родительской категории:" << query.lastError();
            return false;
        }

        depth = query.value(0).toInt() + 1;
    }

    if (parentId == -1) {
        // Для корневых категорий
        query.prepare("SELECT COALESCE(MAX(position), 0) + 1 FROM category WHERE parent_id IS NULL AND project_id = :projectId");
        query.bindValue(":projectId", projectId);

    } else {
        // Объединяем подкатегории и шаблоны для вычисления следующей позиции
        query.prepare("SELECT COALESCE(MAX(position), 0) + 1 FROM ("
                      "  SELECT position FROM category WHERE parent_id = :parentId "
                      "  UNION ALL "
                      "  SELECT position FROM template WHERE category_id = :parentId"
                      ") AS combined");
        query.bindValue(":parentId", parentId);
    }
    //

    if (!query.exec() || !query.next()) {
        qDebug() << "Ошибка определения позиции категории:" << query.lastError();
        return false;
    }

    position = query.value(0).toInt();

    query.prepare("INSERT INTO category (name, parent_id, position, depth, project_id) VALUES "
                  "(:name, :parentId, :position, :depth, :projectId)");
    query.bindValue(":name", name);
    query.bindValue(":parentId", parentId == -1 ? QVariant() : parentId);
    query.bindValue(":position", position);
    query.bindValue(":depth", depth);
    query.bindValue(":projectId", projectId);

    if (!query.exec()) {
        qDebug() << "Ошибка создания категории:" << query.lastError();
        return false;
    }

    return true;
}

bool CategoryManager::updateCategory(int categoryId, const QString &newName) {
    QSqlQuery query(db);

    query.prepare("UPDATE category SET name = :newName WHERE category_id = :categoryId");
    query.bindValue(":newName", newName);
    query.bindValue(":categoryId", categoryId);

    if (!query.exec()) {
        qDebug() << "Ошибка обновления категории:" << query.lastError();
        return false;
    }

    return true;
}

bool CategoryManager::deleteCategory(int categoryId, bool deleteAll) {
    QSqlQuery query(db);

    if (deleteAll) {
        // Удаляем категорию вместе с её подкатегориями и шаблонами
        query.prepare(
            "WITH RECURSIVE subcategories AS ( "
            "    SELECT category_id FROM category WHERE category_id = :categoryId "
            "    UNION ALL "
            "    SELECT c.category_id FROM category c "
            "    INNER JOIN subcategories s ON c.parent_id = s.category_id "
            ") "
            "DELETE FROM template WHERE category_id IN (SELECT category_id FROM subcategories);"
            );
        query.bindValue(":categoryId", categoryId);

        if (!query.exec()) {
            qDebug() << "Ошибка удаления шаблонов связанных с категорией:" << query.lastError();
            return false;
        }

        query.prepare(
            "WITH RECURSIVE subcategories AS ( "
            "    SELECT category_id FROM category WHERE category_id = :categoryId "
            "    UNION ALL "
            "    SELECT c.category_id FROM category c "
            "    INNER JOIN subcategories s ON c.parent_id = s.category_id "
            ") "
            "DELETE FROM category WHERE category_id IN (SELECT category_id FROM subcategories);"
            );
        query.bindValue(":categoryId", categoryId);

        if (!query.exec()) {
            qDebug() << "Ошибка удаления категории и её подкатегорий:" << query.lastError();
            return false;
        }
    } else {
        // "Распаковываем" содержимое в родительскую категорию
        query.prepare("SELECT parent_id FROM category WHERE category_id = :categoryId");
        query.bindValue(":categoryId", categoryId);

        if (!query.exec() || !query.next()) {
            qDebug() << "Ошибка получения родительской категории:" << query.lastError();
            return false;
        }

        int parentId = query.value(0).toInt();

        // Обновляем шаблоны, связанные с категорией
        query.prepare("UPDATE template SET category_id = :parentId WHERE category_id = :categoryId");
        query.bindValue(":parentId", parentId);
        query.bindValue(":categoryId", categoryId);

        if (!query.exec()) {
            qDebug() << "Ошибка перемещения шаблонов в родительскую категорию:" << query.lastError();
            return false;
        }

        // Обновляем подкатегории
        query.prepare("UPDATE category SET parent_id = :parentId WHERE parent_id = :categoryId");
        query.bindValue(":parentId", parentId);
        query.bindValue(":categoryId", categoryId);

        if (!query.exec()) {
            qDebug() << "Ошибка перемещения подкатегорий в родительскую категорию:" << query.lastError();
            return false;
        }

        // Удаляем саму категорию
        query.prepare("DELETE FROM category WHERE category_id = :categoryId");
        query.bindValue(":categoryId", categoryId);

        if (!query.exec()) {
            qDebug() << "Ошибка удаления категории:" << query.lastError();
            return false;
        }
    }

    return true;
}

QVector<Category> CategoryManager::getCategoriesByProject(int projectId) const {
    QVector<Category> categories;
    QSqlQuery query(db);
    query.prepare("SELECT category_id, name, parent_id, position, depth, project_id FROM category WHERE project_id = :projectId ORDER BY position");
    query.bindValue(":projectId", projectId);

    if (!query.exec()) {
        qDebug() << "Ошибка загрузки категорий:" << query.lastError().text();
        return categories;
    }

    while (query.next()) {
        Category category;
        category.categoryId = query.value("category_id").toInt();
        category.name = query.value("name").toString();
        category.parentId = query.value("parent_id").toInt();
        category.position = query.value("position").toInt();
        category.depth = query.value("depth").toInt();
        category.projectId = query.value("project_id").toInt();
        categories.append(category);
    }
    return categories;
}

QVector<Category> CategoryManager::getCategoriesByProjectAndParent(int projectId, const QVariant &parentId)  {
    QVector<Category> categories;
    QSqlQuery query(db);
    if (parentId.isNull()) {
        // Корневые категории (parent_id IS NULL)
        query.prepare("SELECT category_id, name, parent_id, position, depth, project_id "
                      "FROM category WHERE project_id = :projectId AND parent_id IS NULL ORDER BY position");
    } else {
        // Подкатегории (parent_id = заданное значение)
        query.prepare("SELECT category_id, name, parent_id, position, depth, project_id "
                      "FROM category WHERE project_id = :projectId AND parent_id = :parentId ORDER BY position");
        query.bindValue(":parentId", parentId);
    }
    query.bindValue(":projectId", projectId);

    if (!query.exec()) {
        qDebug() << "Ошибка загрузки категорий:" << query.lastError().text();
        return categories;
    }

    while (query.next()) {
        Category category;
        category.categoryId = query.value("category_id").toInt();
        category.name = query.value("name").toString();
        // Если parent_id равен NULL, устанавливаем, например, -1
        category.parentId = query.value("parent_id").isNull() ? -1 : query.value("parent_id").toInt();
        category.position = query.value("position").toInt();
        category.depth = query.value("depth").toInt();
        category.projectId = query.value("project_id").toInt();
        categories.append(category);
    }
    return categories;
}
