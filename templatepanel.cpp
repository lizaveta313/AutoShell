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
#include <QIcon>

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

    // Верхняя часть: контейнер вида (таблица или график)
    viewStack = new QStackedWidget(this);

    // Вид для таблицы
    templateTableWidget = new QTableWidget(viewStack);
    templateTableWidget->setItemDelegate(new RichTextDelegate(this));
    templateTableWidget->installEventFilter(this);
    templateTableWidget->setWordWrap(false);
    templateTableWidget->setFocusPolicy(Qt::StrongFocus);
    templateTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    templateTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    templateTableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    templateTableWidget->setTextElideMode(Qt::ElideNone);
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
    // автосохранение
    connect(templateTableWidget->itemDelegate(), &QAbstractItemDelegate::commitData,
            this, [this](QWidget*){
        if (selectedTemplateId > 0) {
            saveTableData();
        }
    });
    connect(templateTableWidget->horizontalHeader(), &QHeaderView::sectionResized, this, [this](int idx, int /*oldSize*/, int newSize){
        if (idx < 0) return;
        if (savedColWidths.size() < templateTableWidget->columnCount())
            savedColWidths.resize(templateTableWidget->columnCount());
        savedColWidths[idx] = newSize;
        lastSizedTemplateId = selectedTemplateId;
    });
    connect(templateTableWidget->verticalHeader(), &QHeaderView::sectionResized, this, [this](int idx, int /*oldSize*/, int newSize){
        if (idx < 0) return;
        if (savedRowHeights.size() < templateTableWidget->rowCount())
            savedRowHeights.resize(templateTableWidget->rowCount());
        savedRowHeights[idx] = newSize;
        lastSizedTemplateId = selectedTemplateId;
    });

    // Вид для графика
    graphLabel = new QLabel(tr("Schedule will appear here"), viewStack);
    graphLabel->setAlignment(Qt::AlignCenter);
    graphLabel->setScaledContents(true);

    viewStack->addWidget(templateTableWidget); // индекс 0 – таблица
    viewStack->addWidget(graphLabel);            // индекс 1 – график

    mainLayout->addWidget(viewStack, /*stretch=*/6);

    // Нижняя часть: панель с кнопками и заметками
    QWidget *buttonsBar = new QWidget(this);
    QHBoxLayout *barLayout = new QHBoxLayout(buttonsBar);
    barLayout->setContentsMargins(8, 8, 8, 8);
    barLayout->setSpacing(5);

    //  Контейнер для кнопок, специфичных для таблицы
    tableButtonsWidget = new QWidget(buttonsBar);
    QHBoxLayout *tableButtonsLayout = new QHBoxLayout(tableButtonsWidget);
    tableButtonsLayout->setContentsMargins(0, 0, 0, 0);
    tableButtonsLayout->setSpacing(5);

    // кнопки для таблицы — только иконки + тултипы
    addHeaderButton = new QPushButton(tableButtonsWidget);
    addHeaderButton->setIcon(QIcon(":/icons/add_header.png"));
    addHeaderButton->setToolTip(tr("Add title"));
    addHeaderButton->setIconSize(QSize(32, 32));

    addRowButton = new QPushButton(tableButtonsWidget);
    addRowButton->setIcon(QIcon(":/icons/add_row.png"));
    addRowButton->setToolTip(tr("Add row"));
    addRowButton->setIconSize(QSize(32, 32));

    deleteRowButton = new QPushButton(tableButtonsWidget);
    deleteRowButton->setIcon(QIcon(":/icons/delete_row.png"));
    deleteRowButton->setToolTip(tr("Delete row"));
    deleteRowButton->setIconSize(QSize(32, 32));

    addColumnButton = new QPushButton(tableButtonsWidget);
    addColumnButton->setIcon(QIcon(":/icons/add_col.png"));
    addColumnButton->setToolTip(tr("Add column"));
    addColumnButton->setIconSize(QSize(32, 32));

    deleteColumnButton = new QPushButton(tableButtonsWidget);
    deleteColumnButton->setIcon(QIcon(":/icons/delete_col.png"));
    deleteColumnButton->setToolTip(tr("Delete column"));
    deleteColumnButton->setIconSize(QSize(32, 32));

    // Кнопки квадратного размера под иконку
    const int btnSize = 36;
    addHeaderButton->setFixedSize(btnSize, btnSize);
    addRowButton->setFixedSize(btnSize, btnSize);
    deleteRowButton->setFixedSize(btnSize, btnSize);
    addColumnButton->setFixedSize(btnSize, btnSize);
    deleteColumnButton->setFixedSize(btnSize, btnSize);

    tableButtonsLayout->addWidget(addHeaderButton);
    tableButtonsLayout->addWidget(addRowButton);
    tableButtonsLayout->addWidget(deleteRowButton);
    tableButtonsLayout->addWidget(addColumnButton);
    tableButtonsLayout->addWidget(deleteColumnButton);
    tableButtonsWidget->setLayout(tableButtonsLayout);

    // Контейнер для кнопок, спецефичных для графиков
    graphButtonsWidget = new QWidget(buttonsBar);
    QHBoxLayout *graphButtonsLayout = new QHBoxLayout(graphButtonsWidget);
    graphButtonsLayout->setContentsMargins(0, 0, 0, 0);
    graphButtonsLayout->setSpacing(5);
    changeGraphTypeButton = new QPushButton(tr("Change the graph type"), graphButtonsWidget);
    changeGraphTypeButton->setFixedSize(140, 30);
    graphButtonsLayout->addWidget(changeGraphTypeButton);
    graphButtonsWidget->setLayout(graphButtonsLayout);

    //  Контейнер для общих кнопок Утвердить
    QWidget *commonButtonsWidget = new QWidget(buttonsBar);
    QHBoxLayout *commonButtonsLayout = new QHBoxLayout(commonButtonsWidget);
    commonButtonsLayout->setContentsMargins(0, 0, 0, 0);
    commonButtonsLayout->setSpacing(5);

    relatedCombo = new QComboBox(buttonsBar);
    relatedCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    relatedCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    relatedCombo->setMinimumContentsLength(35);
    relatedCombo->setFixedHeight(30);
    relatedCombo->setToolTip(tr("Link this template to another %1 within the project").arg("item"));
    relatedCombo->addItem(QString("— %1 —").arg(tr("no link")), QVariant()); // индекс 0 = нет связи
    connect(relatedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TemplatePanel::onRelatedComboChanged);

    checkButton = new QPushButton(tr("Approve"), commonButtonsWidget);
    checkButton->setFixedSize(140, 30);
    checkButton->setToolTip("Approve the template in the TLG list");

    undoStack = new QUndoStack(this);
    undoStack->setUndoLimit(1);

    undoButton = new QPushButton(tr("Undo"), commonButtonsWidget);
    undoButton->setFixedSize(140, 30);
    undoButton->setToolTip("Canceling a deletion");
    barLayout->addWidget(undoButton);
    connect(undoButton, &QPushButton::clicked, undoStack, &QUndoStack::undo);
    undoButton->setEnabled(false);
    connect(undoStack, &QUndoStack::canUndoChanged,
            undoButton, &QPushButton::setEnabled);

    commonButtonsLayout->addWidget(relatedCombo, 1);
    commonButtonsLayout->addWidget(undoButton);
    commonButtonsLayout->addWidget(checkButton);
    commonButtonsWidget->setLayout(commonButtonsLayout);

    // Добавляем оба контейнера:
    barLayout->addWidget(tableButtonsWidget);
    barLayout->addWidget(graphButtonsWidget);
    barLayout->addWidget(commonButtonsWidget);
    buttonsBar->setLayout(barLayout);

    mainLayout->addWidget(buttonsBar, /*stretch=*/0);

    //  Под кнопками: два блока редакторов в одну строку
    QWidget *editorsContainer = new QWidget(this);
    QHBoxLayout *editorsLayout = new QHBoxLayout(editorsContainer);
    editorsLayout->setContentsMargins(8, 8, 8, 8);
    editorsLayout->setSpacing(10);

    // Контейнер для Подзаголовка
    QWidget *subtitleWidget = new QWidget(editorsContainer);
    QVBoxLayout *subtitleLayout = new QVBoxLayout(subtitleWidget);
    subtitleLayout->setContentsMargins(0,0,0,0);
    subtitleLayout->setSpacing(5);
    QLabel *subtitleLabel = new QLabel(tr("Subtitle"), subtitleWidget);
    subtitleField = new QTextEdit(subtitleWidget);
    subtitleField->setAcceptRichText(true);
    subtitleField->installEventFilter(this);
    subtitleLayout->addWidget(subtitleLabel);
    subtitleLayout->addWidget(subtitleField);
    subtitleWidget->setLayout(subtitleLayout);

    // Правая часть: панель заметок
    QWidget *notesWidget = new QWidget(editorsContainer);
    QVBoxLayout *notesLayout = new QVBoxLayout(notesWidget);
    notesLayout->setContentsMargins(0,0,0,0);
    notesLayout->setSpacing(5);
    QLabel *notesLabel = new QLabel(tr("Notes"), notesWidget);
    notesField = new QTextEdit(notesWidget);
    notesField->setAcceptRichText(true);
    notesField->installEventFilter(this);
    QLabel *notesProgrammingLabel = new QLabel(tr("Program notes"), notesWidget);
    notesProgrammingField = new QTextEdit(notesWidget);
    notesProgrammingField->setAcceptRichText(true);
    notesProgrammingField->installEventFilter(this);
    notesLayout->addWidget(notesLabel);
    notesLayout->addWidget(notesField, 1);
    notesLayout->addWidget(notesProgrammingLabel);
    notesLayout->addWidget(notesProgrammingField, 1);
    notesWidget->setLayout(notesLayout);

    editorsLayout->addWidget(subtitleWidget, /*stretch=*/1);
    editorsLayout->addWidget(notesWidget, /*stretch=*/2);
    editorsContainer->setLayout(editorsLayout);

    mainLayout->addWidget(editorsContainer, /*stretch=*/2);

    // Подключаем сигналы кнопок:
    connect(addHeaderButton, &QPushButton::clicked, this, &TemplatePanel::addHeaderRow);
    connect(addRowButton, &QPushButton::clicked, this, [this]() { addRowOrColumn("row"); });
    connect(addColumnButton, &QPushButton::clicked, this, [this]() { addRowOrColumn("column"); });
    connect(deleteRowButton, &QPushButton::clicked, this, [this]() { deleteRowOrColumn("row"); });
    connect(deleteColumnButton, &QPushButton::clicked, this, [this]() { deleteRowOrColumn("column"); });
    connect(checkButton, &QPushButton::clicked, this, &TemplatePanel::onApproveClicked);
    connect(changeGraphTypeButton, &QPushButton::clicked,
            this, &TemplatePanel::onChangeGraphTypeClicked);

    graphButtonsWidget->hide();
    tableButtonsWidget->hide();

    connect(formatToolBar, &FormatToolBar::cellFontFamilyRequested,
            this, &TemplatePanel::changeCellFontFamily);
    connect(formatToolBar, &FormatToolBar::cellFontSizeRequested,
            this, &TemplatePanel::changeCellFontSize);
    connect(formatToolBar, &FormatToolBar::cellBoldToggled,
            this, &TemplatePanel::toggleCellBold);
    connect(formatToolBar, &FormatToolBar::cellItalicToggled,
            this, &TemplatePanel::toggleCellItalic);
    connect(formatToolBar, &FormatToolBar::cellUnderlineToggled,
            this, &TemplatePanel::toggleCellUnderline);
    connect(formatToolBar, &FormatToolBar::cellAlignmentRequested,
            this, &TemplatePanel::alignCells);
    connect(formatToolBar, &FormatToolBar::cellTextColorRequested,
            this, &TemplatePanel::changeCellTextColor);
    connect(formatToolBar, &FormatToolBar::cellFillRequested,
            this, &TemplatePanel::fillCellColor);
}
void TemplatePanel::clearAll() {

    selectedTemplateId = -1;

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
    graphLabel->setText("There will be a schedule here");

    if (relatedCombo) {
        relatedCombo->blockSignals(true);
        relatedCombo->clear();
        relatedCombo->addItem(QString("— %1 —").arg(tr("no link")), QVariant());
        relatedCombo->blockSignals(false);
    }
}

