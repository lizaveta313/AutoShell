#include "mainwindow.h"
#include "nonmodaldialogue.h"
#include "richtextdelegate.h"
#include <QSplitter>
#include <QInputDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QMenu>
#include <QLabel>
#include <QFontDatabase>
#include <QActionGroup>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QApplication>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {

    // Создаем объект DatabaseHandler и передаем объект базы данных
    dbHandler = new DatabaseHandler(this);

    // Подключение к базе данных
    if (!dbHandler->connectToDatabase()) {
        qDebug() << "Не удалось подключиться к базе данных";
        close();
        return;
    }

    createFormatToolBar();        // Создаем панель форматирования
    setupUI();                    // Настройка интерфейса
    loadProjects();               // Загрузка списка проектов
}

MainWindow::~MainWindow() {}

//
void MainWindow::setupUI() {
    // Основной виджет и компоновка
    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout;

    // Создаем выбор проекта
    projectComboBox = new QComboBox(this);
    projectComboBox->addItem("Выберите проект");
    connect(projectComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProjectSelected);

    // Создаем категории
    categoryTreeWidget = new MyTreeWidget(this);
    categoryTreeWidget->setColumnCount(2);
    categoryTreeWidget->setHeaderLabels({"№", "Название"});
    categoryTreeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    categoryTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    categoryTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    categoryTreeWidget->setEditTriggers(QAbstractItemView::DoubleClicked);
    connect(categoryTreeWidget, &MyTreeWidget::dropped,
            this, &MainWindow::updateNumbering);
    connect(categoryTreeWidget, &QTreeWidget::itemClicked,
            this, &MainWindow::onCategoryOrTemplateSelected);
    connect(categoryTreeWidget, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onCategoryOrTemplateDoubleClickedForEditing);
    connect(categoryTreeWidget, &QWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(projectComboBox);
    leftLayout->addWidget(categoryTreeWidget);

    // Таблица
    templateTableWidget = new QTableWidget(this);

    templateTableWidget->setItemDelegate(new RichTextDelegate(this));
    templateTableWidget->setWordWrap(true);
    templateTableWidget->resizeRowsToContents();
    //templateTableWidget->setEditTriggers(QAbstractItemView::SelectedClicked);

    connect(templateTableWidget, &QTableWidget::cellClicked, this, [this](int row, int column) {
        QTableWidgetItem *item = templateTableWidget->item(row, column);
        if (item) {
            // Открываем постоянный редактор для ячейки, чтобы он не закрывался при потере фокуса.
            templateTableWidget->openPersistentEditor(item);
            // Если нужно, можно вызвать editItem(item) для явного перехода в режим редактирования.
            templateTableWidget->editItem(item);
        }
    });
    connect(templateTableWidget->horizontalHeader(), &QHeaderView::sectionDoubleClicked, this, &MainWindow::editHeader);

    // Заметки (текстовые поля)
    notesField = new QTextEdit(this);
    notesProgrammingField = new QTextEdit(this);

    notesField->setAcceptRichText(true);
    notesProgrammingField->setAcceptRichText(true);

    notesField->installEventFilter(this);
    notesProgrammingField->installEventFilter(this);


    // Подписи для заметок
    QLabel *notesLabel = new QLabel("Заметки", this);
    QLabel *notesProgrammingLabel = new QLabel("Программные заметки", this);

    // Кнопки для работы с таблицей
    addRowButton = new QPushButton("Добавить строку", this);
    addColumnButton = new QPushButton("Добавить столбец", this);
    deleteRowButton = new QPushButton("Удалить строку", this);
    deleteColumnButton = new QPushButton("Удалить столбец", this);
    saveButton = new QPushButton("Сохранить", this);
    checkButton = new QPushButton("Утвердить", this);

    // Подключение сигналов кнопок
    connect(addRowButton, &QPushButton::clicked,
            this, [this]() { addRowOrColumn("row"); });
    connect(addColumnButton, &QPushButton::clicked,
            this, [this]() { addRowOrColumn("column"); });
    connect(deleteRowButton, &QPushButton::clicked,
            this, [this]() { deleteRowOrColumn("row"); });
    connect(deleteColumnButton, &QPushButton::clicked,
            this, [this]() { deleteRowOrColumn("column"); });
    connect(saveButton, &QPushButton::clicked,
            this, &MainWindow::saveTableData);
    connect(checkButton, &QPushButton::clicked,
            this, &MainWindow::onCheckButtonClicked);

    // Правая часть: Таблица и нижняя часть
    QVBoxLayout *rightLayout = new QVBoxLayout;

    // Вертикальный сплиттер: верхняя часть (таблица) и нижняя часть (кнопки, заметки)
    QSplitter *verticalSplitter = new QSplitter(Qt::Vertical, this);
    verticalSplitter->addWidget(templateTableWidget);

    // Нижняя часть: кнопки и заметки
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

    // Справа: подписи + заметки и программные заметки
    QVBoxLayout *notesLayout = new QVBoxLayout;
    notesLayout->addWidget(notesLabel);
    notesLayout->addWidget(notesField);
    notesLayout->addWidget(notesProgrammingLabel);
    notesLayout->addWidget(notesProgrammingField);

    // Добавляем кнопки и заметки в нижнюю часть
    bottomLayout->addLayout(tableButtonLayout); // Кнопки слева
    bottomLayout->addLayout(notesLayout);       // Заметки справа

    // Добавляем нижнюю часть в вертикальный сплиттер
    verticalSplitter->addWidget(bottomWidget);

    // Настройка размеров
    verticalSplitter->setStretchFactor(0, 5); // Таблица занимает 5 частей
    verticalSplitter->setStretchFactor(1, 1); // Заметки и кнопки занимают 1 часть

    // Добавляем вертикальный сплиттер в правый блок
    rightLayout->addWidget(verticalSplitter);

    // Добавляем левые и правые блоки в основной компоновщик
    mainLayout->addLayout(leftLayout, 1);  // Левый блок (TreeWidget)
    mainLayout->addLayout(rightLayout, 4); // Правый блок (Таблица и нижняя часть)

    // Настройка центрального виджета
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    // Настройки окна
    setWindowTitle("AutoShell");
    resize(1000, 600);
}

//
void MainWindow::createFormatToolBar() {
    // Создаем панель форматирования и добавляем ее в главное окно
    formatToolBar = addToolBar("Форматирование");

    // 1. Выбор шрифта
    fontCombo = new QFontComboBox(formatToolBar);
    fontCombo->setFocusPolicy(Qt::NoFocus);
    formatToolBar->addWidget(fontCombo);
    connect(fontCombo, &QFontComboBox::currentFontChanged,
            this, &MainWindow::applyFontFamily);

    // 2. Выбор размера шрифта
    sizeCombo = new QComboBox(formatToolBar);
    sizeCombo->setFocusPolicy(Qt::NoFocus);
    sizeCombo->setEditable(true);
    // Заполняем стандартными размерами
    for (int size : QFontDatabase::standardSizes()) {
        sizeCombo->addItem(QString::number(size));
    }
    formatToolBar->addWidget(sizeCombo);
    connect(sizeCombo, &QComboBox::currentTextChanged,
            this, &MainWindow::applyFontSize);

    // 3. Кнопка "Жирный"
    boldAction = new QAction("B", this);
    boldAction->setCheckable(true);
    QFont boldFont = boldAction->font();
    boldFont.setBold(true);
    boldAction->setFont(boldFont);
    formatToolBar->addAction(boldAction);
    connect(boldAction, &QAction::triggered, this, &MainWindow::toggleBold);

    // 4. Кнопка "Курсив"
    italicAction = new QAction("I", this);
    italicAction->setCheckable(true);
    QFont italicFont = italicAction->font();
    italicFont.setItalic(true);
    italicAction->setFont(italicFont);
    formatToolBar->addAction(italicAction);
    connect(italicAction, &QAction::triggered, [this](){
        toggleItalic(italicAction->isChecked());
    });

    // 5. Кнопка "Подчеркнутый"
    underlineAction = new QAction("U", this);
    underlineAction->setCheckable(true);
    QFont underlineFont = underlineAction->font();
    underlineFont.setUnderline(true);
    underlineAction->setFont(underlineFont);
    formatToolBar->addAction(underlineAction);
    connect(underlineAction, &QAction::triggered, [this](){
        toggleUnderline(underlineAction->isChecked());
    });

    // 6. Кнопки выравнивания
    QActionGroup *alignGroup = new QActionGroup(this);

    leftAlignAction = new QAction("L", alignGroup);
    leftAlignAction->setCheckable(true);
    formatToolBar->addAction(leftAlignAction);
    connect(leftAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
    });

    centerAlignAction = new QAction("C", alignGroup);
    centerAlignAction->setCheckable(true);
    formatToolBar->addAction(centerAlignAction);
    connect(centerAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignHCenter);
    });

    rightAlignAction = new QAction("R", alignGroup);
    rightAlignAction->setCheckable(true);
    formatToolBar->addAction(rightAlignAction);
    connect(rightAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
    });

    justifyAlignAction = new QAction("J", alignGroup);
    justifyAlignAction->setCheckable(true);
    formatToolBar->addAction(justifyAlignAction);
    connect(justifyAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignJustify);
    });

    // По умолчанию оставляем, например, левое выравнивание
    leftAlignAction->setChecked(true);
}

