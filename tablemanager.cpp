#include "tablemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <optional>

TableManager::TableManager(QSqlDatabase &db)
    : db(db) {}

TableManager::~TableManager() {}

bool TableManager::createRowOrColumn(int templateId, const QString &type, const QString &header, int &newOrder) {
    QSqlQuery query(db);

    if (type == "column") {
        query.prepare("SELECT COALESCE(MAX(column_order), 0) + 1 FROM table_column WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);

        if (!query.exec() || !query.next()) {
            qDebug() << "Ошибка получения позиции для нового столбца:" << query.lastError();
            return false;
        }
        newOrder = query.value(0).toInt();  // Новый порядковый номер



        // Вставляем запись в table_column с использованием header
        query.prepare("INSERT INTO table_column (template_id, column_order, header) "
                      "VALUES (:templateId, :columnOrder, :header)");
        query.bindValue(":templateId", templateId);
        query.bindValue(":columnOrder", newOrder);
        query.bindValue(":header", header);
    }
    else if (type == "row") {
        query.prepare("SELECT COALESCE(MAX(row_order), 0) + 1 FROM table_row WHERE template_id = :templateId");
        query.bindValue(":templateId", templateId);

        if (!query.exec() || !query.next()) {
            qDebug() << "Ошибка получения позиции для новой строки:" << query.lastError();
            return false;
        }
        newOrder = query.value(0).toInt();  // Новый порядковый номер

        query.prepare("INSERT INTO table_row (template_id, row_order) VALUES (:templateId, :rowOrder)");
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
        // Обновляем порядок в основной таблице (table_row или table_column)
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

bool TableManager::updateCellColour(int templateId, int rowOrder, int columnOrder, const QString &colour) {
    QSqlQuery query(db);
    query.prepare("UPDATE table_cell SET colour = :colour WHERE template_id = :templateId AND row_order = :rowOrder AND column_order = :columnOrder");
    query.bindValue(":colour", colour);
    query.bindValue(":templateId", templateId);
    query.bindValue(":rowOrder", rowOrder);
    query.bindValue(":columnOrder", columnOrder);

    if (!query.exec()) {
        qDebug() << "Ошибка обновления цвета ячейки:" << query.lastError();
        return false;
    }
    return true;
}

bool TableManager::updateColumnHeader(int templateId, int columnOrder, const QString &newHeader) {
    QSqlQuery query(db);
    query.prepare("UPDATE table_column SET header = :newHeader WHERE template_id = :templateId");
    query.bindValue(":newHeader", newHeader);
    query.bindValue(":templateId", templateId);

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
                                         const std::optional<QVector<QVector<QString>>> &cellData = std::nullopt,
                                         const std::optional<QVector<QVector<QString>>> &cellColours = std::nullopt) {
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

        // Добавляем новые столбцы через HeaderManager
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
                query.prepare("INSERT INTO table_cell (template_id, row_order, column_order, content, colour) VALUES (:templateId, :rowOrder, :columnOrder, :content, :colour)");
                query.bindValue(":templateId", templateId);
                query.bindValue(":rowOrder", row);
                query.bindValue(":columnOrder", col);
                query.bindValue(":content", (*cellData)[row][col]);
                query.bindValue(":colour", (*cellColours)[row][col]);

                if (!query.exec()) {
                    qDebug() << "Ошибка добавления данных ячейки:" << query.lastError();
                    return false;
                }
            }
        }
    }

    return true;
}

