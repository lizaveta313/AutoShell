#include "templatemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QColor>
#include <optional>

TemplateManager::TemplateManager(QSqlDatabase &db) : db(db) {}
TemplateManager::~TemplateManager() {}

bool TemplateManager::createTemplate(int categoryId, const QString &templateName, const QString &templateType) {
    QSqlQuery query(db);

    // Проверяем существование категории
    query.prepare("SELECT 1 FROM category WHERE category_id = :categoryId");
    query.bindValue(":categoryId", categoryId);

    if (!query.exec() || !query.next()) {
        qDebug() << "Ошибка: категория с ID" << categoryId << "не существует.";
        return false;
    }

    query.prepare("SELECT COALESCE(MAX(position), 0) + 1 FROM ("
                  "  SELECT position FROM category WHERE parent_id = :categoryId "
                  "  UNION ALL "
                  "  SELECT position FROM template WHERE category_id = :categoryId"
                  ") AS combined");
    query.bindValue(":categoryId", categoryId);

    if (!query.exec() || !query.next()) {
        qDebug() << "Ошибка получения максимального position:" << query.lastError();
        return false;
    }

    int newPosition = query.value(0).toInt();

    // Вставляем новый шаблон в таблицу template
    query.prepare(R"(
        INSERT INTO template (category_id, name, subtitle, position, notes, programming_notes, template_type)
        VALUES (:categoryId, :name,  '', :position, '', '', :templateType)
    )");
    query.bindValue(":categoryId", categoryId);
    query.bindValue(":name",        templateName);
    query.bindValue(":position",    newPosition);
    query.bindValue(":templateType", templateType);

    if (!query.exec()) {
        qDebug() << "Ошибка добавления шаблона в базу данных:" << query.lastError();
        return false;
    }

    lastCreatedTemplateId = query.lastInsertId().toInt();

    qDebug() << "Шаблон" << templateName << "успешно создан с ID категории" << categoryId;
    return true;
}

bool TemplateManager::duplicateTemplate(int srcId, const QString &newName, int &newId) {
    //  Начинаем транзакцию
    if (!db.transaction()) {
        qDebug() << "duplicateTemplate: не удалось начать транзакцию:" << db.lastError();
        return false;
    }

    //  читаем «шапку» исходного шаблона
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT category_id,
               position,
               template_type,
               subtitle,
               notes,
               programming_notes,
               is_dynamic
        FROM   template
        WHERE  template_id = ?
    )");
    q.addBindValue(srcId);
    if (!q.exec() || !q.next()) {
        qDebug() << "duplicateTemplate: исходный шаблон не найден или ошибка:" << q.lastError();
        db.rollback();
        return false;
    }

    int     catId   = q.value(0).toInt();
    int     posSrc  = q.value(1).toInt();
    QString tType   = q.value(2).toString();
    QString subT    = q.value(3).toString();
    QString notes   = q.value(4).toString();
    QString progNt  = q.value(5).toString();
    bool    isDyn   = q.value(6).toBool();

    //  сдвигаем все шаблоны с position > posSrc в этой категории (DESC!)
    QSqlQuery sel(db);
    sel.prepare(R"(
        SELECT template_id, position
        FROM   template
        WHERE  category_id = ?
          AND  position    > ?
        ORDER  BY position DESC
    )");
    sel.addBindValue(catId);
    sel.addBindValue(posSrc);
    if (!sel.exec()) {
        qDebug() << "duplicateTemplate: не удалось выбрать шаблоны для сдвига:" << sel.lastError();
        db.rollback();
        return false;
    }
    while (sel.next()) {
        int id  = sel.value(0).toInt();
        int pos = sel.value(1).toInt();
        QSqlQuery upd(db);
        upd.prepare("UPDATE template SET position = ? WHERE template_id = ?");
        upd.addBindValue(pos + 1);
        upd.addBindValue(id);
        if (!upd.exec()) {
            qDebug() << "duplicateTemplate: ошибка сдвига шаблона" << id << upd.lastError();
            db.rollback();
            return false;
        }
    }

    //  вставляем копию «шапки» сразу после оригинала
    QSqlQuery ins(db);
    ins.prepare(R"(
        INSERT INTO template
            (category_id, name, subtitle,
             position, notes, programming_notes,
             template_type, is_dynamic)
        VALUES(?,?,?,?,?,?,?,?)
    )");
    ins.addBindValue(catId);
    ins.addBindValue(newName);
    ins.addBindValue(subT);
    ins.addBindValue(posSrc + 1);
    ins.addBindValue(notes);
    ins.addBindValue(progNt);
    ins.addBindValue(tType);
    ins.addBindValue(isDyn);
    if (!ins.exec()) {
        qDebug() << "duplicateTemplate: не удалось вставить новый шаблон:" << ins.lastError();
        db.rollback();
        return false;
    }
    newId = ins.lastInsertId().toInt();

    //  копируем содержимое: либо grid_cells, либо graph
    if (tType == "table" || tType == "listing") {
        QSqlQuery cp(db);
        cp.prepare(R"(
            INSERT INTO grid_cells
                (template_id, cell_type, row_index, col_index,
                 content, colour, row_span, col_span)
            SELECT
                ?, cell_type, row_index, col_index,
                content, colour, row_span, col_span
            FROM grid_cells
            WHERE template_id = ?
        )");
        cp.addBindValue(newId);
        cp.addBindValue(srcId);
        if (!cp.exec()) {
            qDebug() << "duplicateTemplate: не удалось скопировать grid_cells:" << cp.lastError();
            db.rollback();
            return false;
        }
    }
    else if (tType == "graph") {
        QSqlQuery cp(db);
        cp.prepare(R"(
            INSERT INTO graph
                (template_id, name, graph_type, image)
            SELECT
                ?, name, graph_type, image
            FROM graph
            WHERE template_id = ?
        )");
        cp.addBindValue(newId);
        cp.addBindValue(srcId);
        if (!cp.exec()) {
            qDebug() << "duplicateTemplate: не удалось скопировать graph:" << cp.lastError();
            db.rollback();
            return false;
        }
    }

    //  коммитим
    if (!db.commit()) {
        qDebug() << "duplicateTemplate: ошибка при коммите:" << db.lastError();
        return false;
    }

    return true;
}

