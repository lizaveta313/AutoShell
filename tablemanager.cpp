#include "tablemanager.h"
#include <QSqlQuery>
#include <QSqlError>

TableManager::TableManager(QSqlDatabase &db)
    : db(db) {}

TableManager::~TableManager() {}

bool TableManager::addRow(int templateId, bool addToHeader, const QString &headerContent) {
    QSqlQuery q(db);

    /* --- Если шаблон пустой – создаём единственную ячейку (1,1) header --- */
    q.prepare(QLatin1String("SELECT COUNT(*) FROM grid_cells WHERE template_id = :tid"));
    q.bindValue(":tid", templateId);
    if (!q.exec() || !q.next()) {
        qDebug() << "addRow(): COUNT failed:" << q.lastError();
        return false;
    }
    if (q.value(0).toInt() == 0) {
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO grid_cells (template_id, cell_type,
                                    row_index, col_index, content)
            VALUES (:tid, 'header', 1, 1, :content)
        )");
        ins.bindValue(":tid",     templateId);
        ins.bindValue(":content", headerContent);
        if (!ins.exec()) {
            qDebug() << "addRow(): cannot create first cell:" << ins.lastError();
            return false;
        }
        return true;
    }

    /* --- Шаблон не пустой ------------------------------------------------ */

    int newRow = 0;
    if (addToHeader) {                      // вставляем в заголовок
        q.prepare(R"(
            SELECT COALESCE(MAX(row_index),0)
            FROM grid_cells
            WHERE template_id = :tid AND cell_type = 'header'
        )");
        q.bindValue(":tid", templateId);
        if (!q.exec() || !q.next()) {
            qDebug() << "addRow(): header MAX(row_index) failed:" << q.lastError();
            return false;
        }
        newRow = q.value(0).toInt() + 1;

        /* ---------------- Сдвигаем строки «снизу‑вверх» ------------------ */
        QSqlQuery list(db);
        list.prepare(R"(
            SELECT DISTINCT row_index
            FROM grid_cells
            WHERE template_id = :tid
              AND  cell_type   = 'content'
              AND row_index    >= :newRow
            ORDER BY row_index DESC
        )");
        list.bindValue(":tid",    templateId);
        list.bindValue(":newRow", newRow);

        if (!list.exec()) {
            qDebug() << "addRow(): cannot fetch rows to shift:" << list.lastError();
            return false;
        }

        while (list.next()) {
            const int oldRow = list.value(0).toInt();
            const int newIdx = oldRow + 1;

            QSqlQuery upd(db);
            upd.prepare(R"(
            UPDATE grid_cells
            SET row_index = :newIdx
            WHERE template_id = :tid
            AND cell_type   = 'content'
            AND row_index    = :oldRow
            )");
            upd.bindValue(":newIdx", newIdx);
            upd.bindValue(":tid",    templateId);
            upd.bindValue(":oldRow", oldRow);

            if (!upd.exec()) {
                qDebug() << "addRow(): shift row" << oldRow
                         << "->" << newIdx << "failed:" << upd.lastError();
                return false;          // прерываем, чтобы не оставить БД в мусорном состоянии
            }
        }
        /* ---------------------------------------------------------------- */

    } else {                                // вставляем в тело таблицы
        q.prepare(R"(
            SELECT COALESCE(MAX(row_index),0)
            FROM grid_cells
            WHERE template_id = :tid AND cell_type = 'content'
        )");
        q.bindValue(":tid", templateId);
        if (!q.exec() || !q.next()) {
            qDebug() << "addRow(): content MAX(row_index) failed:" << q.lastError();
            return false;
        }
        newRow = q.value(0).toInt() + 1;

        /* Если это первая строка контента */
        if (newRow == 1) {
            newRow = getRowCountForHeader(templateId) + 1;
        }
        /* Сдвиг не требуется – строка добавляется «в самый низ» */
    }

    /* --- Определяем столбцы, в которых нужно создать ячейки ------------- */
    QVector<int> columns;
    QSqlQuery colQ(db);
    colQ.prepare(R"(
        SELECT DISTINCT col_index
        FROM grid_cells
        WHERE template_id = :tid
        ORDER BY col_index
    )");
    colQ.bindValue(":tid", templateId);
    if (!colQ.exec()) {
        qDebug() << "addRow(): fetch cols failed:" << colQ.lastError();
        return false;
    }
    while (colQ.next())
        columns << colQ.value(0).toInt();

    /* --- Вставляем ячейки новой строки ---------------------------------- */
    for (int col : columns) {
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO grid_cells (template_id, cell_type,
                                    row_index, col_index, content)
            VALUES (:tid, :ctype, :row, :col, :cnt)
        )");
        ins.bindValue(":tid",   templateId);
        ins.bindValue(":ctype", addToHeader ? "header" : "content");
        ins.bindValue(":row",   newRow);
        ins.bindValue(":col",   col);
        ins.bindValue(":cnt",   (addToHeader && col == columns.first())
                               ? headerContent : QStringLiteral(""));
        if (!ins.exec()) {
            qDebug() << "addRow(): INSERT failed for col" << col
                     << ins.lastError();
            return false;
        }
    }
    return true;
}

