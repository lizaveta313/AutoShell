#include "mainwindow.h"
#include <QToolBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

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
    formatToolBar   = new FormatToolBar(this);
    projectPanel    = new ProjectPanel(dbHandler, this);
    treeCategoryPanel       = new TreeCategoryPanel(dbHandler, this);
    templatePanel   = new TemplatePanel(dbHandler, formatToolBar,this);
    setupUI();
    setupConnections();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {

    addToolBar(formatToolBar);

    // Левый блок: ещё один вертикальный сплиттер, где сверху ProjectPanel, снизу TreeCategoryPanel
    QSplitter *vSplitterLeft = new QSplitter(Qt::Vertical);
    vSplitterLeft->addWidget(projectPanel);
    vSplitterLeft->addWidget(treeCategoryPanel);
    // Можно настроить начальные пропорции
    vSplitterLeft->setStretchFactor(0, 1); // ProjectPanel
    vSplitterLeft->setStretchFactor(1, 2); // TreeCategoryPanel

    // Правый блок: TemplatePanel (уже внутри себя делает разбиение на таблицу и т.д.)
    // Не нужен ещё один сплиттер, достаточно виджета
    QWidget *rightWidget = templatePanel;

    // Создаём горизонтальный сплиттер и кладём туда два блока
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(vSplitterLeft);
    mainSplitter->addWidget(rightWidget);

    mainSplitter->setStretchFactor(0, 1); // Левая часть
    mainSplitter->setStretchFactor(1, 4); // Правая часть (TemplatePanel)

    setCentralWidget(mainSplitter);

    setWindowTitle("AutoShell");
    resize(1200, 800);
}

void MainWindow::setupConnections() {

    // Загрузка проекта по Id и правильное отображение стиля таблиц в проекте
    connect(projectPanel, &ProjectPanel::projectSelected,
            this, [this](int projectId) {
                // Очистить дерево и шаблоны
                treeCategoryPanel->clearAll();
                templatePanel->clearAll();

                treeCategoryPanel->loadCategoriesAndTemplatesForProject(projectId);
                QString dbStyle = dbHandler->getProjectManager()->getProjectStyle(projectId);

                // Если NULL/пусто в БД:
                if (dbStyle.isEmpty()) {
                    dbHandler->getProjectManager()->updateProjectStyle(projectId, "Default");
                    dbStyle = "Default";
                }
                formatToolBar->setStyleComboText(dbStyle);
    });


    // Загрузка шаблона по Id
    connect(treeCategoryPanel,
            &TreeCategoryPanel::templateSelected,
            templatePanel,
            &TemplatePanel::loadTemplate);

    // Фокус на текст для форматирования
    connect(templatePanel, &TemplatePanel::textEditFocused,
            formatToolBar, &FormatToolBar::setActiveTextEdit);

    // Утвердить шаблон в списке ТЛГ
    connect(templatePanel, &TemplatePanel::checkButtonPressed,
            treeCategoryPanel, &TreeCategoryPanel::onCheckButtonClicked);

    // Обновление списка проектов
    connect(projectPanel, &ProjectPanel::projectListChanged,
            treeCategoryPanel, &TreeCategoryPanel::loadCategoriesAndTemplates);

    // Окрашивание ячейки
    connect(formatToolBar, &FormatToolBar::cellFillRequested,
            templatePanel,   &TemplatePanel::fillCellColor);

    // Получение Id для обновления стиля таблицы
    connect(formatToolBar, &FormatToolBar::styleSelected,
            this, [this](const QString &styleName){
                int projId = treeCategoryPanel->currentProjectId();
                if (projId > 0) {
                    dbHandler->getProjectManager()->updateProjectStyle(projId, styleName);
                }
            });

    // Запрос пересчёта нумерации из ProjectPanel
    connect(projectPanel, &ProjectPanel::recalcNumberingRequested,
            treeCategoryPanel, &TreeCategoryPanel::recalcNumbering);
}