bool TemplateManager::updateTemplate(int templateId,
                                     const std::optional<QString> &name,
                                     const std::optional<QString> &subtitle,
                                     const std::optional<QString> &notes,
                                     const std::optional<QString> &programmingNotes) {
    QSqlQuery query(db);

    // Формируем запрос динамически, обновляя только заданные поля
    QString queryString = "UPDATE template SET ";
    bool firstField = true;

    if (name) {
        queryString += "name = :name";
        firstField = false;
    }
    if (subtitle) {
        queryString += QString(firstField ? "" : ", ") + "subtitle = :subtitle";
        firstField = false;
    }
    if (notes) {
        queryString += QString(firstField ? "" : ", ") + "notes = :notes";
        firstField = false;
    }
    if (programmingNotes) {
        queryString += QString(firstField ? "" : ", ") + "programming_notes = :programmingNotes";
    }
    queryString += " WHERE template_id = :templateId";

    query.prepare(queryString);

    // Привязываем значения только для заданных параметров
    if (name) query.bindValue(":name", *name);
    if (subtitle) query.bindValue(":subtitle", *subtitle);
    if (notes) query.bindValue(":notes", *notes);
    if (programmingNotes) query.bindValue(":programmingNotes", *programmingNotes);
    query.bindValue(":templateId", templateId);

    // Выполнение запроса
    if (!query.exec()) {
        qDebug() << "Ошибка обновления шаблона:" << query.lastError();
        return false;
    }

    return true;
}