void MainWindow::applyFontFamily(const QFont &font) {
    // Устанавливаем выбранный шрифт в выделенный текст
    QTextCharFormat format;
    format.setFontFamilies(QStringList() << font.family());
    mergeFormatOnWordOrSelection(format);
}
void MainWindow::applyFontSize(const QString &sizeText) {
    bool ok = false;
    int size = sizeText.toInt(&ok);
    if (ok && size > 0) {
        QTextCharFormat format;
        format.setFontPointSize(size);
        mergeFormatOnWordOrSelection(format);
    }
}
void MainWindow::toggleBold() {
    // Включаем/выключаем жирный
    QTextCharFormat format;
    format.setFontWeight(boldAction->isChecked() ? QFont::Bold : QFont::Normal);
    mergeFormatOnWordOrSelection(format);
}
void MainWindow::toggleItalic(bool checked) {
    QTextCharFormat format;
    format.setFontItalic(italicAction->isChecked());
    mergeFormatOnWordOrSelection(format);
}
void MainWindow::toggleUnderline(bool checked) {
    QTextCharFormat format;
    format.setFontUnderline(underlineAction->isChecked());
    mergeFormatOnWordOrSelection(format);
}
void MainWindow::setAlignment(Qt::Alignment alignment) {
    if (!activeTextEdit)
        return; // Игнорируем, если нет активного редактора

    activeTextEdit->setAlignment(alignment);
}

