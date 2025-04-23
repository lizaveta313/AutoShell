#include "projectpanel.h"
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QStandardItem>
#include <QCompleter>
#include <QDebug>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QFormLayout>
#include <QLabel>
#include <QFileDialog>
#include <QXmlStreamWriter>
#include <QMessageBox>

ProjectPanel::ProjectPanel(DatabaseHandler *dbHandler, QWidget *parent)
    : QWidget(parent)
    , dbHandler(dbHandler) {
    // Создаём модель и прокси-модель
    projectModel = new QStandardItemModel(this);

    // Добавляем пустой элемент для возможности выбрать "ничего"
    QStandardItem *emptyItem = new QStandardItem("");
    emptyItem->setData(0, Qt::UserRole);
    emptyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    projectModel->appendRow(emptyItem);

    projectProxyModel = new QSortFilterProxyModel(this);
    projectProxyModel->setSourceModel(projectModel);
    projectProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    projectProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    projectProxyModel->sort(0);

    // Создаём QComboBox для выбора проектов
    projectComboBox = new QComboBox(this);
    projectComboBox->setEditable(true);
    projectComboBox->setInsertPolicy(QComboBox::NoInsert);

    projectComboBox->setModel(projectProxyModel);

    // Устанавливаем completer для автодополнения
    QCompleter *completer = new QCompleter(projectProxyModel, this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setFilterMode(Qt::MatchContains);
    projectComboBox->setCompleter(completer);

    // Сброс выбора: поле должно быть пустым при запуске
    projectComboBox->setCurrentIndex(-1);
    projectComboBox->clearEditText();

    // Подключаем фильтрацию:
    connect(projectComboBox->lineEdit(), &QLineEdit::textChanged,
            projectProxyModel, &QSortFilterProxyModel::setFilterFixedString);

    connect(projectComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProjectPanel::onProjectActivated);

    // Контекстное меню
    projectComboBox->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(projectComboBox, &QWidget::customContextMenuRequested,
            this, &ProjectPanel::showProjectContextMenu);

    // // Можно добавить layout и положить projectComboBox туда
    // // Для наглядности:
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel("Выберите проект:", this));
    mainLayout->addWidget(projectComboBox);
    setLayout(mainLayout);

    // Загрузим список проектов
    loadProjectsIntoModel();
}

ProjectPanel::~ProjectPanel() {}

void ProjectPanel::loadProjectsIntoModel() {
    // Сначала очищаем модель и добавляем пустой элемент (если это нужно повторно)
    projectModel->clear();
    QStandardItem *emptyItem = new QStandardItem("");
    emptyItem->setData(0, Qt::UserRole);
    emptyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    projectModel->appendRow(emptyItem);

    // Получаем проекты из базы данных
    QVector<Project> projects = dbHandler->getProjectManager()->getProjects();
    for (const Project &p : projects) {
        QStandardItem *item = new QStandardItem(p.name);
        // Разрешаем редактирование для inline-переименования
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setData(p.projectId, Qt::UserRole);
        projectModel->appendRow(item);
    }
    projectProxyModel->sort(0);
}

void ProjectPanel::onProjectActivated(int index) {
    QModelIndex proxyIdx = projectComboBox->model()->index(index, 0);
    if (!proxyIdx.isValid())
        return;

    // Преобразуем индекс прокси-модели в индекс исходной модели
    QModelIndex sourceIdx = projectProxyModel->mapToSource(proxyIdx);
    if (!sourceIdx.isValid())
        return;

    int projectId = projectModel->item(sourceIdx.row())->data(Qt::UserRole).toInt();
    if (projectId == 0) {
        qDebug() << "Пустой проект выбран. ID=0";
        // Можно послать сигнал, что проект не выбран
        emit projectSelected(0);
        return;
    }

    // Сообщаем, что выбрали проект
    emit projectSelected(projectId);
}

