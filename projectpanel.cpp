#include "projectpanel.h"
#include "exportprojectasxml.h"
#include <QInputDialog>
#include <QMenu>
#include <QStandardItem>
#include <QCompleter>
#include <QDebug>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QFormLayout>
#include <QLabel>
#include <QFileDialog>
#include <QXmlStreamWriter>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTextDocument>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QDateEdit>
#include <qpushbutton.h>

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
    mainLayout->addWidget(new QLabel("Select a project:", this));
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
        QAction *createAction = menu.addAction("Create new project");
        QAction *selectedAction = menu.exec(projectComboBox->view()->viewport()->mapToGlobal(pos));
        if (selectedAction == createAction) {
            createNewProject();
        }
    } else {
        QAction *configuringDataAction = menu.addAction("Configuring data");
        QAction *configureGroupsAction = menu.addAction("Set up groups");
        QAction *renameAction = menu.addAction("Rename it");
        QAction *copyAction   = menu.addAction("Create copy");
        QAction *deleteAction = menu.addAction("Remove");
        QAction *exportXmlAction   = menu.addAction("Export to XML");


        QAction *selectedAction = menu.exec(projectComboBox->view()->viewport()->mapToGlobal(pos));
        if (selectedAction == exportXmlAction) {
            onExportProjectAsXml(projectId);
            return;
        } else if (selectedAction == configureGroupsAction) {
            configureGroups(sourceIndex);
        } else if (selectedAction == renameAction) {
            renameProject(sourceIndex);
        } else if (selectedAction == deleteAction) {
            deleteProject(sourceIndex);
        } else if (selectedAction == copyAction) {
            copyProject(sourceIndex);
        } else if (selectedAction == configuringDataAction) {
            configureProjectData(sourceIndex);
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
    QString newProjectName = QInputDialog::getText(this, tr("Copy the project"),
                                                   tr("Enter a name for the project copy:"),
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
                                            "Renaming the project",
                                            "Enter a new name:",
                                            QLineEdit::Normal,
                                            currentName,
                                            &ok);
    if (!ok || newName.isEmpty() || newName == currentName) {
        return; // пользователь отменил или не ввёл новое имя
    }

    int projId = index.data(Qt::UserRole).toInt();
    if (!dbHandler->getProjectManager()->updateProject(projId, newName)) {
        QMessageBox::warning(this, "Error", "Couldn't rename the project.");
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
                              "Deleting a project",
                              QString("Do you really want to delete the project \"%1\"?").arg(projName))
        == QMessageBox::Yes)
    {
        if (!dbHandler->getProjectManager()->deleteProject(projId)) {
            QMessageBox::warning(this, "Error", "Couldn't delete the project.");
        } else {
            loadProjectsIntoModel();
        }
    }
}

int ProjectPanel::askForGroupCount() {
    bool ok;
    int numGroups = QInputDialog::getInt(this, "Setting up groups",
                                         "Enter the number of groups:",
                                         1, 1, 10, 1, &ok);
    return ok ? numGroups : -1;
}

QVector<QString> ProjectPanel::askForGroupNames(int numGroups) {
    QVector<QString> groupNames;
    QDialog dialog(this);
    dialog.setWindowTitle("Setting up group names");

    QFormLayout form(&dialog);
    QVector<QLineEdit*> edits;

    for (int i = 0; i < numGroups; i++) {
        QLineEdit *lineEdit = new QLineEdit(&dialog);
        form.addRow(QString("Group name %1:").arg(i + 1), lineEdit);
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

void ProjectPanel::configureProjectData(const QModelIndex &index) {
    if (!index.isValid())
        return;

    int projectId = index.data(Qt::UserRole).toInt();
    ProjectDetails currentDetails = dbHandler->getProjectManager()->getProjectDetails(projectId);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Project Data Configuration"));
    QFormLayout form(&dialog);

    QLineEdit *studyEdit = new QLineEdit(currentDetails.study, &dialog);
    QLineEdit *sponsorEdit = new QLineEdit(currentDetails.sponsor, &dialog);
    QDateEdit *cutDateEdit = new QDateEdit(currentDetails.cutDate, &dialog);
    cutDateEdit->setCalendarPopup(true); // Добавляем календарь
    QLineEdit *versionEdit = new QLineEdit(currentDetails.version, &dialog);

    form.addRow(tr("Study:"), studyEdit);
    form.addRow(tr("Sponsor:"), sponsorEdit);
    form.addRow(tr("CutDate:"), cutDateEdit);
    form.addRow(tr("Version:"), versionEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        ProjectDetails newDetails;
        newDetails.study = studyEdit->text().trimmed();
        newDetails.sponsor = sponsorEdit->text().trimmed();
        newDetails.cutDate = cutDateEdit->date();
        newDetails.version = versionEdit->text().trimmed();

        if (!dbHandler->getProjectManager()->updateProjectDetails(projectId, newDetails)) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to update project data."));
        }
    }
}

void ProjectPanel::onExportProjectAsXml(int projectId) {
    if (!editProjectDataWithValidation(projectId)) {
        return;
    }

    //Получаем ключ и читаем последний каталог (по умолчанию — «Документы»)
    const QString KEY = "export/lastDir";
    QSettings settings;
    QString lastDir = settings.value(
                                  KEY,
                                  QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                  ).toString();
    if (!QDir(lastDir).exists())
        lastDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    // 2) Формируем имя по умолчанию и полный стартовый путь
    QString defaultName = QString("project_%1.xml").arg(projectId);
    QString initialPath = QDir(lastDir).filePath(defaultName);

    // 3) Вызываем диалог
    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save the project as XML"),
        initialPath,
        tr("XML files (*.xml)")
        );
    if (filename.isEmpty())
        return; // пользователь отменил

    // 4) Сохраняем новую папку в настройки
    QString newDir = QFileInfo(filename).absolutePath();
    settings.setValue(KEY, newDir);
    settings.sync();

    // 5) Запускаем экспорт
    ExportProjectAsXml exporter(
        dbHandler->getProjectManager(),
        dbHandler->getCategoryManager(),
        dbHandler->getTemplateManager(),
        dbHandler->getTableManager()
        );
    if (!exporter.exportProject(projectId, filename)) {
        QMessageBox::warning(
            this,
            tr("Error"),
            tr("Couldn't save XML file:\n%1").arg(filename)
            );
    }
}

