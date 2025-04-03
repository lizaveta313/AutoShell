#include "projectmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

ProjectManager::ProjectManager(QSqlDatabase &db) : db(db) {}
ProjectManager::~ProjectManager() {}

int ProjectManager::createProject(const QString &name) {
    QSqlQuery query(db);
    query.prepare("INSERT INTO project (name) VALUES (:name)");
    query.bindValue(":name", name);
    if (!query.exec()) {
        qDebug() << "Ошибка создания проекта:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
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

int ProjectManager::copyProject(int oldProjectId, const QString &newProjectName) {
    // 1) Начинаем транзакцию
    if (!db.transaction()) {
        qDebug() << "Не удалось начать транзакцию:" << db.lastError().text();
        return -1;
    }

    // 2) Получаем имя исходного проекта
    QString originalName;
    {
        QSqlQuery q(db);
        q.prepare("SELECT name FROM project WHERE project_id = :pid");
        q.bindValue(":pid", oldProjectId);
        if (!q.exec() || !q.next()) {
            qDebug() << "Ошибка: проект с id" << oldProjectId << "не найден.";
            db.rollback();
            return -1;
        }
        originalName = q.value("name").toString();
    }

    // 3) Создаем новую запись в таблице project
    //    Добавляем, например, суффикс " (copy)" к названию.
    //    Пользователь потом может переименовать через updateProject.
    int newProjectId = -1;
    {
        QString copyName = newProjectName.isEmpty() ? (originalName + " (copy)") : newProjectName;
        QSqlQuery q(db);
        q.prepare("INSERT INTO project (name) VALUES (:name)");
        q.bindValue(":name", copyName);
        if (!q.exec()) {
            qDebug() << "Ошибка создания копии проекта:" << q.lastError().text();
            db.rollback();
            return -1;
        }
        newProjectId = q.lastInsertId().toInt();
    }

    // 4) Рекурсивно копируем категории и шаблоны
    //    Используем QMap для хранения соответствий старых и новых id
    QMap<int,int> categoryIdMap;  // oldCategoryId -> newCategoryId
    QMap<int,int> templateIdMap;  // oldTemplateId  -> newTemplateId

    // Начинаем копировать категории, у которых parent_id = NULL (или -1) в старом проекте
    // Параметр oldParentId = -1 (или QVariant()), newParentId = -1
    if (!copyCategoriesRecursively(oldProjectId, newProjectId,
                                   /*oldParentId=*/0, /*newParentId=*/0,
                                   categoryIdMap, templateIdMap))
    {
        db.rollback();
        return -1;
    }

    // 5) Если всё хорошо – фиксируем транзакцию
    if (!db.commit()) {
        qDebug() << "Не удалось зафиксировать транзакцию:" << db.lastError().text();
        db.rollback();
        return -1;
    }

    // Возвращаем id нового проекта
    return newProjectId;
}

bool ProjectManager::copyCategoriesRecursively(int oldProjectId, int newProjectId,
                                               int oldParentId, int newParentId,
                                               QMap<int,int> &categoryIdMap,
                                               QMap<int,int> &templateIdMap) {
    // 1) Выбираем все категории, принадлежащие oldProjectId и имеющие parent_id = oldParentId
    //    (Возможно, у вас в БД parent_id = NULL; тогда замените условие)
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT category_id, name, position, depth
        FROM category
        WHERE project_id = :oldProj
          AND ( (parent_id IS NULL AND :oldParent = 0)
                OR (parent_id = :oldParent) )
        ORDER BY position
    )");
    query.bindValue(":oldProj", oldProjectId);
    query.bindValue(":oldParent", oldParentId);

    if (!query.exec()) {
        qDebug() << "Ошибка чтения категорий:" << query.lastError().text();
        return false;
    }

    while (query.next()) {
        int oldCatId  = query.value("category_id").toInt();
        QString cName = query.value("name").toString();
        int cPos      = query.value("position").toInt();
        int cDepth    = query.value("depth").toInt();

        // 2) Создаем новую категорию в новом проекте
        //    parent_id = newParentId, project_id = newProjectId
        QSqlQuery insertQ(db);
        insertQ.prepare(R"(
            INSERT INTO category (name, parent_id, project_id, position, depth)
            VALUES (:name, :parentId, :projId, :pos, :depth)
        )");
        insertQ.bindValue(":name", cName);
        if (newParentId < 0) {
            insertQ.bindValue(":parentId", QVariant()); // NULL
        } else {
            insertQ.bindValue(":parentId", newParentId);
        }
        insertQ.bindValue(":projId", newProjectId);
        insertQ.bindValue(":pos", cPos);
        insertQ.bindValue(":depth", cDepth);

        if (!insertQ.exec()) {
            qDebug() << "Ошибка вставки категории:" << insertQ.lastError().text();
            return false;
        }
        int newCatId = insertQ.lastInsertId().toInt();

        // Сохраняем сопоставление
        categoryIdMap.insert(oldCatId, newCatId);

        // 3) Копируем шаблоны (Template), принадлежащие этой категории
        if (!copyTemplatesForCategory(oldCatId, newCatId, templateIdMap)) {
            return false;
        }

        // 4) Рекурсивно копируем подкатегории
        if (!copyCategoriesRecursively(oldProjectId, newProjectId,
                                       oldCatId, newCatId,
                                       categoryIdMap, templateIdMap))
        {
            return false;
        }
    }
    return true;
}

bool ProjectManager::copyTemplatesForCategory(int oldCategoryId, int newCategoryId,
                                              QMap<int,int> &templateIdMap) {
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT template_id, name, notes, programming_notes,
               position, is_dynamic
        FROM template
        WHERE category_id = :catId
        ORDER BY position
    )");
    q.bindValue(":catId", oldCategoryId);

    if (!q.exec()) {
        qDebug() << "Ошибка чтения шаблонов:" << q.lastError().text();
        return false;
    }

    while (q.next()) {
        int oldTmplId  = q.value("template_id").toInt();
        QString tName  = q.value("name").toString();
        QString notes  = q.value("notes").toString();
        QString pNotes = q.value("programming_notes").toString();
        int tPos       = q.value("position").toInt();
        bool isDynamic = q.value("is_dynamic").toBool();

        // Создаем новый шаблон
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO template (name, category_id, notes, programming_notes,
                                  position, is_dynamic)
            VALUES (:name, :catId, :notes, :pnotes, :pos, :isDyn)
        )");
        ins.bindValue(":name", tName);
        ins.bindValue(":catId", newCategoryId);
        ins.bindValue(":notes", notes);
        ins.bindValue(":pnotes", pNotes);
        ins.bindValue(":pos", tPos);
        ins.bindValue(":isDyn", isDynamic);

        if (!ins.exec()) {
            qDebug() << "Ошибка вставки шаблона:" << ins.lastError().text();
            return false;
        }
        int newTmplId = ins.lastInsertId().toInt();

        // Сохраняем соответствие
        templateIdMap.insert(oldTmplId, newTmplId);

        // Копируем связанные данные
        if (!copySingleTemplate(oldTmplId, newTmplId)) {
            return false;
        }
    }
    return true;
}

