#ifndef TABLEMANAGER_H
#define TABLEMANAGER_H

#include <optional>
#include <QSqlDatabase>

class TableManager {
public:
    TableManager(QSqlDatabase &db);
    ~TableManager();

    bool addRow(int templateId, bool addToHeader, const QString &headerContent = "");
    bool addColumn(int templateId, const QString &headerContent = "");
    bool deleteRow(int templateId, int row);
    bool deleteColumn(int templateId, int col);

    int getRowCountForHeader(int templateId);
    int getColCountForHeader(int templateId);

    bool updateCellColour(int templateId, int rowIndex, int colIndex, const QString &colour);

    bool saveDataTableTemplate(int templateId,
                               const std::optional<QVector<QString>> &headers,
                               const std::optional<QVector<QVector<QString>>> &cellData,
                               const std::optional<QVector<QVector<QString>>> &cellColours);

    bool generateColumnsForDynamicTemplate(int templateId, const QVector<QString>& groupNames);

    bool mergeCells(int templateId, const QString &cellType,
                    int startRow, int startCol,
                    int rowSpan, int colSpan);

    bool unmergeCells(int templateId, const QString &cellType,
                      int row, int col);

private:
    QSqlDatabase &db;
};

#endif // TABLEMANAGER_H
