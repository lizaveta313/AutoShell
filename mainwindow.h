#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "databasehandler.h"
#include "formattoolbar.h"
#include "projectpanel.h"
#include "templatepanel.h"
#include "treecategorypanel.h"
#include <QMainWindow>
#include <QSqlDatabase>


class MainWindow : public QMainWindow {
    Q_OBJECT

public:

    explicit MainWindow(DatabaseHandler *dbHandler, QWidget *parent = nullptr);
    ~MainWindow();


private:

    void setupUI();
    void setupConnections();

    //QSqlDatabase db;            // Объявляем объект базы данных
    DatabaseHandler *dbHandler; // Обработчик базы данных

    // Наши основные «панели»
    FormatToolBar   *formatToolBar ;
    ProjectPanel    *projectPanel;
    TreeCategoryPanel *treeCategoryPanel;
    TemplatePanel   *templatePanel;
};

#endif // MAINWINDOW_H