void ProjectPanel::showProjectContextMenu(const QPoint &pos) {
    QModelIndex index = projectComboBox->view()->indexAt(pos);
    if (!index.isValid())
        return;

    QModelIndex sourceIndex = projectProxyModel->mapToSource(index);
    if (!sourceIndex.isValid())
        return;

    int projectId = projectModel->item(sourceIndex.row())->data(Qt::UserRole).toInt();

    QMenu menu(this);
    if (projectId == 0) {
        // Пустой элемент – только создание нового проекта
        QAction *createAction = menu.addAction("Создать новый проект");
        QAction *selectedAction = menu.exec(projectComboBox->view()->viewport()->mapToGlobal(pos));
        if (selectedAction == createAction) {
            createNewProject();
        }
    } else {
        QAction *recalcAct = menu.addAction("Пересчитать нумерацию проекта");
        QAction *configureGroupsAction = menu.addAction("Настроить группы");
        QAction *renameAction = menu.addAction("Переименовать");
        QAction *copyAction   = menu.addAction("Создать копию");
        QAction *deleteAction = menu.addAction("Удалить");
        QAction *exportXmlAction   = menu.addAction("Экспорт в XML");
        QAction *exportExcelAction = menu.addAction("Экспорт в Excel");


        QAction *selectedAction = menu.exec(projectComboBox->view()->viewport()->mapToGlobal(pos));
        if (selectedAction == recalcAct) {
            emit recalcNumberingRequested(projectId);
            return;
        } else if (selectedAction == exportXmlAction) {
            onExportProjectAsXml(projectId);
            return;
        } else if (selectedAction == exportExcelAction) {
            onExportProjectAsExcel(projectId);
            return;
        } else if (selectedAction == configureGroupsAction) {
            configureGroups(sourceIndex);
        } else if (selectedAction == renameAction) {
            renameProject(sourceIndex);
        } else if (selectedAction == deleteAction) {
            deleteProject(sourceIndex);
        } else if (selectedAction == copyAction) {
            copyProject(sourceIndex);
        }
    }
}

void ProjectPanel::createNewProject() {
    bool ok;
    QString newName = QInputDialog::getText(this,
                                            "Создание проекта",
                                            "Введите имя нового проекта:",
                                            QLineEdit::Normal,
                                            "",
                                            &ok);
    if (!ok || newName.isEmpty())
        return;

    // Создаем новый проект в базе данных (функция createProject должна возвращать новый projectId)
    int newProjectId = dbHandler->getProjectManager()->createProject(newName);

    // Добавляем новый проект в модель
    QStandardItem *item = new QStandardItem(newName);
    item->setData(newProjectId, Qt::UserRole);
    // Разрешаем inline-редактирование
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    projectModel->appendRow(item);

    // Сортируем список проектов (прокси-модель)
    projectProxyModel->sort(0);

    // Находим новый проект в модели и устанавливаем его как выбранный
    int newIndex = -1;
    for (int i = 0; i < projectModel->rowCount(); ++i) {
        if (projectModel->item(i)->data(Qt::UserRole).toInt() == newProjectId) {
            newIndex = i;
            break;
        }
    }
    if (newIndex != -1) {
        QModelIndex sourceIdx = projectModel->index(newIndex, 0);
        QModelIndex proxyIdx = projectProxyModel->mapFromSource(sourceIdx);
        projectComboBox->setCurrentIndex(proxyIdx.row());
    }

    emit projectListChanged();
}

