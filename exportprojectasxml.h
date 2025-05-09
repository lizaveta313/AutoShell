#ifndef EXPORTPROJECTASXML_H
#define EXPORTPROJECTASXML_H

#include <QString>
#include <QVariant>
#include <QXmlStreamWriter>
#include "projectmanager.h"
#include "categorymanager.h"
#include "templatemanager.h"
#include "tablemanager.h"

class ExportProjectAsXml {
public:
    ExportProjectAsXml(ProjectManager* projectManager,
                       CategoryManager* categoryManager,
                       TemplateManager* templateManager,
                       TableManager* tableManager);

    bool exportProject(int projectId, const QString& filename);
private:
    ProjectManager*  projectManager;
    CategoryManager* categoryManager;
    TemplateManager* templateManager;
    TableManager* tableManager;

    void writeProjectBlock(QXmlStreamWriter& xml, int projectId);
    void dumpCategory(QXmlStreamWriter& xml,
                      int projectId,
                      const QVariant& parentId,
                      const QString& path,
                      const QList<Category>& ancestors);
    QString stripHtml(const QString& html) const;
    void writeCellStyles(QXmlStreamWriter& xml,
                         const QString& htmlCell,
                         const QString& tagBase,
                         int colIndex);
};

#endif // EXPORTPROJECTASXML_H
