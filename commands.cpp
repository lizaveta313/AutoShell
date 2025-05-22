#include "commands.h"
#include "templatepanel.h"
#include <QTableWidgetItem>

DeleteRowCommand::DeleteRowCommand(TemplatePanel *panel,
                                   int templateId,
                                   int rowIndex,
                                   QVector<CellData> backup,
                                   QUndoCommand *parent)
    : QUndoCommand(parent),
    panel(panel),
    templateId(templateId),
    rowIndex(rowIndex),
    backupRow(std::move(backup))
{
    setText(QObject::tr("Delete row %1").arg(rowIndex));
}

void DeleteRowCommand::redo() {
    // удаляем строку в UI
    panel->templateTableWidget->removeRow(rowIndex - 1);
    // сохраняем состояние (таблица + текстовые поля)
    panel->saveTableData();
}

void DeleteRowCommand::undo() {
    // вставляем строку обратно
    panel->templateTableWidget->insertRow(rowIndex - 1);
    // восстанавливаем каждую ячейку
    for (auto &cell : backupRow) {
        auto *item = new QTableWidgetItem(cell.content);
        item->setBackground(cell.colour);
        panel->templateTableWidget
            ->setItem(cell.row - 1, cell.col - 1, item);
        panel->templateTableWidget
            ->setSpan(cell.row - 1, cell.col - 1,
                      cell.rowSpan, cell.colSpan);
    }
    panel->saveTableData();
}

DeleteColumnCommand::DeleteColumnCommand(TemplatePanel *panel,
                                         int templateId,
                                         int colIndex,
                                         QVector<CellData> backup,
                                         QUndoCommand *parent)
    : QUndoCommand(parent),
    panel(panel),
    templateId(templateId),
    colIndex(colIndex),
    backupCol(std::move(backup))
{
    setText(QObject::tr("Delete column %1").arg(colIndex));
}

void DeleteColumnCommand::redo() {
    panel->templateTableWidget->removeColumn(colIndex - 1);
    panel->saveTableData();
}

void DeleteColumnCommand::undo() {
    panel->templateTableWidget->insertColumn(colIndex - 1);
    for (auto &cell : backupCol) {
        auto *item = new QTableWidgetItem(cell.content);
        item->setBackground(cell.colour);
        panel->templateTableWidget
            ->setItem(cell.row - 1, cell.col - 1, item);
        panel->templateTableWidget
            ->setSpan(cell.row - 1, cell.col - 1,
                      cell.rowSpan, cell.colSpan);
    }
    panel->saveTableData();
}
