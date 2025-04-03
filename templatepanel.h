#ifndef TEMPLATEPANEL_H
#define TEMPLATEPANEL_H

#include <QWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QSqlDatabase>
#include <QPointer>
#include <QVector>
#include <QPair>
#include <QAction>
#include "databasehandler.h"
#include "formattoolbar.h"

class TemplatePanel : public QWidget
{
    Q_OBJECT
public:
    explicit TemplatePanel(DatabaseHandler *dbHandler, FormatToolBar *formatToolBar, QWidget *parent = nullptr);
    ~TemplatePanel();

    void loadTableTemplate(int templateId);

    // Взаимодействия с таблицей
    void editHeader(int column);
    void addRowOrColumn(const QString &type);
    void deleteRowOrColumn(const QString &type);
    void saveTableData();

    //
    bool eventFilter(QObject *obj, QEvent *event);
    void onCurrentChanged(const QModelIndex &current, const QModelIndex &previous);


    void setCurrentTemplateId(int templateId) { selectedTemplateId = templateId; }
    int currentTemplateId() const { return selectedTemplateId; }

signals:
    void textEditFocused(QTextEdit *editor);
    void checkButtonPressed();

public slots:
    void fillCellColor(const QColor &color);

private:
    DatabaseHandler *dbHandler;
    int selectedTemplateId = -1; // Текущий выбранный шаблон (если нужно)
    FormatToolBar *formatToolBar;

    QTableWidget *templateTableWidget;  // Таблица данных
    QTextEdit *notesField;              // Поле для заметок
    QTextEdit *notesProgrammingField;   // Поле для программных заметок
    QPushButton *addRowButton;          // Кнопка добавления строки
    QPushButton *addColumnButton;       // Кнопка добавления столбца
    QPushButton *deleteRowButton;       // Кнопка удаления строки
    QPushButton *deleteColumnButton;    // Кнопка удаления столбца
    QPushButton *saveButton;            // Кнопка сохранения
    QPushButton *checkButton;           // Кнопка утверждения

    QPointer<QTextEdit> activeTextEdit;
};

#endif // TEMPLATEPANEL_H
