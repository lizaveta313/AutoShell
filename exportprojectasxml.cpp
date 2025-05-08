#include "exportprojectasxml.h"
#include <QList>
#include <QFileDialog>
#include <QTextDocument>
#include <QRegularExpression>
#include <QStandardPaths>

ExportProjectAsXml::ExportProjectAsXml(ProjectManager* projectManager,
                                       CategoryManager* categoryManager,
                                       TemplateManager* templateManager,
                                       TableManager* tableManager)
    : projectManager(projectManager),
    categoryManager(categoryManager),
    templateManager(templateManager),
    tableManager(tableManager) {}

bool ExportProjectAsXml::exportProject(int projectId, const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("MAIN");

    writeProjectBlock(xml, projectId);
    dumpCategory(xml, projectId, QVariant(), QString(), QList<Category>());

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
                                      const QString& path,
                                      const QList<Category>& ancestors) {
    const auto cats = categoryManager->getCategoriesByProjectAndParent(projectId, parentId);
    for (const Category& cat : cats) {
        // добавить этого предка в цепочку
        QList<Category> chain = ancestors;
        chain.append(cat);

        // обновить путь
        QString catPath = path.isEmpty()
                              ? QString::number(cat.position)
                              : path + '.' + QString::number(cat.position);

        // получить все шаблоны в этой категории
        const auto tmpls = templateManager->getTemplatesForCategory(cat.categoryId);
        // обход шаблонов
        for (int idx = 0; idx < tmpls.size(); ++idx) {
            const Template& t = tmpls[idx];

            // полный и подчёркнутый путь
            QString fullPath = catPath + '.' + QString::number(idx + 1);
            QString underscored = fullPath;
            underscored.replace('.', '_');

            // определить теги
            QString type = templateManager->getTemplateType(t.templateId);
            QString tag    = (type == "listing" ? "LISTING" : (type == "table" ? "TABLE" : "GRATH"));
            QString prefix = (type == "listing" ? "List" : (type == "table" ? "Tab" : "Fig"));

            if (type == "table" || type == "listing") {
                // получить матрицу таблицы или листинга
                TableMatrix mtx = templateManager->getTableData(t.templateId);
                if (mtx.isEmpty())
                    continue;

                // для объединения ячеек
                auto findOwner = [&](int r, int c) -> QPair<int,int>
                {
                    for (int rr = r; rr >= 0; --rr)
                        for (int cc = c; cc >= 0; --cc) {
                            const Cell& probe = mtx[rr][cc];
                            if (probe.rowSpan > 0 && probe.colSpan > 0) {
                                bool inRowSpan = rr + probe.rowSpan - 1 >= r;
                                bool inColSpan = cc + probe.colSpan - 1 >= c;
                                if (inRowSpan && inColSpan)
                                    return { rr, cc };           // нашли владельца
                            }
                        }
                    return { r, c };                              // fallback
                };

                // сколько строк — заголовков
                int headerRows = tableManager->getRowCountForHeader(t.templateId);

                //  Заголовки в <TABLE>…</TABLE> или <LISTING>…</LISTING>
                for (int hr = 0; hr < headerRows; ++hr) {
                    if (hr == 0) {
                        // строка заглушка "x"
                        xml.writeStartElement(tag);
                        for (int lvl = 0; lvl < chain.size(); ++lvl)
                            xml.writeTextElement(QString("CHAPTER%1").arg(lvl+1), "x");
                        xml.writeTextElement("TabID",     "x");
                        xml.writeTextElement("TabName",   "x");
                        xml.writeTextElement("Path",      "x");
                        xml.writeTextElement("Subtitle",  "x");
                        xml.writeTextElement("Notes",     "x");
                        xml.writeTextElement("ProgNotes", "x");
                        xml.writeTextElement("color",     "x");
                        xml.writeTextElement("OrderHeader","x");
                        for (int c = 0; c < mtx[hr].size(); ++c) {
                            const QString base = QString("ColHeader%1").arg(c+1);
                            xml.writeTextElement(base,               "x");
                            xml.writeTextElement(base+"StyleBold",       "x");
                            xml.writeTextElement(base+"StyleItalic",     "x");
                            xml.writeTextElement(base+"StyleUnderline",  "x");
                            xml.writeTextElement(base+"Align",           "x");
                        }
                        xml.writeEndElement();
                        // первый заголовок
                        xml.writeStartElement(tag);
                        for (int lvl = 0; lvl < chain.size(); ++lvl)
                            xml.writeTextElement(QString("CHAPTER%1").arg(lvl+1),
                                                 stripHtml(categoryManager->getCategoryName(chain[lvl].categoryId)));
                        xml.writeTextElement("TabID",   QString("%1_%2").arg(prefix, underscored));
                        xml.writeTextElement("TabName", stripHtml(fullPath + ' ' + t.name));
                        xml.writeTextElement("Path",    fullPath);
                        xml.writeTextElement("Subtitle",
                                             stripHtml(templateManager->getSubtitleForTemplate(t.templateId)));
                        xml.writeTextElement("Notes",
                                             stripHtml(templateManager->getNotesForTemplate(t.templateId)));
                        QString progHtml = templateManager->getProgrammingNotesForTemplate(t.templateId);
                        xml.writeTextElement("ProgNotes", stripHtml(progHtml));
                        QRegularExpression cre("color\\s*:\\s*(#[0-9A-Fa-f]{6})");
                        auto m = cre.match(progHtml);
                        xml.writeTextElement("color", m.hasMatch() ? m.captured(1) : QString());
                        xml.writeTextElement("OrderHeader", QString("%1").arg(hr+1,3,10,QChar('0')));
                        for (int c = 0; c < mtx[hr].size(); ++c) {
                            auto own = findOwner(hr, c);
                            QString h = mtx[own.first][own.second].text;
                            xml.writeTextElement(QString("ColHeader%1").arg(c+1), stripHtml(h));
                            writeCellStyles(xml, h, "ColHeader", c+1);
                        }
                        xml.writeEndElement();
                    } else {
                        // последующие заголовки
                        xml.writeStartElement(tag);
                        xml.writeTextElement("OrderHeader", QString("%1").arg(hr+1,3,10,QChar('0')));
                        for (int c = 0; c < mtx[hr].size(); ++c) {
                            auto own = findOwner(hr, c);
                            QString h = mtx[own.first][own.second].text;
                            xml.writeTextElement(QString("ColHeader%1").arg(c+1), stripHtml(h));
                            writeCellStyles(xml, h, "ColHeader", c+1);
                        }
                        xml.writeEndElement(); // </TABLE> или </LISTING>
                    }
                }

                // Данные строк в <TABLE_SHELLS>…</TABLE_SHELLS>
                for (int r = headerRows; r < mtx.size(); ++r) {
                    if (r == headerRows) {
                        // placeholder
                        xml.writeStartElement(tag + "_SHELLS");
                        xml.writeTextElement("TabID", "x");
                        xml.writeTextElement("Order", "x");
                        for (int c = 0; c < mtx[r].size(); ++c) {
                            const QString base = QString("Col%1").arg(c+1);
                            xml.writeTextElement(base,               "x");
                            xml.writeTextElement(base+"StyleBold",       "x");
                            xml.writeTextElement(base+"StyleItalic",     "x");
                            xml.writeTextElement(base+"StyleUnderline",  "x");
                            xml.writeTextElement(base+"Align",           "x");
                        }
                        xml.writeEndElement();
                        // actual first row
                        xml.writeStartElement(tag + "_SHELLS");
                        xml.writeTextElement("TabID", QString("%1_%2").arg(prefix, underscored));
                        xml.writeTextElement("Order", QString("%1").arg(r-headerRows+1,3,10,QChar('0')));
                        for (int c = 0; c < mtx[r].size(); ++c) {
                            auto own = findOwner(r, c);
                            QString cell = mtx[own.first][own.second].text;
                            xml.writeTextElement(QString("Col%1").arg(c+1), stripHtml(cell));
                            writeCellStyles(xml, cell, "Col", c+1);
                        }
                        xml.writeEndElement();
                    } else {
                        xml.writeStartElement(tag + "_SHELLS");
                        xml.writeTextElement("Order", QString("%1").arg(r-headerRows+1,3,10,QChar('0')));
                        for (int c = 0; c < mtx[r].size(); ++c) {
                            auto own = findOwner(r, c);
                            QString cell = mtx[own.first][own.second].text;
                            xml.writeTextElement(QString("Col%1").arg(c+1), stripHtml(cell));
                            writeCellStyles(xml, cell, "Col", c+1);
                        }
                        xml.writeEndElement(); // </TABLE_SHELLS> или </LISTING_SHELLS>
                    }
                }
            } else {
                xml.writeStartElement(tag);

                // строка заглушка для графиков
                for (int lvl = 0; lvl < chain.size(); ++lvl)
                    xml.writeTextElement(QString("CHAPTER%1").arg(lvl+1),   "x");
                xml.writeTextElement("TabID",     "x");
                xml.writeTextElement("TabName",   "x");
                xml.writeTextElement("Order",     "1");
                xml.writeTextElement("Subtitle",  "x");
                xml.writeTextElement("Notes",     "x");
                xml.writeTextElement("ProgNotes", "x");
                xml.writeTextElement("color",     "x");
                xml.writeTextElement("grtype",     "x");
                xml.writeEndElement();

                xml.writeStartElement(tag);
                // вывод основной информации по графику
                for (int lvl = 0; lvl < chain.size(); ++lvl) {
                    xml.writeTextElement(
                        QString("CHAPTER%1").arg(lvl+1),
                        stripHtml(categoryManager->getCategoryName(chain[lvl].categoryId))
                        );
                }
                xml.writeTextElement("TabID", QString("%1_%2").arg(prefix, underscored));
                xml.writeTextElement("TabName", stripHtml(t.name));
                xml.writeTextElement("Order", QString::number(1));
                xml.writeTextElement("Subtitle",
                                     stripHtml(templateManager->getSubtitleForTemplate(t.templateId)));
                xml.writeTextElement("Notes",
                                     stripHtml(templateManager->getNotesForTemplate(t.templateId)));
                QString progHtml = templateManager->getProgrammingNotesForTemplate(t.templateId);
                xml.writeTextElement("ProgNotes", stripHtml(progHtml));
                QRegularExpression colorRe("color\\s*:\\s*(#[0-9A-Fa-f]{6})");
                auto match = colorRe.match(progHtml);
                xml.writeTextElement("color", match.hasMatch() ? match.captured(1) : QString());
                xml.writeTextElement("grtype",
                                     stripHtml(templateManager->getGraphType(t.templateId)));
                xml.writeEndElement();
            }
        }

        // рекурсия
        dumpCategory(xml, projectId, cat.categoryId, catPath, chain);
    }
}