//
void TemplatePanel::loadTableTemplate(int templateId) {

    selectedTemplateId = templateId;
    if (templateId == lastSizedTemplateId) {
        const int prevCols = templateTableWidget->columnCount();
        const int prevRows = templateTableWidget->rowCount();

        savedColWidths.resize(prevCols);
        for (int c = 0; c < prevCols; ++c)
            savedColWidths[c] = templateTableWidget->columnWidth(c);

        savedRowHeights.resize(prevRows);
        for (int r = 0; r < prevRows; ++r)
            savedRowHeights[r] = templateTableWidget->rowHeight(r);
    } else {
        savedColWidths.clear();
        savedRowHeights.clear();
    }
    templateTableWidget->clear();
    templateTableWidget->clearSpans();

    const QString ttype = dbHandler->getTemplateManager()->getTemplateType(templateId);
    const bool isListing = (ttype == "listing");

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

            if (isListing) {
                item->setTextAlignment(Qt::AlignLeft);
            } else {
                item->setTextAlignment((c == 0) ? Qt::AlignLeft :  Qt::AlignCenter);
            }


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

    applySizingPreservingUserChanges(nR, nC);

    // Загружаем подзаголовок, заметки и программные заметки
    QString subtitle = dbHandler->getTemplateManager()->getSubtitleForTemplate(templateId);
    QString notes = dbHandler->getTemplateManager()->getNotesForTemplate(templateId);
    QString programmingNotes = dbHandler->getTemplateManager()->getProgrammingNotesForTemplate(templateId);
    subtitleField->setHtml(subtitle);
    notesField->setHtml(notes);
    notesProgrammingField->setHtml(programmingNotes);

    // templateTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    // templateTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // templateTableWidget->resizeColumnsToContents();
    // templateTableWidget->resizeRowsToContents();

    // // делаем колонки фиксированными по ширине «под содержимое»
    // templateTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    // const int columnCount = templateTableWidget->columnCount();
    // for (int c = 0; c < columnCount; ++c) {
    //     // текущая «натуральная» ширина после resizeColumnsToContents()
    //     int w = templateTableWidget->columnWidth(c);
    //     templateTableWidget->setColumnWidth(c, w + 20);  // +20px запаса
    // }


    // templateTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    // templateTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);

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
        graphLabel->setText("No chart data available");
        return;
    }
    // Загружаем изображение в QPixmap
    QPixmap pixmap;
    if (!pixmap.loadFromData(imageData)) {
        graphLabel->setText("Image upload error");
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
    if (selectedTemplateId > 0) {
        saveTableData();
    }
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
    updateApproveUI();
    populateRelatedCombo(templateId);
}

