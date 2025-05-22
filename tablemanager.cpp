#include "tablemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>

TableManager::TableManager(QSqlDatabase &db)
    : db(db) {}

TableManager::~TableManager() {}

bool TableManager::addRow(int templateId, bool addToHeader, const QString &headerContent) {
    QSqlQuery q(db);

    // Если шаблон пустой – создаём единственную ячейку (1,1) header
    q.prepare(QLatin1String("SELECT COUNT(*) FROM grid_cells WHERE template_id = :tid"));
    q.bindValue(":tid", templateId);
    if (!q.exec() || !q.next())
        return false;

    if (q.value(0).toInt() == 0) {
        // первая ячейка
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO grid_cells (template_id, cell_type,
                                    row_index, col_index, content)
            VALUES (:tid, 'header', 1, 1, :content)
        )");
        ins.bindValue(":tid",     templateId);
        ins.bindValue(":content", headerContent);
        if (!ins.exec())
            return false;

        return true;
    }

    // Шаблон не пустой
    int newRow = 0;
    if (addToHeader) {                      // вставляем в заголовок
        q.prepare(R"(
            SELECT COALESCE(MAX(row_index),0)
            FROM grid_cells
            WHERE template_id = :tid AND cell_type = 'header'
        )");
        q.bindValue(":tid", templateId);
        if (!q.exec() || !q.next())
            return false;

        newRow = q.value(0).toInt() + 1;

        // Сдвигаем строки «снизу‑вверх»
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

    } else { // вставляем в тело таблицы
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

        // Если это первая строка контента
        if (newRow == 1) {
            newRow = getRowCountForHeader(templateId) + 1;
        }
        // Сдвиг не требуется – строка добавляется «в самый низ»
    }

    // Определяем столбцы, в которых нужно создать ячейки
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

    // Вставляем ячейки новой строки
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
                                  ? headerContent : QString());
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

    // Пустая таблица
    q.prepare("SELECT COUNT(*) FROM grid_cells WHERE template_id = :tid");
    q.bindValue(":tid", templateId);
    if (!q.exec() || !q.next())
        return false;

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

    // Определяем новый номер столбца
    q.prepare("SELECT MAX(col_index) FROM grid_cells WHERE template_id = :tid");
    q.bindValue(":tid", templateId);
    if (!q.exec() || !q.next())
        return false;

    const int newCol      = q.value(0).toInt() + 1;
    const int refCol      = newCol - 1;               // «сосед» слева

    // Читаем все строки и их тип в колонке‑образце
    QMap<int, QString> rowType;      // row_index → "header"/"content"
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
    if (!refQ.exec())
        return false;

    while (refQ.next())
        rowType[refQ.value(0).toInt()] = refQ.value(1).toString();

    // Создаём ячейки нового столбца в соответствии с образцом
    for (auto it = rowType.constBegin(); it != rowType.constEnd(); ++it) {
        const int      row  = it.key();
        const QString &type = it.value();  // "header"/"content"

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
        ins.bindValue(":cnt", firstHeader ? headerContent : QString());
        if (!ins.exec())
            return false;
    }

    return true;
}

