#include "templatepanel.h"
#include "nonmodaldialogue.h"
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
    templateTableWidget->resizeRowsToContents();
    templateTableWidget->setFocusPolicy(Qt::StrongFocus);
    templateTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    templateTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    templateTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    templateTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);


    connect(templateTableWidget, &QTableWidget::cellClicked, this, [this](int row, int column) {
        QTableWidgetItem *item = templateTableWidget->item(row, column);
        if (item) {
            templateTableWidget->openPersistentEditor(item);
            templateTableWidget->editItem(item);
        }
    });
    connect(templateTableWidget->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &TemplatePanel::onCurrentChanged);

    connect(templateTableWidget->horizontalHeader(), &QHeaderView::sectionDoubleClicked, this, &TemplatePanel::editHeader);

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
    addRowButton = new QPushButton(tr("Добавить строку"), tableButtonsWidget);
    deleteRowButton = new QPushButton(tr("Удалить строку"), tableButtonsWidget);
    addColumnButton = new QPushButton(tr("Добавить столбец"), tableButtonsWidget);
    deleteColumnButton = new QPushButton(tr("Удалить столбец"), tableButtonsWidget);
    // Можно задать фиксированную политику размеров, чтобы кнопки не растягивались
    addRowButton->setFixedSize(140, 30);
    deleteRowButton->setFixedSize(140, 30);
    addColumnButton->setFixedSize(140, 30);
    deleteColumnButton->setFixedSize(140, 30);
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
    bottomLayout->addWidget(notesWidget, 1);          // растягивается
    bottomContainer->setLayout(bottomLayout);

    // --- Собираем вертикальный сплиттер ---
    verticalSplitter->addWidget(viewStack);
    verticalSplitter->addWidget(bottomContainer);
    verticalSplitter->setStretchFactor(0, 5);
    verticalSplitter->setStretchFactor(1, 1);
    mainLayout->addWidget(verticalSplitter);

    // Подключаем сигналы кнопок:
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

}
void TemplatePanel::clearAll() {
    // 1) Очищаем таблицу
    templateTableWidget->clear();
    templateTableWidget->setRowCount(0);
    templateTableWidget->setColumnCount(0);

    // 2) Очищаем поля заметок
    notesField->clear();
    notesProgrammingField->clear();

    // 3) Сбрасываем график
    graphLabel->clear();
    graphLabel->setText("Здесь будет график");
    // Или устанавливаем пустую картинку:
    // graphLabel->setPixmap(QPixmap());

    // 4) Обнуляем идентификатор текущего шаблона
    selectedTemplateId = -1;  // или 0, смотря какую логику вы используете
}