bool TemplateManager::deleteTemplate(int templateId) {
    //  Выясняем, какой это тип шаблона
    QSqlQuery typeQuery(db);
    typeQuery.prepare("SELECT template_type FROM template WHERE template_id = :templateId");
    typeQuery.bindValue(":templateId", templateId);

    if (!typeQuery.exec() || !typeQuery.next()) {
        qDebug() << "Ошибка: шаблон с ID" << templateId << "не найден или нет поля template_type";
        return false;
    }
    QString tmplType = typeQuery.value(0).toString();

    // Удаляем связанные данные в зависимости от типа
    if (tmplType == "table" || tmplType == "listing") {
        QSqlQuery query(db);
        query.prepare("DELETE FROM grid_cells WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);
        if (!query.exec()) {
            qDebug() << "Ошибка удаления ячеек из grid_cells:" << query.lastError().text();
            return false;
        }
    } else if (tmplType == "graph") {
        QSqlQuery query(db);
        query.prepare("DELETE FROM graph WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);
        if (!query.exec()) {
            qDebug() << "Ошибка удаления данных из graph:" << query.lastError();
            return false;
        }
    }

    QSqlQuery delTemplate(db);
    delTemplate.prepare("DELETE FROM template WHERE template_id = :templateId");
    delTemplate.bindValue(":templateId", templateId);
    if (!delTemplate.exec()) {
        qDebug() << "Ошибка удаления шаблона (из template):" << delTemplate.lastError();
        return false;
    }

    return true;
}

bool TemplateManager::copyGraphFromLibrary(const QString &graphTypeKey, int newTemplateId) {
    // 1) Находим «эталонный» граф в graph_library,
    //    где graph_type = :graphTypeKey (graph_type является PK)
    QSqlQuery libQuery(db);
    libQuery.prepare(R"(
        SELECT name, graph_type, image
        FROM graph_library
        WHERE graph_type = :gType
    )");
    libQuery.bindValue(":gType", graphTypeKey);

    if (!libQuery.exec() || !libQuery.next()) {
        qDebug() << "Ошибка: не найден граф с graph_type =" << graphTypeKey
                 << "в таблице graph_library, либо ошибка запроса:"
                 << libQuery.lastError();
        return false;
    }

    // Извлекаем данные из найденной записи
    QString baseName    = libQuery.value("name").toString();
    QString baseGType   = libQuery.value("graph_type").toString(); // совпадает с graphTypeKey
    QByteArray baseBlob = libQuery.value("image").toByteArray();

    // 2) Вставляем «копию» этого графика в таблицу graph,
    //    привязывая её к существующему template_id
    QSqlQuery insertQ(db);
    insertQ.prepare(R"(
        INSERT INTO graph (template_id, name, graph_type, image)
        VALUES (:tid, :nm, :gt, :img)
    )");
    insertQ.bindValue(":tid", newTemplateId);
    insertQ.bindValue(":nm",  baseName);
    insertQ.bindValue(":gt",  baseGType);
    insertQ.bindValue(":img", baseBlob);

    if (!insertQ.exec()) {
        qDebug() << "Ошибка вставки копии графика в таблицу 'graph':"
                 << insertQ.lastError();
        return false;
    }

    return true;
}

bool TemplateManager::updateGraphFromLibrary(const QString &graphTypeKey, int templateId) {
    //  Ищем нужный «эталон» в graph_library
    QSqlQuery libQuery(db);
    libQuery.prepare(R"(
        SELECT name, graph_type, image
        FROM graph_library
        WHERE graph_type = :gType
    )");
    libQuery.bindValue(":gType", graphTypeKey);

    if (!libQuery.exec() || !libQuery.next()) {
        qDebug() << "Ошибка: не найдено graph_type =" << graphTypeKey
                 << "в graph_library. Err:" << libQuery.lastError();
        return false;
    }

    QString newName  = libQuery.value("name").toString();
    QString newGType = libQuery.value("graph_type").toString();
    QByteArray newImg= libQuery.value("image").toByteArray();

    //  Обновляем текущую запись в graph
    QSqlQuery updQ(db);
    updQ.prepare(R"(
        UPDATE graph
        SET name = :nm,
            graph_type = :gt,
            image = :img
        WHERE template_id = :tid
    )");
    updQ.bindValue(":nm", newName);
    updQ.bindValue(":gt", newGType);
    updQ.bindValue(":img", newImg);
    updQ.bindValue(":tid", templateId);

    if (!updQ.exec()) {
        qDebug() << "Ошибка UPDATE graph:" << updQ.lastError();
        return false;
    }

    return true;
}

bool TemplateManager::setTemplateDynamic(int templateId, bool dynamic) {
    QSqlQuery query(db);
    query.prepare("UPDATE template SET is_dynamic = :dyn WHERE template_id = :tid");
    query.bindValue(":dyn", dynamic);
    query.bindValue(":tid", templateId);

    if (!query.exec()) {
        qDebug() << "Ошибка обновления is_dynamic:" << query.lastError().text();
        return false;
    }
    return true;
}

bool TemplateManager::isTemplateDynamic(int templateId) {
    QSqlQuery query(db);
    query.prepare("SELECT is_dynamic FROM template WHERE template_id = :tid");
    query.bindValue(":tid", templateId);

    if (!query.exec() || !query.next()) {
        qDebug() << "Ошибка чтения is_dynamic:" << query.lastError().text();
        return false; // по умолчанию
    }
    return query.value(0).toBool();
}

bool TemplateManager::updateTemplateCategory(int templateId, int newCategoryId) {
    QSqlQuery q(db);
    q.prepare("UPDATE template SET category_id = :newCat WHERE template_id = :tid");
    if (newCategoryId < 0)
        q.bindValue(":newCat", QVariant());      // NULL
    else
        q.bindValue(":newCat", newCategoryId);
    q.bindValue(":tid", templateId);
    if (!q.exec()) {
        qDebug() << "Ошибка обновления категории шаблона:" << q.lastError();
        return false;
    }
    return true;
}

bool TemplateManager::updateTemplatePosition(int templateId, int position) {
    QSqlQuery query(db);
    query.prepare("UPDATE template SET position = :position WHERE template_id = :id");
    query.bindValue(":position", position);
    query.bindValue(":id",       templateId);
    if (!query.exec()) {
        qDebug() << "Ошибка обновления позиции шаблона:" << query.lastError();
        return false;
    }
    return true;
}

QVector<int> TemplateManager::getDynamicTemplatesForProject(int projectId) {
    QVector<int> templateIds;
    QSqlQuery query(db);

    query.prepare(
        "SELECT t.template_id "
        "FROM template t "
        "JOIN category c ON t.category_id = c.category_id "
        "WHERE c.project_id = ? AND t.is_dynamic = TRUE"
        );
    query.addBindValue(projectId);

    if (!query.exec()) {
        qDebug() << "Ошибка загрузки динамических шаблонов проекта:" << query.lastError();
        return templateIds;
    }

    while (query.next()) {
        templateIds.append(query.value(0).toInt());
    }

    return templateIds;

}

QVector<Template> TemplateManager::getTemplatesForCategory(int categoryId) {
    QVector<Template> templates;
    QSqlQuery query(db);
    query.prepare("SELECT template_id, name, subtitle, notes, programming_notes, position "
                  "FROM template WHERE category_id = :categoryId ORDER BY position");
    query.bindValue(":categoryId", categoryId);

    if (!query.exec()) {
        qDebug() << "Ошибка получения шаблонов для категории:" << query.lastError();
        return templates;
    }

    while (query.next()) {
        templates.append({
            query.value(0).toInt(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString(),
            query.value(4).toString(),
            query.value(5).toInt()
        });
    }

    return templates;
}

TableMatrix TemplateManager::getTableData(int templateId) {
    TableMatrix table;                              // результат

    /* ---------- 1. формируем списки уникальных строк/столбцов ------ */
    QVector<int> rows, cols;
    {
        QSqlQuery q(db);
        q.prepare("SELECT DISTINCT row_index FROM grid_cells "
                  "WHERE template_id = :tid ORDER BY row_index");
        q.bindValue(":tid", templateId);
        if (q.exec())
            while (q.next()) rows << q.value(0).toInt();
    }
    {
        QSqlQuery q(db);
        q.prepare("SELECT DISTINCT col_index FROM grid_cells "
                  "WHERE template_id = :tid ORDER BY col_index");
        q.bindValue(":tid", templateId);
        if (q.exec())
            while (q.next()) cols << q.value(0).toInt();
    }
    if (rows.isEmpty() || cols.isEmpty())
        return table;                               // шаблон пуст

    const int nR = rows.size();
    const int nC = cols.size();
    table.resize(nR);
    for (int r = 0; r < nR; ++r)
        table[r].resize(nC);                        // Cell() по‑умолчанию

    /* ---------- 2. выбираем все ячейки с span‑ами ------------------- */
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT row_index, col_index, content, colour,
               COALESCE(row_span,1) AS rs,
               COALESCE(col_span,1) AS cs
        FROM   grid_cells
        WHERE  template_id = :tid
        ORDER  BY row_index, col_index)");
    q.bindValue(":tid", templateId);
    if (!q.exec()) {
        qDebug() << "getTableData(): query failed" << q.lastError();
        return table;
    }

    while (q.next()) {
        int dbRow  = q.value(0).toInt();
        int dbCol  = q.value(1).toInt();
        int r      = rows.indexOf(dbRow);
        int c      = cols.indexOf(dbCol);
        if (r < 0 || c < 0) continue;               // защита от мусора

        Cell &cell   = table[r][c];
        cell.text    = q.value(2).toString();
        QString clr  = q.value(3).toString();
        cell.colour  = (QColor(clr).isValid() ? clr : "#FFFFFF");
        cell.rowSpan = q.value(4).toInt();
        cell.colSpan = q.value(5).toInt();

        /* помечаем «теневые» ячейки внутри объединённой области */
        if (cell.rowSpan > 1 || cell.colSpan > 1) {
            for (int dr = 0; dr < cell.rowSpan; ++dr)
                for (int dc = 0; dc < cell.colSpan; ++dc)
                    if (dr || dc)
                        table[r+dr][c+dc].rowSpan = 0;   // ≤ 0 → нет ячейки
        }
    }
    return table;
}

QString TemplateManager::getSubtitleForTemplate(int templateId) {
    QSqlQuery query(db);
    query.prepare("SELECT subtitle FROM template WHERE template_id = :tid");
    query.bindValue(":tid", templateId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    } else {
        qDebug() << "Ошибка загрузки подзаголовка:" << query.lastError().text();
        return QString();
    }
}

QString TemplateManager::getNotesForTemplate(int templateId) {
    QSqlQuery query(db);
    query.prepare("SELECT notes FROM template WHERE template_id = :templateId");
    query.bindValue(":templateId", templateId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    } else {
        qDebug() << "Ошибка загрузки заметок:" << query.lastError().text();
        return QString();
    }
}

QString TemplateManager::getProgrammingNotesForTemplate(int templateId) {
    QSqlQuery query(db);
    query.prepare("SELECT programming_notes FROM template WHERE template_id = :templateId");
    query.bindValue(":templateId", templateId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    } else {
        qDebug() << "Ошибка загрузки программных заметок:" << query.lastError().text();
        return QString();
    }
}

QString TemplateManager::getTemplateType(int templateId) {
    QSqlQuery query(db);
    query.prepare("SELECT template_type FROM template WHERE template_id = :tid");
    query.bindValue(":tid", templateId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "table"; // выкинуть предупреждение
}

QByteArray TemplateManager::getGraphImage(int templateId) {
    QSqlQuery query(db);
    query.prepare("SELECT image FROM graph WHERE template_id = :tid");
    query.bindValue(":tid", templateId);
    if (query.exec() && query.next()) {
        return query.value(0).toByteArray();
    }
    return QByteArray();
}

QString TemplateManager::getGraphType(int templateId) {
    QSqlQuery query(db);
    query.prepare("SELECT graph_type FROM graph WHERE template_id = :id");
    query.bindValue(":id", templateId);

    if (!query.exec()) {
        qWarning() << query.lastError().text();
        return QString();
    }

    if (query.next()) {
        return query.value(0).toString().trimmed();
    }

    return QString();
}

QStringList TemplateManager::getGraphTypesFromLibrary() {
    QStringList types;
    QSqlQuery query(db);

    query.prepare("SELECT graph_type FROM graph_library ORDER BY graph_type");
    if (!query.exec()) {
        qDebug() << "Ошибка получения типов графиков:" << query.lastError();
        return types; // вернёт пустой список
    }

    while (query.next()) {
        QString gtype = query.value(0).toString();
        types << gtype;
    }
    return types;
}

int TemplateManager::getLastCreatedTemplateId() const {
    return lastCreatedTemplateId;
}