bool TableManager::deleteRow(int templateId, int row) {
    QSqlQuery q(db);

    // Удаляем ячейки строки любого типа
    q.prepare(R"(
        DELETE FROM grid_cells
        WHERE template_id = :tid
          AND row_index   = :row
    )");
    q.bindValue(":tid", templateId);
    q.bindValue(":row", row);
    if (!q.exec())
        return false;


    // безопасно сдвигаем строки на −1
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

    if (!list.exec())
        return false;


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

    // Удаляем ячейки столбца (и header, и content)
    q.prepare(R"(
        DELETE FROM grid_cells
        WHERE template_id = :tid
          AND col_index   = :col
    )");
    q.bindValue(":tid", templateId);
    q.bindValue(":col", col);
    if (!q.exec())
        return false;


    // Сдвигаем остальные столбцы
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

    if (!list.exec())
        return false;


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
                                         const std::optional<QVector<QString>>& headers,
                                         const std::optional<QVector<QVector<QString>>>& cellData,
                                         const std::optional<QVector<QVector<QString>>>& cellColours) {
    if (!db.transaction()) {
        qDebug() << "saveDataTableTemplate(): cannot start tx" << db.lastError();
        return false;
    }

    // сохраняем span-ы существующей таблицы
    QMap<QPair<int,int>, QPair<int,int>> spanMap;
    QSet<QPair<int,int>>                  inner;
    {
        QSqlQuery q(db);
        q.prepare(u8R"(
            SELECT row_index, col_index,
                   COALESCE(row_span,1), COALESCE(col_span,1)
            FROM   grid_cells
            WHERE  template_id = :tid)");
        q.bindValue(":tid", templateId);
        if (!q.exec()) { db.rollback(); return false; }

        while (q.next()) {
            int r = q.value(0).toInt(), c = q.value(1).toInt();
            int rs = q.value(2).toInt(), cs = q.value(3).toInt();
            spanMap[{r,c}] = {rs,cs};
            for (int dr = 0; dr < rs; ++dr)
                for (int dc = 0; dc < cs; ++dc)
                    if (dr || dc) inner.insert({r+dr,c+dc});
        }
    }

    // полностью чистим старые ячейки
    {
        QSqlQuery del(db);
        del.prepare(u8"DELETE FROM grid_cells WHERE template_id = :tid");
        del.bindValue(":tid", templateId);
        if (!del.exec()) { db.rollback(); return false; }
    }

    if (!cellData) { db.commit(); return true; }    // нечего вставлять

    // Готовим INSERT
    QSqlQuery ins(db);
    ins.prepare(u8R"(
        INSERT INTO grid_cells(template_id, cell_type,
                               row_index , col_index ,
                               content   , colour   ,
                               row_span  , col_span )
        VALUES(:tid,:ctype,:r,:c,:cnt,:clr,:rs,:cs))");

    int headerRows = headers ? headers->size() : 0;
    const auto &tbl = *cellData;

    // Заполняем ячейки
    for (int r = 0; r < tbl.size(); ++r) {
        const bool isHeader = (r < headerRows);
        const QString ctype = isHeader ? "header" : "content";
        for (int c = 0; c < tbl[r].size(); ++c) {

            if (inner.contains({r+1,c+1})) continue;      // «внутренняя»

            auto span = spanMap.value({r+1,c+1}, {1,1});
            QString colour = "#FFFFFF";
            if (cellColours && r < cellColours->size()
                && c < (*cellColours)[r].size())
                colour = (*cellColours)[r][c];

            ins.bindValue(":tid"  , templateId);
            ins.bindValue(":ctype", ctype);
            ins.bindValue(":r"    , r+1);
            ins.bindValue(":c"    , c+1);
            ins.bindValue(":cnt"  , tbl[r][c]);
            ins.bindValue(":clr"  , colour);
            ins.bindValue(":rs"   , span.first);
            ins.bindValue(":cs"   , span.second);

            if (!ins.exec()) {
                qDebug() << "insert failed at" << r+1 << c+1 << ins.lastError();
                db.rollback();
                return false;
            }
        }
    }

    // завершаем транзакцию
    if (!db.commit()) {
        qDebug() << "saveDataTableTemplate(): commit fail" << db.lastError();
        db.rollback();
        return false;
    }
    return true;
}