void ProjectPanel::copyProject(const QModelIndex &index) {
    // Проверяем, выбран ли текущий проект для копирования
    int currentIndex = projectComboBox->currentIndex();
    if (currentIndex < 0)
        return;

    QModelIndex proxyIdx = projectComboBox->model()->index(currentIndex, 0);
    QModelIndex sourceIdx = projectProxyModel->mapToSource(proxyIdx);
    if (!sourceIdx.isValid())
        return;

    int projectId = projectModel->item(sourceIdx.row())->data(Qt::UserRole).toInt();

    // Запрос нового имени для копии проекта
    bool ok;
    QString newProjectName = QInputDialog::getText(this, tr("Копировать проект"),
                                                   tr("Введите имя для копии проекта:"),
                                                   QLineEdit::Normal, "", &ok);
    if (!ok || newProjectName.isEmpty())
        return;

    // Создаем копию проекта в базе данных (функция copyProject должна вернуть новый projectId)
    int newProjectId = dbHandler->getProjectManager()->copyProject(projectId, newProjectName);

    // Добавляем копию в модель
    QStandardItem *item = new QStandardItem(newProjectName);
    item->setData(newProjectId, Qt::UserRole);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    projectModel->appendRow(item);

    // Сортируем список проектов
    projectProxyModel->sort(0);

    // Находим новый проект в модели и устанавливаем его как выбранный
    int newIndex = -1;
    for (int i = 0; i < projectModel->rowCount(); ++i) {
        if (projectModel->item(i)->data(Qt::UserRole).toInt() == newProjectId) {
            newIndex = i;
            break;
        }
    }
    if (newIndex != -1) {
        QModelIndex sourceIdx = projectModel->index(newIndex, 0);
        QModelIndex proxyIdx = projectProxyModel->mapFromSource(sourceIdx);
        projectComboBox->setCurrentIndex(proxyIdx.row());
        projectComboBox->setCurrentText(newProjectName);
        emit projectSelected(newProjectId);
    }

    emit projectListChanged();
}

void ProjectPanel::renameProject(const QModelIndex &index) {
    bool ok;
    QString currentName = index.data(Qt::DisplayRole).toString();
    QString newName = QInputDialog::getText(this,
                                            "Переименование проекта",
                                            "Введите новое имя:",
                                            QLineEdit::Normal,
                                            currentName,
                                            &ok);
    if (!ok || newName.isEmpty() || newName == currentName) {
        return; // пользователь отменил или не ввёл новое имя
    }

    int projId = index.data(Qt::UserRole).toInt();
    if (!dbHandler->getProjectManager()->updateProject(projId, newName)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось переименовать проект.");
    } else {
        //  Обновляем в модели
        projectModel->item(index.row())->setText(newName);

        projectComboBox->lineEdit()->setText(newName);
        projectProxyModel->setFilterFixedString(newName);

    }
}

void ProjectPanel::deleteProject(const QModelIndex &index) {
    int projId = index.data(Qt::UserRole).toInt();
    QString projName = index.data(Qt::DisplayRole).toString();
    if (QMessageBox::question(this,
                              "Удаление проекта",
                              QString("Вы действительно хотите удалить проект \"%1\"?").arg(projName))
        == QMessageBox::Yes)
    {
        if (!dbHandler->getProjectManager()->deleteProject(projId)) {
            QMessageBox::warning(this, "Ошибка", "Не удалось удалить проект.");
        } else {
            loadProjectsIntoModel();
        }
    }
}

int ProjectPanel::askForGroupCount() {
    bool ok;
    int numGroups = QInputDialog::getInt(this, "Настройка групп",
                                         "Введите количество групп для анализа:",
                                         1, 1, 10, 1, &ok);
    return ok ? numGroups : -1;
}

QVector<QString> ProjectPanel::askForGroupNames(int numGroups) {
    QVector<QString> groupNames;
    QDialog dialog(this);
    dialog.setWindowTitle("Настройка названий групп");

    QFormLayout form(&dialog);
    QVector<QLineEdit*> edits;

    for (int i = 0; i < numGroups; i++) {
        QLineEdit *lineEdit = new QLineEdit(&dialog);
        form.addRow(QString("Название группы %1:").arg(i + 1), lineEdit);
        edits.append(lineEdit);
    }

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        for (auto edit : edits) {
            groupNames.append(edit->text().trimmed());
        }
    }
    return groupNames;
}