void MainWindow::mergeFormatOnWordOrSelection(const QTextCharFormat &format) {

    if (!activeTextEdit) {
        qDebug() << "Редактор не установлен (activeTextEdit == nullptr)";
        return;
    }

    if (!activeTextEdit->hasFocus()) {
        qDebug() << "Редактор не в режиме редактирования (нет фокуса)";
        return;
    }

    QTextCursor cursor = activeTextEdit->textCursor();

    if (!cursor.hasSelection()) {
        // Если редактор является ячейкой таблицы, принудительно выделяем весь документ,
        // чтобы форматирование применялось к всему тексту в ячейке.
        if (activeTextEdit->parent()->inherits("QTableWidget"))
            cursor.select(QTextCursor::Document);
        else
            return; // Для остальных редакторов — ничего не делаем.
    }

    cursor.mergeCharFormat(format);
    activeTextEdit->mergeCurrentCharFormat(format);
}
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::FocusIn) {
        if (QTextEdit *ed = qobject_cast<QTextEdit*>(obj)) {
            activeTextEdit = ed;
            connect(ed, &QTextEdit::cursorPositionChanged, this, &MainWindow::updateFormatActions);
            connect(ed, &QTextEdit::currentCharFormatChanged, this, &MainWindow::updateFormatActions);
        }
    } else if (event->type() == QEvent::FocusOut) {
        if (QTextEdit *ed = qobject_cast<QTextEdit*>(obj)) {
            // Определяем, на какой виджет переходит фокус
            QWidget *newFocus = QApplication::focusWidget();
            // Если новый виджет является дочерним от панели форматирования,
            // то не сбрасываем activeTextEdit.
            if (formatToolBar && newFocus && formatToolBar->isAncestorOf(newFocus)) {
                // Не обрабатываем FocusOut для сохранения ссылки на редактор
                return QMainWindow::eventFilter(obj, event);
            }
            // Иначе, если редактируемый объект совпадает с activeTextEdit, сбрасываем указатель.
            if (ed == activeTextEdit)
                activeTextEdit = nullptr;
            boldAction->setChecked(false);
            italicAction->setChecked(false);
            underlineAction->setChecked(false);
            // Устанавливаем выравнивание по умолчанию (например, влево)
            leftAlignAction->setChecked(true);
            centerAlignAction->setChecked(false);
            rightAlignAction->setChecked(false);
            justifyAlignAction->setChecked(false);
            // Можно сбросить и combobox с шрифтом/размером, если нужно
            //fontCombo->setCurrentIndex(0);
            //sizeCombo->setCurrentIndex(0);
            }
    }
    return QMainWindow::eventFilter(obj, event);
}
void MainWindow::updateFormatActions() {
    // Если активный редактор отсутствует, выходим
    if (!activeTextEdit)
        return;

    // Получаем текущий формат текста из активного редактора.
    // Если ничего не выделено, берётся формат текущего курсора.
    QTextCharFormat currentFormat = activeTextEdit->currentCharFormat();

    // Обновляем состояние кнопки "Жирный":
    // Если вес шрифта равен QFont::Bold, считаем, что текст жирный.
    bool isBold = (currentFormat.fontWeight() == QFont::Bold);
    boldAction->setChecked(isBold);

    // Обновляем состояние кнопки "Курсив":
    bool isItalic = currentFormat.fontItalic();
    italicAction->setChecked(isItalic);

    // Обновляем состояние кнопки "Подчёркнутый":
    bool isUnderline = currentFormat.fontUnderline();
    underlineAction->setChecked(isUnderline);

    // Обновляем состояние кнопок выравнивания на основе текущего выравнивания редактора.
    // Здесь используются битовые операции, поскольку выравнивание может комбинироваться.
    Qt::Alignment alignment = activeTextEdit->alignment();
    leftAlignAction->setChecked(alignment & Qt::AlignLeft);
    centerAlignAction->setChecked(alignment & Qt::AlignHCenter);
    rightAlignAction->setChecked(alignment & Qt::AlignRight);
    justifyAlignAction->setChecked(alignment & Qt::AlignJustify);

    // Дополнительно: обновляем комбобоксы для выбора шрифта и размера
    // Обновляем выбор шрифта: ищем в fontCombo текущий шрифт редактора
    QFont currentFont = currentFormat.font();
    int fontIndex = fontCombo->findText(currentFont.family());
    if (fontIndex != -1) {
        fontCombo->setCurrentIndex(fontIndex);
    }
    // Обновляем выбор размера шрифта: ищем в sizeCombo размер шрифта в пунктах
    QString fontSizeStr = QString::number(currentFont.pointSize());
    int sizeIndex = sizeCombo->findText(fontSizeStr);
    if (sizeIndex != -1) {
        sizeCombo->setCurrentIndex(sizeIndex);
    }
}

