#ifndef TEMPLATEMANAGER_H
#define TEMPLATEMANAGER_H

#include <QVector>
#include <QString>
#include <optional>
#include <QSqlDatabase>

struct Template {
    int templateId;
    QString name;
    QString notes;
    QString programmingNotes;
    int position;
    int categoryId;
};

class TemplateManager {
public:
    TemplateManager(QSqlDatabase &db);

    bool createTemplate(int categoryId, const QString &templateName);
    bool updateTemplate(int templateId,
                        const std::optional<QString> &name,
                        const std::optional<QString> &notes,
                        const std::optional<QString> &programmingNotes);
    bool deleteTemplate(int templateId);

    QVector<Template> getTemplatesForCategory(int categoryId);    // Получение шаблонов по категории
    QVector<QString> getColumnHeadersForTemplate(int templateId); // Получение заголовков столбцов в шаблоне
    QVector<int> getRowOrdersForTemplate(int templateId);         // Получение количества строк для шаблона
    QVector<int> getColumnOrdersForTemplate(int templateId);      // Получение количества столбцов для шаблона
    QVector<QStringList> getTableData(int templateId);            // Получение данных таблицы для шаблона
    QString getNotesForTemplate(int templateId);                  // Получение заметок
    QString getProgrammingNotesForTemplate(int templateId);       // Получение программных заметок
private:
    QSqlDatabase &db;
};

#endif // TEMPLATEMANAGER_H
