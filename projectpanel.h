#ifndef PROJECTPANEL_H
#define PROJECTPANEL_H

#include "databasehandler.h"
#include <QWidget>
#include <QComboBox>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QModelIndex>
#include <QPoint>
#include <QSqlDatabase>


class ProjectPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectPanel(DatabaseHandler *dbHandler, QWidget *parent);
    ~ProjectPanel();


    void loadProjectsIntoModel();
    void onProjectActivated(int index);

    void showProjectContextMenu(const QPoint &pos);
    void createNewProject();
    void copyProject(const QModelIndex &index);
    void renameProject(const QModelIndex &index);
    void deleteProject(const QModelIndex &index);

    int askForGroupCount();
    QVector<QString> askForGroupNames(int numGroups);

    void onExportProjectAsXml(int projectId);

private slots:

    void configureGroups(const QModelIndex &index);
    void configureProjectData(const QModelIndex &index);
signals:
    void projectSelected(int projectId);
    void projectListChanged();

private:

    DatabaseHandler *dbHandler;

    // Поля для управления списком проектов
    QStandardItemModel *projectModel;
    QSortFilterProxyModel *projectProxyModel;
    QComboBox *projectComboBox;

};

#endif // PROJECTPANEL_H