//
QVector<CellData> TemplatePanel::collectRowBackup(int rowIndex) {
    QVector<CellData> backup;
    int r = rowIndex - 1;  // в QTableWidget строки 0-based
    int cols = templateTableWidget->columnCount();

    for (int c = 0; c < cols; ++c) {
        if (auto *item = templateTableWidget->item(r, c)) {
            CellData cd;
            cd.row      = rowIndex;
            cd.col      = c + 1;
            cd.content  = item->text();
            cd.colour   = item->background().color();
            cd.rowSpan  = templateTableWidget->rowSpan(r, c);
            cd.colSpan  = templateTableWidget->columnSpan(r, c);
            backup.append(cd);
        }
    }
    return backup;
}
QVector<CellData> TemplatePanel::collectColumnBackup(int colIndex) {
    QVector<CellData> backup;
    int c = colIndex - 1;  // 0-based
    int rows = templateTableWidget->rowCount();

    for (int r = 0; r < rows; ++r) {
        if (auto *item = templateTableWidget->item(r, c)) {
            CellData cd;
            cd.row      = r + 1;
            cd.col      = colIndex;
            cd.content  = item->text();
            cd.colour   = item->background().color();
            cd.rowSpan  = templateTableWidget->rowSpan(r, c);
            cd.colSpan  = templateTableWidget->columnSpan(r, c);
            backup.append(cd);
        }
    }
    return backup;
}

