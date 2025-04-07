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
        INSERT INTO template (category_id, name, position, notes, programming_notes, template_type)
        VALUES (:categoryId, :name, :position, '', '', :templateType)
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

bool TemplateManager::updateTemplate(int templateId,
                                     const std::optional<QString> &name,
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
        // Табличные/листинговые шаблоны - удаляем строки, столбцы, ячейки
        QSqlQuery query(db);

        query.prepare("DELETE FROM table_cell WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);
        if (!query.exec()) {
            qDebug() << "Ошибка удаления table_cell:" << query.lastError();
            return false;
        }

        query.prepare("DELETE FROM table_row WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);
        if (!query.exec()) {
            qDebug() << "Ошибка удаления table_row:" << query.lastError();
            return false;
        }

        query.prepare("DELETE FROM table_column WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);
        if (!query.exec()) {
            qDebug() << "Ошибка удаления table_column:" << query.lastError();
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
    query.prepare("SELECT template_id, name, notes, programming_notes, position "
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
            query.value(4).toInt()
        });
    }

    return templates;
}

QVector<QString> TemplateManager::getColumnHeadersForTemplate(int templateId) {
    QVector<QString> columnHeaders;
    QSqlQuery query(db);
    query.prepare("SELECT header FROM table_column WHERE template_id = :templateId ORDER BY column_order");
    query.bindValue(":templateId", templateId);

    if (!query.exec()) {
        qDebug() << "Ошибка загрузки заголовков столбцов:" << query.lastError();
        return columnHeaders;
    }

    while (query.next()) {
        columnHeaders.append(query.value(0).toString());
    }
    return columnHeaders;
}

QVector<int> TemplateManager::getRowOrdersForTemplate(int templateId) {
    QVector<int> rowOrders;
    QSqlQuery query(db);
    query.prepare("SELECT row_order FROM table_row WHERE template_id = :templateId ORDER BY row_order");
    query.bindValue(":templateId", templateId);

    if (!query.exec()) {
        qDebug() << "Ошибка загрузки строк таблицы:" << query.lastError();
        return rowOrders;
    }

    while (query.next()) {
        rowOrders.append(query.value(0).toInt());
    }
    return rowOrders;
}

QVector<int> TemplateManager::getColumnOrdersForTemplate(int templateId) {
    QVector<int> columnOrders;
    QSqlQuery query(db);
    query.prepare("SELECT column_order FROM table_column WHERE template_id = :templateId ORDER BY column_order");
    query.bindValue(":templateId", templateId);

    if (!query.exec()) {
        qDebug() << "Ошибка загрузки столбцов таблицы:" << query.lastError();
        return columnOrders;
    }

    while (query.next()) {
        columnOrders.append(query.value(0).toInt());
    }
    return columnOrders;
}

QVector<QVector<QPair<QString, QString>>> TemplateManager::getTableData(int templateId) {
    QVector<QVector<QPair<QString, QString>>> tableData;

    // Получаем порядки строк и столбцов
    QVector<int> rowOrders = getRowOrdersForTemplate(templateId);
    QVector<int> columnOrders = getColumnOrdersForTemplate(templateId);

    // Проверка наличия строк и столбцов
    if (rowOrders.isEmpty() || columnOrders.isEmpty()) {
        qDebug() << "Таблица пуста или отсутствуют строки/столбцы.";
        return tableData; // Возвращаем пустую таблицу
    }

    // Инициализация пустой таблицы на основе строк и столбцов
    tableData.resize(rowOrders.size());
    for (int row = 0; row < rowOrders.size(); ++row) {
        tableData[row].resize(columnOrders.size());
        for (int col = 0; col < columnOrders.size(); ++col) {
            tableData[row][col] = qMakePair(QString(""), QString("#FFFFFF"));
        }
    }

    // Получаем данные ячеек
    QSqlQuery cellQuery(db);
    cellQuery.prepare(
        "SELECT row_order, column_order, content, colour "
        "FROM table_cell "
        "WHERE template_id = :templateId "
        "ORDER BY row_order, column_order"
        );
    cellQuery.bindValue(":templateId", templateId);

    if (!cellQuery.exec()) {
        qDebug() << "Ошибка загрузки данных таблицы:" << cellQuery.lastError();
        return tableData;
    }

    // Заполняем таблицу данными
    while (cellQuery.next()) {
        int rowOrder = cellQuery.value(0).toInt();
        int columnOrder = cellQuery.value(1).toInt();
        QString content = cellQuery.value(2).toString();
        QString colour = cellQuery.value(3).toString();

        // Если значение цвета отсутствует или недопустимо, устанавливаем белый (#FFFFFF)
        if (colour.isEmpty() || !QColor(colour).isValid())
            colour = "#FFFFFF";

        // Ищем индексы строки и столбца в порядке rowOrders и columnOrders
        int rowIndex = rowOrders.indexOf(rowOrder);
        int columnIndex = columnOrders.indexOf(columnOrder);

        if (rowIndex != -1 && columnIndex != -1) {
            tableData[rowIndex][columnIndex] = qMakePair(content, colour);
        }
    }

    return tableData;
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