bool TableManager::addColumn(int templateId, const QString &headerContent) {
    QSqlQuery q(db);

    /* --- 1. Пустая таблица ------------------------------------------- */
    q.prepare("SELECT COUNT(*) FROM grid_cells WHERE template_id = :tid");
    q.bindValue(":tid", templateId);
    if (!q.exec() || !q.next()) {
        qDebug() << "addColumn(): COUNT failed:" << q.lastError();
        return false;
    }
    if (q.value(0).toInt() == 0) {
        // создаём первую ячейку (1,1) header
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO grid_cells (template_id, cell_type,
                                    row_index, col_index, content)
            VALUES (:tid,'header',1,1,:cnt)
        )");
        ins.bindValue(":tid", templateId);
        ins.bindValue(":cnt", headerContent);
        return ins.exec();
    }

    /* --- 2. Определяем новый номер столбца --------------------------- */
    q.prepare("SELECT MAX(col_index) FROM grid_cells WHERE template_id = :tid");
    q.bindValue(":tid", templateId);
    if (!q.exec() || !q.next()) {
        qDebug() << "addColumn(): MAX(col_index) failed:" << q.lastError();
        return false;
    }
    const int newCol      = q.value(0).toInt() + 1;
    const int refCol      = newCol - 1;               // «сосед» слева

    /* --- 3. Читаем все строки и их тип в колонке‑образце ------------- */
    QMap<int, QString> rowType;                       // row_index → "header"/"content"
    QSqlQuery refQ(db);
    refQ.prepare(R"(
        SELECT row_index, cell_type
        FROM   grid_cells
        WHERE  template_id = :tid
          AND  col_index   = :ref
        ORDER BY row_index
    )");
    refQ.bindValue(":tid", templateId);
    refQ.bindValue(":ref", refCol);
    if (!refQ.exec()) {
        qDebug() << "addColumn(): fetch ref column failed:" << refQ.lastError();
        return false;
    }
    while (refQ.next())
        rowType[refQ.value(0).toInt()] = refQ.value(1).toString();

    /* --- 4. Создаём ячейки нового столбца в соответствии с образцом --- */
    for (auto it = rowType.constBegin(); it != rowType.constEnd(); ++it) {
        const int      row  = it.key();
        const QString &type = it.value();             // already "header"/"content"

        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO grid_cells (template_id, cell_type,
                                    row_index, col_index, content)
            VALUES (:tid, :ctype, :row, :col, :cnt)
        )");
        ins.bindValue(":tid",   templateId);
        ins.bindValue(":ctype", type);
        ins.bindValue(":row",   row);
        ins.bindValue(":col",   newCol);
        // только в самой верхней header‑ячейке ставим текст, если передан
        const bool firstHeader = (type == "header" && row == rowType.firstKey());
        ins.bindValue(":cnt", firstHeader ? headerContent : QStringLiteral(""));
        if (!ins.exec()) {
            qDebug() << "addColumn(): INSERT row" << row << "failed:" << ins.lastError();
            return false;
        }
    }
    return true;
}

