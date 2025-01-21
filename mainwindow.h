#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "databasehandler.h"
#include <QMainWindow>
#include <QSqlDatabase>
#include <QTreeWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Настройка интерфейса
    void setupUI();

    // Загрузка
    void loadProjects();
    void onProjectSelected(int index);
    void loadCategoriesAndTemplates();
    void loadTableTemplate(int templateId);
    void loadCategoriesForCategory(const Category &category, QTreeWidgetItem *parentItem, const QString &parentPath);
    void loadCategoriesForProject(int projectId, QTreeWidgetItem *parentItem, const QString &parentPath);
    void loadTemplatesForCategory(int categoryId, QTreeWidgetItem *parentItem, const QString &parentPath);

    // Взаимодействия со списком ТЛГ
    void showContextMenu(const QPoint &pos);
    void createCategoryOrTemplate(bool isCategory);
    void deleteCategoryOrTemplate();

    // Обработка кликов
    void onCategoryOrTemplateSelected(QTreeWidgetItem *item, int column);
    void onCategoryOrTemplateDoubleClickedForEditing(QTreeWidgetItem *item, int column);
    void onCheckButtonClicked();

    // Функции для нумерации
    void updateNumbering();
    void numberChildItems(QTreeWidgetItem *parent, const QString &prefix);
    void updateNumberingFromItem(QTreeWidgetItem *parentItem);
    void dropEvent(QDropEvent *event);  // Переопределение перетаскивания

    // Взаимодействия с таблицей
    void editHeader(int column);
    void addRowOrColumn(const QString &type);
    void deleteRowOrColumn(const QString &type);
    void saveTableData();



private:

    QSqlDatabase db;            // Объявляем объект базы данных
    DatabaseHandler *dbHandler; // Обработчик базы данных

    QComboBox *projectComboBox;         // Выбор проекта
    QTreeWidget *categoryTreeWidget;    // Иерархический вид категорий и шаблонов
    QTableWidget *templateTableWidget;  // Таблица данных
    QTextEdit *notesField;              // Поле для заметок
    QTextEdit *notesProgrammingField;   // Поле для программных заметок
    QPushButton *addRowButton;          // Кнопка добавления строки
    QPushButton *addColumnButton;       // Кнопка добавления столбца
    QPushButton *deleteRowButton;       // Кнопка удаления строки
    QPushButton *deleteColumnButton;    // Кнопка удаления столбца
    QPushButton *saveButton;            // Кнопка сохранения
    QPushButton *checkButton;           // Кнопка утверждения
};

#endif // MAINWINDOW_H
