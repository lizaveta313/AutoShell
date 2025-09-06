#ifndef TEMPLATEPANEL_H
#define TEMPLATEPANEL_H

#include <QWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSet>
#include <QPair>
#include <QTextEdit>
#include <QPushButton>
#include <QSqlDatabase>
#include <QPointer>
#include <QVector>
#include <QPair>
#include <QLabel>
#include <QAction>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QUndoStack>
#include "databasehandler.h"
#include "formattoolbar.h"
#include "commands.h"

class TemplatePanel : public QWidget
{
    Q_OBJECT
public:
    explicit TemplatePanel(DatabaseHandler *dbHandler, FormatToolBar *formatToolBar, QWidget *parent = nullptr);
    ~TemplatePanel();

    void setupUI();
    void clearAll();

    void loadTableTemplate(int templateId);
    void loadTemplate(int templateId);
    void loadGraphTemplate(int templateId);

    void addHeaderRow();
    void addRowOrColumn(const QString &type);
    void deleteRowOrColumn(const QString &type);
    void saveTableData();

    friend class DeleteRowCommand;
    friend class DeleteColumnCommand;

    QVector<CellData> collectRowBackup(int rowIndex);
    QVector<CellData> collectColumnBackup(int colIndex);

    void onChangeGraphTypeClicked();

    void onTableContextMenu(const QPoint &pos);
    void mergeSelectedCells();
    void unmergeSelectedCells();

    bool eventFilter(QObject *obj, QEvent *event);
    void onCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

    void setCurrentTemplateId(int templateId) { selectedTemplateId = templateId; }
    int currentTemplateId() const { return selectedTemplateId; }

    void updateApproveUI();

    void applySizingPreservingUserChanges(int nR, int nC);

signals:
    void textEditFocused(QTextEdit *editor);
    void checkButtonPressed();

public slots:
    void changeCellFontFamily(const QFont &font);
    void changeCellFontSize(int size);
    void toggleCellBold(bool bold);
    void toggleCellItalic(bool italic);
    void toggleCellUnderline(bool underline);
    void alignCells(Qt::Alignment alignment);
    void fillCellColor(const QColor &color);
    void changeCellTextColor(const QColor &color);
    void onApproveClicked();

private:
    DatabaseHandler *dbHandler;
    int selectedTemplateId = -1; // Текущий выбранный шаблон (если нужно)
    FormatToolBar *formatToolBar;

    QStackedWidget *viewStack;
    QTableWidget *templateTableWidget;  // Таблица данных
    QLabel *graphLabel;                 // Графики
    QWidget *tableButtonsWidget;        // Набор кнопок для таблиц и листингов
    QWidget *graphButtonsWidget;        // Набор кнопок для графиков
    QTextEdit *subtitleField;           // Поле для подзаголовка
    QTextEdit *notesField;              // Поле для заметок
    QTextEdit *notesProgrammingField;   // Поле для программных заметок
    QPushButton *addHeaderButton;       // Кнопка добавления строки заголовка
    QPushButton *addRowButton;          // Кнопка добавления строки
    QPushButton *addColumnButton;       // Кнопка добавления столбца
    QPushButton *deleteRowButton;       // Кнопка удаления строки
    QPushButton *deleteColumnButton;    // Кнопка удаления столбца
    QPushButton *checkButton;           // Кнопка утверждения
    QPushButton *undoButton;            // Кнопка отмены
    QPushButton *changeGraphTypeButton; // Кнопка изменения типа графика

    QUndoStack *undoStack;
    QPointer<QTextEdit> activeTextEdit;

    template<typename Func>
    void applyToSelection(Func f) {
        // Берём все выделенные индексы (строка, колонка)
        auto idxs = templateTableWidget->selectionModel()->selectedIndexes();
        // Если ничего не выделено — используем текущую ячейку
        if (idxs.isEmpty() && templateTableWidget->currentIndex().isValid())
            idxs.append(templateTableWidget->currentIndex());

        // Чтобы не дублировать ячейки
        QSet<QPair<int,int>> seen;
        for (const QModelIndex &idx : idxs) {
            int r = idx.row(), c = idx.column();
            if (!seen.insert({r,c})->second) continue;
            // Создаём item, если его нет
            QTableWidgetItem *item = templateTableWidget->item(r, c);
            if (!item) {
                item = new QTableWidgetItem;
                templateTableWidget->setItem(r, c, item);
            }
            f(item);
        }
    }

    int lastSizedTemplateId = -1;
    QVector<int> savedColWidths;
    QVector<int> savedRowHeights;

};

#endif // TEMPLATEPANEL_H