bool TableManager::deleteRow(int templateId, int row) {
    QSqlQuery q(db);

    /* Удаляем ячейки строки любого типа */
    q.prepare(R"(
        DELETE FROM grid_cells
        WHERE template_id = :tid
          AND row_index   = :row
    )");
    q.bindValue(":tid", templateId);
    q.bindValue(":row", row);
    if (!q.exec()) {
        qDebug() << "deleteRow(): DELETE failed:" << q.lastError();
        return false;
    }

    /* ---------- безопасно сдвигаем строки на −1 -------------------- */
    QSqlQuery list(db);
    list.prepare(R"(
    SELECT DISTINCT row_index
    FROM   grid_cells
    WHERE  template_id = :tid
      AND  row_index   > :deleted
    ORDER  BY row_index ASC          -- ← идём снизу ↑ вверх
)");
    list.bindValue(":tid",     templateId);
    list.bindValue(":deleted", row);     // row — та, что мы только что стерли

    if (!list.exec()) {
        qDebug() << "deleteRow(): fetch rows failed:" << list.lastError();
        return false;
    }

    while (list.next()) {
        const int oldRow = list.value(0).toInt();  // 11, 12, 13 …
        const int newRow = oldRow - 1;             // 10, 11, 12 …

        QSqlQuery upd(db);
        upd.prepare(R"(
        UPDATE grid_cells
           SET row_index = :newRow
         WHERE template_id = :tid
           AND row_index   = :oldRow
    )");
        upd.bindValue(":newRow", newRow);
        upd.bindValue(":tid",    templateId);
        upd.bindValue(":oldRow", oldRow);

        if (!upd.exec()) {
            qDebug() << "deleteRow(): shift row" << oldRow
                     << "→" << newRow << "failed:" << upd.lastError();
            return false;           // прекращаем во избежание разрыва данных
        }
    }
    return true;
}

bool TableManager::deleteColumn(int templateId, int col) {
    QSqlQuery q(db);

    /* Удаляем ячейки столбца (и header, и content) */
    q.prepare(R"(
        DELETE FROM grid_cells
        WHERE template_id = :tid
          AND col_index   = :col
    )");
    q.bindValue(":tid", templateId);
    q.bindValue(":col", col);
    if (!q.exec()) {
        qDebug() << "deleteColumn(): DELETE failed:" << q.lastError();
        return false;
    }

    /* Сдвигаем остальные столбцы */
    QSqlQuery list(db);
    list.prepare(R"(
        SELECT DISTINCT col_index
        FROM   grid_cells
        WHERE  template_id = :tid
          AND  col_index   > :deleted
        ORDER  BY col_index ASC
    )");
    list.bindValue(":tid",     templateId);
    list.bindValue(":deleted", col);

    if (!list.exec()) {
        qDebug() << "deleteColumn(): fetch cols failed:" << list.lastError();
        return false;
    }

    while (list.next()) {
        const int oldCol = list.value(0).toInt();
        const int newCol = oldCol - 1;

        QSqlQuery upd(db);
        upd.prepare(R"(
            UPDATE grid_cells
            SET col_index = :newCol
            WHERE template_id = :tid
              AND col_index   = :oldCol
        )");
        upd.bindValue(":newCol", newCol);
        upd.bindValue(":tid",    templateId);
        upd.bindValue(":oldCol", oldCol);

        if (!upd.exec()) {
            qDebug() << "deleteColumn(): shift col" << oldCol << "→" << newCol << "failed:" << upd.lastError();
            return false;
        }
    }
    return true;
}