bool ProjectManager::copySingleTemplate(int oldTemplateId, int newTemplateId) {
    // Копируем строки/столбцы/ячейки
    if (!copyTableRowsColumnsCells(oldTemplateId, newTemplateId)) {
        return false;
    }
    // Копируем listing
    if (!copyListing(oldTemplateId, newTemplateId)) {
        return false;
    }
    // Копируем figures
    if (!copyFigures(oldTemplateId, newTemplateId)) {
        return false;
    }
    return true;
}

bool ProjectManager::copyTableRowsColumnsCells(int oldTemplateId, int newTemplateId) {
    // 1) Копируем table_row
    {
        QSqlQuery sel(db);
        sel.prepare(R"(
            SELECT row_order FROM table_row
            WHERE template_id = :tid
            ORDER BY row_order
        )");
        sel.bindValue(":tid", oldTemplateId);
        if (!sel.exec()) {
            qDebug() << "Ошибка чтения table_row:" << sel.lastError().text();
            return false;
        }
        while (sel.next()) {
            int rowOrder = sel.value("row_order").toInt();
            QSqlQuery ins(db);
            ins.prepare(R"(
                INSERT INTO table_row (template_id, row_order)
                VALUES (:newTid, :rowOrder)
            )");
            ins.bindValue(":newTid", newTemplateId);
            ins.bindValue(":rowOrder", rowOrder);
            if (!ins.exec()) {
                qDebug() << "Ошибка вставки table_row:" << ins.lastError().text();
                return false;
            }
        }
    }

    // 2) Копируем table_columns
    {
        QSqlQuery sel(db);
        sel.prepare(R"(
            SELECT column_order, header
            FROM table_columns
            WHERE template_id = :tid
            ORDER BY column_order
        )");
        sel.bindValue(":tid", oldTemplateId);
        if (!sel.exec()) {
            qDebug() << "Ошибка чтения table_columns:" << sel.lastError().text();
            return false;
        }
        while (sel.next()) {
            int colOrder  = sel.value("column_order").toInt();
            QString header= sel.value("header").toString();
            QSqlQuery ins(db);
            ins.prepare(R"(
                INSERT INTO table_columns (template_id, column_order, header)
                VALUES (:newTid, :colOrder, :header)
            )");
            ins.bindValue(":newTid", newTemplateId);
            ins.bindValue(":colOrder", colOrder);
            ins.bindValue(":header", header);
            if (!ins.exec()) {
                qDebug() << "Ошибка вставки table_columns:" << ins.lastError().text();
                return false;
            }
        }
    }

    // 3) Копируем table_cell
    {
        QSqlQuery sel(db);
        sel.prepare(R"(
            SELECT row_order, column_order, content, color
            FROM table_cell
            WHERE template_id = :tid
        )");
        sel.bindValue(":tid", oldTemplateId);
        if (!sel.exec()) {
            qDebug() << "Ошибка чтения table_cell:" << sel.lastError().text();
            return false;
        }
        while (sel.next()) {
            int rowOrd    = sel.value("row_order").toInt();
            int colOrd    = sel.value("column_order").toInt();
            QString content = sel.value("content").toString();
            QString color   = sel.value("color").toString();
            QSqlQuery ins(db);
            ins.prepare(R"(
                INSERT INTO table_cell (template_id, row_order, column_order, content, color)
                VALUES (:newTid, :rowOrd, :colOrd, :cont, :clr)
            )");
            ins.bindValue(":newTid", newTemplateId);
            ins.bindValue(":rowOrd", rowOrd);
            ins.bindValue(":colOrd", colOrd);
            ins.bindValue(":cont",   content);
            ins.bindValue(":clr",    color);
            if (!ins.exec()) {
                qDebug() << "Ошибка вставки table_cell:" << ins.lastError().text();
                return false;
            }
        }
    }

    return true;
}

