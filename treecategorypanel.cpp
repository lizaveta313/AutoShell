#include "treecategorypanel.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QMenu>
#include <QDebug>
#include <QPushButton>
#include <QTreeWidgetItem>
#include <QHeaderView>

TreeCategoryPanel::TreeCategoryPanel(DatabaseHandler *dbHandler, QWidget *parent)
    : QWidget(parent)
    , dbHandler(dbHandler) {
    // Создаем сам виджет дерева
    categoryTreeWidget = new MyTreeWidget(this);
    categoryTreeWidget->setColumnCount(2);
    categoryTreeWidget->setHeaderLabels({"№", "Название"});
    categoryTreeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    categoryTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    categoryTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    categoryTreeWidget->setEditTriggers(QAbstractItemView::DoubleClicked);

    // Подключаем сигналы
    connect(categoryTreeWidget, &MyTreeWidget::dropped,
            this, &TreeCategoryPanel::updateNumbering);
    connect(categoryTreeWidget, &QTreeWidget::itemClicked,
            this, &TreeCategoryPanel::onCategoryOrTemplateSelected);
    connect(categoryTreeWidget, &QTreeWidget::itemDoubleClicked,
            this, &TreeCategoryPanel::onCategoryOrTemplateDoubleClickedForEditing);
    connect(categoryTreeWidget, &QWidget::customContextMenuRequested,
            this, &TreeCategoryPanel::showTreeContextMenu);

    // Размещаем виджет в layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(categoryTreeWidget);
    setLayout(layout);
}

TreeCategoryPanel::~TreeCategoryPanel() {}

void TreeCategoryPanel::loadCategoriesAndTemplatesForProject(int projectId) {
    selectedProjectId = projectId;    // обновляем поле
    loadCategoriesAndTemplates();     // вызываем ваш метод, который строит дерево
}

//  Загрузки
void TreeCategoryPanel::loadCategoriesAndTemplates() {
    QSet<int> expandedIds = saveExpandedState();
    categoryTreeWidget->clear();

    int projectId = selectedProjectId;

    loadItemsForCategory(projectId, QVariant(), nullptr, QString());
    restoreExpandedState(expandedIds);
}
void TreeCategoryPanel::loadItemsForCategory(int projectId,
                                             const QVariant &parentId,
                                             QTreeWidgetItem *parentItem,
                                             const QString &parentPath) {
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
void TreeCategoryPanel::loadCategoriesForProject(int projectId,
                                                 QTreeWidgetItem *parentItem,
                                                 const QString &parentPath) {
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
void TreeCategoryPanel::loadCategoriesForCategory(const Category &category,
                                                  QTreeWidgetItem *parentItem,
                                                  const QString &parentPath) {
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
void TreeCategoryPanel::loadTemplatesForCategory(int categoryId,
                                                 QTreeWidgetItem *parentItem,
                                                 const QString &parentPath) {
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


//  Обработчики кликов

void TreeCategoryPanel::onCategoryOrTemplateSelected(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column);

    if (!item) return;

    if (item->parent() == nullptr) {
        // Это категория
        item->setExpanded(!item->isExpanded()); // Раскрываем или сворачиваем список шаблонов
    } else {
        // Это шаблон
        int templateId = item->data(0, Qt::UserRole).toInt();

        emit templateSelected(templateId);
    }
}
void TreeCategoryPanel::onCategoryOrTemplateDoubleClickedForEditing(QTreeWidgetItem *item, int column) {
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
void TreeCategoryPanel::onCheckButtonClicked() {
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

//  Нумерация

void TreeCategoryPanel::updateAllSiblingNumbering(QTreeWidgetItem *parent) {
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
void TreeCategoryPanel::updateSiblingNumbering(QTreeWidgetItem *editedItem, int newNumber) {
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
void TreeCategoryPanel::updateNumbering() {
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
void TreeCategoryPanel::updateNumberingFromItem(QTreeWidgetItem *parentItem) {
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
}
void TreeCategoryPanel::numberChildItems(QTreeWidgetItem *parent, const QString &parentPath) {
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

//  Контекстное меню

void TreeCategoryPanel::showTreeContextMenu(const QPoint &pos) {
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
            contextMenu.addAction("Удалить категорию", this, &TreeCategoryPanel::deleteCategoryOrTemplate);
        } else {
            contextMenu.addAction("Удалить шаблон", this, &TreeCategoryPanel::deleteCategoryOrTemplate);
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
void TreeCategoryPanel::createCategoryOrTemplate(bool isCategory) {
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

    int projectId = selectedProjectId;
    if (projectId == 0) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Выберите проект перед созданием."));
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
void TreeCategoryPanel::deleteCategoryOrTemplate() {
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
QTreeWidgetItem* TreeCategoryPanel::findItemById(QTreeWidgetItem* parent, int id) {
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

//  Сохранение / восстановление состояния
QSet<int> TreeCategoryPanel::saveExpandedState() {
    QSet<int> expandedIds;
    for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i) {
        saveExpandedRecursive(categoryTreeWidget->topLevelItem(i), expandedIds);
    }
    return expandedIds;
}
void TreeCategoryPanel::saveExpandedRecursive(QTreeWidgetItem *item, QSet<int> &expandedIds) {
    // Если элемент является категорией и развёрнут, сохраняем его id
    if (item->data(0, Qt::UserRole + 1).toBool() && item->isExpanded()) {
        expandedIds.insert(item->data(0, Qt::UserRole).toInt());
    }
    for (int i = 0; i < item->childCount(); ++i) {
        saveExpandedRecursive(item->child(i), expandedIds);
    }
}
void TreeCategoryPanel::restoreExpandedState(const QSet<int> &expandedIds) {
    for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i) {
        restoreExpandedRecursive(categoryTreeWidget->topLevelItem(i), expandedIds);
    }
}
void TreeCategoryPanel::restoreExpandedRecursive(QTreeWidgetItem *item, const QSet<int> &expandedIds) {
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
