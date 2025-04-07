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
    ~TemplateManager();

    bool createTemplate(int categoryId, const QString &templateName, const QString &templateType);
    bool updateTemplate(int templateId,
                        const std::optional<QString> &name,
                        const std::optional<QString> &notes,
                        const std::optional<QString> &programmingNotes);
    bool deleteTemplate(int templateId);

    bool copyGraphFromLibrary(const QString &graphTypeKey, int newTemplateId);
    bool updateGraphFromLibrary(const QString &graphTypeKey, int templateId);

    QVector<int> getDynamicTemplatesForProject(int projectId);
    QVector<Template> getTemplatesForCategory(int categoryId);    // Получение шаблонов по категории
    QVector<QString> getColumnHeadersForTemplate(int templateId); // Получение заголовков столбцов в шаблоне
    QVector<int> getRowOrdersForTemplate(int templateId);         // Получение количества строк для шаблона
    QVector<int> getColumnOrdersForTemplate(int templateId);      // Получение количества столбцов для шаблона
    QVector<QVector<QPair<QString, QString>>> getTableData(int templateId);            // Получение данных таблицы для шаблона
    QString getNotesForTemplate(int templateId);                  // Получение заметок
    QString getProgrammingNotesForTemplate(int templateId);       // Получение программных заметок

    QString getTemplateType(int templateId);
    QByteArray getGraphImage(int templateId);
    QStringList getGraphTypesFromLibrary();

    int getLastCreatedTemplateId() const;

private:
    QSqlDatabase &db;
    int lastCreatedTemplateId = -1;
};

#endif // TEMPLATEMANAGER_H
