#include "exportprojectasxml.h"
#include <QFileDialog>
#include <QTextDocument>
#include <QRegularExpression>
#include <QStandardPaths>

ExportProjectAsXml::ExportProjectAsXml(ProjectManager* projectManager,
                                       CategoryManager* categoryManager,
                                       TemplateManager* templateManager)
    : projectManager(projectManager)
    , categoryManager(categoryManager)
    , templateManager(templateManager) {

}

bool ExportProjectAsXml::exportProject(int projectId, const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("MAIN");

    writeProjectBlock(xml, projectId);
    dumpCategory(xml, projectId, QVariant(), QString());

    xml.writeEndElement(); // MAIN
    xml.writeEndDocument();
    file.close();
    return true;
}

void ExportProjectAsXml::writeProjectBlock(QXmlStreamWriter& xml, int projectId) {
    auto writeProj = [&](const QString& var, const QString& val, const QString& pos) {
        xml.writeStartElement("PROJECT");
        xml.writeTextElement("Variable", var);
        xml.writeTextElement("Value", val);
        xml.writeTextElement("Position", pos);
        xml.writeEndElement();
    };
    QString name = projectManager->getProjectName(projectId);
    writeProj("Client", name, "Top Left");
    writeProj("Study",  name, "Top Right");
    writeProj("Version","1",    "Bottom Middle");
    writeProj("CutDate","01.01.2025","Footnote");
    writeProj("dtfolder", name, "");
    writeProj("TempleteStyle", projectManager->getProjectStyle(projectId), "");
}

void ExportProjectAsXml::dumpCategory(QXmlStreamWriter& xml,
                                      int projectId,
                                      const QVariant& parentId,
                                      const QString& path) {
    const auto cats = categoryManager->getCategoriesByProjectAndParent(projectId, parentId);
    for (const Category& cat : cats) {
        QString catPath = path.isEmpty() ? QString::number(cat.position)
                                          : path + '.' + QString::number(cat.position);
        const auto tmpls = templateManager->getTemplatesForCategory(cat.categoryId);
        for (int idx = 0; idx < tmpls.size(); ++idx) {
            const Template& t = tmpls[idx];
            QString fullPath = catPath + '.' + QString::number(idx + 1);
            QString underscored = fullPath;
            underscored.replace('.', '_');

            QString type = templateManager->getTemplateType(t.templateId);
            QString tag, prefix;
            if (type == "table")       { tag = "TABLE";   prefix = "Tab";  }
            else if (type == "listing") { tag = "LISTING"; prefix = "List"; }
            else                          { tag = "GRATH";   prefix = "Fig";  }

            xml.writeStartElement(tag);
            xml.writeTextElement("TabID", QString("%1_%2").arg(prefix, underscored));

            if (type == "table" || type == "listing") {
                xml.writeTextElement("TabName", stripHtml(fullPath + ' ' + t.name));
                xml.writeTextElement("Path",    fullPath);
            } else {
                xml.writeTextElement("TabName", stripHtml(t.name));
                xml.writeTextElement("order",   QString::number(1));
            }

            xml.writeTextElement("Subtitle",
                stripHtml(templateManager->getSubtitleForTemplate(t.templateId)));
            xml.writeTextElement("Notes",
                stripHtml(templateManager->getNotesForTemplate(t.templateId)));

            QString progHtml = templateManager->getProgrammingNotesForTemplate(t.templateId);
            xml.writeTextElement("ProgNotes", stripHtml(progHtml));
            QRegularExpression colorRe("color\\s*:\\s*(#[0-9A-Fa-f]{6})");
            auto match = colorRe.match(progHtml);
            xml.writeTextElement("color", match.hasMatch() ? match.captured(1) : QString());

            if (type == "table" || type == "listing") {
                auto mtx = templateManager->getTableData(t.templateId);
                if (!mtx.isEmpty()) {
                    for (int c = 0; c < mtx[0].size(); ++c) {
                        QString htmlHdr = mtx[0][c].text;
                        xml.writeTextElement(QString("ColHeader%1").arg(c+1),
                                             stripHtml(htmlHdr));
                        xml.writeEmptyElement(QString("ColHeader%1StyleBold").arg(c+1));
                        xml.writeEmptyElement(QString("ColHeader%1StyleItalic").arg(c+1));
                        xml.writeEmptyElement(QString("ColHeader%1StyleUnderline").arg(c+1));
                    }
                    for (int r = 1; r < mtx.size(); ++r) {
                        xml.writeTextElement("Order",
                            QString("%1").arg(r, 3, 10, QChar('0')));
                        for (int c = 0; c < mtx[r].size(); ++c) {
                            QString htmlCell = mtx[r][c].text;
                            xml.writeTextElement(QString("Col%1").arg(c+1),
                                                  stripHtml(htmlCell));
                            writeCellStyles(xml, htmlCell, c+1);
                        }
                    }
                }
            } else {
                xml.writeTextElement("grtype",
                    stripHtml(templateManager->getGraphType(t.templateId)));
            }
            xml.writeEndElement();
        }
        dumpCategory(xml, projectId, cat.categoryId, catPath);
    }
}

QString ExportProjectAsXml::stripHtml(const QString& html) const {
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toPlainText().trimmed();
}

void ExportProjectAsXml::writeCellStyles(QXmlStreamWriter& xml,
                                         const QString& htmlCell,
                                         int colIndex) {
    bool bold = htmlCell.contains("<b>", Qt::CaseInsensitive)
             || htmlCell.contains("<strong>", Qt::CaseInsensitive)
             || htmlCell.contains("font-weight:bold", Qt::CaseInsensitive);
    bool italic = htmlCell.contains("<i>", Qt::CaseInsensitive)
               || htmlCell.contains("<em>", Qt::CaseInsensitive)
               || htmlCell.contains("font-style:italic", Qt::CaseInsensitive);
    bool underline = htmlCell.contains("<u>", Qt::CaseInsensitive)
                  || htmlCell.contains("text-decoration:underline", Qt::CaseInsensitive);
    xml.writeTextElement(QString("Col%1StyleBold").arg(colIndex),
                          bold ? "Y" : QString());
    xml.writeTextElement(QString("Col%1StyleItalic").arg(colIndex),
                          italic ? "Y" : QString());
    xml.writeTextElement(QString("Col%1StyleUnderline").arg(colIndex),
                          underline ? "Y" : QString());
}
