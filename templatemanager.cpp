#include "TemplateManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <optional>

TemplateManager::TemplateManager(QSqlDatabase &db) : db(db) {}

bool TemplateManager::createTemplate(int categoryId, const QString &templateName) {
    QSqlQuery query(db);

    // Проверяем существование категории
    query.prepare("SELECT 1 FROM category WHERE category_id = :categoryId");
    query.bindValue(":categoryId", categoryId);

    if (!query.exec() || !query.next()) {
        qDebug() << "Ошибка: категория с ID" << categoryId << "не существует.";
        return false;
    }

    // Получаем максимальную позицию среди шаблонов в данной категории
    //query.prepare("SELECT COALESCE(MAX(position), 0) + 1 FROM table_template WHERE category_id = :categoryId");
    query.prepare("SELECT COALESCE(MAX(position), 0) + 1 FROM ("
                  "  SELECT position FROM category WHERE parent_id = :categoryId "
                  "  UNION ALL "
                  "  SELECT position FROM table_template WHERE category_id = :categoryId"
                  ") AS combined");
    query.bindValue(":categoryId", categoryId);

    if (!query.exec() || !query.next()) {
        qDebug() << "Ошибка получения максимального position:" << query.lastError();
        return false;
    }

    int newPosition = query.value(0).toInt();

    // Вставляем новый шаблон в таблицу table_template
    query.prepare("INSERT INTO table_template (category_id, name, position, notes, programming_notes) "
                  "VALUES (:categoryId, :name, :position, '', '')");
    query.bindValue(":categoryId", categoryId);
    query.bindValue(":name", templateName);
    query.bindValue(":position", newPosition);

    if (!query.exec()) {
        qDebug() << "Ошибка добавления шаблона в базу данных:" << query.lastError();
        return false;
    }

    qDebug() << "Шаблон" << templateName << "успешно создан с ID категории" << categoryId;
    return true;
}

bool TemplateManager::updateTemplate(int templateId,
                                     const std::optional<QString> &name,
                                     const std::optional<QString> &notes,
                                     const std::optional<QString> &programmingNotes) {
    QSqlQuery query(db);

    // Формируем запрос динамически, обновляя только заданные поля
    QString queryString = "UPDATE table_template SET ";
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
    QSqlQuery query(db);

    // Удаляем связанные данные
    query.prepare("DELETE FROM table_cell WHERE template_id = :templateId");
    query.bindValue(":templateId", templateId);
    if (!query.exec()) {
        qDebug() << "Ошибка удаления данных из table_cell:" << query.lastError();
        return false;
    }

    query.prepare("DELETE FROM table_row WHERE template_id = :templateId");
    query.bindValue(":templateId", templateId);
    if (!query.exec()) {
        qDebug() << "Ошибка удаления данных из table_row:" << query.lastError();
        return false;
    }

    query.prepare("DELETE FROM table_column WHERE template_id = :templateId");
    query.bindValue(":templateId", templateId);
    if (!query.exec()) {
        qDebug() << "Ошибка удаления данных из table_column:" << query.lastError();
        return false;
    }

    // Удаляем сам шаблон
    query.prepare("DELETE FROM table_template WHERE template_id = :templateId");
    query.bindValue(":templateId", templateId);
    if (!query.exec()) {
        qDebug() << "Ошибка удаления шаблона:" << query.lastError();
        return false;
    }

    return true;
}

QVector<Template> TemplateManager::getTemplatesForCategory(int categoryId) {
    QVector<Template> templates;
    QSqlQuery query(db);
    query.prepare("SELECT template_id, name, notes, programming_notes, position "
                  "FROM table_template WHERE category_id = :categoryId ORDER BY position");
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

QVector<QVector<QString>> TemplateManager::getTableData(int templateId) {
    QVector<QVector<QString>> tableData;

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
    }

    // Получаем данные ячеек
    QSqlQuery cellQuery(db);
    cellQuery.prepare(
        "SELECT row_order, column_order, content "
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

        // Ищем индексы строки и столбца в порядке rowOrders и columnOrders
        int rowIndex = rowOrders.indexOf(rowOrder);
        int columnIndex = columnOrders.indexOf(columnOrder);

        if (rowIndex != -1 && columnIndex != -1) {
            tableData[rowIndex][columnIndex] = content;
        }
    }

    return tableData;
}

QString TemplateManager::getNotesForTemplate(int templateId) {
    QSqlQuery query(db);
    query.prepare("SELECT notes FROM table_template WHERE template_id = :templateId");
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
    query.prepare("SELECT programming_notes FROM table_template WHERE template_id = :templateId");
    query.bindValue(":templateId", templateId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    } else {
        qDebug() << "Ошибка загрузки программных заметок:" << query.lastError().text();
        return QString();
    }
}
