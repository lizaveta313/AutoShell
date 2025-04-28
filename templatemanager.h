#ifndef TEMPLATEMANAGER_H
#define TEMPLATEMANAGER_H

#include <QVector>
#include <QString>
#include <optional>
#include <QSqlDatabase>

struct Template {
    int templateId;
    QString name;
    QString subtitle;
    QString notes;
    QString programmingNotes;
    int position;
    int categoryId;
};

struct Cell {
    QString text      = "";
    QString colour    = "#FFFFFF";
    int     rowSpan   = 1;
    int     colSpan   = 1;
};

using TableMatrix = QVector<QVector<Cell>>;

class TemplateManager {
public:
    TemplateManager(QSqlDatabase &db);
    ~TemplateManager();

    bool createTemplate(int categoryId, const QString &templateName, const QString &templateType);
    bool duplicateTemplate(int srcTemplateId, const QString &newName, int &newIdOut);
    bool updateTemplate(int templateId,
                        const std::optional<QString> &name,
                        const std::optional<QString> &subtitle,
                        const std::optional<QString> &notes,
                        const std::optional<QString> &programmingNotes);
    bool deleteTemplate(int templateId);

    bool copyGraphFromLibrary(const QString &graphTypeKey, int newTemplateId);
    bool updateGraphFromLibrary(const QString &graphTypeKey, int templateId);

    bool setTemplateDynamic(int templateId, bool dynamic);
    bool isTemplateDynamic(int templateId);

    bool updateTemplateCategory(int templateId, int newCategoryId);
    bool updateTemplatePosition(int templateId, int position);

    QVector<int> getDynamicTemplatesForProject(int projectId);
    QVector<Template> getTemplatesForCategory(int categoryId);    // Получение шаблонов по категории

    TableMatrix getTableData(int templateId);

    QString getSubtitleForTemplate(int templateId);               // Получение подзаголовков
    QString getNotesForTemplate(int templateId);                  // Получение заметок
    QString getProgrammingNotesForTemplate(int templateId);       // Получение программных заметок

    QString getTemplateType(int templateId);
    QByteArray getGraphImage(int templateId);
    QString getGraphType(int templateId);
    QStringList getGraphTypesFromLibrary();

    int getLastCreatedTemplateId() const;

private:
    QSqlDatabase &db;
    int lastCreatedTemplateId = -1;
};

#endif // TEMPLATEMANAGER_H