//
void MainWindow::onCategoryOrTemplateSelected(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column);

    if (!item) return;

    if (item->parent() == nullptr) {
        // Это категория
        item->setExpanded(!item->isExpanded()); // Раскрываем или сворачиваем список шаблонов
    } else {
        // Это шаблон
        int templateId = item->data(0, Qt::UserRole).toInt();
        loadTableTemplate(templateId); // Загружаем таблицу шаблона
    }
}
void MainWindow::onCategoryOrTemplateDoubleClickedForEditing(QTreeWidgetItem *item, int column) {
    if (!item) return;

    if (column == 0) { // Редактирование нумерации
        QString currentNumeration = item->text(column);

        bool ok;
        QString newNumeration = QInputDialog::getText(this, "Редактирование нумерации",
                                                      "Введите новую нумерацию:", QLineEdit::Normal,
                                                      currentNumeration, &ok);

        if (ok && !newNumeration.isEmpty() && newNumeration != currentNumeration) {
            bool convOk;
            int manualNum = newNumeration.toInt(&convOk);

            //qDebug() << "Введённое значение для нумерации:" << newNumeration;
            //qDebug() << "Результат конвертации в число:" << manualNum << "Статус конвертации:" << convOk;

            if (!convOk) {
                QMessageBox::warning(this, "Ошибка", "Неверный формат номера. Пожалуйста, введите число.");
                return;
            }
            updateSiblingNumbering(item, manualNum);
        }
    } else if (column == 1) { // Редактирование названия
        QString currentName = item->text(column);

        bool ok;
        QString newName = QInputDialog::getText(this, "Редактирование названия",
                                                "Введите новое название:", QLineEdit::Normal,
                                                currentName, &ok);

        if (ok && !newName.isEmpty() && newName != currentName) {
            item->setText(column, newName);

            // Сохранение изменений в базе данных
            if (item->data(0, Qt::UserRole + 1).toBool()) {
                int categoryId = item->data(0, Qt::UserRole).toInt();
                dbHandler->getCategoryManager()->updateCategory(categoryId, newName);
            } else {
                int templateId = item->data(0, Qt::UserRole).toInt();
                dbHandler->getTemplateManager()->updateTemplate(templateId, newName, std::nullopt, std::nullopt);
            }
        }
    }
}
void MainWindow::onCheckButtonClicked() {
    // Получаем текущий выбранный элемент
    QTreeWidgetItem *selectedItem = categoryTreeWidget->currentItem();

    if (!selectedItem) {
        qDebug() << "Нет выбранного элемента для утверждения.";
        return;
    }

    // Проверяем текущий цвет текста элемента
    if (selectedItem->foreground(1).color() == Qt::red) {
        // Меняем цвет с красного на зеленый
        selectedItem->setForeground(1, QBrush(Qt::darkGreen));
    } else if (selectedItem->foreground(1).color() == Qt::darkGreen) {
        // (Опционально) Можно вернуть цвет обратно в красный
        selectedItem->setForeground(1, QBrush(Qt::red));
    }

    qDebug() << "Цвет выбранного элемента обновлен.";
}