bool ProjectPanel::editProjectDataWithValidation(int projectId) {
    while (true) {
        ProjectDetails current = dbHandler->getProjectManager()->getProjectDetails(projectId);

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Project Data Configuration"));
        QFormLayout form(&dialog);

        QLineEdit *studyEdit   = new QLineEdit(current.study,   &dialog);
        QLineEdit *sponsorEdit = new QLineEdit(current.sponsor, &dialog);
        QDateEdit *cutDateEdit = new QDateEdit(&dialog);
        QLineEdit *versionEdit = new QLineEdit(current.version, &dialog);
        cutDateEdit->setCalendarPopup(true);

        const QDate sentinel(1900, 1, 1);
        cutDateEdit->setMinimumDate(sentinel);
        cutDateEdit->setSpecialValueText(tr("Select date..."));
        if (current.cutDate.isValid()) cutDateEdit->setDate(current.cutDate);
        else                           cutDateEdit->setDate(sentinel);

        auto markRequired = [](QWidget* w, const QString& ph){
            w->setToolTip(QObject::tr("Required field"));
            if (auto le = qobject_cast<QLineEdit*>(w)) le->setPlaceholderText(ph);
        };
        markRequired(studyEdit,   tr("Study"));
        markRequired(sponsorEdit, tr("Sponsor"));
        markRequired(versionEdit, tr("Version"));
        cutDateEdit->setToolTip(tr("Required field"));

        // Метки — обычные, без добавлений
        form.addRow(tr("Study:"),   studyEdit);
        form.addRow(tr("Sponsor:"), sponsorEdit);
        form.addRow(tr("CutDate:"), cutDateEdit);
        form.addRow(tr("Version:"), versionEdit);

        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                 Qt::Horizontal, &dialog);
        form.addRow(&buttons);
        auto okBtn = buttons.button(QDialogButtonBox::Ok);
        QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        auto recompute = [&](){
            const bool studyEmpty   = studyEdit->text().trimmed().isEmpty();
            const bool sponsorEmpty = sponsorEdit->text().trimmed().isEmpty();
            const bool versionEmpty = versionEdit->text().trimmed().isEmpty();
            const bool dateEmpty    = (cutDateEdit->date() == sentinel);

            okBtn->setEnabled(!(studyEmpty || sponsorEmpty || versionEmpty || dateEmpty));
        };

        QObject::connect(studyEdit,   &QLineEdit::textChanged, &dialog, recompute);
        QObject::connect(sponsorEdit, &QLineEdit::textChanged, &dialog, recompute);
        QObject::connect(versionEdit, &QLineEdit::textChanged, &dialog, recompute);
        QObject::connect(cutDateEdit, &QDateEdit::dateChanged, &dialog, recompute);

        recompute();

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        const QString study   = studyEdit->text().trimmed();
        const QString sponsor = sponsorEdit->text().trimmed();
        const QDate   cutDate = cutDateEdit->date();
        const QString version = versionEdit->text().trimmed();

        if (study.isEmpty() || sponsor.isEmpty() || !cutDate.isValid() || cutDate == sentinel || version.isEmpty()) {
            QMessageBox::warning(this,
                                 tr("Incomplete data"),
                                 tr("Please fill in all the fields (Study, Sponsor, CutDate, Version)."));
            continue;
        }

        ProjectDetails updated;
        updated.study   = study;
        updated.sponsor = sponsor;
        updated.cutDate = cutDate;
        updated.version = version;

        if (!dbHandler->getProjectManager()->updateProjectDetails(projectId, updated)) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to update project data."));
            continue;
        }

        return true;
    }
}