bool TableManager::generateColumnsForDynamicTemplate(int templateId, const QVector<QString>& groupNames) {
    int numGroups = groupNames.size();
    if (numGroups < 1) {
        qDebug() << "Число групп не может быть меньше 1.";
        return false;
    }
    if (!db.transaction()) {
        qDebug() << "Не удалось начать транзакцию:" << db.lastError();
        return false;
    }

    // Лямбда: удаляем HTML-теги, заменяем &nbsp;, берём последнюю непустую строку, сжимаем пробелы
    auto extractHeaderName = [](const QString& html) -> QString {
        QString plain = html;
        plain.remove(QRegularExpression("<[^>]*>"));      // убрать теги
        plain.replace(QChar(0x00A0), ' ');                // NBSP → обычный пробел
        QStringList lines = plain.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        if (lines.isEmpty())
            return QString();
        QString last = lines.last().trimmed();
        return last.simplified();
    };

    // STEP 1: выбираем все header-ячейки и находим колонки, начинающиеся с "group"
    QSqlQuery selectHeaders(db);
    selectHeaders.prepare(R"(
        SELECT col_index, content
        FROM grid_cells
        WHERE template_id = :tid
          AND cell_type = 'header'
        ORDER BY col_index
    )");
    selectHeaders.bindValue(":tid", templateId);
    if (!selectHeaders.exec()) {
        qDebug() << "Ошибка получения столбцов:" << selectHeaders.lastError();
        db.rollback();
        return false;
    }

    int group1Order = -1;
    QVector<int> otherDynamicOrders;
    while (selectHeaders.next()) {
        int colIndex = selectHeaders.value(0).toInt();
        QString hdrHtml = selectHeaders.value(1).toString();
        QString hdrName = extractHeaderName(hdrHtml);

        qDebug() << "col" << colIndex << "-> hdrName:" << hdrName;

        // Надёжно ловим "group" и "Group", "group1" и т.п.
        if (hdrName.toLower().startsWith("group")) {
            if (group1Order < 0) {
                group1Order = colIndex;
            } else {
                otherDynamicOrders.append(colIndex);
            }
        }
    }

    if (group1Order < 0) {
        qDebug() << "Не найдена ни одна колонка с префиксом 'group'.";
        db.rollback();
        return false;
    }

    // STEP 2: переименовать первую "group" в groupNames[0]
    {
        QSqlQuery q(db);
        q.prepare(R"(
            UPDATE grid_cells
            SET content = :newHeader
            WHERE template_id = :tid
              AND cell_type = 'header'
              AND col_index = :col
        )");
        q.bindValue(":newHeader", groupNames[0]);
        q.bindValue(":tid", templateId);
        q.bindValue(":col", group1Order);
        if (!q.exec()) {
            qDebug() << "Ошибка при переименовании первой группы:" << q.lastError();
            db.rollback();
            return false;
        }
    }

    // STEP 3: удалить все остальные динамические "group"-колонки
    for (int col : otherDynamicOrders) {
        QSqlQuery q(db);
        q.prepare(R"(
            DELETE FROM grid_cells
            WHERE template_id = :tid
              AND col_index = :col
        )");
        q.bindValue(":tid", templateId);
        q.bindValue(":col", col);
        if (!q.exec()) {
            qDebug() << "Ошибка удаления колонки" << col << ":" << q.lastError();
            db.rollback();
            return false;
        }
    }

    // STEP 4: сдвинуть все колонки правее первой на (numGroups-1)
    int shift = numGroups - 1;
    if (shift > 0) {
        QSqlQuery sel(db);
        sel.prepare(R"(
            SELECT col_index
            FROM grid_cells
            WHERE template_id = :tid
              AND col_index > :base
            ORDER BY col_index DESC
        )");
        sel.bindValue(":tid", templateId);
        sel.bindValue(":base", group1Order);
        if (!sel.exec()) {
            qDebug() << "Ошибка выбора колонок для сдвига:" << sel.lastError();
            db.rollback();
            return false;
        }

        QVector<int> toShift;
        while (sel.next())
            toShift.append(sel.value(0).toInt());

        for (int oldCol : toShift) {
            QSqlQuery upd(db);
            upd.prepare(R"(
                UPDATE grid_cells
                SET col_index = :newCol
                WHERE template_id = :tid
                  AND col_index = :oldCol
            )");
            upd.bindValue(":newCol", oldCol + shift);
            upd.bindValue(":tid", templateId);
            upd.bindValue(":oldCol", oldCol);
            if (!upd.exec()) {
                qDebug() << "Ошибка сдвига колонки" << oldCol << ":" << upd.lastError();
                db.rollback();
                return false;
            }
        }
    }

    // STEP 5: читаем содержимое первой группы
    QMap<int, QString> group1Data;
    {
        QSqlQuery readG1(db);
        readG1.prepare(R"(
            SELECT row_index, content
            FROM grid_cells
            WHERE template_id = :tid
              AND cell_type = 'content'
              AND col_index = :col
            ORDER BY row_index
        )");
        readG1.bindValue(":tid", templateId);
        readG1.bindValue(":col", group1Order);
        if (!readG1.exec()) {
            qDebug() << "Ошибка чтения содержимого первой группы:" << readG1.lastError();
            db.rollback();
            return false;
        }
        while (readG1.next())
            group1Data[readG1.value(0).toInt()] = readG1.value(1).toString();
    }

    // STEP 6: вставляем новые группы 2..N и копируем данные
    for (int i = 2; i <= numGroups; ++i) {
        int newCol = group1Order + (i - 1);
        QString hdr = groupNames[i - 1];

        // вставка header
        {
            QSqlQuery insH(db);
            insH.prepare(R"(
                INSERT INTO grid_cells
                    (template_id, cell_type, row_index, col_index, content)
                VALUES
                    (:tid, 'header', 1, :col, :hdr)
            )");
            insH.bindValue(":tid", templateId);
            insH.bindValue(":col", newCol);
            insH.bindValue(":hdr", hdr);
            if (!insH.exec()) {
                qDebug() << "Ошибка вставки header группы" << i << ":" << insH.lastError();
                db.rollback();
                return false;
            }
        }

        // вставка content
        for (auto it = group1Data.constBegin(); it != group1Data.constEnd(); ++it) {
            QSqlQuery insC(db);
            insC.prepare(R"(
                INSERT INTO grid_cells
                    (template_id, cell_type, row_index, col_index, content)
                VALUES
                    (:tid, 'content', :row, :col, :cont)
            )");
            insC.bindValue(":tid", templateId);
            insC.bindValue(":row", it.key());
            insC.bindValue(":col", newCol);
            insC.bindValue(":cont", it.value());
            if (!insC.exec()) {
                qDebug() << "Ошибка вставки content группы" << i << ":" << insC.lastError();
                db.rollback();
                return false;
            }
        }
    }

    // коммитим транзакцию
    if (!db.commit()) {
        qDebug() << "Ошибка коммита:" << db.lastError();
        return false;
    }

    qDebug() << "Динамические столбцы обновлены.";
    return true;
}