//
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
    // 1) Сначала убираем старую команду, если была
    undoStack->clear();

    // 2) Собираем backup и пушим новую команду как раньше
    if (type == "row") {
        int row = templateTableWidget->currentRow() + 1;
        QVector<CellData> backup = collectRowBackup(row);
        undoStack->push(new DeleteRowCommand(
            this, selectedTemplateId, row, std::move(backup)
            ));
    } else {
        int col = templateTableWidget->currentColumn() + 1;
        QVector<CellData> backup = collectColumnBackup(col);
        undoStack->push(new DeleteColumnCommand(
            this, selectedTemplateId, col, std::move(backup)
            ));
    }
}
void TemplatePanel::saveTableData() {
    // Сохраняем заметки (как и раньше)
    dbHandler->getTemplateManager()->updateTemplate(
        selectedTemplateId,
        std::nullopt,
        subtitleField->toHtml(),
        notesField->toHtml(),
        notesProgrammingField->toHtml()
        );

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
        tr("Change the graph type"),
        tr("Select a new type:"),
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
        QMessageBox::warning(this, "Error", "The graph entry could not be updated.");
        return;
    }

    //  Обновляем шаблон в UI: заново загружаем граф
    loadGraphTemplate(selectedTemplateId);

    qDebug() << "Тип графика успешно обновлён на" << chosenGraph;
    QMessageBox::information(this, "Done", tr("The graph type has been changed to: %1").arg(chosenGraph));
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

    // Предложения вставки
    QAction* insertRowAbove   = menu.addAction(tr("Insert row above"));
    QAction* insertRowBelow   = menu.addAction(tr("Insert row below"));
    QAction* insertColBefore  = menu.addAction(tr("Insert column before"));
    QAction* insertColAfter   = menu.addAction(tr("Insert column after"));

    QAction* mergeAction = nullptr;
    QAction* unmergeAction = nullptr;
    if(canMerge){
        mergeAction = menu.addAction("Merge cells");
    }
    if(canUnmerge){
        unmergeAction = menu.addAction("Unmerge cells");
    }

    QAction* chosen = menu.exec(templateTableWidget->viewport()->mapToGlobal(pos));
    if(!chosen) return;

    // Вставка строк
    if (chosen == insertRowAbove || chosen == insertRowBelow) {
        // вычисляем 1-based позицию
        int dbRow = (chosen == insertRowAbove ? minRow : maxRow) + 1 + (chosen == insertRowBelow ? 1 : 0);
        bool hdr = (dbRow <= headerRows);
        if (!dbHandler->getTableManager()->insertRow(selectedTemplateId, dbRow, hdr)) {
            QMessageBox::warning(this, tr("Error"), tr("Cannot insert row"));
        }
        loadTableTemplate(selectedTemplateId);
        return;
    }

    // Вставка столбцов
    if (chosen == insertColBefore || chosen == insertColAfter) {
        int dbCol = (chosen == insertColBefore ? minCol : maxCol) + 1 + (chosen == insertColAfter ? 1 : 0);
        if (!dbHandler->getTableManager()->insertColumn(selectedTemplateId, dbCol, QString())) {
            QMessageBox::warning(this, tr("Error"), tr("Cannot insert column"));
        }
        loadTableTemplate(selectedTemplateId);
        return;
    }

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
        QMessageBox::information(this, tr("Merge"),
                                 tr("You must select at least two cells."));
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
        QMessageBox::warning(this, tr("Merge"),
                             tr("Select a rectangular block of cells."));
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
        QMessageBox::warning(this, tr("Merge"),
                             tr("You cannot combine header and content cells at the same time."));
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
        QMessageBox::warning(this, tr("Error"),
                             tr("Couldn't merge cells in the database."));
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
        QMessageBox::warning(this, tr("Error"),
                             tr("Unable to disconnect: the cell was not found in the database."));
        return;
    }

    if (!dbHandler->getTableManager()->unmergeCells(
            selectedTemplateId, cellType, dbRow, dbCol))
    {
        QMessageBox::warning(this, tr("Error"),
                             tr("Failed to disconnect the cell."));
        return;
    }

    loadTableTemplate(selectedTemplateId);
    templateTableWidget->clearSelection();
    templateTableWidget->setCurrentCell(savedRow, savedCol);
}

