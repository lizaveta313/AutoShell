#include "mainwindow.h"
#include "nonmodaldialogue.h"
#include <QSplitter>
#include <QInputDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QMenu>

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
    connect(projectComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onProjectSelected);

    categoryTreeWidget = new QTreeWidget(this);
    categoryTreeWidget->setColumnCount(2);
    categoryTreeWidget->setHeaderLabels({"№", "Название"});
    categoryTreeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    categoryTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    categoryTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    categoryTreeWidget->setEditTriggers(QAbstractItemView::DoubleClicked);
    connect(categoryTreeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onCategoryOrTemplateSelected);
    connect(categoryTreeWidget, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onCategoryOrTemplateDoubleClickedForEditing);
    connect(categoryTreeWidget, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(projectComboBox);
    leftLayout->addWidget(categoryTreeWidget);

    // Таблица
    templateTableWidget = new QTableWidget(this);
    connect(templateTableWidget->horizontalHeader(), &QHeaderView::sectionDoubleClicked, this, &MainWindow::editHeader);

    // Заметки
    notesField = new QTextEdit(this);
    notesProgrammingField = new QTextEdit(this);

    // Кнопки для работы с таблицей
    addRowButton = new QPushButton("Добавить строку", this);
    addColumnButton = new QPushButton("Добавить столбец", this);
    deleteRowButton = new QPushButton("Удалить строку", this);
    deleteColumnButton = new QPushButton("Удалить столбец", this);
    saveButton = new QPushButton("Сохранить", this);
    checkButton = new QPushButton("Утвердить", this);

    // Подключение сигналов кнопок
    connect(addRowButton, &QPushButton::clicked, this, [this]() { MainWindow::addRowOrColumn("row"); });
    connect(addColumnButton, &QPushButton::clicked, this, [this]() { MainWindow::addRowOrColumn("column"); });
    connect(deleteRowButton, &QPushButton::clicked, this, [this]() { MainWindow::deleteRowOrColumn("row"); });
    connect(deleteColumnButton, &QPushButton::clicked, this, [this]() { MainWindow::deleteRowOrColumn("column"); });
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveTableData);

    connect(checkButton, &QPushButton::clicked, this, &MainWindow::onCheckButtonClicked);

    // Правая часть: Таблица и нижняя часть
    QVBoxLayout *rightLayout = new QVBoxLayout;

    // Таблица занимает большую часть
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

    // Справа: заметки и программные заметки
    QVBoxLayout *notesLayout = new QVBoxLayout;
    notesLayout->addWidget(notesField);
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
    mainLayout->addLayout(leftLayout, 1);   // Левый блок (ТЛГ)
    mainLayout->addLayout(rightLayout, 4); // Правый блок (Таблица и нижняя часть)

    // Настройка центрального виджета
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    // Настройки окна
    setWindowTitle("AutoShell");
    resize(1000, 600);
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
            item->setText(column, newNumeration);
            updateNumberingFromItem(item); // Автоматическое обновление
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
            if (item->parent() == nullptr) {
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
    // Проверяем, выбран ли проект
    QVariant projectData = projectComboBox->itemData(index);
    if (!projectData.isValid()) {
        categoryTreeWidget->clear(); // Очищаем дерево, если проект не выбран
        return;
    }

    int projectId = projectData.toInt();
    categoryTreeWidget->clear(); // Очищаем дерево перед загрузкой новых данных
    loadCategoriesForProject(projectId, nullptr, QString());
}

void MainWindow::loadCategoriesAndTemplates() {
    int projectId = projectComboBox->currentData().toInt();
    categoryTreeWidget->clear();
    loadCategoriesForProject(projectId, nullptr, QString());
}

void MainWindow::loadCategoriesForProject(int projectId, QTreeWidgetItem *parentItem, const QString &parentPath) {
    QVector<Category> categories = dbHandler->getCategoryManager()->getCategoriesByProject(projectId);

    for (const Category &category : categories) {
        QTreeWidgetItem *categoryItem = nullptr;

        if (parentItem == nullptr) {
            categoryItem = new QTreeWidgetItem(categoryTreeWidget);
        } else {
            categoryItem = new QTreeWidgetItem(parentItem);
        }

        categoryItem->setText(1, category.name);
        categoryItem->setData(0, Qt::UserRole, QVariant::fromValue(category.categoryId));

        QString numeration = parentPath.isEmpty() ? QString::number(category.position) : parentPath + "." + QString::number(category.position);
        categoryItem->setText(0, numeration);

        loadCategoriesForCategory(category, categoryItem, numeration);
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
            templateTableWidget->setItem(row, col, item);
        }
    }

    // Загрузка заметок и программных заметок
    QString notes = dbHandler->getTemplateManager()->getNotesForTemplate(templateId);
    QString programmingNotes = dbHandler->getTemplateManager()->getProgrammingNotesForTemplate(templateId);

    notesField->setText(notes);
    notesProgrammingField->setText(programmingNotes);

    qDebug() << "Шаблон таблицы с ID" << templateId << "загружен.";
}

//
void MainWindow::dropEvent(QDropEvent *event) {
    MainWindow::dropEvent(event);
    updateNumbering();  // Обновление после перетаскивания
}

