#ifndef TREECATEGORYPANEL_H
#define TREECATEGORYPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QSqlDatabase>
#include <QPointer>
#include <QSet>

#include "databasehandler.h"
#include "mytreewidget.h"


struct CombinedItem {
    bool isCategory;
    int position;
    int id;         // category_id или template_id
    QString name;
    Category category;    // Заполняется, если isCategory == true
    Template templ;       // Заполняется, если isCategory == false
};

class TreeCategoryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TreeCategoryPanel(DatabaseHandler *dbHandler, QWidget *parent = nullptr);
    ~TreeCategoryPanel();


    void loadCategoriesAndTemplates();
    void loadItemsForCategory(int projectId, const QVariant &parentId,
                              QTreeWidgetItem *parentItem, const QString &parentPath);

    void loadCategoriesForProject(int projectId, QTreeWidgetItem *parentItem, const QString &parentPath);
    void loadCategoriesForCategory(const Category &category, QTreeWidgetItem *parentItem, const QString &parentPath);
    void loadTemplatesForCategory(int categoryId, QTreeWidgetItem *parentItem, const QString &parentPath);

    // Обработка кликов
    void onCategoryOrTemplateSelected(QTreeWidgetItem *item, int column);
    void onCategoryOrTemplateDoubleClickedForEditing(QTreeWidgetItem *item, int column);
    void onCheckButtonClicked();

    // Методы для нумерации
    void updateAllSiblingNumbering(QTreeWidgetItem *parent);
    void updateSiblingNumbering(QTreeWidgetItem *editedItem, int newNumber);
    void updateNumbering();
    void updateNumberingFromItem(QTreeWidgetItem *parentItem);
    void numberChildItems(QTreeWidgetItem *parent, const QString &parentPath);

    // Взаимодействия со списком ТЛГ
    void showTreeContextMenu(const QPoint &pos);
    void createCategoryOrTemplate(bool isCategory);
    void deleteCategoryOrTemplate();
    QTreeWidgetItem* findItemById(QTreeWidgetItem* parent, int id);

    // Сохранение/восстановление состояния дерева
    QSet<int> saveExpandedState();
    void saveExpandedRecursive(QTreeWidgetItem *item, QSet<int> &expandedIds);
    void restoreExpandedState(const QSet<int> &expandedIds);
    void restoreExpandedRecursive(QTreeWidgetItem *item, const QSet<int> &expandedIds);

    void setCurrentProjectId(int projectId) { selectedProjectId = projectId; }
    int currentProjectId() const { return selectedProjectId; }

public slots:
    void loadCategoriesAndTemplatesForProject(int projectId);

signals:

    void templateSelected(int templateId);

private:
    DatabaseHandler *dbHandler;

    MyTreeWidget *categoryTreeWidget;   // Иерархический вид категорий и шаблонов
    int selectedProjectId = -1;

};

#endif // TREECATEGORYPANEL_H