bool TableManager::generateColumnsForDynamicTemplate(int templateId, const QVector<QString>& groupNames) {

    int numGroups = groupNames.size();

    // Минимум 1 группа
    if (numGroups < 1) {
        qDebug() << "Число групп не может быть меньше 1.";
        return false;
    }

    // 1. Начинаем транзакцию
    if (!db.transaction()) {
        qDebug() << "Не удалось начать транзакцию:" << db.lastError();
        return false;
    }

    // 2. Получаем все столбцы для данного шаблона
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT column_order, header
        FROM table_column
        WHERE template_id = :templateId
        ORDER BY column_order
    )");
    query.bindValue(":templateId", templateId);
    if (!query.exec()) {
        qDebug() << "Ошибка получения столбцов:" << query.lastError();
        db.rollback();
        return false;
    }

    // Ищем «Группу 1» (шаблон) и прочие динамические столбцы
    int group1Order = -1;
    QString group1Header;
    QVector<int> otherDynamicOrders; // все «Группа X», кроме первой

    // Сохраняем все столбцы в вектор, если понадобится
    QVector<QPair<int, QString>> columns;
    while (query.next()) {
        int colOrder = query.value(0).toInt();
        QString header = query.value(1).toString();
        columns.append({colOrder, header});

        // Ищем динамические столбцы, начинающиеся с «Группа »
        if (header.startsWith("Группа ")) {
            if (group1Order < 0) {
                // Первый найденный считаем «Группой 1»
                group1Order = colOrder;
                group1Header = header;
            } else {
                // Остальные — в список на удаление
                otherDynamicOrders.append(colOrder);
            }
        }
    }

    // Если «Группа 1» не найдена — прерываем
    if (group1Order < 0) {
        qDebug() << "Не найдена «Группа 1». Прерываем.";
        db.rollback();
        return false;
    }

    // 3. Удаляем все прочие динамические столбцы (кроме «Группы 1»)
    for (int dynCol : otherDynamicOrders) {
        // Удаляем связанные ячейки
        QSqlQuery delCell(db);
        delCell.prepare("DELETE FROM table_cell WHERE template_id = :tid AND column_order = :co");
        delCell.bindValue(":tid", templateId);
        delCell.bindValue(":co", dynCol);
        if (!delCell.exec()) {
            qDebug() << "Ошибка удаления ячеек динамического столбца" << dynCol << delCell.lastError();
            db.rollback();
            return false;
        }

        // Удаляем столбец
        QSqlQuery delC(db);
        delC.prepare("DELETE FROM table_column WHERE template_id = :tid AND column_order = :co");
        delC.bindValue(":tid", templateId);
        delC.bindValue(":co", dynCol);
        if (!delC.exec()) {
            qDebug() << "Ошибка удаления динамического столбца" << dynCol << delC.lastError();
            db.rollback();
            return false;
        }
    }

    // 4. Сдвигаем столбцы, которые идут ПОСЛЕ «Группы 1», на (numGroups - 2)
    int shift = numGroups - 2; // Если numGroups=3, сдвигаем на 2 и т.д.
    if (shift > 0) {
        // Сдвиг для table_column: сначала выбираем нужные столбцы в порядке убывания
        QSqlQuery selectQuery(db);
        selectQuery.prepare(R"(
        SELECT column_order
        FROM table_column
        WHERE template_id = :tid
          AND column_order > :group1Order
        ORDER BY column_order DESC
    )");
        selectQuery.bindValue(":tid", templateId);
        selectQuery.bindValue(":group1Order", group1Order);
        if (!selectQuery.exec()) {
            qDebug() << "Ошибка выбора столбцов для сдвига:" << selectQuery.lastError();
            db.rollback();
            return false;
        }
        QVector<int> orders;
        while (selectQuery.next()) {
            orders.append(selectQuery.value(0).toInt());
        }
        // Обновляем каждый столбец по одному, начиная с наибольшего значения column_order
        for (int oldOrder : orders) {
            int newOrder = oldOrder + shift;
            QSqlQuery updateQuery(db);
            updateQuery.prepare(R"(
             UPDATE table_column
             SET column_order = :newOrder
             WHERE template_id = :tid AND column_order = :oldOrder
         )");
            updateQuery.bindValue(":newOrder", newOrder);
            updateQuery.bindValue(":tid", templateId);
            updateQuery.bindValue(":oldOrder", oldOrder);
            if (!updateQuery.exec()) {
                qDebug() << "Ошибка сдвига столбца:" << updateQuery.lastError();
                db.rollback();
                return false;
            }
        }

        // Аналогично для table_cell
        QSqlQuery selectCellQuery(db);
        selectCellQuery.prepare(R"(
         SELECT column_order
         FROM table_cell
         WHERE template_id = :tid
           AND column_order > :group1Order
         ORDER BY column_order DESC
    )");
        selectCellQuery.bindValue(":tid", templateId);
        selectCellQuery.bindValue(":group1Order", group1Order);
        if (!selectCellQuery.exec()) {
            qDebug() << "Ошибка выбора ячеек для сдвига:" << selectCellQuery.lastError();
            db.rollback();
            return false;
        }
        QVector<int> cellOrders;
        while (selectCellQuery.next()) {
            cellOrders.append(selectCellQuery.value(0).toInt());
        }
        for (int oldOrder : cellOrders) {
            int newOrder = oldOrder + shift;
            QSqlQuery updateCellQuery(db);
            updateCellQuery.prepare(R"(
             UPDATE table_cell
             SET column_order = :newOrder
             WHERE template_id = :tid AND column_order = :oldOrder
         )");
            updateCellQuery.bindValue(":newOrder", newOrder);
            updateCellQuery.bindValue(":tid", templateId);
            updateCellQuery.bindValue(":oldOrder", oldOrder);
            if (!updateCellQuery.exec()) {
                qDebug() << "Ошибка сдвига ячеек:" << updateCellQuery.lastError();
                db.rollback();
                return false;
            }
        }
    }

    // 5. Считываем данные «Группы 1» (row_order -> content)
    QMap<int, QString> group1Data;
    {
        QSqlQuery read1(db);
        read1.prepare(R"(
            SELECT row_order, content
            FROM table_cell
            WHERE template_id = :tid
              AND column_order = :group1Order
            ORDER BY row_order
        )");
        read1.bindValue(":tid", templateId);
        read1.bindValue(":group1Order", group1Order);
        if (!read1.exec()) {
            qDebug() << "Ошибка чтения ячеек группы 1:" << read1.lastError();
            db.rollback();
            return false;
        }
        while (read1.next()) {
            int rowOrder = read1.value(0).toInt();
            QString content = read1.value(1).toString();
            group1Data[rowOrder] = content;
        }
    }

    // 6. Создаём группы 2..numGroups
    //    «Группа 1» остаётся на месте. Новые столбцы в промежутке.
    for (int i = 2; i <= numGroups; i++) {
        int newOrder = group1Order + (i - 1);
        // Например, если group1Order=3 и i=2, newOrder=4, i=3 -> 5, и т.д.

        // Вместо формирования нового заголовка через replace, берем из вектора:
        QString newHeader = groupNames[i - 1]; // индексация: i=2 соответствует элементу с индексом 1

        // Вставляем столбец
        QSqlQuery insCol(db);
        insCol.prepare(R"(
            INSERT INTO table_column (template_id, column_order, header)
            VALUES (:tid, :colOrder, :hdr)
        )");
        insCol.bindValue(":tid", templateId);
        insCol.bindValue(":colOrder", newOrder);
        insCol.bindValue(":hdr", newHeader);
        if (!insCol.exec()) {
            qDebug() << "Ошибка вставки новой группы" << i << insCol.lastError();
            db.rollback();
            return false;
        }

        // Копируем ячейки из «Группы 1»
        for (auto it = group1Data.constBegin(); it != group1Data.constEnd(); ++it) {
            int rowOrder = it.key();
            QString content = it.value();

            QSqlQuery insCell(db);
            insCell.prepare(R"(
                INSERT INTO table_cell (template_id, row_order, column_order, content)
                VALUES (:tid, :rowOrder, :colOrder, :content)
            )");
            insCell.bindValue(":tid", templateId);
            insCell.bindValue(":rowOrder", rowOrder);
            insCell.bindValue(":colOrder", newOrder);
            insCell.bindValue(":content", content);
            if (!insCell.exec()) {
                qDebug() << "Ошибка копирования ячеек для группы" << i << insCell.lastError();
                db.rollback();
                return false;
            }
        }
    }

    // Коммит транзакции
    if (!db.commit()) {
        qDebug() << "Ошибка коммита:" << db.lastError();
        return false;
    }

    qDebug() << "Динамические столбцы обновлены, все столбцы переупорядочены.";
    return true;
}