void MainWindow::updateNumbering() {
    for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *topLevelItem = categoryTreeWidget->topLevelItem(i);

        // Обновляем нумерацию для корневых элементов
        QString newNumeration = QString::number(i + 1);
        topLevelItem->setText(0, newNumeration);

        int itemId = topLevelItem->data(0, Qt::UserRole).toInt();
        int parentId = -1; // У корневых элементов нет родителя
        int depth = 1;

        // Обновляем в базе данных
        dbHandler->updateNumerationDB(itemId, parentId, newNumeration, depth);

        // Рекурсивно обновляем вложенные элементы
        updateNumberingFromItem(topLevelItem);
    }
}

void MainWindow::updateNumberingFromItem(QTreeWidgetItem *parentItem) {
    if (!parentItem) return;

    QString parentPath = parentItem->text(0);
    for (int i = 0; i < parentItem->childCount(); ++i) {
        QTreeWidgetItem *childItem = parentItem->child(i);

        QString newNumeration = parentPath + "." + QString::number(i + 1);
        childItem->setText(0, newNumeration);

        int itemId = childItem->data(0, Qt::UserRole).toInt();
        int parentId = parentItem->data(0, Qt::UserRole).toInt();
        int depth = newNumeration.count('.') + 1;

        bool isCategory = (childItem->childCount() > 0); // Если есть дети, это категория

        if (!dbHandler->updateNumerationDB(itemId, parentId, newNumeration, depth)) {
            qDebug() << "Ошибка обновления нумерации для элемента ID" << itemId;
        }

        if (isCategory) {
            updateNumberingFromItem(childItem); // Рекурсивно обновляем только для категорий
        }
    }
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

    QTreeWidgetItem* parentItem = categoryTreeWidget->currentItem();
    int parentId = -1; // -1 интерпретируется как NULL в БД
    if (parentItem) {
        parentId = parentItem->data(0, Qt::UserRole).toInt();
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

    loadCategoriesAndTemplates(); // Обновляем дерево категорий и шаблонов
}

void MainWindow::deleteCategoryOrTemplate()
{
    QTreeWidgetItem* selectedItem = categoryTreeWidget->currentItem();
    if (!selectedItem) return;

    int itemId       = selectedItem->data(0, Qt::UserRole).toInt();
    bool isCategory  = selectedItem->data(0, Qt::UserRole + 1).toBool();

    if (isCategory) {
        // Диалог "Удалить / Распаковать / Отмена"
        QMessageBox msgBox;
        msgBox.setWindowTitle("Удаление категории");
        msgBox.setText(QString("Категория \"%1\" будет удалена.").arg(selectedItem->text(0)));
        msgBox.setInformativeText("Выберите действие:");
        QPushButton *deleteButton = msgBox.addButton("Удалить вместе со всем содержимым",
                                                     QMessageBox::DestructiveRole);
        QPushButton *unpackButton = msgBox.addButton("Распаковать (поднять дочерние)",
                                                     QMessageBox::AcceptRole);
        QPushButton *cancelButton = msgBox.addButton(QMessageBox::Cancel);

        msgBox.exec();

        if (msgBox.clickedButton() == cancelButton) {
            return;
        }
        else if (msgBox.clickedButton() == deleteButton) {
            // Удаляем вместе с дочерними (в БД)
            dbHandler->getCategoryManager()->deleteCategory(itemId, /*deleteChildren=*/true);
            // Обновляем дерево
            loadCategoriesAndTemplates();
        }
        else if (msgBox.clickedButton() == unpackButton) {
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
        }
    }
    else {
        // Это шаблон
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Удаление шаблона",
            QString("Вы действительно хотите удалить шаблон \"%1\"?").arg(selectedItem->text(0)),
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
    }
    // Если добавляем строку
    else if (type == "row") {
        newOrder = templateTableWidget->rowCount();
    }

    // Добавление строки или столбца в базу данных
    if (!dbHandler->getTableManager()->createRowOrColumn(templateId, type, header, newOrder)) {
        qDebug() << QString("Ошибка добавления %1 в базу данных.").arg(type);
        return;
    }

    // Обновление интерфейса
    if (type == "row") {
        templateTableWidget->insertRow(newOrder);
        for (int col = 0; col < templateTableWidget->columnCount(); ++col) {
            templateTableWidget->setItem(newOrder, col, new QTableWidgetItem());
        }
        qDebug() << "Строка добавлена.";
    }
    else if (type == "column") {
        templateTableWidget->insertColumn(newOrder);
        templateTableWidget->setHorizontalHeaderItem(newOrder, new QTableWidgetItem(header));
        for (int row = 0; row < templateTableWidget->rowCount(); ++row) {
            templateTableWidget->setItem(row, newOrder, new QTableWidgetItem());
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
            rowData.append(item ? item->text() : "");
        }
        tableData.append(rowData);
    }

    // Сохранение заголовков столбцов
    for (int col = 0; col < templateTableWidget->columnCount(); ++col) {
        QTableWidgetItem *headerItem = templateTableWidget->horizontalHeaderItem(col);
        columnHeaders.append(headerItem ? headerItem->text() : "");
    }

    // Получение заметок и программных заметок из соответствующих полей
    QString notes = notesField->toPlainText();
    QString programmingNotes = notesProgrammingField->toPlainText();

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