bool TableManager::mergeCells(int templateId, const QString &cellType,
                              int startRow, int startCol,
                              int rowSpan, int colSpan) {
    //  Обновляем главную ячейку (startRow, startCol):
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

    // Помечаем все остальные ячейки «теневыми»
    QSqlQuery upd(db);
    upd.prepare(R"(
       UPDATE grid_cells
       SET row_span = 0, col_span = 0
       WHERE template_id = :tid
         AND cell_type   = :ctype
         AND (row_index BETWEEN :r1 AND :r2)
         AND (col_index BETWEEN :c1 AND :c2)
         AND NOT (row_index = :r AND col_index = :c)
    )");
    upd.bindValue(":tid",   templateId);
    upd.bindValue(":ctype", cellType);
    upd.bindValue(":r1",    startRow);
    upd.bindValue(":r2",    startRow + rowSpan  - 1);
    upd.bindValue(":c1",    startCol);
    upd.bindValue(":c2",    startCol + colSpan  - 1);
    upd.bindValue(":r",     startRow);
    upd.bindValue(":c",     startCol);
    if (!upd.exec()) {
        qDebug() << "mergeCells() error marking inner cells:" << upd.lastError();
        return false;
    }

    return true;
}

bool TableManager::unmergeCells(int templateId, const QString &cellType, int rowIndex1, int colIndex1) {
    //  Считываем текущий span основной ячейки
    QSqlQuery sel(db);
    sel.prepare(R"(
        SELECT COALESCE(row_span,1) AS rs,
               COALESCE(col_span,1) AS cs
        FROM   grid_cells
        WHERE  template_id = :tid
          AND  cell_type   = :ctype
          AND  row_index   = :r
          AND  col_index   = :c
    )");
    sel.bindValue(":tid",   templateId);
    sel.bindValue(":ctype", cellType);
    sel.bindValue(":r",     rowIndex1);
    sel.bindValue(":c",     colIndex1);
    if (!sel.exec() || !sel.next()) {
        qDebug() << "unmergeCells(): main cell not found or query failed:" << sel.lastError();
        return false;
    }
    int rs = qMax(1, sel.value("rs").toInt());
    int cs = qMax(1, sel.value("cs").toInt());

    // если спанов нет — ничего не делаем
    if (rs == 1 && cs == 1)
        return true;

    //  Сбрасываем span у главной ячейки
    QSqlQuery updMain(db);
    updMain.prepare(R"(
        UPDATE grid_cells
        SET   row_span = 1,
              col_span = 1
        WHERE template_id = :tid
          AND cell_type   = :ctype
          AND row_index   = :r
          AND col_index   = :c
    )");
    updMain.bindValue(":tid",   templateId);
    updMain.bindValue(":ctype", cellType);
    updMain.bindValue(":r",     rowIndex1);
    updMain.bindValue(":c",     colIndex1);
    if (!updMain.exec()) {
        qDebug() << "unmergeCells(): failed to reset span on main cell:" << updMain.lastError();
        return false;
    }

    //  Восстанавливаем «внутренние» ячейки — просто обновляем их span
    QSqlQuery updInner(db);
    updInner.prepare(R"(
        UPDATE grid_cells
        SET   row_span = 1,
              col_span = 1
        WHERE template_id = :tid
          AND cell_type   = :ctype
          AND row_index BETWEEN :r1 AND :r2
          AND col_index BETWEEN :c1 AND :c2
    )");
    updInner.bindValue(":tid",   templateId);
    updInner.bindValue(":ctype", cellType);
    updInner.bindValue(":r1",    rowIndex1);
    updInner.bindValue(":r2",    rowIndex1 + rs - 1);
    updInner.bindValue(":c1",    colIndex1);
    updInner.bindValue(":c2",    colIndex1 + cs - 1);
    if (!updInner.exec()) {
        qDebug() << "unmergeCells(): failed to restore inner cells:" << updInner.lastError();
        return false;
    }

    return true;
}

