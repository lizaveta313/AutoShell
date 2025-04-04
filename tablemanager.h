#ifndef TABLEMANAGER_H
#define TABLEMANAGER_H

#include <optional>
#include <QSqlDatabase>

class TableManager {
public:
    TableManager(QSqlDatabase &db);
    ~TableManager();

    bool createRowOrColumn(int templateId, const QString &type, const QString &header, int &newOrder);
    bool updateOrder(const QString &type, int templateId, const QVector<int> &newOrder);
    bool updateCellColour(int templateId, int rowOrder, int columnOrder, const QString &colour);
    bool updateColumnHeader(int templateId, int columnOrder, const QString &newHeader);
    bool deleteRowOrColumn(int templateId, int order, const QString &type);

    bool saveDataTableTemplate(int templateId,
                               const std::optional<QVector<QString>> &headers,
                               const std::optional<QVector<QVector<QString>>> &cellData,
                               const std::optional<QVector<QVector<QString>>> &cellColours);

    bool generateColumnsForDynamicTemplate(int templateId, const QVector<QString>& groupNames);

private:
    QSqlDatabase &db;
};

#endif // TABLEMANAGER_H
