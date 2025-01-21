#include "tableManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <optional>

TableManager::TableManager(QSqlDatabase &db) : db(db) {}

bool TableManager::createRowOrColumn(int templateId, const QString &type, const QString &header, int &newOrder) {
    QSqlQuery query(db);

    if (type == "column") {
        query.prepare("SELECT COALESCE(MAX(column_order), 0) + 1 FROM table_column WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);

        if (!query.exec() || !query.next()) {
            qDebug() << "Ошибка получения позиции для нового столбца:" << query.lastError();
            return false;
        }
        newOrder = query.value(0).toInt();  // Определяем новый порядковый номер

        query.prepare("INSERT INTO table_column (template_id, column_order, header) "
                      "VALUES (:templateId, :columnOrder, :header)");
        query.bindValue(":templateId", templateId);
        query.bindValue(":columnOrder", newOrder);
        query.bindValue(":header", header);

    } else if (type == "row") {
        query.prepare("SELECT COALESCE(MAX(row_order), 0) + 1 FROM table_row WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);

        if (!query.exec() || !query.next()) {
            qDebug() << "Ошибка получения позиции для новой строки:" << query.lastError();
            return false;
        }
        newOrder = query.value(0).toInt();  // Определяем новый порядковый номер

        query.prepare("INSERT INTO table_row (template_id, row_order) "
                      "VALUES (:templateId, :rowOrder)");
        query.bindValue(":templateId", templateId);
        query.bindValue(":rowOrder", newOrder);
    }

    if (!query.exec()) {
        qDebug() << "Ошибка добавления в таблицу:" << query.lastError();
        return false;
    }

    return true;
}

bool TableManager::updateOrder(const QString &type, int templateId, const QVector<int> &newOrder) {
    QSqlQuery query(db);

    QString tableName, orderColumn;
    if (type == "row") {
        tableName = "table_row";
        orderColumn = "row_order";
    } else if (type == "column") {
        tableName = "table_column";
        orderColumn = "column_order";
    } else {
        qDebug() << "Неизвестный тип для обновления порядка:" << type;
        return false;
    }

    // Обновляем порядок для каждого элемента
    for (int i = 0; i < newOrder.size(); ++i) {
        query.prepare(QString("UPDATE %1 SET %2 = :newOrder WHERE template_id = :templateId AND %2 = :currentOrder")
                          .arg(tableName, orderColumn));
        query.bindValue(":templateId", templateId);
        query.bindValue(":currentOrder", newOrder[i]);
        query.bindValue(":newOrder", i);

        if (!query.exec()) {
            qDebug() << "Ошибка обновления" << orderColumn << "в" << tableName << ":" << query.lastError();
            return false;
        }
    }

    return true;
}

bool TableManager::updateColumnHeader(int templateId, int columnOrder, const QString &newHeader) {
    QSqlQuery query(db);
    query.prepare("UPDATE table_column SET header = :newHeader WHERE template_id = :templateId AND column_order = :columnOrder");
    query.bindValue(":newHeader", newHeader);
    query.bindValue(":templateId", templateId);
    query.bindValue(":columnOrder", columnOrder);

    if (!query.exec()) {
        qDebug() << "Ошибка обновления заголовка столбца:" << query.lastError();
        return false;
    }

    return true;
}

bool TableManager::deleteRowOrColumn(int templateId, int order, const QString &type) {
    QSqlQuery query(db);

    QString tableName, orderColumn, relatedTable;
    if (type == "row") {
        tableName = "table_row";
        orderColumn = "row_order";
        relatedTable = "table_cell";
    } else if (type == "column") {
        tableName = "table_column";
        orderColumn = "column_order";
        relatedTable = "table_cell";
    } else {
        qDebug() << "Неизвестный тип для удаления:" << type;
        return false;
    }

    // Шаг 1: Удаляем строку или столбец из основной таблицы
    query.prepare(QString("DELETE FROM %1 WHERE template_id = :templateId AND %2 = :order")
                      .arg(tableName, orderColumn));
    query.bindValue(":templateId", templateId);
    query.bindValue(":order", order);

    if (!query.exec()) {
        qDebug() << "Ошибка удаления из" << tableName << ":" << query.lastError();
        return false;
    }

    // Шаг 2: Удаляем связанные данные из table_cell
    query.prepare(QString("DELETE FROM %1 WHERE template_id = :templateId AND %2 = :order")
                      .arg(relatedTable, orderColumn));
    query.bindValue(":templateId", templateId);
    query.bindValue(":order", order);

    if (!query.exec()) {
        qDebug() << "Ошибка удаления связанных данных из" << relatedTable << ":" << query.lastError();
        return false;
    }

    // Шаг 3: Обновляем порядок оставшихся строк или столбцов
    query.prepare(QString("UPDATE %1 SET %2 = %2 - 1 WHERE template_id = :templateId AND %2 > :order")
                      .arg(tableName, orderColumn));
    query.bindValue(":templateId", templateId);
    query.bindValue(":order", order);

    if (!query.exec()) {
        qDebug() << "Ошибка обновления порядка в" << tableName << ":" << query.lastError();
        return false;
    }

    return true;
}

bool TableManager::saveDataTableTemplate(int templateId,
                                         const std::optional<QVector<QString>> &headers = std::nullopt,
                                         const std::optional<QVector<QVector<QString>>> &cellData = std::nullopt) {
    QSqlQuery query(db);

    // Шаг 1: Обновляем заголовки столбцов, если переданы
    if (headers) {
        // Удаляем старую структуру столбцов
        query.prepare("DELETE FROM table_column WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);
        if (!query.exec()) {
            qDebug() << "Ошибка удаления столбцов таблицы:" << query.lastError();
            return false;
        }

        // Добавляем новые столбцы
        for (int col = 0; col < headers->size(); ++col) {
            query.prepare("INSERT INTO table_column (template_id, column_order, header) VALUES (:templateId, :columnOrder, :header)");
            query.bindValue(":templateId", templateId);
            query.bindValue(":columnOrder", col);
            query.bindValue(":header", (*headers)[col]);

            if (!query.exec()) {
                qDebug() << "Ошибка добавления столбца:" << query.lastError();
                return false;
            }
        }
    }

    // Шаг 2: Обновляем данные ячеек, если переданы
    if (cellData) {
        // Удаляем старую структуру строк и ячеек
        query.prepare("DELETE FROM table_row WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);
        if (!query.exec()) {
            qDebug() << "Ошибка удаления строк таблицы:" << query.lastError();
            return false;
        }

        query.prepare("DELETE FROM table_cell WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);
        if (!query.exec()) {
            qDebug() << "Ошибка удаления ячеек таблицы:" << query.lastError();
            return false;
        }

        // Добавляем новые строки и ячейки
        for (int row = 0; row < cellData->size(); ++row) {
            query.prepare("INSERT INTO table_row (template_id, row_order) VALUES (:templateId, :rowOrder)");
            query.bindValue(":templateId", templateId);
            query.bindValue(":rowOrder", row);

            if (!query.exec()) {
                qDebug() << "Ошибка добавления строки:" << query.lastError();
                return false;
            }

            for (int col = 0; col < (*cellData)[row].size(); ++col) {
                query.prepare("INSERT INTO table_cell (template_id, row_order, column_order, content) VALUES (:templateId, :rowOrder, :columnOrder, :content)");
                query.bindValue(":templateId", templateId);
                query.bindValue(":rowOrder", row);
                query.bindValue(":columnOrder", col);
                query.bindValue(":content", (*cellData)[row][col]);

                if (!query.exec()) {
                    qDebug() << "Ошибка добавления данных ячейки:" << query.lastError();
                    return false;
                }
            }
        }
    }

    return true;
}

