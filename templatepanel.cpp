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


TemplatePanel::TemplatePanel(DatabaseHandler *dbHandler, FormatToolBar *formatToolBar, QWidget *parent)
    : QWidget(parent)
    , dbHandler(dbHandler)
    , formatToolBar(formatToolBar) {

    // Таблица
    templateTableWidget = new QTableWidget(this);

    templateTableWidget->setItemDelegate(new RichTextDelegate(this));
    templateTableWidget->installEventFilter(this);
    templateTableWidget->setWordWrap(true);
    templateTableWidget->resizeRowsToContents();
    templateTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

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

    // Создаём текстовые поля
    notesField = new QTextEdit(this);
    notesProgrammingField = new QTextEdit(this);
    notesField->setAcceptRichText(true);
    notesProgrammingField->setAcceptRichText(true);
    notesField->installEventFilter(this);
    notesProgrammingField->installEventFilter(this);

    // Подписи для заметок
    QLabel *notesLabel = new QLabel("Заметки", this);
    QLabel *notesProgrammingLabel = new QLabel("Программные заметки", this);

    // Создаём кнопки
    addRowButton = new QPushButton(tr("Добавить строку"), this);
    addColumnButton = new QPushButton(tr("Добавить столбец"), this);
    deleteRowButton = new QPushButton(tr("Удалить строку"), this);
    deleteColumnButton = new QPushButton(tr("Удалить столбец"), this);
    saveButton = new QPushButton(tr("Сохранить"), this);
    checkButton = new QPushButton(tr("Утвердить"), this);

    // Подключаем сигналы кнопок
    connect(addRowButton, &QPushButton::clicked,
            this, [this]() { addRowOrColumn("row"); });
    connect(addColumnButton, &QPushButton::clicked,
            this, [this]() { addRowOrColumn("column"); });
    connect(deleteRowButton, &QPushButton::clicked,
            this, [this]() { deleteRowOrColumn("row"); });
    connect(deleteColumnButton, &QPushButton::clicked,
            this, [this]() { deleteRowOrColumn("column"); });
    connect(saveButton, &QPushButton::clicked,
            this, &TemplatePanel::saveTableData);
    connect(checkButton, &QPushButton::clicked,
            this, [this]() {
                emit checkButtonPressed();
            });

    // Компоновка
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Верхняя часть: QTableWidget
    QSplitter *verticalSplitter = new QSplitter(Qt::Vertical, this);
    verticalSplitter->addWidget(templateTableWidget);

    // Нижняя часть (кнопки + заметки)
    QWidget *bottomWidget = new QWidget(this);
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomWidget);

    // Слева: кнопки для работы с таблицей
    QVBoxLayout *tableButtonLayout = new QVBoxLayout;
    tableButtonLayout->addWidget(addRowButton);
    tableButtonLayout->addWidget(deleteRowButton);
    tableButtonLayout->addWidget(addColumnButton);
    tableButtonLayout->addWidget(deleteColumnButton);
    tableButtonLayout->addWidget(saveButton);
    tableButtonLayout->addWidget(checkButton);

    // Справа: заметки
    QVBoxLayout *notesLayout = new QVBoxLayout;
    notesLayout->addWidget(notesLabel);
    notesLayout->addWidget(notesField);
    notesLayout->addWidget(notesProgrammingLabel);
    notesLayout->addWidget(notesProgrammingField);

    bottomLayout->addLayout(tableButtonLayout);
    bottomLayout->addLayout(notesLayout);
    verticalSplitter->addWidget(bottomWidget);

    // Настройки сплиттера
    verticalSplitter->setStretchFactor(0, 5);
    verticalSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(verticalSplitter);
    setLayout(mainLayout);
}

TemplatePanel::~TemplatePanel() {}


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

    qDebug() << "Шаблон таблицы с ID" << templateId << "загружен.";
}

void TemplatePanel::editHeader(int column) {
    if (column < 0 || !templateTableWidget) {
        qDebug() << "Некорректный столбец для редактирования.";
        return;
    }

    int templateId = selectedTemplateId;

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

            // Сохраняем изменения в базе данных
            if (!dbHandler->getTableManager()->updateColumnHeader(templateId, column, newHeader)) {
                qDebug() << "Ошибка обновления заголовка столбца в базе данных.";
            } else {
                qDebug() << "Заголовок столбца успешно обновлен в базе данных.";
            }
        }
    }
}
void TemplatePanel::addRowOrColumn(const QString &type) {
    // 1) Проверяем, есть ли текущий шаблон
    if (selectedTemplateId <= 0) {
        qDebug() << "Нет выбранного шаблона (m_currentTemplateId <= 0).";
        return;
    }

    int templateId = selectedTemplateId;
    QString header;
    int newOrder = -1;  // Новый порядковый номер

    // Если добавляем столбец
    if (type == "column") {
        header = QInputDialog::getText(this, "Добавить столбец", "Введите название столбца:");
        if (header.isEmpty()) {
            qDebug() << "Добавление столбца отменено.";
            return;
        }
        newOrder = templateTableWidget->columnCount();
        templateTableWidget->setColumnCount(newOrder + 1);
    }
    // Если добавляем строку
    else if (type == "row") {
        newOrder = templateTableWidget->rowCount();
        templateTableWidget->setRowCount(newOrder + 1);
    }

    // Добавление строки или столбца в базу данных
    if (!dbHandler->getTableManager()->createRowOrColumn(templateId, type, header, newOrder)) {
        qDebug() << QString("Ошибка добавления %1 в базу данных.").arg(type);
        return;
    }

    // Обновление интерфейса
    if (type == "row") {
        int uiRow = newOrder - 1; // корректировка индекса для QTableWidget
        templateTableWidget->insertRow(uiRow);
        for (int col = 0; col < templateTableWidget->columnCount(); ++col) {
            templateTableWidget->setItem(uiRow, col, new QTableWidgetItem());
        }
        qDebug() << "Строка добавлена.";
    } else if (type == "column") {
        int uiColumn = newOrder - 1; // корректировка индекса
        templateTableWidget->insertColumn(uiColumn);
        templateTableWidget->setHorizontalHeaderItem(uiColumn, new QTableWidgetItem(header));
        for (int row = 0; row < templateTableWidget->rowCount(); ++row) {
            templateTableWidget->setItem(row, uiColumn, new QTableWidgetItem());
        }
        qDebug() << "Столбец добавлен.";
    }
}
void TemplatePanel::deleteRowOrColumn(const QString &type) {
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

    int templateId = selectedTemplateId;

    if (type == "column") {
        int colCount = templateTableWidget->columnCount();
        if (colCount == 0) return;
        if (!dbHandler->getTableManager()->deleteRowOrColumn(templateId, colCount - 1, type)) {
            qDebug() << "Ошибка удаления столбца";
            return;
        }
        templateTableWidget->setColumnCount(colCount - 1);
    } else if (type == "row") {
        int rowCount = templateTableWidget->rowCount();
        if (rowCount == 0) return;
        if (!dbHandler->getTableManager()->deleteRowOrColumn(templateId, rowCount - 1, type)) {
            qDebug() << "Ошибка удаления строки";
            return;
        }
        templateTableWidget->setRowCount(rowCount - 1);
    }

    // Удаляем строку или столбец в базе данных
    if (!dbHandler->getTableManager()->deleteRowOrColumn(templateId, currentIndex, type)) {
        qDebug() << QString("Ошибка удаления %1 из базы данных.").arg(type == "row" ? "строки" : "столбца");
        return;
    }

    // Обновляем интерфейс
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
}
