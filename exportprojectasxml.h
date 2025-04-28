#ifndef EXPORTPROJECTASXML_H
#define EXPORTPROJECTASXML_H

#include <QString>
#include <QVariant>
#include <QXmlStreamWriter>
#include "projectManager.h"
#include "categoryManager.h"
#include "templateManager.h"

class ExportProjectAsXml {
public:
    ExportProjectAsXml(ProjectManager* projectManager,
                       CategoryManager* categoryManager,
                       TemplateManager* templateManager);

    bool exportProject(int projectId, const QString& filename);
private:
    ProjectManager*  projectManager;
    CategoryManager* categoryManager;
    TemplateManager* templateManager;

    void writeProjectBlock(QXmlStreamWriter& xml, int projectId);
    void dumpCategory(QXmlStreamWriter& xml,
                      int projectId,
                      const QVariant& parentId,
                      const QString& path);
    QString stripHtml(const QString& html) const;
    void writeCellStyles(QXmlStreamWriter& xml,
                         const QString& htmlCell,
                         int colIndex);
};

#endif // EXPORTPROJECTASXML_H