QString ExportProjectAsXml::stripHtml(const QString& html) const {
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toPlainText().trimmed();
}

void ExportProjectAsXml::writeCellStyles(QXmlStreamWriter& xml,
                                         const QString& htmlCell,
                                         const QString& tagBase,
                                         int colIndex) {
    // detect <b> or <strong>
    QRegularExpression reB1("<b\\b[^>]*>", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reB2("<strong\\b", QRegularExpression::CaseInsensitiveOption);
    // detect font-weight: bold или font-weight: 600..900
    QRegularExpression reCssBold("font-weight\\s*:\\s*(bold|[6-9]00)", QRegularExpression::CaseInsensitiveOption);
    bool bold = htmlCell.contains(reB1) || htmlCell.contains(reB2) || htmlCell.contains(reCssBold);

    // italic как было
    QRegularExpression reI("<i\\b|<em\\b", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reCssIt("font-style\\s*:\\s*italic", QRegularExpression::CaseInsensitiveOption);
    bool italic = htmlCell.contains(reI) || htmlCell.contains(reCssIt);

    // detect <u> и CSS text-decoration: underline
    QRegularExpression reU("<u\\b", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reCssUnder("text-decoration\\s*:\\s*underline", QRegularExpression::CaseInsensitiveOption);
    bool underline = htmlCell.contains(reU) || htmlCell.contains(reCssUnder);

    // detect alignment: left/right/center/justify → l/r/c/j
    QRegularExpression reAlignHtml("align\\s*=\\s*['\"]?(left|right|center|justify)['\"]?", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reCssAlign("text-align\\s*:\\s*(left|right|center|justify)", QRegularExpression::CaseInsensitiveOption);
    QString align = "l";
    auto m1 = reAlignHtml.match(htmlCell);
    auto m2 = reCssAlign.match(htmlCell);
    if (m1.hasMatch() || m2.hasMatch()) {
        QString val = (m1.hasMatch() ? m1.captured(1) : m2.captured(1)).toLower();
    if (val == "left")    align = "l";
        else if (val == "right")   align = "r";
        else if (val == "center")  align = "c";
        else if (val == "justify") align = "c";
        // в связи с особенностями проекта выравнивание по ширине в .xml будет определятся как по центру
    }


    xml.writeTextElement(QString("%1%2StyleBold").arg(tagBase).arg(colIndex),
                         bold ? "Y" : QString());
    xml.writeTextElement(QString("%1%2StyleItalic").arg(tagBase).arg(colIndex),
                         italic ? "Y" : QString());
    xml.writeTextElement(QString("%1%2StyleUnderline").arg(tagBase).arg(colIndex),
                         underline ? "Y" : QString());
    xml.writeTextElement(QString("%1%2Align").arg(tagBase).arg(colIndex), align);
}

