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
    categoryTreeWidget->setHeaderLabels({"№", "Title"});
    categoryTreeWidget->header()->setSectionResizeMode(QHeaderView::Interactive);
    categoryTreeWidget->header()->setStretchLastSection(true);
    categoryTreeWidget->setColumnWidth(0, 30);
    categoryTreeWidget->setMouseTracking(true);
    categoryTreeWidget->viewport()->setMouseTracking(true);
    categoryTreeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    categoryTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    categoryTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    categoryTreeWidget->setEditTriggers(QAbstractItemView::DoubleClicked);

    // Подключаем сигналы
    connect(categoryTreeWidget, &MyTreeWidget::dropped, this, [=](){
        updateHierarchy();
        updateNumbering();
    });
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

void TreeCategoryPanel::clearAll() {
    categoryTreeWidget->clear();
    selectedProjectId = 0;  // Или -1, если так принято
}

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
void TreeCategoryPanel::extracted(QVector<CombinedItem> &items,
                                  QVector<Category> &categories) {
    for (const Category &cat : categories) {
        CombinedItem ci;
        ci.isCategory = true;
        ci.position = cat.position;
        ci.id = cat.categoryId;
        ci.name = cat.name;
        // ci.category = cat;
        items.append(ci);
    }
}
void TreeCategoryPanel::loadItemsForCategory(int projectId,
                                             const QVariant &parentId,
                                             QTreeWidgetItem *parentItem,
                                             const QString &parentPath) {
    QVector<CombinedItem> items;

    // Если parentId равен NULL – это корневой уровень, шаблоны не существуют
    if (parentId.isNull()) {
        QVector<Category> categories =
            dbHandler->getCategoryManager()->getCategoriesByProjectAndParent(
            projectId, parentId);
        extracted(items, categories);
    } else {
        // Для выбранной категории – получаем подкатегории
        QVector<Category> subCategories =
            dbHandler->getCategoryManager()->getCategoriesByProjectAndParent(
            projectId, parentId);
        for (const Category &cat : subCategories) {
            CombinedItem ci;
            ci.isCategory = true;
            ci.position = cat.position;
            ci.id = cat.categoryId;
            ci.name = cat.name;
            // ci.category = cat;
            items.append(ci);
        }
        // И шаблоны, привязанные к данной категории
        QVector<Template> templates =
            dbHandler->getTemplateManager()->getTemplatesForCategory(
            parentId.toInt());
        for (const Template &tmpl : templates) {
            CombinedItem ci;
            ci.isCategory = false;
            ci.position = tmpl.position;
            ci.id = tmpl.templateId;
            ci.name = tmpl.name;
            // ci.templ = tmpl;
            items.append(ci);
        }
    }

    // Сортируем по полю position
    std::sort(items.begin(), items.end(),
              [](const CombinedItem &a, const CombinedItem &b) {
        return a.position < b.position;
    });

    // Создаем узлы дерева; для отображения используем сохранённое значение
    // position
    for (const CombinedItem &ci : items) {
        QTreeWidgetItem *item = (parentItem == nullptr)
                                    ? new QTreeWidgetItem(categoryTreeWidget)
                                    : new QTreeWidgetItem(parentItem);
        item->setText(1, ci.name);

        // Отображаем номер, используя сохранённое значение из БД:
        QString nodeNumber = QString::number(ci.position);
        QString numeration =
            parentPath.isEmpty() ? nodeNumber : parentPath + "." + nodeNumber;
        // QString numeration = parentPath.isEmpty() ? QString::number(index + 1)
        //                                           : parentPath + "." +
        //                                           QString::number(index + 1);
        item->setText(0, numeration);
        if (ci.isCategory) {
            item->setData(0, Qt::UserRole, ci.id);
            item->setData(0, Qt::UserRole + 1, true); // помечаем как категория
            // Рекурсивно загружаем вложенные элементы для этой категории
            loadItemsForCategory(projectId, ci.id, item, numeration);
        } else {
            item->setData(0, Qt::UserRole, ci.id);
            item->setData(0, Qt::UserRole + 1, false); // помечаем как шаблон
            bool isDyn = dbHandler->getTemplateManager()->isTemplateDynamic(ci.id);
            QString tip = isDyn ? tr("Dynamic template") : tr("Static template");
            // повесим его на колонку с именем
            item->setToolTip(0, tip);
            item->setToolTip(1, tip);
            // Для шаблонов можно задать, например, красный цвет названия
            bool approved = dbHandler->getTemplateManager()->isTemplateApproved(ci.id);
            item->setForeground(1, approved ? QBrush(Qt::darkGreen) : QBrush(Qt::red));
        }
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

        bool approved = dbHandler->getTemplateManager()->isTemplateApproved(tmpl.templateId);
        templateItem->setForeground(1, approved ? QBrush(Qt::darkGreen) : QBrush(Qt::red));
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

    if (column == 1) { // Редактирование названия
        QString currentName = item->text(column);

        bool ok;
        QString newName = QInputDialog::getText(this, "Editing the name",
                                                "Enter a new name:", QLineEdit::Normal,
                                                currentName, &ok);

        if (ok && !newName.isEmpty() && newName != currentName) {
            item->setText(column, newName);

            // Сохранение изменений в базе данных
            if (item->data(0, Qt::UserRole + 1).toBool()) {
                int categoryId = item->data(0, Qt::UserRole).toInt();
                dbHandler->getCategoryManager()->updateCategory(categoryId, newName);
            } else {
                int templateId = item->data(0, Qt::UserRole).toInt();
                dbHandler->getTemplateManager()->updateTemplate(templateId, newName, std::nullopt, std::nullopt, std::nullopt);
            }
        }
    }
}
void TreeCategoryPanel::onCheckButtonClicked() {
    QTreeWidgetItem *selectedItem = categoryTreeWidget->currentItem();
    if (!selectedItem) {
        qDebug() << "Нет выбранного элемента для утверждения.";
        return;
    }
    // Берём реальный статус из БД — и красим в соответствии с ним
    const int templateId = selectedItem->data(0, Qt::UserRole).toInt();
    bool approved = dbHandler->getTemplateManager()->isTemplateApproved(templateId);
    selectedItem->setForeground(1, approved ? QBrush(Qt::darkGreen) : QBrush(Qt::red));
}

//  Нумерация

void TreeCategoryPanel::updateNumbering() {
    for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = categoryTreeWidget->topLevelItem(i);
        int pos = i + 1;
        bool isCat = item->data(0, Qt::UserRole + 1).toBool();
        int id = item->data(0, Qt::UserRole).toInt();
        // Сохраняем только position
        if (isCat)
            dbHandler->getCategoryManager()->updateCategoryFields(id, std::nullopt, pos, std::nullopt);
        else
            dbHandler->getTemplateManager()->updateTemplatePosition(id, pos);
            item->setText(0, QString::number(pos));
            renumberChildren(item);
    }
}
void TreeCategoryPanel::renumberChildren(QTreeWidgetItem *parent) {
    QString prefix = parent->text(0);
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        int pos = i + 1;
        QString num = prefix + "." + QString::number(pos);
        bool isCat = child->data(0, Qt::UserRole + 1).toBool();
        int id = child->data(0, Qt::UserRole).toInt();
        if (isCat)
            dbHandler->getCategoryManager()->updateCategoryFields(id, std::nullopt, pos, std::nullopt);
        else
            dbHandler->getTemplateManager()->updateTemplatePosition(id, pos);
        child->setText(0, num);
        if (isCat)
            renumberChildren(child);
    }
}
void TreeCategoryPanel::changeItemPosition() {
    QTreeWidgetItem *item = categoryTreeWidget->currentItem();
    if (!item) return;

    // 1. Собираем соседей и индекс текущего
    QList<QTreeWidgetItem*> siblings;
    QTreeWidgetItem *parent = item->parent();
    auto collect = [&](QTreeWidgetItem *p){
        for (int i = 0; i < (p ? p->childCount()
                               : categoryTreeWidget->topLevelItemCount()); ++i)
            siblings << (p ? p->child(i)
                           : categoryTreeWidget->topLevelItem(i));
    };
    collect(parent);
    int idx = siblings.indexOf(item);

    // 2. Спрашиваем новый номер
    bool ok = false;
    int newPos = QInputDialog::getInt(this, tr("Change position"),
                                      tr("Enter new position:"),
                                      idx+1, 1, INT_MAX, 1, &ok);
    if (!ok || newPos == idx+1) return;

    // 3. Префикс ― всё до последней точки
    const QString full = item->text(0);
    const int lastDot  = full.lastIndexOf('.');
    const QString prefix = lastDot == -1 ? QString() : full.left(lastDot);

    auto setNumber = [&](QTreeWidgetItem *n, int pos){
        const bool isCat = n->data(0,Qt::UserRole+1).toBool();
        const int  id    = n->data(0,Qt::UserRole).toInt();
        const QString num = prefix.isEmpty()
                                ? QString::number(pos)
                                : prefix + '.' + QString::number(pos);
        n->setText(0, num);
        if (isCat)
            dbHandler->getCategoryManager()->updateCategoryFields(id, std::nullopt, pos, std::nullopt);
        else
            dbHandler->getTemplateManager()->updateTemplatePosition(id,pos);
        if (isCat) renumberChildren(n);          // рекурсивно для вложенных
    };

    // 4. Ставим новый номер выбранному элементу
    setNumber(item, newPos);

    // 5. Перенумеруем все последующие по порядку
    int current = newPos;
    for (int i = idx+1; i < siblings.size(); ++i)
        setNumber(siblings[i], ++current);
}
void TreeCategoryPanel::updateHierarchy() {
    // Пройтись по всем топ-левел категориям
    for (int i = 0; i < categoryTreeWidget->topLevelItemCount(); ++i) {
        updateItemHierarchy(categoryTreeWidget->topLevelItem(i), -1, 0);
    }
}
void TreeCategoryPanel::updateItemHierarchy(QTreeWidgetItem* item, int newParentId, int depth) {
    bool isCat = item->data(0, Qt::UserRole + 1).toBool();
    int id     = item->data(0, Qt::UserRole).toInt();

    QTreeWidgetItem* parent = item->parent();
    int pos;
    if (parent) {
        pos = parent->indexOfChild(item) + 1;
    } else {
        pos = categoryTreeWidget->indexOfTopLevelItem(item) + 1;
    }

    if (isCat) {
        // Для категорий — одновременно parent_id, position и depth
        std::optional<int> parentOpt = (newParentId < 0
                                            ? std::nullopt
                                            : std::optional<int>(newParentId));
        std::optional<int> posOpt    = pos;
        std::optional<int> depthOpt  = depth;
        dbHandler->getCategoryManager()
            ->updateCategoryFields(id, parentOpt, posOpt, depthOpt);
    } else {
        // Для шаблонов — только перенос в новую категорию и позицию
        dbHandler->getTemplateManager()->updateTemplateCategory(id, newParentId);
        dbHandler->getTemplateManager()->updateTemplatePosition(id, pos);
    }

    // 3) Передаём дальше в рекурсию: если это категория, то она — новый parent
    int childParent = isCat ? id : newParentId;
    for (int i = 0; i < item->childCount(); ++i) {
        updateItemHierarchy(item->child(i), childParent, depth + 1);
    }
}

