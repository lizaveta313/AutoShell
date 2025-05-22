#ifndef COMMANDS_H
#define COMMANDS_H

#include <QColor>
#include <QString>
#include <QUndoCommand>
#include <QVector>

struct CellData {
    int row, col;
    QString content;
    QColor colour;
    int rowSpan, colSpan;
};
class TemplatePanel;

// Команда удаления строки
class DeleteRowCommand : public QUndoCommand {
public:
    DeleteRowCommand(TemplatePanel *panel,
                     int templateId,
                     int rowIndex,
                     QVector<CellData> backup,
                     QUndoCommand *parent = nullptr);

    void redo() override;
    void undo() override;

private:
    TemplatePanel *panel;
    int templateId;
    int rowIndex;
    QVector<CellData> backupRow;
};

// Команда удаления столбца
class DeleteColumnCommand : public QUndoCommand {
public:
    DeleteColumnCommand(TemplatePanel *panel,
                        int templateId,
                        int colIndex,
                        QVector<CellData> backup,
                        QUndoCommand *parent = nullptr);

    void redo() override;
    void undo() override;

private:
    TemplatePanel *panel;
    int templateId;
    int colIndex;
    QVector<CellData> backupCol;
};

#endif // COMMANDS_H