//
void TemplatePanel::fillCellColor(const QColor &color) {
    applyToSelection([&](QTableWidgetItem* item){
        item->setBackground(color);
        dbHandler->getTableManager()->updateCellColour(
            selectedTemplateId,
            item->row(), item->column(),
            color.name()
            );
    });
}
void TemplatePanel::changeCellFontFamily(const QFont &font) {
    applyToSelection([&](QTableWidgetItem* item){
        QFont f = item->font();
        f.setFamily(font.family());
        item->setFont(f);
    });
}

void TemplatePanel::changeCellFontSize(int size) {
    applyToSelection([&](QTableWidgetItem* item){
        QFont f = item->font();
        f.setPointSize(size);
        item->setFont(f);
    });
}

void TemplatePanel::toggleCellBold(bool bold) {
    applyToSelection([&](QTableWidgetItem* item){
        QFont f = item->font();
        f.setBold(bold);
        item->setFont(f);
    });
}

void TemplatePanel::toggleCellItalic(bool italic) {
    applyToSelection([&](QTableWidgetItem* item){
        QFont f = item->font();
        f.setItalic(italic);
        item->setFont(f);
    });
}

void TemplatePanel::toggleCellUnderline(bool underline) {
    applyToSelection([&](QTableWidgetItem* item){
        QFont f = item->font();
        f.setUnderline(underline);
        item->setFont(f);
    });
}

