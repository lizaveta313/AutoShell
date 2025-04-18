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
    // В бд должно быть ON DELETE CASCADE у зависимых таблиц
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
        originalName = q.value(0).toString();
    }

    // 3) Создаем новую запись в таблице project
    //    Добавляем, например, суффикс " (copy)" к названию.
    //    Пользователь потом может переименовать через updateProject.
    int newProjectId = -1;
    {
        QString copyName = newProjectName.isEmpty()
                               ? (originalName + " (copy)")
                               : newProjectName;
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

    // Начинаем копировать категории, у которых parent_id = NULL
    if (!copyCategoriesRecursively(oldProjectId,
                                   newProjectId,
                                   /*oldParentId=*/QVariant(),
                                   /*newParentId=*/QVariant(),
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
                                               const QVariant &oldParentId,
                                               const QVariant &newParentId,
                                               QMap<int,int> &categoryIdMap,
                                               QMap<int,int> &templateIdMap) {
    //    Выбираем категории, где project_id=oldProjectId
    //    и parent_id IS NULL (если oldParentId isNull)
    //    или parent_id = oldParentId (в ином случае).
    QString sql = R"(
        SELECT category_id, name, position, depth
        FROM category
        WHERE project_id = :oldProj
    )";
    if (oldParentId.isNull()) {
        sql += " AND parent_id IS NULL";
    } else {
        sql += " AND parent_id = :oldParent";
    }
    sql += " ORDER BY position";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":oldProj", oldProjectId);
    if (!oldParentId.isNull()) {
        query.bindValue(":oldParent", oldParentId);
    }
    if (!query.exec()) {
        qDebug() << "Ошибка чтения категорий:" << query.lastError().text();
        return false;
    }

    // Для каждой найденной категории -> вставляем в новый проект
    while (query.next()) {
        int oldCatId  = query.value("category_id").toInt();
        QString cName = query.value("name").toString();
        int cPos      = query.value("position").toInt();
        int cDepth    = query.value("depth").toInt();

        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO category (name, parent_id, project_id, position, depth)
            VALUES (:name, :parentId, :projId, :pos, :depth)
        )");
        ins.bindValue(":name", cName);
        if (newParentId.isNull()) {
            // корневой parent_id = NULL
            ins.bindValue(":parent", QVariant(QVariant()));
        } else {
            // родитель = конкретный newCatId
            ins.bindValue(":parent", newParentId);
        }
        ins.bindValue(":projId", newProjectId);
        ins.bindValue(":pos", cPos);
        ins.bindValue(":depth", cDepth);

        if (!ins.exec()) {
            qDebug() << "Ошибка вставки категории:" << ins.lastError().text();
            return false;
        }
        int newCatId = ins.lastInsertId().toInt();

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
               position, is_dynamic, template_type
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
        QString tmplType= q.value("template_type").toString();

        // Создаем новый шаблон
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO template (name, category_id, notes, programming_notes,
                                  position, is_dynamic, template_type)
            VALUES (:name, :catId, :notes, :pNotes, :pos, :dyn, :tType)
        )");
        ins.bindValue(":name", tName);
        ins.bindValue(":catId", newCategoryId);
        ins.bindValue(":notes", notes);
        ins.bindValue(":pnotes", pNotes);
        ins.bindValue(":pos", tPos);
        ins.bindValue(":isDyn", isDynamic);
        ins.bindValue(":tType", tmplType);

        if (!ins.exec()) {
            qDebug() << "Ошибка вставки шаблона:" << ins.lastError().text();
            return false;
        }
        int newTmplId = ins.lastInsertId().toInt();

        // Сохраняем соответствие
        templateIdMap.insert(oldTmplId, newTmplId);

        // Копируем связанные данные
        if (!copySingleTemplate(oldTmplId, newTmplId, tmplType)) {
            return false;
        }
    }
    return true;
}

bool ProjectManager::copySingleTemplate(int oldTemplateId, int newTemplateId, const QString &tmplType) {

    if (tmplType == "table" || tmplType == "listing") {
        if (!copyTableOrListing(oldTemplateId, newTemplateId))
            return false;
    } else if (tmplType == "graph") {
        if (!copyGraph(oldTemplateId, newTemplateId))
            return false;
    } else {
        // Неизвестный тип - вернуть false
        qDebug() << "Неизвестный template_type:" << tmplType;
        return false;
    }
    return true;
}

bool ProjectManager::copyTableOrListing(int oldTemplateId, int newTemplateId) {
    QSqlQuery sel(db);
    // Выбираем все ячейки для старого шаблона
    sel.prepare(R"(
        SELECT cell_type, row_index, col_index, row_span, col_span, content, colour
        FROM grid_cells
        WHERE template_id = :tid
    )");
    sel.bindValue(":tid", oldTemplateId);
    if (!sel.exec()) {
        qDebug() << "Ошибка чтения grid_cells:" << sel.lastError().text();
        return false;
    }

    // Проходим по всем выбранным записям и копируем их в новый шаблон
    while (sel.next()) {
        QString cellType = sel.value("cell_type").toString();
        int rowIndex = sel.value("row_index").toInt();
        int colIndex = sel.value("col_index").toInt();
        int rowSpan = sel.value("row_span").toInt();
        int colSpan = sel.value("col_span").toInt();
        QString content = sel.value("content").toString();
        QString colour = sel.value("colour").toString();

        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO grid_cells (template_id, cell_type, row_index, col_index, row_span, col_span, content, colour)
            VALUES (:newTid, :cellType, :rowIndex, :colIndex, :rowSpan, :colSpan, :content, :colour)
        )");
        ins.bindValue(":newTid", newTemplateId);
        ins.bindValue(":cellType", cellType);
        ins.bindValue(":rowIndex", rowIndex);
        ins.bindValue(":colIndex", colIndex);
        ins.bindValue(":rowSpan", rowSpan);
        ins.bindValue(":colSpan", colSpan);
        ins.bindValue(":content", content);
        ins.bindValue(":colour", colour);
        if (!ins.exec()) {
            qDebug() << "Ошибка вставки grid_cells:" << ins.lastError().text();
            return false;
        }
    }
    return true;
}

bool ProjectManager::copyGraph(int oldTemplateId, int newTemplateId) {
    // 1) Считываем записи из таблицы graph, связанные с oldTemplateId
    QSqlQuery sel(db);
    sel.prepare(R"(
        SELECT name, graph_type, image
        FROM graph
        WHERE template_id = :oldTid
    )");
    sel.bindValue(":oldTid", oldTemplateId);

    if (!sel.exec()) {
        qDebug() << "Ошибка чтения из graph:" << sel.lastError().text();
        return false;
    }

    // 2) Для каждой найденной записи вставляем новую в ту же таблицу `graph`,
    //    но уже с template_id = newTemplateId
    while (sel.next()) {
        QString gName    = sel.value("name").toString();
        QString gType    = sel.value("graph_type").toString();
        QByteArray gImage= sel.value("image").toByteArray();

        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO graph (template_id, name, graph_type, image)
            VALUES (:newTid, :nm, :gtype, :img)
        )");
        ins.bindValue(":newTid", newTemplateId);
        ins.bindValue(":nm",     gName);
        ins.bindValue(":gtype",  gType);
        ins.bindValue(":img",    gImage);

        if (!ins.exec()) {
            qDebug() << "Ошибка вставки в graph:" << ins.lastError().text();
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