int TableManager::getRowCountForHeader(int templateId) {
    QSqlQuery q(db);
    q.prepare("SELECT MAX(row_index) FROM grid_cells WHERE template_id=:tid AND cell_type='header'");
    q.bindValue(":tid", templateId);
    if(q.exec() && q.next()){
        return q.value(0).toInt();
    }
    return 0;
}

int TableManager::getColCountForHeader(int templateId) {
    QSqlQuery q(db);
    q.prepare("SELECT MAX(col_index) FROM grid_cells WHERE template_id=:tid AND cell_type='header'");
    q.bindValue(":tid", templateId);
    if(q.exec() && q.next()){
        return q.value(0).toInt();
    }
    return 0;
}

bool TableManager::updateCellColour(int templateId, int rowIndex, int colIndex, const QString &colour) {
    QSqlQuery query(db);
    query.prepare("UPDATE grid_cells SET colour = :colour WHERE template_id = :templateId AND cell_type = 'content' AND row_index = :rowIndex AND col_index = :colIndex");
    query.bindValue(":colour", colour);
    query.bindValue(":templateId", templateId);
    query.bindValue(":rowIndex", rowIndex);
    query.bindValue(":colIndex", colIndex);
    if (!query.exec()) {
        qDebug() << "Ошибка обновления цвета ячейки:" << query.lastError();
        return false;
    }
    return true;
}

bool TableManager::saveDataTableTemplate(int templateId,
                                         const std::optional<QVector<QString>> &headers = std::nullopt,
                                         const std::optional<QVector<QVector<QString>>> &cellData = std::nullopt,
                                         const std::optional<QVector<QVector<QString>>> &cellColours = std::nullopt) {
    const int headerRowsOld = getRowCountForHeader(templateId);   // ← УЖЕ есть в классе

    QSqlQuery query(db);

    /* --- 1. Чистим таблицу ------------------------------------------- */
    query.prepare("DELETE FROM grid_cells WHERE template_id = :templateId");
    query.bindValue(":templateId", templateId);
    if (!query.exec()) {
        qDebug() << "Ошибка удаления старых ячеек:" << query.lastError();
        return false;
    }

    int currentRow = 1;                    // нумерация в БД

    /* --- 2. Если переданы headers – работаем по старой логике --------- */
    if (headers) {
        /* … существующий код вставки headers … */
        /* currentRow увеличивается до 1‑й строки контента */
    }

    /* --- 3. Вставляем cellData --------------------------------------- */
    if (cellData) {
        const int numRows = cellData->size();
        for (int i = 0; i < numRows; ++i) {

            const QVector<QString> &rowData = (*cellData)[i];
            const bool isHeaderRow = headers
                                         ? false                       // headers уже вставлены
                                         : (i < headerRowsOld);        // ← наше нововведение

            for (int col = 0; col < rowData.size(); ++col) {
                query.prepare(R"(
                    INSERT INTO grid_cells (template_id, cell_type,
                                            row_index,   col_index,
                                            content,     colour)
                    VALUES (:tid, :ctype, :row, :col, :content, :colour)
                )");
                query.bindValue(":tid",   templateId);
                query.bindValue(":ctype", isHeaderRow ? "header" : "content");
                query.bindValue(":row",   currentRow + i);
                query.bindValue(":col",   col + 1);
                query.bindValue(":content", rowData[col]);

                QString clr = "#FFFFFF";
                if (cellColours && i < cellColours->size() && col < (*cellColours)[i].size())
                    clr = (*cellColours)[i][col];
                query.bindValue(":colour", clr);

                if (!query.exec()) {
                    qDebug() << "Ошибка вставки ячейки (row:"
                             << currentRow + i << ", col:" << col + 1 << "):"
                             << query.lastError();
                    return false;
                }
            }
        }
    }
    return true;
}