void TemplatePanel::changeCellTextColor(const QColor &color) {
    applyToSelection([&](QTableWidgetItem* item){
        item->setForeground(QBrush(color));
    });
}

void TemplatePanel::onApproveClicked() {
    if (selectedTemplateId <= 0) return;
    bool curr = dbHandler->getTemplateManager()->isTemplateApproved(selectedTemplateId);
    if (!dbHandler->getTemplateManager()->setTemplateApproved(selectedTemplateId, !curr)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to change the approval state."));
        return;
    }
    updateApproveUI();          // подправили подпись и подсказку
    emit checkButtonPressed();  // пусть дерево перекрасится под новый статус
}

void TemplatePanel::onRelatedComboChanged(int index) {
    if (selectedTemplateId <= 0) return;
    QVariant v = relatedCombo->itemData(index);
    std::optional<int> rid;
    if (v.isValid() && v.canConvert<int>()) rid = v.toInt();
    // index 0 -> QVariant() -> rid пустой -> запишем NULL
    dbHandler->getTemplateManager()->setRelatedTemplateId(selectedTemplateId, rid);
}


void TemplatePanel::populateRelatedCombo(int templateId) {
    if (!relatedCombo) return;

    relatedCombo->blockSignals(true);
    relatedCombo->clear();
    relatedCombo->addItem(QString("— %1 —").arg(tr("no link")), QVariant());

    // тип и проект
    const QString ttype = dbHandler->getTemplateManager()->getTemplateType(templateId);
    const int     pid   = dbHandler->getTemplateManager()->getProjectIdByTemplate(templateId);

    // все шаблоны проекта данного типа
    auto list = dbHandler->getTemplateManager()->getTemplatesByProjectAndType(pid, ttype);

    // текущее связанное значение
    auto currentRelated = dbHandler->getTemplateManager()->getRelatedTemplateId(templateId);

    int currentIndex = 0;
    for (int i = 0; i < list.size(); ++i) {
        const auto& br = list[i];
        if (br.id == templateId) continue; // себя не показываем
        relatedCombo->addItem(br.name, br.id);
        if (currentRelated && *currentRelated == br.id) {
            currentIndex = relatedCombo->count() - 1;
        }
    }

    relatedCombo->setCurrentIndex(currentIndex); // 0 = «нет связи», иначе — найденный
    // Подсказка/лейбл по типу
    QString typeLabel = (ttype == "table" ? tr("table")
                         : ttype == "listing" ? tr("listing") : tr("graph"));
    relatedCombo->setToolTip(tr("Select a related %1 from this project").arg(typeLabel));
    relatedCombo->blockSignals(false);
}