//
void MainWindow::loadItemsForCategory(int projectId, const QVariant &parentId, QTreeWidgetItem *parentItem, const QString &parentPath) {
    QVector<CombinedItem> items;

    // Если parentId равен NULL – это корневой уровень, шаблоны не существуют
    if (parentId.isNull()) {
        QVector<Category> categories = dbHandler->getCategoryManager()->getCategoriesByProjectAndParent(projectId, parentId);
        for (const Category &cat : categories) {
            CombinedItem ci;
            ci.isCategory = true;
            ci.position = cat.position;
            ci.id = cat.categoryId;
            ci.name = cat.name;
            //ci.category = cat;
            items.append(ci);
        }
    } else {
        // Для выбранной категории – получаем подкатегории
        QVector<Category> subCategories = dbHandler->getCategoryManager()->getCategoriesByProjectAndParent(projectId, parentId);
        for (const Category &cat : subCategories) {
            CombinedItem ci;
            ci.isCategory = true;
            ci.position = cat.position;
            ci.id = cat.categoryId;
            ci.name = cat.name;
            //ci.category = cat;
            items.append(ci);
        }
        // И шаблоны, привязанные к данной категории
        QVector<Template> templates = dbHandler->getTemplateManager()->getTemplatesForCategory(parentId.toInt());
        for (const Template &tmpl : templates) {
            CombinedItem ci;
            ci.isCategory = false;
            ci.position = tmpl.position;
            ci.id = tmpl.templateId;
            ci.name = tmpl.name;
            //ci.templ = tmpl;
            items.append(ci);
        }
    }

    // Сортируем по полю position
    std::sort(items.begin(), items.end(), [](const CombinedItem &a, const CombinedItem &b) {
        return a.position < b.position;
    });

    // Создаем узлы дерева; для отображения используем сохранённое значение position
    //int index = 0;
    for (const CombinedItem &ci : items) {
        QTreeWidgetItem *item = (parentItem == nullptr)
        ? new QTreeWidgetItem(categoryTreeWidget)
        : new QTreeWidgetItem(parentItem);
        item->setText(1, ci.name);

        // Отображаем номер, используя сохранённое значение из БД:
        QString nodeNumber = QString::number(ci.position);
        QString numeration = parentPath.isEmpty() ? nodeNumber : parentPath + "." + nodeNumber;
        // QString numeration = parentPath.isEmpty() ? QString::number(index + 1)
        //                                           : parentPath + "." + QString::number(index + 1);
        item->setText(0, numeration);
        if (ci.isCategory) {
            item->setData(0, Qt::UserRole, ci.id);
            item->setData(0, Qt::UserRole + 1, true); // помечаем как категория
            // Рекурсивно загружаем вложенные элементы для этой категории
            loadItemsForCategory(projectId, ci.id, item, numeration);
        } else {
            item->setData(0, Qt::UserRole, ci.id);
            item->setData(0, Qt::UserRole + 1, false); // помечаем как шаблон
            // Для шаблонов можно задать, например, красный цвет названия
            item->setForeground(1, QBrush(Qt::red));
        }
        //index++;
    }
}
void MainWindow::loadProjects() {
    projectComboBox->clear();
    projectComboBox->addItem("Выберите проект", QVariant()); // Пустой элемент по умолчанию

    QVector<Project> projects = dbHandler->getProjectManager()->getProjects();
    for (const Project &project : projects) {
        projectComboBox->addItem(project.name, project.projectId);
    }

    categoryTreeWidget->clear(); // Очищаем дерево категорий
}
void MainWindow::onProjectSelected(int index) {
    QVariant projectData = projectComboBox->itemData(index);
    if (!projectData.isValid()) {
        categoryTreeWidget->clear();
        return;
    }

    int projectId = projectData.toInt();

    // Спрашиваем у пользователя количество групп
    int numGroups = askForGroupCount();
    if (numGroups == -1) {
        qDebug() << "Пользователь отменил выбор количества групп.";
        return;
    }

    // Получаем только динамические шаблоны
    QVector<int> dynamicTemplates = dbHandler->getTemplateManager()->getDynamicTemplatesForProject(projectId);
    for (int templateId : dynamicTemplates) {
        dbHandler->getTableManager()->generateColumnsForDynamicTemplate(templateId, numGroups);
    }

    // Перезагружаем таблицы
    loadCategoriesAndTemplates();
    //int projectId = projectData.toInt();
    //categoryTreeWidget->clear();
}
void MainWindow::loadCategoriesAndTemplates() {
    QSet<int> expandedIds = saveExpandedState();
    int projectId = projectComboBox->currentData().toInt();
    categoryTreeWidget->clear();
    loadItemsForCategory(projectId, QVariant(), nullptr, QString());
    restoreExpandedState(expandedIds);
}
void MainWindow::loadCategoriesForProject(int projectId, QTreeWidgetItem *parentItem, const QString &parentPath) {
    QVariant parentId;
    if (parentItem == nullptr) {
        // Для корневых категорий parent_id = NULL
        parentId = QVariant();
    } else {
        parentId = parentItem->data(0, Qt::UserRole);
    }

    QVector<Category> categories = dbHandler->getCategoryManager()->getCategoriesByProjectAndParent(projectId, parentId);

    for (const Category &category : categories) {
        QTreeWidgetItem *categoryItem = (parentItem == nullptr)
        ? new QTreeWidgetItem(categoryTreeWidget)
        : new QTreeWidgetItem(parentItem);

        categoryItem->setText(1, category.name);
        categoryItem->setData(0, Qt::UserRole, category.categoryId);
        categoryItem->setData(0, Qt::UserRole + 1, true); // Отмечаем как категория

        QString numeration = parentPath.isEmpty() ? QString::number(category.position)
                                                  : parentPath + "." + QString::number(category.position);
        categoryItem->setText(0, numeration);

        // Рекурсивно загружаем подкатегории и шаблоны внутри данной категории
        loadCategoriesForProject(projectId, categoryItem, numeration);
        loadTemplatesForCategory(category.categoryId, categoryItem, numeration);
    }
}
void MainWindow::loadCategoriesForCategory(const Category &category, QTreeWidgetItem *parentItem, const QString &parentPath) {
    QVector<Category> subCategories = dbHandler->getCategoryManager()->getCategoriesByProject(category.projectId);

    for (const Category &subCategory : subCategories) {
        if (subCategory.parentId == category.categoryId) {
            QTreeWidgetItem *subCategoryItem = new QTreeWidgetItem(parentItem);
            subCategoryItem->setText(1, subCategory.name);
            subCategoryItem->setData(0, Qt::UserRole, QVariant::fromValue(subCategory.categoryId));

            QString numeration = parentPath + "." + QString::number(subCategory.position);
            subCategoryItem->setText(0, numeration);

            loadCategoriesForCategory(subCategory, subCategoryItem, numeration);
            loadTemplatesForCategory(subCategory.categoryId, subCategoryItem, numeration);
        }
    }
}
void MainWindow::loadTemplatesForCategory(int categoryId, QTreeWidgetItem *parentItem, const QString &parentPath) {
    QVector<Template> templates = dbHandler->getTemplateManager()->getTemplatesForCategory(categoryId);

    for (const Template &tmpl : templates) {
        QTreeWidgetItem *templateItem = new QTreeWidgetItem(parentItem);
        templateItem->setText(1, tmpl.name);
        templateItem->setData(0, Qt::UserRole, QVariant::fromValue(tmpl.templateId));
        templateItem->setData(0, Qt::UserRole + 1, false); // помечаем как шаблон

        QString numeration = parentPath + "." + QString::number(tmpl.position);
        templateItem->setText(0, numeration);

        // Устанавливаем красный цвет текста по умолчанию
        templateItem->setForeground(1, QBrush(Qt::red));
    }
}
void MainWindow::loadTableTemplate(int templateId) {
    // Очистка текущей таблицы
    templateTableWidget->clear();

    // Загрузка заголовков столбцов
    QVector<QString> columnHeaders = dbHandler->getTemplateManager()->getColumnHeadersForTemplate(templateId);
    templateTableWidget->setColumnCount(columnHeaders.size());
    templateTableWidget->setHorizontalHeaderLabels(columnHeaders);

    // Загрузка данных таблицы
    QVector<QVector<QString>> tableData = dbHandler->getTemplateManager()->getTableData(templateId);
    templateTableWidget->setRowCount(tableData.size());

    for (int row = 0; row < tableData.size(); ++row) {
        for (int col = 0; col < tableData[row].size(); ++col) {
            QTableWidgetItem *item = new QTableWidgetItem(tableData[row][col]);
            //templateTableWidget->setItem(row, col, item);
            // Сохраняем HTML-содержимое в роли редактирования
            item->setData(Qt::EditRole, tableData[row][col]);
            item->setData(Qt::DisplayRole, tableData[row][col]);
            // При отображении можно выставить plain text (например, удалив HTML-теги) или оставить HTML, если делегат его обрабатывает
            //item->setText(tableData[row][col]);
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

//
int MainWindow::askForGroupCount() {
    bool ok;
    int numGroups = QInputDialog::getInt(this, "Настройка групп",
                                         "Введите количество групп для анализа:",
                                         1, 1, 10, 1, &ok);
    return ok ? numGroups : -1;  // Если пользователь нажал "Отмена", возвращаем -1
}

//
QSet<int> MainWindow::saveExpandedState() {
    QSet<int> expandedIds;
    for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i) {
        saveExpandedRecursive(categoryTreeWidget->topLevelItem(i), expandedIds);
    }
    return expandedIds;
}
void MainWindow::saveExpandedRecursive(QTreeWidgetItem *item, QSet<int> &expandedIds) {
    // Если элемент является категорией и развёрнут, сохраняем его id
    if (item->data(0, Qt::UserRole + 1).toBool() && item->isExpanded()) {
        expandedIds.insert(item->data(0, Qt::UserRole).toInt());
    }
    for (int i = 0; i < item->childCount(); ++i) {
        saveExpandedRecursive(item->child(i), expandedIds);
    }
}
void MainWindow::restoreExpandedState(const QSet<int> &expandedIds) {
    for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i) {
        restoreExpandedRecursive(categoryTreeWidget->topLevelItem(i), expandedIds);
    }
}
void MainWindow::restoreExpandedRecursive(QTreeWidgetItem *item, const QSet<int> &expandedIds) {
    if (item->data(0, Qt::UserRole + 1).toBool()) {
        int id = item->data(0, Qt::UserRole).toInt();
        if (expandedIds.contains(id)) {
            item->setExpanded(true);
        }
    }
    for (int i = 0; i < item->childCount(); ++i) {
        restoreExpandedRecursive(item->child(i), expandedIds);
    }
}

//
void MainWindow::updateAllSiblingNumbering(QTreeWidgetItem *parent) {
    QList<QTreeWidgetItem*> siblings;
    if (parent == nullptr) {
        for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i)
            siblings.append(categoryTreeWidget->topLevelItem(i));
        for (int i = 0; i < siblings.size(); i++) {
            QString newDisplay = QString::number(i + 1);
            siblings[i]->setText(0, newDisplay);
            int itemId = siblings[i]->data(0, Qt::UserRole).toInt();
            dbHandler->updateNumerationDB(itemId, -1, newDisplay, 1);
        }
    } else {
        for (int i = 0; i < parent->childCount(); ++i)
            siblings.append(parent->child(i));
        for (int i = 0; i < siblings.size(); i++) {
            QString newDisplay = parent->text(0) + "." + QString::number(i + 1);
            siblings[i]->setText(0, newDisplay);
            int itemId = siblings[i]->data(0, Qt::UserRole).toInt();
            int parentId = parent->data(0, Qt::UserRole).toInt();
            int depth = newDisplay.count('.') + 1;
            dbHandler->updateNumerationDB(itemId, parentId, newDisplay, depth);
        }
    }
}
void MainWindow::updateSiblingNumbering(QTreeWidgetItem *editedItem, int newNumber) {
    QTreeWidgetItem *parent = editedItem->parent();
    QList<QTreeWidgetItem*> siblings;
    if (parent == nullptr) {
        // Для корневых категорий
        for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i)
            siblings.append(categoryTreeWidget->topLevelItem(i));
    } else {
        // Для дочерних категорий
        for (int i = 0; i < parent->childCount(); ++i)
            siblings.append(parent->child(i));
    }
    int pos = siblings.indexOf(editedItem);

    // Обновляем редактируемый элемент
    QString newDisplay = (parent == nullptr) ? QString::number(newNumber)
                                             : parent->text(0) + "." + QString::number(newNumber);
    editedItem->setText(0, newDisplay);
    int itemId = editedItem->data(0, Qt::UserRole).toInt();
    int parentId = (parent == nullptr) ? -1 : parent->data(0, Qt::UserRole).toInt();
    int depth = (parent == nullptr) ? 1 : newDisplay.count('.') + 1;
    dbHandler->updateNumerationDB(itemId, parentId, newDisplay, depth);

    // Обновляем все последующие элементы
    for (int i = pos + 1; i < siblings.size(); i++) {
        int assignedNumber = newNumber + (i - pos);
        QString siblingDisplay = (parent == nullptr) ? QString::number(assignedNumber)
                                                     : parent->text(0) + "." + QString::number(assignedNumber);
        siblings[i]->setText(0, siblingDisplay);
        int sibId = siblings[i]->data(0, Qt::UserRole).toInt();
        int sibParentId = (parent == nullptr) ? -1 : parent->data(0, Qt::UserRole).toInt();
        int sibDepth = (parent == nullptr) ? 1 : siblingDisplay.count('.') + 1;
        dbHandler->updateNumerationDB(sibId, sibParentId, siblingDisplay, sibDepth);
    }
}
void MainWindow::updateNumbering() {
    // Для каждого корневого элемента вычисляем новый порядок и обновляем в БД
    for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *topLevelItem = categoryTreeWidget->topLevelItem(i);
        // Новый порядок для корневого элемента – i+1
        int newPos = i + 1;
        topLevelItem->setText(0, QString::number(newPos));
        int itemId = topLevelItem->data(0, Qt::UserRole).toInt();
        // Для корневых категорий parent_id = NULL (передаем -1) и depth = 1
        dbHandler->updateNumerationDB(itemId, -1, QString::number(newPos), 1);
        updateNumberingFromItem(topLevelItem);
    }
}
void MainWindow::updateNumberingFromItem(QTreeWidgetItem *parentItem) {
    if (!parentItem) return;

    QString parentNumber = parentItem->text(0);
    for (int i = 0; i < parentItem->childCount(); ++i) {
        QTreeWidgetItem *childItem = parentItem->child(i);
        int newPos = i + 1;
        QString newNumber = parentNumber + "." + QString::number(newPos);
        childItem->setText(0, newNumber);
        int itemId = childItem->data(0, Qt::UserRole).toInt();
        int parentId = parentItem->data(0, Qt::UserRole).toInt();
        int depth = newNumber.count('.') + 1;
        // Обновляем позицию в базе
        dbHandler->updateNumerationDB(itemId, parentId, newNumber, depth);
        // Если это категория – рекурсивно обновляем вложенные элементы
        bool isCategory = childItem->data(0, Qt::UserRole + 1).toBool();
        if (isCategory) {
            updateNumberingFromItem(childItem);
        }
    }

    // QString parentPath = parentItem->text(0);
    // for (int i = 0; i < parentItem->childCount(); ++i) {
    //     QTreeWidgetItem *childItem = parentItem->child(i);

    //     QString newNumeration = parentPath + "." + QString::number(i + 1);
    //     childItem->setText(0, newNumeration);
    //     updateNumberingFromItem(childItem);
    // }
}
void MainWindow::numberChildItems(QTreeWidgetItem *parent, const QString &parentPath) {
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        QString newPath = parentPath + "." + QString::number(i + 1);
        child->setText(0, newPath);

        int itemId = child->data(0, Qt::UserRole).toInt();
        int parentId = parent->data(0, Qt::UserRole).toInt();
        int depth = newPath.count('.') + 1;

        dbHandler->updateNumerationDB(itemId, parentId, newPath, depth);

        numberChildItems(child, newPath);  // Рекурсивный вызов
    }
}