bool TableManager::generateColumnsForDynamicTemplate(int templateId, const QVector<QString>& groupNames) {

    int numGroups = groupNames.size();

    // Проверка: должно быть не менее одной группы
    if (numGroups < 1) {
        qDebug() << "Число групп не может быть меньше 1.";
        return false;
    }

    // Начинаем транзакцию
    if (!db.transaction()) {
        qDebug() << "Не удалось начать транзакцию:" << db.lastError();
        return false;
    }

    // STEP 1: Получаем все заголовочные столбцы для данного шаблона
    QSqlQuery selectHeaders(db);
    selectHeaders.prepare(R"(
        SELECT col_index, content
        FROM grid_cells
        WHERE template_id = :tid AND cell_type = 'header'
        ORDER BY col_index
    )");
    selectHeaders.bindValue(":tid", templateId);
    if (!selectHeaders.exec()) {
        qDebug() << "Ошибка получения столбцов:" << selectHeaders.lastError();
        db.rollback();
        return false;
    }

    int group1Order = -1;
    QString group1Header;
    QVector<int> otherDynamicOrders;    // Динамические столбцы, кроме первой группы
    QVector<QPair<int, QString>> headerColumns;  // Сохраняем (col_index, content) для всех заголовков
    while (selectHeaders.next()) {
        int colIndex = selectHeaders.value(0).toInt();
        QString header = selectHeaders.value(1).toString();
        headerColumns.append({colIndex, header});

        // Определяем динамические столбцы по названию, начинающемуся с "Группа "
        if (header.startsWith("Группа ")) {
            if (group1Order < 0) {
                group1Order = colIndex;
                group1Header = header;
            } else {
                otherDynamicOrders.append(colIndex);
            }
        }
    }

    if (group1Order < 0) {
        qDebug() << "Не найдена «Группа 1». Прерываем.";
        db.rollback();
        return false;
    }

    // STEP 2: Переименовываем заголовок группы 1
    QSqlQuery renameG1(db);
    renameG1.prepare(R"(
        UPDATE grid_cells
        SET content = :newHeader
        WHERE template_id = :tid AND cell_type = 'header' AND col_index = :group1Order
    )");
    renameG1.bindValue(":newHeader", groupNames[0]);  // Новое название для Группы 1
    renameG1.bindValue(":tid", templateId);
    renameG1.bindValue(":group1Order", group1Order);
    if (!renameG1.exec()) {
        qDebug() << "Ошибка переименования «Группы 1»:" << renameG1.lastError();
        db.rollback();
        return false;
    }

    // STEP 3: Удаляем все динамические заголовочные столбцы, кроме Группы 1
    for (int dynCol : otherDynamicOrders) {
        QSqlQuery delHeader(db);
        delHeader.prepare("DELETE FROM grid_cells WHERE template_id = :tid AND col_index = :col");
        delHeader.bindValue(":tid", templateId);
        delHeader.bindValue(":col", dynCol);
        if (!delHeader.exec()) {
            qDebug() << "Ошибка удаления динамического заголовочного столбца" << dynCol << ":" << delHeader.lastError();
            db.rollback();
            return false;
        }
    }

    // STEP 4: Сдвигаем все заголовочные столбцы, которые идут после Группы 1, на (numGroups - 2)
    int shift = numGroups - 2;
    if (shift > 0) {
        QSqlQuery selectShift(db);
        selectShift.prepare(R"(
            SELECT col_index
            FROM grid_cells
            WHERE template_id = :tid AND col_index > :group1Order
            ORDER BY col_index DESC
        )");
        selectShift.bindValue(":tid", templateId);
        selectShift.bindValue(":group1Order", group1Order);
        if (!selectShift.exec()) {
            qDebug() << "Ошибка выбора столбцов для сдвига:" << selectShift.lastError();
            db.rollback();
            return false;
        }
        QVector<int> headerShiftOrders;
        while (selectShift.next()) {
            headerShiftOrders.append(selectShift.value(0).toInt());
        }
        for (int oldOrder : headerShiftOrders) {
            int newOrder = oldOrder + shift;
            QSqlQuery updateShift(db);
            updateShift.prepare(R"(
                UPDATE grid_cells
                SET col_index = :newOrder
                WHERE template_id = :tid AND col_index = :oldOrder
            )");
            updateShift.bindValue(":newOrder", newOrder);
            updateShift.bindValue(":tid", templateId);
            updateShift.bindValue(":oldOrder", oldOrder);
            if (!updateShift.exec()) {
                qDebug() << "Ошибка сдвига заголовочного столбца:" << updateShift.lastError();
                db.rollback();
                return false;
            }
        }
    }

    // STEP 5: Читаем данные содержимого для Группы 1
    QMap<int, QString> group1Data;  // Ключ: row_index, Значение: content
    {
        QSqlQuery readGroup1(db);
        readGroup1.prepare(R"(
            SELECT row_index, content
            FROM grid_cells
            WHERE template_id = :tid AND cell_type = 'content' AND col_index = :group1Order
            ORDER BY row_index
        )");
        readGroup1.bindValue(":tid", templateId);
        readGroup1.bindValue(":group1Order", group1Order);
        if (!readGroup1.exec()) {
            qDebug() << "Ошибка чтения ячеек группы 1:" << readGroup1.lastError();
            db.rollback();
            return false;
        }
        while (readGroup1.next()) {
            int rowIndex = readGroup1.value(0).toInt();
            QString content = readGroup1.value(1).toString();
            group1Data[rowIndex] = content;
        }
    }

    // STEP 6: Создаем новые динамические группы (группы 2 .. numGroups).
    // Группа 1 остается на месте, а для каждой новой группы вставляем новый заголовочный столбец и копируем содержимое из группы 1.
    for (int i = 2; i <= numGroups; i++) {
        int newColOrder = group1Order + (i - 1); // Расчет нового номера столбца
        QString newHeader = groupNames[i - 1];    // Для группы 2 индекс 1, и так далее

        // Вставляем новую заголовочную ячейку
        QSqlQuery insHeader(db);
        insHeader.prepare(R"(
            INSERT INTO grid_cells (template_id, cell_type, row_index, col_index, content)
            VALUES (:tid, 'header', 1, :colOrder, :hdr)
        )");
        insHeader.bindValue(":tid", templateId);
        insHeader.bindValue(":colOrder", newColOrder);
        insHeader.bindValue(":hdr", newHeader);
        if (!insHeader.exec()) {
            qDebug() << "Ошибка вставки новой группы (заголовок) для группы" << i << ":" << insHeader.lastError();
            db.rollback();
            return false;
        }

        // Копируем данные содержимого из Группы 1 для каждого ряда
        for (auto it = group1Data.constBegin(); it != group1Data.constEnd(); ++it) {
            int rowIndex = it.key();
            QString content = it.value();
            QSqlQuery insContent(db);
            insContent.prepare(R"(
                INSERT INTO grid_cells (template_id, cell_type, row_index, col_index, content)
                VALUES (:tid, 'content', :rowIndex, :colOrder, :content)
            )");
            insContent.bindValue(":tid", templateId);
            insContent.bindValue(":rowIndex", rowIndex);
            insContent.bindValue(":colOrder", newColOrder);
            insContent.bindValue(":content", content);
            if (!insContent.exec()) {
                qDebug() << "Ошибка копирования ячеек для группы" << i << ":" << insContent.lastError();
                db.rollback();
                return false;
            }
        }
    }

    // Завершаем транзакцию
    if (!db.commit()) {
        qDebug() << "Ошибка коммита:" << db.lastError();
        return false;
    }

    qDebug() << "Динамические столбцы обновлены, все столбцы переупорядочены.";
    return true;
}