bool TableManager::cellExists(int templateId, const QString &cellType,
                              int rowIndex, int colIndex) const {
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT 1
        FROM   grid_cells
        WHERE  template_id = :tid
          AND  cell_type   = :ctype
          AND  row_index   = :r
          AND  col_index   = :c
        LIMIT  1
    )");
    q.bindValue(":tid",    templateId);
    q.bindValue(":ctype",  cellType);
    q.bindValue(":r",      rowIndex);
    q.bindValue(":c",      colIndex);
    if (!q.exec()) {
        qDebug() << "cellExists(): exec failed:" << q.lastError();
        return false;
    }
    return q.next();
}

bool TableManager::insertRow(int templateId, int beforeRow, bool addToHeader, const QString &headerContent) {
    QSqlQuery shift(db);
    // 1) Сдвигаем все строки с row_index >= beforeRow вниз
    shift.prepare(R"(
        SELECT DISTINCT row_index
        FROM grid_cells
        WHERE template_id = :tid
          AND row_index >= :pos
        ORDER BY row_index DESC
    )");
    shift.bindValue(":tid", templateId);
    shift.bindValue(":pos", beforeRow);
    if (!shift.exec()) return false;

    while (shift.next()) {
        int oldRow = shift.value(0).toInt();
        QSqlQuery upd(db);
        upd.prepare(R"(
            UPDATE grid_cells
               SET row_index = :newIdx
             WHERE template_id = :tid
               AND row_index   = :oldRow
        )");
        upd.bindValue(":newIdx", oldRow + 1);
        upd.bindValue(":tid",    templateId);
        upd.bindValue(":oldRow", oldRow);
        if (!upd.exec()) return false;
    }

    // 2) Собираем все существующие колонки
    QVector<int> cols;
    QSqlQuery colQ(db);
    colQ.prepare(R"(
        SELECT DISTINCT col_index
        FROM grid_cells
        WHERE template_id = :tid
        ORDER BY col_index
    )");
    colQ.bindValue(":tid", templateId);
    if (!colQ.exec()) return false;
    while (colQ.next()) cols << colQ.value(0).toInt();

    // 3) Вставляем новую строку
    for (int col : cols) {
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO grid_cells(template_id, cell_type,
                                   row_index, col_index, content)
            VALUES(:tid, :ctype, :row, :col, :cnt)
        )");
        ins.bindValue(":tid", templateId);
        ins.bindValue(":ctype", addToHeader ? "header" : "content");
        ins.bindValue(":row", beforeRow);
        ins.bindValue(":col", col);
        // Для header-строки только в первом столбце вставляем текст
        QString cnt = (addToHeader && col == cols.first()) ? headerContent : QString();
        ins.bindValue(":cnt", cnt);
        if (!ins.exec()) return false;
    }
    return true;
}

