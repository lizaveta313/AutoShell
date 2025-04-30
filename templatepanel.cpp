#include "templatepanel.h"
#include "richtextdelegate.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QColor>
#include <QLabel>
#include <QEvent>
#include <QApplication>
#include <QMouseEvent>
#include <QStackedLayout>
#include <QMenu>
#include <QtMath>


TemplatePanel::TemplatePanel(DatabaseHandler *dbHandler, FormatToolBar *formatToolBar, QWidget *parent)
    : QWidget(parent)
    , dbHandler(dbHandler)
    , formatToolBar(formatToolBar) {
    setupUI();
}

TemplatePanel::~TemplatePanel() {}

void TemplatePanel::setupUI() {

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);

    // --- Верхняя часть: контейнер вида (таблица или график) ---
    QSplitter *verticalSplitter = new QSplitter(Qt::Vertical, this);
    viewStack = new QStackedWidget(this);

    // Вид для таблицы
    templateTableWidget = new QTableWidget(viewStack);
    templateTableWidget->setItemDelegate(new RichTextDelegate(this));
    templateTableWidget->installEventFilter(this);
    templateTableWidget->setWordWrap(true);
    templateTableWidget->setFocusPolicy(Qt::StrongFocus);

    templateTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    templateTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    templateTableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);

    templateTableWidget->horizontalHeader()->hide();
    templateTableWidget->verticalHeader()->hide();

    templateTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(templateTableWidget, &QTableWidget::customContextMenuRequested,
            this, &TemplatePanel::onTableContextMenu);

    connect(templateTableWidget, &QTableWidget::cellClicked, this, [this](int row, int column) {
        QTableWidgetItem *item = templateTableWidget->item(row, column);
        if (item) {
            templateTableWidget->openPersistentEditor(item);
            templateTableWidget->editItem(item);
        }
    });
    connect(templateTableWidget->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &TemplatePanel::onCurrentChanged);

    // Вид для графика – QLabel
    graphLabel = new QLabel(tr("Здесь будет график"), viewStack);
    graphLabel->setAlignment(Qt::AlignCenter);
    graphLabel->setScaledContents(true);
    viewStack->addWidget(templateTableWidget); // индекс 0 – таблица
    viewStack->addWidget(graphLabel);            // индекс 1 – график

    // --- Нижняя часть: панель с кнопками и заметками ---
    QWidget *bottomContainer = new QWidget(this);
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomContainer);

    // Левая часть: кнопки
    QWidget *leftButtonsWidget = new QWidget(bottomContainer);
    QVBoxLayout *leftButtonsLayout = new QVBoxLayout(leftButtonsWidget);
    leftButtonsLayout->setContentsMargins(0, 0, 0, 0);
    leftButtonsLayout->setSpacing(5);

    //  Контейнер для кнопок, специфичных для таблицы
    tableButtonsWidget = new QWidget(leftButtonsWidget);
    QVBoxLayout *tableButtonsLayout = new QVBoxLayout(tableButtonsWidget);
    tableButtonsLayout->setContentsMargins(0, 0, 0, 0);
    tableButtonsLayout->setSpacing(5);
    addHeaderButton = new QPushButton(tr("Добавить заголовок"), tableButtonsWidget);
    addRowButton = new QPushButton(tr("Добавить строку"), tableButtonsWidget);
    deleteRowButton = new QPushButton(tr("Удалить строку"), tableButtonsWidget);
    addColumnButton = new QPushButton(tr("Добавить столбец"), tableButtonsWidget);
    deleteColumnButton = new QPushButton(tr("Удалить столбец"), tableButtonsWidget);
    // Можно задать фиксированную политику размеров, чтобы кнопки не растягивались
    addHeaderButton->setFixedSize(140,30);
    addRowButton->setFixedSize(140, 30);
    deleteRowButton->setFixedSize(140, 30);
    addColumnButton->setFixedSize(140, 30);
    deleteColumnButton->setFixedSize(140, 30);
    tableButtonsLayout->addWidget(addHeaderButton);
    tableButtonsLayout->addWidget(addRowButton);
    tableButtonsLayout->addWidget(deleteRowButton);
    tableButtonsLayout->addWidget(addColumnButton);
    tableButtonsLayout->addWidget(deleteColumnButton);
    tableButtonsWidget->setLayout(tableButtonsLayout);

    // Контейнер для кнопок, спецефичных для графиков
    graphButtonsWidget = new QWidget(leftButtonsWidget);
    QVBoxLayout *graphButtonsLayout = new QVBoxLayout(graphButtonsWidget);
    graphButtonsLayout->setContentsMargins(0, 0, 0, 0);
    graphButtonsLayout->setSpacing(5);
    changeGraphTypeButton = new QPushButton(tr("Изменить тип графика"), graphButtonsWidget);
    changeGraphTypeButton->setFixedSize(140, 30);
    graphButtonsLayout->addWidget(changeGraphTypeButton);
    graphButtonsWidget->setLayout(graphButtonsLayout);

    //  Контейнер для общих кнопок Save и Утвердить
    QWidget *commonButtonsWidget = new QWidget(leftButtonsWidget);
    QVBoxLayout *commonButtonsLayout = new QVBoxLayout(commonButtonsWidget);
    commonButtonsLayout->setContentsMargins(0, 0, 0, 0);
    commonButtonsLayout->setSpacing(5);
    saveButton = new QPushButton(tr("Сохранить"), commonButtonsWidget);
    checkButton = new QPushButton(tr("Утвердить"), commonButtonsWidget);
    saveButton->setFixedSize(140, 30);
    checkButton->setFixedSize(140, 30);
    commonButtonsLayout->addWidget(saveButton);
    commonButtonsLayout->addWidget(checkButton);
    commonButtonsWidget->setLayout(commonButtonsLayout);

    // Добавляем оба контейнера в левый столбец:
    leftButtonsLayout->addWidget(tableButtonsWidget);
    leftButtonsLayout->addWidget(graphButtonsWidget);
    leftButtonsLayout->addWidget(commonButtonsWidget);
    leftButtonsWidget->setLayout(leftButtonsLayout);

    // Контейнер для Подзаголовка
    QWidget *subtitleWidget = new QWidget(bottomContainer);
    QVBoxLayout *subtitleLayout = new QVBoxLayout(subtitleWidget);
    subtitleLayout->setContentsMargins(0,0,0,0);
    subtitleLayout->setSpacing(5);
    QLabel *subtitleLabel = new QLabel(tr("Подзаголовок"), subtitleWidget);
    subtitleField = new QTextEdit(subtitleWidget);
    subtitleField->setAcceptRichText(true);
    subtitleField->installEventFilter(this);
    subtitleLayout->addWidget(subtitleLabel);
    subtitleLayout->addWidget(subtitleField);
    subtitleWidget->setLayout(subtitleLayout);

    // Правая часть: панель заметок
    QWidget *notesWidget = new QWidget(bottomContainer);
    QVBoxLayout *notesLayout = new QVBoxLayout(notesWidget);
    QLabel *notesLabel = new QLabel(tr("Заметки"), notesWidget);
    notesField = new QTextEdit(notesWidget);
    notesField->setAcceptRichText(true);
    notesField->installEventFilter(this);
    QLabel *notesProgrammingLabel = new QLabel(tr("Программные заметки"), notesWidget);
    notesProgrammingField = new QTextEdit(notesWidget);
    notesProgrammingField->setAcceptRichText(true);
    notesProgrammingField->installEventFilter(this);
    notesLayout->addWidget(notesLabel);
    notesLayout->addWidget(notesField, 1);
    notesLayout->addWidget(notesProgrammingLabel);
    notesLayout->addWidget(notesProgrammingField, 1);
    notesWidget->setLayout(notesLayout);

    // Добавляем левую и правую части в нижний layout:
    bottomLayout->addWidget(leftButtonsWidget, 0);  // фиксированный размер
    bottomLayout->addWidget(subtitleWidget, 0);
    bottomLayout->addWidget(notesWidget, 1);          // растягивается
    bottomContainer->setLayout(bottomLayout);

    // --- Собираем вертикальный сплиттер ---
    verticalSplitter->addWidget(viewStack);
    verticalSplitter->addWidget(bottomContainer);
    verticalSplitter->setStretchFactor(0, 5);
    verticalSplitter->setStretchFactor(1, 1);
    mainLayout->addWidget(verticalSplitter);

    // Подключаем сигналы кнопок:
    connect(addHeaderButton, &QPushButton::clicked, this, &TemplatePanel::addHeaderRow);
    connect(addRowButton, &QPushButton::clicked, this, [this]() { addRowOrColumn("row"); });
    connect(addColumnButton, &QPushButton::clicked, this, [this]() { addRowOrColumn("column"); });
    connect(deleteRowButton, &QPushButton::clicked, this, [this]() { deleteRowOrColumn("row"); });
    connect(deleteColumnButton, &QPushButton::clicked, this, [this]() { deleteRowOrColumn("column"); });
    connect(saveButton, &QPushButton::clicked, this, &TemplatePanel::saveTableData);
    connect(checkButton, &QPushButton::clicked, this, [this]() {
        emit checkButtonPressed();
    });
    connect(changeGraphTypeButton, &QPushButton::clicked,
            this, &TemplatePanel::onChangeGraphTypeClicked);

    graphButtonsWidget->hide();
    tableButtonsWidget->hide();
}
void TemplatePanel::clearAll() {
    //  Очищаем таблицу
    templateTableWidget->clear();
    templateTableWidget->setRowCount(0);
    templateTableWidget->setColumnCount(0);

    //  Очищаем поля заметок
    subtitleField->clear();
    notesField->clear();
    notesProgrammingField->clear();

    //  Сбрасываем график
    graphLabel->clear();
    graphLabel->setText("Здесь будет график");

    //  Обнуляем идентификатор текущего шаблона
    selectedTemplateId = -1;  // или 0, смотря какую логику вы используете
}