// НИЖЕ ИСПРАВИТЬ
bool TableManager::mergeCells(int templateId, const QString &cellType,
                              int startRow, int startCol,
                              int rowSpan, int colSpan) {
    // 1. Обновляем главную ячейку (startRow, startCol):
    QSqlQuery q(db);
    q.prepare(R"(
       UPDATE grid_cells
       SET row_span = :rs, col_span = :cs
       WHERE template_id=:tid
         AND cell_type=:ctype
         AND row_index=:r
         AND col_index=:c
    )");
    q.bindValue(":rs", rowSpan);
    q.bindValue(":cs", colSpan);
    q.bindValue(":tid", templateId);
    q.bindValue(":ctype", cellType);
    q.bindValue(":r", startRow);
    q.bindValue(":c", startCol);
    if(!q.exec()){
        qDebug() << "mergeCells() error upd main cell:" << q.lastError();
        return false;
    }

    // 2. Удаляем (или обнуляем) остальные ячейки внутри этого прямоугольника
    QSqlQuery delQ(db);
    delQ.prepare(R"(
       DELETE FROM grid_cells
       WHERE template_id=:tid
         AND cell_type=:ctype
         AND (row_index BETWEEN :r1 AND :r2)
         AND (col_index BETWEEN :c1 AND :c2)
         AND NOT(row_index=:r AND col_index=:c)
    )");
    delQ.bindValue(":tid", templateId);
    delQ.bindValue(":ctype", cellType);
    delQ.bindValue(":r1", startRow);
    delQ.bindValue(":r2", startRow + rowSpan - 1);
    delQ.bindValue(":c1", startCol);
    delQ.bindValue(":c2", startCol + colSpan - 1);
    delQ.bindValue(":r",  startRow);
    delQ.bindValue(":c",  startCol);
    if(!delQ.exec()){
        qDebug() << "mergeCells() error del others:" << delQ.lastError();
        return false;
    }

    return true;
}