//
void MainWindow::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem* selectedItem = categoryTreeWidget->itemAt(pos);
    QMenu contextMenu(this);

    if (selectedItem) {
        // Считываем "isCategory" из UserRole + 1
        bool isCategory = selectedItem->data(0, Qt::UserRole + 1).toBool();
        if (isCategory) {
            contextMenu.addAction("Добавить категорию", this, [this]() {
                createCategoryOrTemplate(true);
            });
            contextMenu.addAction("Добавить шаблон", this, [this]() {
                createCategoryOrTemplate(false);
            });
            contextMenu.addAction("Удалить категорию", this, &MainWindow::deleteCategoryOrTemplate);
        } else {
            contextMenu.addAction("Удалить шаблон", this, &MainWindow::deleteCategoryOrTemplate);
        }
    } else {
        // Клик вне элементов - добавляем корневую категорию
        categoryTreeWidget->clearSelection();
        contextMenu.addAction("Добавить категорию", this, [this]() {
            createCategoryOrTemplate(true);
        });
    }

    contextMenu.exec(categoryTreeWidget->viewport()->mapToGlobal(pos));
}
void MainWindow::createCategoryOrTemplate(bool isCategory) {
    QString title = isCategory ? "Создать категорию" : "Создать шаблон";
    QString prompt = isCategory ? "Введите название категории:" : "Введите название шаблона:";
    QString name = QInputDialog::getText(this, title, prompt);
    if (name.isEmpty()) return;

    QList<QTreeWidgetItem *> selectedItems = categoryTreeWidget->selectedItems();
    QTreeWidgetItem* parentItem = selectedItems.isEmpty() ? nullptr : selectedItems.first();
    int parentId = -1; // -1 означает корневой уровень (NULL в БД)

    // Если выбран элемент, то проверяем его тип:
    if (parentItem) {
        bool parentIsCategory = parentItem->data(0, Qt::UserRole + 1).toBool();
        if (isCategory) {
            parentId = parentIsCategory ? parentItem->data(0, Qt::UserRole).toInt() : -1;
        } else {
            // Создание шаблона возможно только внутри категории
            if (!parentIsCategory) {
                QMessageBox::warning(this, "Ошибка", "Шаблон можно создать только внутри категории.");
                return;
            }
            parentId = parentItem->data(0, Qt::UserRole).toInt();
        }
    } else {
        // Если ничего не выбрано, то шаблон создать нельзя
        if (!isCategory) {
            QMessageBox::warning(this, "Ошибка", "Выберите категорию для создания шаблона.");
            return;
        }
    }

    int projectId = projectComboBox->currentData().toInt(); // Получение текущего проекта
    if (projectId == 0) {
        QMessageBox::warning(this, "Ошибка", "Выберите проект перед созданием.");
        return;
    }

    bool success = false;
    if (isCategory) {
        success = dbHandler->getCategoryManager()->createCategory(name, parentId, projectId);
    } else {
        success = dbHandler->getTemplateManager()->createTemplate(parentId, name);
    }

    if (!success) {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать элемент в базе данных.");
        return;
    }

    // Сохраняем id родительской категории, если элемент создавался вложенным
    int savedParentId = parentId;
    loadCategoriesAndTemplates();

    // Если создавался вложенный элемент, найдем родительский узел и развёрнем его
    if (savedParentId != -1) {
        QTreeWidgetItem* parentNode = findItemById(nullptr, savedParentId);
        if (parentNode)
            parentNode->setExpanded(true);
    }
}
void MainWindow::deleteCategoryOrTemplate()
{
    QTreeWidgetItem* selectedItem = categoryTreeWidget->currentItem();
    if (!selectedItem) return;

    int itemId       = selectedItem->data(0, Qt::UserRole).toInt();
    bool isCategory  = selectedItem->data(0, Qt::UserRole + 1).toBool();

    if (isCategory) {
        // Диалог для удаления категории
        QMessageBox msgBox;
        msgBox.setWindowTitle("Удаление категории");
        msgBox.setText(QString("Категория \"%1\" будет удалена.").arg(selectedItem->text(1)));
        msgBox.setInformativeText("Выберите действие:");
        QPushButton *deleteButton = msgBox.addButton("Удалить вместе со всем содержимым",
                                                     QMessageBox::DestructiveRole);
        QPushButton *unpackButton = msgBox.addButton("Распаковать (поднять вложенные)",
                                                     QMessageBox::AcceptRole);
        QPushButton *cancelButton = msgBox.addButton(QMessageBox::Cancel);

        msgBox.exec();

        if (msgBox.clickedButton() == cancelButton) {
            return;
        }
        else if (msgBox.clickedButton() == deleteButton) {
            // Удаляем вместе с дочерними (в БД)
            dbHandler->getCategoryManager()->deleteCategory(itemId, /*deleteChildren=*/true);
            loadCategoriesAndTemplates();
            //updateNumbering();
        }
        else if (msgBox.clickedButton() == unpackButton) {
            // // Распаковка: переносим все дочерние элементы на уровень родителя
            // QTreeWidgetItem* parentItem = selectedItem->parent();
            // while (selectedItem->childCount() > 0) {
            //     QTreeWidgetItem* child = selectedItem->takeChild(0);
            //     int childId = child->data(0, Qt::UserRole).toInt();
            //     int newParentId = (parentItem == nullptr) ? -1 : parentItem->data(0, Qt::UserRole).toInt();
            //     if (!dbHandler->updateParentId(childId, newParentId)) {
            //         qDebug() << "Ошибка обновления parent_id для элемента с ID:" << childId;
            //     }
            //     // Перемещаем узел в дерево: если родителя нет – добавляем как корневой
            //     if (parentItem == nullptr) {
            //         categoryTreeWidget->addTopLevelItem(child);
            //     } else {
            //         parentItem->addChild(child);
            //     }
            // }
            // loadCategoriesAndTemplates();  // Обновляем дерево после распаковки

            // "Распаковать" = перенести всех детей на верхний уровень (parent_id=NULL)
            // 1) В БД: для каждого дочернего category/template делаем update parent_id = NULL
            // 2) В дереве: переносим их как top-level
            while (selectedItem->childCount() > 0) {
                QTreeWidgetItem* child = selectedItem->takeChild(0);
                int childId = child->data(0, Qt::UserRole).toInt();
                bool childIsCategory = child->data(0, Qt::UserRole + 1).toBool();

                if (childIsCategory) {
                    dbHandler->updateParentId(childId, /*newParent=*/-1);
                } else {
                    dbHandler->updateParentId(childId, /*newParent=*/-1);
                }
                // Перенести в дерево как top-level
                categoryTreeWidget->addTopLevelItem(child);
            }
            // Теперь удаляем саму категорию без детей
            dbHandler->getCategoryManager()->deleteCategory(itemId, /*deleteChildren=*/false);
            loadCategoriesAndTemplates();
            // //updateNumbering();
        }
    }
    else {
        // Это шаблон
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Удаление шаблона",
            QString("Вы действительно хотите удалить шаблон \"%1 - %2\"?")
                .arg(selectedItem->text(0))
                .arg(selectedItem->text(1)),
            QMessageBox::Yes | QMessageBox::No
            );
        if (reply == QMessageBox::Yes) {
            bool ok = dbHandler->getTemplateManager()->deleteTemplate(itemId);
            if (!ok) {
                QMessageBox::warning(this, "Ошибка",
                                     "Не удалось удалить шаблон из базы данных!");
            }
            loadCategoriesAndTemplates();
        }
    }
}