bool ProjectManager::copyListing(int oldTemplateId, int newTemplateId) {
    QSqlQuery sel(db);
    sel.prepare(R"(
        SELECT listing_data
        FROM listing
        WHERE template_id = :tid
    )");
    sel.bindValue(":tid", oldTemplateId);
    if (!sel.exec()) {
        qDebug() << "Ошибка чтения listing:" << sel.lastError().text();
        return false;
    }
    while (sel.next()) {
        QByteArray blob = sel.value("listing_data").toByteArray();
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO listing (template_id, listing_data)
            VALUES (:newTid, :blob)
        )");
        ins.bindValue(":newTid", newTemplateId);
        ins.bindValue(":blob", blob);
        if (!ins.exec()) {
            qDebug() << "Ошибка вставки listing:" << ins.lastError().text();
            return false;
        }
    }
    return true;
}

bool ProjectManager::copyFigures(int oldTemplateId, int newTemplateId) {
    QSqlQuery sel(db);
    sel.prepare(R"(
        SELECT linkage
        FROM figures
        WHERE template_id = :tid
    )");
    sel.bindValue(":tid", oldTemplateId);
    if (!sel.exec()) {
        qDebug() << "Ошибка чтения figures:" << sel.lastError().text();
        return false;
    }
    while (sel.next()) {
        QByteArray linkage = sel.value("linkage").toByteArray();
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO figures (template_id, linkage)
            VALUES (:newTid, :linkage)
        )");
        ins.bindValue(":newTid", newTemplateId);
        ins.bindValue(":linkage", linkage);
        if (!ins.exec()) {
            qDebug() << "Ошибка вставки figures:" << ins.lastError().text();
            return false;
        }
    }
    return true;
}

QString ProjectManager::getProjectStyle(int projectId) {
    QString styleName;
    QSqlQuery query(db);

    query.prepare("SELECT template_style FROM project WHERE project_id = :id");
    query.bindValue(":id", projectId);

    if (!query.exec()) {
        qDebug() << "Не удалось получить стиль проекта:" << query.lastError().text();
        return styleName;
    }

    if (query.next()) {
        styleName = query.value(0).toString();
    }
    return styleName;
}

bool ProjectManager::updateProjectStyle(int projectId, const QString &styleName) {
    QSqlQuery query(db);

    query.prepare("UPDATE project SET template_style = :style WHERE project_id = :id");
    query.bindValue(":style", styleName);
    query.bindValue(":id",    projectId);

    if (!query.exec()) {
        qDebug() << "Ошибка при обновлении стиля проекта:" << query.lastError().text();
        return false;
    }

    return true;
}