void TemplatePanel::loadTableTemplate(int templateId) {

    selectedTemplateId = templateId;

    // Очистка текущей таблицы
    templateTableWidget->clear();

    // Загрузка заголовков столбцов
    QVector<QString> columnHeaders = dbHandler->getTemplateManager()->getColumnHeadersForTemplate(templateId);
    templateTableWidget->setColumnCount(columnHeaders.size());
    templateTableWidget->setHorizontalHeaderLabels(columnHeaders);

    QVector<QVector<QPair<QString, QString>>> tableData = dbHandler->getTemplateManager()->getTableData(templateId);
    templateTableWidget->setRowCount(tableData.size());

    for (int row = 0; row < tableData.size(); ++row) {
        for (int col = 0; col < tableData[row].size(); ++col) {
            QPair<QString, QString> cell = tableData[row][col];
            QTableWidgetItem *item = new QTableWidgetItem(cell.first);
            item->setData(Qt::EditRole, cell.first);
            item->setData(Qt::DisplayRole, cell.first);
            // Если значение цвета пустое или некорректное, устанавливаем белый фон
            QColor bg = (cell.second.isEmpty() || !QColor(cell.second).isValid()) ? Qt::white : QColor(cell.second);
            item->setBackground(bg);
            templateTableWidget->setItem(row, col, item);
        }
    }

    // Загрузка заметок и программных заметок
    QString notes = dbHandler->getTemplateManager()->getNotesForTemplate(templateId);
    QString programmingNotes = dbHandler->getTemplateManager()->getProgrammingNotesForTemplate(templateId);

    notesField->setHtml(notes);
    notesProgrammingField->setHtml(programmingNotes);

    // Подгоняем ширину/высоту
    templateTableWidget->resizeColumnsToContents();
    templateTableWidget->resizeRowsToContents();

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

    // Также можно загрузить заметки, если они есть
    QString notes = dbHandler->getTemplateManager()->getNotesForTemplate(templateId);
    QString progNotes = dbHandler->getTemplateManager()->getProgrammingNotesForTemplate(templateId);
    notesField->setHtml(notes);
    notesProgrammingField->setHtml(progNotes);

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


void TemplatePanel::editHeader(int column) {
    if (column < 0 || !templateTableWidget) {
        qDebug() << "Некорректный столбец для редактирования.";
        return;
    }

    // Получаем текущий заголовок столбца
    QTableWidgetItem *headerItem = templateTableWidget->horizontalHeaderItem(column);
    QString currentHeader = headerItem ? headerItem->text() : tr("Новый столбец");

    // Открываем кастомный диалог для редактирования заголовка
    DialogEditName dialog(currentHeader, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString newHeader = dialog.getNewName();

        if (!newHeader.isEmpty() && newHeader != currentHeader) {
            // Обновляем заголовок в QTableWidget
            if (!headerItem) {
                headerItem = new QTableWidgetItem(newHeader);
                templateTableWidget->setHorizontalHeaderItem(column, headerItem);
            } else {
                headerItem->setText(newHeader);
            }
        }
    }
}
void TemplatePanel::addRowOrColumn(const QString &type) {
    // Проверяем, выбран ли шаблон
    if (selectedTemplateId <= 0) {
        qDebug() << "Нет выбранного шаблона (selectedTemplateId <= 0).";
        return;
    }

    QString header;
    int newOrder = -1;  // Новый порядковый номер

    if (type == "column") {
        // Запрашиваем имя нового столбца
        header = QInputDialog::getText(this, "Добавить столбец", "Введите название столбца:");
        if (header.isEmpty()) {
            qDebug() << "Добавление столбца отменено.";
            return;
        }
        newOrder = templateTableWidget->columnCount();  // новый столбец будет добавлен в конец

        // Вставляем новый столбец в конец
        templateTableWidget->insertColumn(newOrder);
        // Устанавливаем заголовок для нового столбца
        templateTableWidget->setHorizontalHeaderItem(newOrder, new QTableWidgetItem(header));
        // Для каждой строки создаем новый элемент, если он отсутствует
        for (int row = 0; row < templateTableWidget->rowCount(); ++row) {
            QTableWidgetItem *newItem = new QTableWidgetItem();
            // Присваиваем пустой текст
            newItem->setData(Qt::EditRole, "");
            newItem->setData(Qt::DisplayRole, "");
            newItem->setBackground(Qt::white);

            if (!templateTableWidget->item(row, newOrder))
                templateTableWidget->setItem(row, newOrder, newItem);
        }
        qDebug() << "Столбец добавлен.";
    }
    else if (type == "row") {
        newOrder = templateTableWidget->rowCount();  // новый ряд добавится в конец
        templateTableWidget->insertRow(newOrder);
        for (int col = 0; col < templateTableWidget->columnCount(); ++col) {
            QTableWidgetItem *newItem = new QTableWidgetItem();
            // Присваиваем пустой текст
            newItem->setData(Qt::EditRole, "");
            newItem->setData(Qt::DisplayRole, "");
            newItem->setBackground(Qt::white);

            if (!templateTableWidget->item(newOrder, col))
                templateTableWidget->setItem(newOrder, col, newItem);
        }
        qDebug() << "Строка добавлена.";
    }
}
void TemplatePanel::deleteRowOrColumn(const QString &type) {
    // Если в таблице ничего не выделено, не удаляем ни строку, ни столбец.
    if (templateTableWidget->selectedItems().isEmpty()) {
        qDebug() << QString("Не выбрана %1 для удаления.").arg(type == "row" ? "строка" : "столбец");
        return;
    }

    int currentIndex = (type == "row") ? templateTableWidget->currentRow() : templateTableWidget->currentColumn();
    if (currentIndex < 0) {
        qDebug() << QString("Не выбран %1 для удаления.").arg(type == "row" ? "строка" : "столбец");
        return;
    }

    // Проверяем выбранный шаблон
    if (selectedTemplateId <= 0) {
        qDebug() << "Нет выбранного шаблона.";
        return;
    }

    if (type == "row") {
        templateTableWidget->removeRow(currentIndex);
        qDebug() << "Строка успешно удалена.";
    } else if (type == "column") {
        templateTableWidget->removeColumn(currentIndex);
        qDebug() << "Столбец успешно удален.";
    }
}
void TemplatePanel::saveTableData() {
    if (!templateTableWidget) return;

    if (selectedTemplateId <= 0) {
        qDebug() << "Нет выбранного шаблона.";
        return;
    }


    int templateId = selectedTemplateId;

    if (viewStack->currentIndex() == 1) { // Режим графика
        QString notes = notesField->toHtml();
        QString programmingNotes = notesProgrammingField->toHtml();
        if (!dbHandler->getTemplateManager()->updateTemplate(templateId, std::nullopt, notes, programmingNotes)) {
            qDebug() << "Ошибка сохранения заметок для графика.";
            return;
        }
        qDebug() << "Заметки графика успешно сохранены.";
    } else { // Режим таблицы
        QVector<QVector<QString>> tableData;
        QVector<QVector<QString>> cellColours;
        QVector<QString> columnHeaders;

        // Сохранение данных строк
        for (int row = 0; row < templateTableWidget->rowCount(); ++row) {
            QVector<QString> rowData;
            QVector<QString> rowColours;
            for (int col = 0; col < templateTableWidget->columnCount(); ++col) {
                QTableWidgetItem *item = templateTableWidget->item(row, col);
                // Используем данные из роли редактирования, где хранится HTML
                rowData.append(item ? item->data(Qt::EditRole).toString() : "");
                rowColours.append(item ? item->background().color().name() : "#FFFFFF");
            }
            tableData.append(rowData);
            cellColours.append(rowColours);
        }

        // Сохранение заголовков столбцов
        for (int col = 0; col < templateTableWidget->columnCount(); ++col) {
            QTableWidgetItem *headerItem = templateTableWidget->horizontalHeaderItem(col);
            //columnHeaders.append(headerItem ? headerItem->text() : "");
            // Аналогично, получаем HTML для заголовков, если он сохранён
            columnHeaders.append(headerItem ? headerItem->data(Qt::EditRole).toString() : "");
        }

        // Получение заметок и программных заметок из соответствующих полей
        QString notes = notesField->toHtml();
        QString programmingNotes = notesProgrammingField->toHtml();

        // Сохранение данных таблицы
        if (!dbHandler->getTableManager()->saveDataTableTemplate(templateId, columnHeaders, tableData, cellColours)) {
            qDebug() << "Ошибка сохранения данных таблицы.";
            return;
        }

        // Сохранение заметок и программных заметок
        if (!dbHandler->getTemplateManager()->updateTemplate(templateId, std::nullopt, notes, programmingNotes)) {
            qDebug() << "Ошибка сохранения заметок.";
            return;
        }

        qDebug() << "Данные таблицы, заметки и программные заметки успешно сохранены.";
    }
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