//
QTreeWidgetItem* MainWindow::findItemById(QTreeWidgetItem* parent, int id) {
    if (!parent) {
        for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = findItemById(categoryTreeWidget->topLevelItem(i), id);
            if (item)
                return item;
        }
    } else {
        if (parent->data(0, Qt::UserRole).toInt() == id)
            return parent;
        for (int i = 0; i < parent->childCount(); ++i) {
            QTreeWidgetItem* item = findItemById(parent->child(i), id);
            if (item)
                return item;
        }
    }
    return nullptr;
}

//
void MainWindow::editHeader(int column) {
    if (column < 0 || !templateTableWidget) {
        qDebug() << "Некорректный столбец для редактирования.";
        return;
    }

    QList<QTreeWidgetItem *> selectedItems = categoryTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        qDebug() << "Нет выбранного шаблона.";
        return;
    }

    int templateId = selectedItems.first()->data(0, Qt::UserRole).toInt();

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
void MainWindow::addRowOrColumn(const QString &type) {
    // Проверка выбранного шаблона
    QList<QTreeWidgetItem *> selectedItems = categoryTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        qDebug() << "Нет выбранного шаблона.";
        return;
    }

    int templateId = selectedItems.first()->data(0, Qt::UserRole).toInt();
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
void MainWindow::deleteRowOrColumn(const QString &type) {
    int currentIndex = (type == "row") ? templateTableWidget->currentRow() : templateTableWidget->currentColumn();
    if (currentIndex < 0) {
        qDebug() << QString("Не выбран %1 для удаления.").arg(type == "row" ? "строка" : "столбец");
        return;
    }

    // Проверяем выбранный шаблон
    QList<QTreeWidgetItem *> selectedItems = categoryTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        qDebug() << "Не выбран шаблон для изменения.";
        return;
    }

    int templateId = selectedItems.first()->data(0, Qt::UserRole).toInt();

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
void MainWindow::saveTableData() {
    QList<QTreeWidgetItem *> selectedItems = categoryTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        qDebug() << "Нет выбранного шаблона.";
        return;
    }

    int templateId = selectedItems.first()->data(0, Qt::UserRole).toInt();

    QVector<QVector<QString>> tableData;
    QVector<QString> columnHeaders;

    // Сохранение данных строк
    for (int row = 0; row < templateTableWidget->rowCount(); ++row) {
        QVector<QString> rowData;
        for (int col = 0; col < templateTableWidget->columnCount(); ++col) {
            QTableWidgetItem *item = templateTableWidget->item(row, col);
            //rowData.append(item ? item->text() : "");
            // Используем данные из роли редактирования, где хранится HTML
            rowData.append(item ? item->data(Qt::EditRole).toString() : "");
        }
        tableData.append(rowData);
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
    if (!dbHandler->getTableManager()->saveDataTableTemplate(templateId, columnHeaders, tableData)) {
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