//  Контекстное меню

void TreeCategoryPanel::showTreeContextMenu(const QPoint &pos) {
    QTreeWidgetItem* selectedItem = categoryTreeWidget->itemAt(pos);
    QMenu contextMenu(this);

    if (selectedItem) {
        contextMenu.addAction("Change number", this, &TreeCategoryPanel::changeItemPosition);
        // Считываем "isCategory" из UserRole + 1
        bool isCategory = selectedItem->data(0, Qt::UserRole + 1).toBool();
        if (isCategory) {
            contextMenu.addAction("Add category", this, [this]() {
                createCategoryOrTemplate(true);
            });
            contextMenu.addAction("Add template", this, [this]() {
                createCategoryOrTemplate(false);
            });
            contextMenu.addAction("Delete category", this, &TreeCategoryPanel::deleteCategoryOrTemplate);
        } else {
            int templateId = selectedItem->data(0, Qt::UserRole).toInt();

            QAction *copyAct = contextMenu.addAction("Copy template");
            connect(copyAct, &QAction::triggered, this,
                    [this, templateId]() { duplicateTemplate(templateId); });

            bool isDyn = dbHandler->getTemplateManager()->isTemplateDynamic(templateId);
            QString actionText = isDyn ? QStringLiteral("Make it static") : QStringLiteral("Make it dynamic");

            QAction *toggleDynAction = contextMenu.addAction(actionText);
            connect(toggleDynAction, &QAction::triggered, this, [this, templateId, isDyn]() {
                toggleDynamicState(templateId, !isDyn);
            });
            contextMenu.addAction("Delete template", this, &TreeCategoryPanel::deleteCategoryOrTemplate);
        }
    } else {
        // Клик вне элементов - добавляем корневую категорию
        categoryTreeWidget->clearSelection();
        contextMenu.addAction("Add category", this, [this]() {
            createCategoryOrTemplate(true);
        });
    }

    contextMenu.exec(categoryTreeWidget->viewport()->mapToGlobal(pos));
}
void TreeCategoryPanel::createCategoryOrTemplate(bool isCategory) {
    QString title = isCategory ? "Create category" : "Create template";
    QString prompt = isCategory ? "Enter category name:" : "Enter template name:";
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
                QMessageBox::warning(this, "Error", "A template can only be created within a category.");
                return;
            }
            parentId = parentItem->data(0, Qt::UserRole).toInt();
        }
    } else {
        // Если ничего не выбрано, то шаблон создать нельзя
        if (!isCategory) {
            QMessageBox::warning(this, "Error", "Select a category to create a template.");
            return;
        }
    }

    int projectId = selectedProjectId;
    if (projectId == 0) {
        QMessageBox::warning(this, tr("Error"), tr("Select a project before creating it."));
        return;
    }

    bool success = false;
    if (isCategory) {
        success = dbHandler->getCategoryManager()->createCategory(name, parentId, projectId);
    } else {
        //  Диалог выбора типа шаблона (table/listing/graph)
        QStringList templateTypes;
        templateTypes << "Table" << "Listing" << "Graph";
        bool okType = false;
        QString chosenType = QInputDialog::getItem(
            this,
            "Choosing the template type",
            "Select type:",
            templateTypes,
            0,        // индекс по умолчанию
            false,    // editable
            &okType
            );
        if (!okType || chosenType.isEmpty()) {
            return;  // пользователь отменил
        }

        // Определяем значение template_type для базы
        QString templateTypeForDB;
        if (chosenType == "Table") {
            templateTypeForDB = "table";
        } else if (chosenType == "Listing") {
            templateTypeForDB = "listing";
        } else {
            templateTypeForDB = "graph";
        }

        //  Если это график — позволим выбрать «подтип графика»
        QString chosenGraphSubType; // например, "pie", "bar", "line", ...
        if (templateTypeForDB == "graph") {
            // Покажем дополнительный диалог
            QStringList graphTypes;
            graphTypes = dbHandler->getTemplateManager()->getGraphTypesFromLibrary();
            bool okGraph = false;
            chosenGraphSubType = QInputDialog::getItem(
                this,
                "Choosing a graph subtype",
                "Graph type:",
                graphTypes,
                0,
                false,
                &okGraph
                );
            if (!okGraph || chosenGraphSubType.isEmpty()) {
                return; // отмена
            }
        }
        success = dbHandler->getTemplateManager()->createTemplate(parentId, name, templateTypeForDB);

        //  Если это график — копируем базовый график из вашей «библиотеки» в таблицу graph
        //    (привязывая к только что созданному template_id).
        if (templateTypeForDB == "graph") {
            int newTemplateId = dbHandler->getTemplateManager()->getLastCreatedTemplateId();
            if (newTemplateId <= 0) {
                QMessageBox::warning(this, "Error", "Couldn't get the ID of the created graph.");
                return;
            }

            // Далее — логика копирования «базового» графика (chosenGraphSubType)
            // Примерно так:
            if (!dbHandler->getTemplateManager()->copyGraphFromLibrary(chosenGraphSubType, newTemplateId)) {
                QMessageBox::warning(this, "Error", "Couldn't copy the blank graph.");
                return;
            }
        }
    }

    if (!success) {
        QMessageBox::warning(this, "Error", "Failed to create an item in the database.");
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
        msgBox.setWindowTitle("Deleting a category");
        msgBox.setText(QString("The \"%1\" category will be deleted.").arg(selectedItem->text(1)));
        msgBox.setInformativeText("Select an action:");
        QPushButton *deleteButton = msgBox.addButton("Delete along with all the contents",
                                                     QMessageBox::DestructiveRole);
        QPushButton *unpackButton = msgBox.addButton("Unpack (lift the attachments)",
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
            QMessageBox::information(
                this,
                tr("Check numbering"),
                tr("The category has been deleted.\nPlease check that the numbering in the tree is correct.")
                );
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
                    dbHandler->getCategoryManager()->updateCategoryFields(childId, NULL, std::nullopt, std::nullopt);
                } else {
                    dbHandler->getCategoryManager()->updateCategoryFields(childId, NULL, std::nullopt, std::nullopt);
                }
                // Перенести в дерево как top-level
                categoryTreeWidget->addTopLevelItem(child);
            }
            // Теперь удаляем саму категорию без детей
            dbHandler->getCategoryManager()->deleteCategory(itemId, /*deleteChildren=*/false);
            loadCategoriesAndTemplates();
            QMessageBox::information(
                this,
                tr("Check numbering"),
                tr("The category has been deleted.\nPlease check that the numbering in the tree is correct.")
                );
        }
    }
    else {
        // Это шаблон
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Deleting a template",
            QString("Do you really want to delete the template \"%1 - %2\"?")
                .arg(selectedItem->text(0))
                .arg(selectedItem->text(1)),
            QMessageBox::Yes | QMessageBox::No
            );
        if (reply == QMessageBox::Yes) {
            bool ok = dbHandler->getTemplateManager()->deleteTemplate(itemId);
            if (!ok) {
                QMessageBox::warning(this, "Error",
                                     "Couldn't delete the template from the database!");
            }
            loadCategoriesAndTemplates();
            QMessageBox::information(
                this,
                tr("Check numbering"),
                tr("The template has been deleted.\nPlease check that the numbering in the tree is correct.")
                );
        }
    }
}
void TreeCategoryPanel::duplicateTemplate(int srcId) {
    bool ok = false;
    QString newName = QInputDialog::getText(this, "Copying a template",
                                            "Copy name:", QLineEdit::Normal,
                                            "Copy", &ok);
    if (!ok || newName.trimmed().isEmpty()) return;

    int newId = -1;
    if (!dbHandler->getTemplateManager()
             ->duplicateTemplate(srcId, newName.trimmed(), newId)) {
        QMessageBox::warning(this, "Error", "Couldn't create a copy.");
        return;
    }

    loadCategoriesAndTemplates();      // перерисовываем дерево
    emit templateSelected(newId);
}
void TreeCategoryPanel::toggleDynamicState(int templateId, bool makeDynamic) {
    bool ok = dbHandler->getTemplateManager()->setTemplateDynamic(templateId, makeDynamic);
    if (!ok) {
        QMessageBox::warning(this, "Error", "Couldn't switch the template state.");
        return;
    }
    // Успешно
    QString newState = makeDynamic ? "dynamic" : "static";
    QMessageBox::information(this, "Template status",
                             QString("The template is '%1' now")
                                 .arg(newState));
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