bool TableManager::insertColumn(int templateId, int beforeCol, const QString &headerContent) {
    QSqlQuery shift(db);
    // 1) Сдвигаем все колонки с col_index >= beforeCol вправо
    shift.prepare(R"(
        SELECT DISTINCT col_index
        FROM grid_cells
        WHERE template_id = :tid
          AND col_index >= :pos
        ORDER BY col_index DESC
    )");
    shift.bindValue(":tid", templateId);
    shift.bindValue(":pos", beforeCol);
    if (!shift.exec()) return false;

    while (shift.next()) {
        int oldCol = shift.value(0).toInt();
        QSqlQuery upd(db);
        upd.prepare(R"(
            UPDATE grid_cells
               SET col_index = :newIdx
             WHERE template_id = :tid
               AND col_index   = :oldCol
        )");
        upd.bindValue(":newIdx", oldCol + 1);
        upd.bindValue(":tid",    templateId);
        upd.bindValue(":oldCol", oldCol);
        if (!upd.exec()) return false;
    }

    // 2) Собираем все строки, чтобы вставить в каждую новую ячейку
    QMap<int, QString> rowType; // row_index → cell_type
    QSqlQuery refQ(db);
    refQ.prepare(R"(
        SELECT row_index, cell_type
        FROM grid_cells
        WHERE template_id = :tid
        GROUP BY row_index, cell_type
        ORDER BY row_index
    )");
    refQ.bindValue(":tid", templateId);
    if (!refQ.exec()) return false;
    while (refQ.next())
        rowType[refQ.value(0).toInt()] = refQ.value(1).toString();

    // 3) Вставляем новую колонку
    for (auto it = rowType.constBegin(); it != rowType.constEnd(); ++it) {
        int row = it.key();
        QString type = it.value();
        QSqlQuery ins(db);
        ins.prepare(R"(
            INSERT INTO grid_cells(template_id, cell_type,
                                   row_index, col_index, content)
            VALUES(:tid, :ctype, :row, :col, :cnt)
        )");
        ins.bindValue(":tid",    templateId);
        ins.bindValue(":ctype",  type);
        ins.bindValue(":row",    row);
        ins.bindValue(":col",    beforeCol);
        // Если это первая header-ячейка, ставим текст
        bool firstHdr = (type == "header" && row == rowType.firstKey());
        ins.bindValue(":cnt", firstHdr ? headerContent : QString());
        if (!ins.exec()) return false;
    }
    return true;
}