void ProjectPanel::configureGroups(const QModelIndex &index) {
    if (!index.isValid())
        return;

    int projId = index.data(Qt::UserRole).toInt();
    int numGroups = askForGroupCount();
    if (numGroups < 1) {
        qDebug() << "Неверное количество групп.";
        return;
    }
    QVector<QString> groupNames = askForGroupNames(numGroups);
    if (groupNames.size() != numGroups) {
        qDebug() << "Пользователь отменил ввод названий групп.";
        return;
    }

    QVector<int> dynamicTemplates = dbHandler->getTemplateManager()->getDynamicTemplatesForProject(projId);
    for (int templateId : dynamicTemplates) {
        dbHandler->getTableManager()->generateColumnsForDynamicTemplate(templateId, groupNames);
    }

    emit projectListChanged();
}

void ProjectPanel::onExportProjectAsXml(int projectId) {
    QString fn = QFileDialog::getSaveFileName(this,
                                              tr("Сохранить проект как XML"), QDir::homePath(),
                                              tr("XML файлы (*.xml)"));
    if (fn.isEmpty()) return;

    QFile file(fn);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось открыть файл для записи"));
        return;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("Project");
    xml.writeAttribute("id",   QString::number(projectId));
    xml.writeAttribute("name", dbHandler->getProjectManager()->getProjectName(projectId));

    // Рекурсивно проходим категории и шаблоны:
    std::function<void(int)> writeCats = [&](int parentId){
        auto cats = dbHandler->getCategoryManager()
        ->getCategoriesByProjectAndParent(projectId, parentId);
        for (const auto &cat : cats) {
            xml.writeStartElement("Category");
            xml.writeAttribute("id",   QString::number(cat.categoryId));
            xml.writeAttribute("name", cat.name);
            writeCats(cat.categoryId);
            auto tmpls = dbHandler->getTemplateManager()
                             ->getTemplatesForCategory(cat.categoryId);
            for (const auto &t : tmpls) {
                xml.writeStartElement("Template");
                xml.writeAttribute("id",   QString::number(t.templateId));
                xml.writeAttribute("name", t.name);
                xml.writeEndElement(); // Template
            }
            xml.writeEndElement(); // Category
        }
    };
    writeCats(QVariant().toInt()); // NULL–корневой уровень

    xml.writeEndElement(); // Project
    xml.writeEndDocument();
    file.close();
}

void ProjectPanel::onExportProjectAsExcel(int projectId) {
    QString fn = QFileDialog::getSaveFileName(this,
                                              tr("Сохранить проект как Excel"), QDir::homePath(),
                                              tr("Excel файлы (*.xlsx)"));
    if (fn.isEmpty()) return;

#ifdef USE_QTXLSX
    QXlsx::Document xls;
    xls.write("A1", "Project ID");
    xls.write("B1", projectId);
    xls.write("A2", "Project Name");
    xls.write("B2", dbHandler->getProjectManager()->getProjectName(projectId));

    int row = 4;
    std::function<void(int,int)> writeCatsXls = [&](int parentId, int level){
        auto cats = dbHandler->getCategoryManager()
        ->getCategoriesByProjectAndParent(projectId, parentId);
        for (const auto &cat : cats) {
            xls.write(row, level, cat.name);
            row++;
            writeCatsXls(cat.categoryId, level+1);
            auto tmpls = dbHandler->getTemplateManager()
                             ->getTemplatesForCategory(cat.categoryId);
            for (const auto &t : tmpls) {
                xls.write(row, level+1, t.name);
                row++;
            }
        }
    };
    writeCatsXls(QVariant().toInt(), 1);
    xls.saveAs(fn);
#else
    // Без библиотеки — можно сгенерировать CSV или XML Spreadsheet 2003
    QMessageBox::information(this, tr("Не поддерживается"),
                             tr("Для экспорта в XLSX соберите с поддержкой QtXlsx."));
#endif
}