bool TableManager::unmergeCells(int templateId, const QString &cellType,
                                int row, int col) {
    // 1. Узнаём, какой сейчас row_span,col_span:
    QSqlQuery q(db);
    q.prepare(R"(
       SELECT row_span,col_span,content FROM grid_cells
       WHERE template_id=:tid AND cell_type=:ctype
         AND row_index=:r AND col_index=:c
    )");
    q.bindValue(":tid", templateId);
    q.bindValue(":ctype", cellType);
    q.bindValue(":r", row);
    q.bindValue(":c", col);
    if(!q.exec() || !q.next()){
        qDebug() << "unmergeCells() can't find main cell" << q.lastError();
        return false;
    }
    int rs = q.value(0).toInt();
    int cs = q.value(1).toInt();
    QString content = q.value(2).toString();

    if(rs<=1 && cs<=1){
        // нечего разъединять
        return true;
    }

    // 2. Сбрасываем главную ячейку на (row_span=1, col_span=1)
    QSqlQuery upd(db);
    upd.prepare(R"(
       UPDATE grid_cells
       SET row_span=1, col_span=1
       WHERE template_id=:tid AND cell_type=:ctype
         AND row_index=:r AND col_index=:c
    )");
    upd.bindValue(":tid", templateId);
    upd.bindValue(":ctype", cellType);
    upd.bindValue(":r", row);
    upd.bindValue(":c", col);
    if(!upd.exec()){
        qDebug() << "unmergeCells() error upd main cell" << upd.lastError();
        return false;
    }

    // 3. Восстанавливаем все ячейки внутри (row..row+rs-1 x col..col+cs-1)
    // (кроме самой (row,col), которая уже есть)
    for(int rr=row; rr<row+rs; rr++){
        for(int cc=col; cc<col+cs; cc++){
            if(rr==row && cc==col) continue; // пропускаем главную
            // Вставляем строку
            QSqlQuery ins(db);
            ins.prepare(R"(
               INSERT INTO grid_cells(template_id, cell_type, row_index, col_index, row_span, col_span, content)
               VALUES (:tid,:ctype,:r,:c,1,1,'')
            )");
            ins.bindValue(":tid", templateId);
            ins.bindValue(":ctype", cellType);
            ins.bindValue(":r", rr);
            ins.bindValue(":c", cc);
            if(!ins.exec()){
                qDebug() << "unmergeCells() error insert sub" << ins.lastError();
                // не выходим; можно return false
            }
        }
    }

    return true;
}