void TemplatePanel::alignCells(Qt::Alignment alignment) {
    applyToSelection([&](QTableWidgetItem* item){
        item->setTextAlignment(alignment);
    });
}


//
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

void TemplatePanel::updateApproveUI() {
    if (selectedTemplateId <= 0) return;
    bool approved = dbHandler->getTemplateManager()->isTemplateApproved(selectedTemplateId);
    if (approved) {
        checkButton->setText(tr("Disapprove"));
        checkButton->setToolTip(tr("Disapprove the template in the TLG list"));
    } else {
        checkButton->setText(tr("Approve"));
        checkButton->setToolTip(tr("Approve the template in the TLG list"));
    }
}

void TemplatePanel::applySizingPreservingUserChanges(int nR, int nC) {
    auto hh = templateTableWidget->horizontalHeader();
    auto vh = templateTableWidget->verticalHeader();

    hh->setSectionResizeMode(QHeaderView::Interactive);
    vh->setSectionResizeMode(QHeaderView::Interactive);

    for (int c = 0; c < nC; ++c) {
        bool haveSaved = (c < savedColWidths.size() && savedColWidths[c] > 0);

        if (haveSaved) {
            templateTableWidget->setColumnWidth(c, savedColWidths[c]);
        } else {
            // Автоподгон только этой колонки
            hh->setSectionResizeMode(c, QHeaderView::ResizeToContents);
            templateTableWidget->resizeColumnToContents(c);
            int w = templateTableWidget->columnWidth(c);
            templateTableWidget->setColumnWidth(c, w + 20); // небольшой запас
            hh->setSectionResizeMode(c, QHeaderView::Interactive);
        }
    }

    for (int r = 0; r < nR; ++r) {
        bool haveSaved = (r < savedRowHeights.size() && savedRowHeights[r] > 0);

        if (haveSaved) {
            templateTableWidget->setRowHeight(r, savedRowHeights[r]);
        } else {
            vh->setSectionResizeMode(r, QHeaderView::ResizeToContents);
            templateTableWidget->resizeRowToContents(r);
            int h = templateTableWidget->rowHeight(r);
            templateTableWidget->setRowHeight(r, h); // оставляем «как есть»
            vh->setSectionResizeMode(r, QHeaderView::Interactive);
        }
    }

    lastSizedTemplateId = selectedTemplateId;
    if (savedColWidths.size() != nC) savedColWidths.resize(nC);
    if (savedRowHeights.size() != nR) savedRowHeights.resize(nR);
}