//
void TemplatePanel::loadTableTemplate(int templateId) {

    selectedTemplateId = templateId;
    templateTableWidget->clear();
    templateTableWidget->clearSpans();

    TableMatrix cells = dbHandler->getTemplateManager()->getTableData(templateId);
    const int nR = cells.size();
    const int nC = nR ? cells[0].size() : 0;

    templateTableWidget->setRowCount(nR);
    templateTableWidget->setColumnCount(nC);

    // Получаем число строк-заголовков (т.е. максимальное значение row_index для ячеек типа header)
    int headerRows = dbHandler->getTableManager()->getRowCountForHeader(templateId);

    for (int r = 0; r < nR; ++r) {
        for (int c = 0; c < nC; ++c) {

            const Cell &cell = cells[r][c];
            if (cell.rowSpan == 0)              // «теневая» – пропускаем
                continue;

            auto *item = new QTableWidgetItem(cell.text);
            item->setBackground(QColor(cell.colour));
            if (r < headerRows)                 // серый фон заголовка
                item->setBackground(Qt::lightGray);
            templateTableWidget->setItem(r, c, item);

            int rs = cell.rowSpan, cs = cell.colSpan;
            if (rs < 1) rs = 1;
            if (cs < 1) cs = 1;
            if ((rs > 1 || cs > 1) && r + rs <= nR && c + cs <= nC) {
                templateTableWidget->setSpan(r, c, rs, cs);
            }

        }
    }

    // Загружаем подзаголовок, заметки и программные заметки
    QString subtitle = dbHandler->getTemplateManager()->getSubtitleForTemplate(templateId);
    QString notes = dbHandler->getTemplateManager()->getNotesForTemplate(templateId);
    QString programmingNotes = dbHandler->getTemplateManager()->getProgrammingNotesForTemplate(templateId);
    subtitleField->setHtml(subtitle);
    notesField->setHtml(notes);
    notesProgrammingField->setHtml(programmingNotes);

    templateTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    templateTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    templateTableWidget->resizeColumnsToContents();
    templateTableWidget->resizeRowsToContents();

    templateTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    templateTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    templateTableWidget->horizontalHeader()->show();
    templateTableWidget->verticalHeader()->show();

    templateTableWidget->horizontalHeader()->setFixedHeight(5);
    templateTableWidget->verticalHeader()->setFixedWidth(5);

    templateTableWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    qDebug() << "Шаблон таблицы с ID" << templateId << "загружен.";

}
void TemplatePanel::loadGraphTemplate(int templateId) {

    // Получаем данные графика (байтовый массив)
    QByteArray imageData = dbHandler->getTemplateManager()->getGraphImage(templateId);
    if (imageData.isEmpty()) {
        graphLabel->setText("Нет данных графика");
        return;
    }
    // Загружаем изображение в QPixmap
    QPixmap pixmap;
    if (!pixmap.loadFromData(imageData)) {
        graphLabel->setText("Ошибка загрузки изображения");
        return;
    }
    graphLabel->setPixmap(pixmap.scaled(graphLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    graphLabel->show();

    QString subtitle = dbHandler->getTemplateManager()->getSubtitleForTemplate(templateId);
    QString notes = dbHandler->getTemplateManager()->getNotesForTemplate(templateId);
    QString programmingNotes = dbHandler->getTemplateManager()->getProgrammingNotesForTemplate(templateId);
    subtitleField->setHtml(subtitle);
    notesField->setHtml(notes);
    notesProgrammingField->setHtml(programmingNotes);


    qDebug() << "График с ID" << templateId << "загружен.";
}
void TemplatePanel::loadTemplate(int templateId) {
    selectedTemplateId = templateId;
    QString type = dbHandler->getTemplateManager()->getTemplateType(templateId);
    if (type == "graph") {
        viewStack->setCurrentIndex(1);
        tableButtonsWidget->hide();
        graphButtonsWidget->show();
        loadGraphTemplate(templateId);
    } else {
        viewStack->setCurrentIndex(0);
        tableButtonsWidget->show();
        graphButtonsWidget->hide();
        loadTableTemplate(templateId);
    }
}

void TemplatePanel::addHeaderRow() {
    if (selectedTemplateId <= 0) {
        qDebug() << "Нет выбранного шаблона.";
        return;
    }
    // Если таблица пуста — просто первая строка
    if (templateTableWidget->rowCount() == 0) {
        if (!dbHandler->getTableManager()->addRow(selectedTemplateId, true, QString())) {
            qDebug() << "Ошибка добавления первой строки (заголовок).";
        }
        loadTableTemplate(selectedTemplateId);
        return;
    }

    // Добавляем новую header‑строку
    if (!dbHandler->getTableManager()->addRow(selectedTemplateId, true, QString())) {
        qDebug() << "Ошибка добавления заголовка.";
    } else {
        qDebug() << "Заголовок успешно добавлен.";
    }
    loadTableTemplate(selectedTemplateId);
}
void TemplatePanel::addRowOrColumn(const QString &type) {
    if (selectedTemplateId <= 0) {
        qDebug() << "Нет выбранного шаблона.";
        return;
    }

    if (type == "row") {
        // Если таблица вообще пуста
        if (templateTableWidget->rowCount() == 0) {
            dbHandler->getTableManager()->addRow(selectedTemplateId,
                                                 /*header=*/true, QString());
        } else {
            dbHandler->getTableManager()->addRow(selectedTemplateId,
                                                 /*header=*/false, QString());
        }
        loadTableTemplate(selectedTemplateId);

    } else if (type == "column") {
        dbHandler->getTableManager()->addColumn(selectedTemplateId, QString());
        loadTableTemplate(selectedTemplateId);
    }
}
void TemplatePanel::deleteRowOrColumn(const QString &type) {
    if (templateTableWidget->selectedItems().isEmpty()) {
        qDebug() << QString("Не выбрана %1 для удаления.").arg(type == "row" ? "строка" : "столбец");
        return;
    }
    int currentIndex = (type == "row") ? templateTableWidget->currentRow() : templateTableWidget->currentColumn();
    if (currentIndex < 0) {
        qDebug() << QString("Не выбрана %1 для удаления.").arg(type == "row" ? "строка" : "столбец");
        return;
    }
    if (selectedTemplateId <= 0) {
        qDebug() << "Нет выбранного шаблона.";
        return;
    }
    // При удалении используем 0-based индекс + 1 для БД
    bool result = false;
    if (type == "row")
        result = dbHandler->getTableManager()->deleteRow(selectedTemplateId, currentIndex + 1);
    else
        result = dbHandler->getTableManager()->deleteColumn(selectedTemplateId, currentIndex + 1);

    if (!result)
        qDebug() << "Ошибка удаления " << type;
    else
        qDebug() << "Элемент успешно удалён.";
    loadTableTemplate(selectedTemplateId);
}
void TemplatePanel::saveTableData() {
    if (viewStack->currentIndex() != 0 || selectedTemplateId <= 0)
        return;

    // Закрываем все открытые редакторы, чтобы данные попали в item-ы
    templateTableWidget->closePersistentEditor(nullptr);
    templateTableWidget->clearFocus();

    const int rows = templateTableWidget->rowCount();
    const int cols = templateTableWidget->columnCount();
    if (rows == 0 || cols == 0) return;

    // Собираем данные и цвета
    QVector<QVector<QString>>   cellData(rows);
    QVector<QVector<QString>>   cellColours(rows);
    for (int r = 0; r < rows; ++r) {
        cellData[r].resize(cols);
        cellColours[r].resize(cols);
        for (int c = 0; c < cols; ++c) {
            if (auto *it = templateTableWidget->item(r,c)) {
                cellData   [r][c] = it->text();
                cellColours[r][c] = it->background().color().name();
            } else {
                cellData   [r][c] = QString();
                cellColours[r][c] = QString("#FFFFFF");
            }
        }
    }

    // Кол-во строк-заголовков узнаём из БД
    int headerRows =
        dbHandler->getTableManager()->getRowCountForHeader(selectedTemplateId);

    QVector<QString> headers(headerRows);  // содержимое тут неважно – нужна длина
    dbHandler->getTableManager()->saveDataTableTemplate(
        selectedTemplateId,
        headerRows ? std::optional{headers} : std::nullopt,
        std::optional{cellData},
        std::optional{cellColours}
        );

    // Сохраняем заметки (как и раньше)
    dbHandler->getTemplateManager()->updateTemplate(
        selectedTemplateId,
        std::nullopt,
        subtitleField->toHtml(),
        notesField->toHtml(),
        notesProgrammingField->toHtml()
        );

}
void TemplatePanel::onChangeGraphTypeClicked() {
    if (selectedTemplateId <= 0) {
        qDebug() << "Нечего менять: нет выбранного шаблона!";
        return;
    }

    //  Спросим новый подтип
    QStringList graphTypes;
    graphTypes = dbHandler->getTemplateManager()->getGraphTypesFromLibrary();
    bool ok = false;
    QString chosenGraph = QInputDialog::getItem(
        this,
        tr("Изменить тип графика"),
        tr("Выберите новый тип:"),
        graphTypes,
        0,
        false,
        &ok
        );
    if (!ok || chosenGraph.isEmpty()) {
        qDebug() << "Пользователь отменил смену типа графика.";
        return;
    }

    bool updateOk = dbHandler->getTemplateManager()->updateGraphFromLibrary(chosenGraph, selectedTemplateId);
    if (!updateOk) {
        QMessageBox::warning(this, "Ошибка", "Не удалось обновить запись в graph.");
        return;
    }

    //  Обновляем шаблон в UI: заново загружаем граф
    loadGraphTemplate(selectedTemplateId);

    qDebug() << "Тип графика успешно обновлён на" << chosenGraph;
    QMessageBox::information(this, "Готово", tr("Тип графика изменён на: %1").arg(chosenGraph));
}


void TemplatePanel::onTableContextMenu(const QPoint &pos) {
    QMenu menu(this);

    // Проверяем выделенные ячейки
    const auto items = templateTableWidget->selectedItems();
    if (items.isEmpty())
        return;

    // Проверяем, чтобы все ячейки были либо header, либо content
    const int headerRows = dbHandler->getTableManager()->getRowCountForHeader(selectedTemplateId);
    bool allHeader = true, allContent = true;
    for (auto *item : items) {
        if (item->row() < headerRows)  allContent = false;
        else                            allHeader  = false;
    }

    // Проверяем прямоугольность выделения
    int minRow = INT_MAX, maxRow = -1, minCol = INT_MAX, maxCol = -1;
    QSet<QPair<int,int>> coords;
    for (auto *item : items) {
        int r = item->row(), c = item->column();
        coords.insert({r,c});
        minRow = qMin(minRow, r);
        maxRow = qMax(maxRow, r);
        minCol = qMin(minCol, c);
        maxCol = qMax(maxCol, c);
    }
    int rowSpan = maxRow - minRow + 1;
    int colSpan = maxCol - minCol + 1;
    bool isRect = (coords.size() == rowSpan * colSpan);

    // Определяем, можно ли сливать
    bool canMerge = (items.count() > 1)
                    && (allHeader || allContent)
                    && isRect;

    // Определяем, можно ли разъединять (только если ровно одна ячейка и у неё span >1)
    bool canUnmerge = false;
    if (items.count() == 1) {
        auto *it = items.first();
        int r = it->row(), c = it->column();
        if (templateTableWidget->rowSpan(r,c) > 1 ||
            templateTableWidget->columnSpan(r,c) > 1)
        {
            canUnmerge = true;
        }
    }

    QAction* mergeAction = nullptr;
    QAction* unmergeAction = nullptr;
    if(canMerge){
        mergeAction = menu.addAction("Слить ячейки");
    }
    if(canUnmerge){
        unmergeAction = menu.addAction("Разъединить ячейки");
    }

    QAction* chosen = menu.exec(templateTableWidget->viewport()->mapToGlobal(pos));
    if(!chosen) return;

    if(chosen == mergeAction){
        mergeSelectedCells();
    } else if(chosen == unmergeAction){
        unmergeSelectedCells();
    }
}
void TemplatePanel::mergeSelectedCells() {
    // проверяем выделение
    const QList<QTableWidgetItem*> items = templateTableWidget->selectedItems();
    if (items.size() < 2) {
        QMessageBox::information(this, tr("Слияние"),
                                 tr("Необходимо выделить минимум две ячейки."));
        return;
    }

    // строим прямоугольник выделения
    int minRow = INT_MAX, maxRow = -1;
    int minCol = INT_MAX, maxCol = -1;
    QSet<QPair<int,int>> coords;               // уникальные (row,col)

    for (auto *it : items) {
        coords.insert({it->row(), it->column()});
        minRow = qMin(minRow, it->row());
        maxRow = qMax(maxRow, it->row());
        minCol = qMin(minCol, it->column());
        maxCol = qMax(maxCol, it->column());
    }

    const int rowSpan = maxRow - minRow + 1;
    const int colSpan = maxCol - minCol + 1;
    if (coords.size() != rowSpan * colSpan) {      // «дырявый» прямоугольник
        QMessageBox::warning(this, tr("Слияние"),
                             tr("Выделите прямоугольный блок ячеек."));
        return;
    }

    // убеждаемся, что все ячейки одного типа
    const int headerRows = dbHandler->getTableManager()
                               ->getRowCountForHeader(selectedTemplateId);

    bool allHeader  = true;
    bool allContent = true;
    for (auto *it : items) {
        if (it->row() < headerRows)  allContent = false;
        else                         allHeader  = false;
    }
    if (!allHeader && !allContent) {
        QMessageBox::warning(this, tr("Слияние"),
                             tr("Нельзя объединять ячейки заголовка и содержимого одновременно."));
        return;
    }
    const QString cellType = allHeader ? "header" : "content";

    // пишем изменения в БД
    const int dbRow = minRow + 1;                 // 1‑based
    const int dbCol = minCol + 1;
    if (!dbHandler->getTableManager()->mergeCells(
            selectedTemplateId, cellType,
            dbRow, dbCol, rowSpan, colSpan))
    {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Не удалось объединить ячейки в базе данных."));
        return;
    }

    // перерисовываем таблицу
    loadTableTemplate(selectedTemplateId);
    templateTableWidget->clearSelection();
    templateTableWidget->setCurrentCell(minRow, minCol);

}
void TemplatePanel::unmergeSelectedCells() {
    QTableWidgetItem *item = templateTableWidget->currentItem();
    if (!item) return;

    const int savedRow = item->row();        // 1) сохраняем
    const int savedCol = item->column();

    const int dbRow = savedRow + 1;          // в БД счёт с 1
    const int dbCol = savedCol + 1;

    int headerRows = dbHandler->getTableManager()->getRowCountForHeader(selectedTemplateId);
    QString cellType = (dbRow <= headerRows) ? "header" : "content";

    if (!dbHandler->getTableManager()
             ->cellExists(selectedTemplateId, cellType, dbRow, dbCol))
    {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Невозможно разъединить: ячейка не найдена в БД."));
        return;
    }

    if (!dbHandler->getTableManager()->unmergeCells(
            selectedTemplateId, cellType, dbRow, dbCol))
    {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Не удалось разъединить ячейку."));
        return;
    }

    loadTableTemplate(selectedTemplateId);
    templateTableWidget->clearSelection();
    templateTableWidget->setCurrentCell(savedRow, savedCol);
}

//
void TemplatePanel::fillCellColor(const QColor &color)  {
    // Получаем текущий выбранный элемент таблицы
    QTableWidgetItem *item = templateTableWidget->currentItem();
    if (!item)
        return;

    // Устанавливаем фон всей ячейки
    item->setBackground(QBrush(color));

    // Определяем порядковые номера строки и столбца.
    int rowIndex = templateTableWidget->currentRow();
    int colIndex = templateTableWidget->currentColumn();

    if (!dbHandler->getTableManager()->updateCellColour(selectedTemplateId, rowIndex, colIndex, color.name())) {
        qDebug() << "Не удалось обновить цвет ячейки в БД";
    } else {
        qDebug() << "Цвет ячейки успешно обновлён в БД:" << color.name();
    }
}
bool TemplatePanel::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::FocusIn) {
        if (QTextEdit *ed = qobject_cast<QTextEdit*>(obj)) {
            activeTextEdit = ed;
            emit textEditFocused(ed);
        }
    } else if (event->type() == QEvent::FocusOut) {
        if (QTextEdit *ed = qobject_cast<QTextEdit*>(obj)) {
            QTextCursor cursor = ed->textCursor();
            // Если текст выделён, оставляем активный редактор
            if (cursor.hasSelection())
                return QWidget::eventFilter(obj, event);
            // Если новый фокус переходит на тулбар – тоже оставляем активный редактор
            QWidget *newFocus = QApplication::focusWidget();
            if (formatToolBar && newFocus && formatToolBar->isAncestorOf(newFocus))
                return QWidget::eventFilter(obj, event);
            // Иначе сбрасываем активный редактор
            activeTextEdit = nullptr;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        // Если активный редактор существует и пользователь кликает вне его области
        if (activeTextEdit) {
            QRect globalEditorRect(activeTextEdit->mapToGlobal(QPoint(0, 0)), activeTextEdit->size());
            //if (!globalEditorRect.contains(mouseEvent->globalPos())) {
            if (!globalEditorRect.contains(mouseEvent->globalPosition().toPoint())) {
                // Если редактор находится в ячейке таблицы, закрываем persistent editor
                if (activeTextEdit->parent()->inherits("QTableWidget")) {
                    QTableWidget *table = qobject_cast<QTableWidget*>(activeTextEdit->parent());
                    if (table) {
                        table->closePersistentEditor(table->currentItem());
                    }
                }
                activeTextEdit->clearFocus();
                activeTextEdit = nullptr;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
void TemplatePanel::onCurrentChanged(const QModelIndex &current, const QModelIndex &previous) {
    // Если предыдущий индекс валиден, получаем элемент и закрываем его persistent editor
    if (previous.isValid()) {
        QTableWidgetItem *prevItem = templateTableWidget->item(previous.row(), previous.column());
        if (prevItem) {
            templateTableWidget->closePersistentEditor(prevItem);
        }
    }
    // Можно дополнительно установить activeTextEdit в nullptr, если редактор закрыт
    activeTextEdit = nullptr;
    if (formatToolBar) {
        formatToolBar->resetState();
    }
}
