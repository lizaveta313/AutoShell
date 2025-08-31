#include "exportprojectasxml.h"
#include <QList>
#include <QFileDialog>
#include <QTextDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDate>

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
    ProjectDetails details = projectManager->getProjectDetails(projectId);
    writeProj("Client", details.sponsor, "Top Left");
    writeProj("Study",  details.study, "Top Right");
    writeProj("Version", details.version, "Bottom Middle");
    writeProj("CutDate", details.cutDate.toString("dd.MM.yyyy"), "Footnote");
    writeProj("dtfolder", projectManager->getProjectName(projectId), "");
    writeProj("TempleteStyle", projectManager->getProjectStyle(projectId), "");
}

void ExportProjectAsXml::dumpCategory(QXmlStreamWriter& xml,
                                      int projectId,
                                      const QVariant& parentId,
                                      const QString& path,
                                      const QList<Category>& ancestors) {
    bool firstTableOrListing = true;
    bool firstGraph = true;

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
                int maxColumns = mtx.isEmpty() ? 0 : mtx[0].size();


                //  Заголовки в <TABLE>…</TABLE> или <LISTING>…</LISTING>
                for (int hr = 0; hr < headerRows; ++hr) {
                    if (firstTableOrListing && hr == 0) {
                        // строка заглушка "x" только для первой таблицы/листинга
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
                        xml.writeTextElement("font", "x");
                        xml.writeTextElement("fontsize", "x");
                        xml.writeTextElement("nestedheader", "x");
                        xml.writeTextElement("columns", "x");
                        for (int c = 0; c < maxColumns; ++c) {
                            const QString base = QString("ColHeader%1").arg(c+1); // Keep original for placeholder
                            xml.writeTextElement(base,               "x");
                            xml.writeTextElement(QString("ColHeaderStyleBold%1").arg(c+1),       "x");
                            xml.writeTextElement(QString("ColHeaderStyleItalic%1").arg(c+1),     "x");
                            xml.writeTextElement(QString("ColHeaderStyleUnderline%1").arg(c+1),  "x");
                            xml.writeTextElement(QString("ColHeaderAlign%1").arg(c+1),           "x");
                        }
                        xml.writeEndElement();
                        firstTableOrListing = false; // Mark as no longer the first
                    }
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

                    // Font and Fontsize
                    QRegularExpression fontRe("font-family\\s*:\\s*([^;]+)");
                    QRegularExpression fontSizeRe("font-size\\s*:\\s*([^;]+)");
                    auto fontMatch = fontRe.match(progHtml);
                    auto fontSizeMatch = fontSizeRe.match(progHtml);
                    xml.writeTextElement("font", fontMatch.hasMatch() ? stripHtml(fontMatch.captured(1)) : QString());
                    xml.writeTextElement("fontsize", fontSizeMatch.hasMatch() ? stripHtml(fontSizeMatch.captured(1)) : QString());
                    xml.writeTextElement("nestedheader", QString::number(headerRows));
                    xml.writeTextElement("columns", QString::number(maxColumns));

                    for (int c = 0; c < mtx[hr].size(); ++c) {
                        auto own = findOwner(hr, c);
                        QString h = mtx[own.first][own.second].text;
                        xml.writeTextElement(QString("ColHeader%1").arg(c+1), stripHtml(h)); // Renamed here
                        writeCellStyles(xml, h, "ColHeader", c+1);
                    }
                    xml.writeEndElement();
                }

                // Данные строк в <TABLE_SHELLS>…</TABLE_SHELLS>
                for (int r = headerRows; r < mtx.size(); ++r) {
                    if (firstTableOrListing && r == headerRows) { // This condition will likely not be true due to previous flag reset
                        // placeholder
                        xml.writeStartElement(tag + "_SHELLS");
                        xml.writeTextElement("TabID", "x");
                        xml.writeTextElement("Order", "x");
                        xml.writeTextElement("commontext", "x"); // Placeholder for commontext
                        for (int c = 0; c < maxColumns; ++c) {
                            xml.writeTextElement(QString("Col%1").arg(c+1),               "x"); // Renamed here
                            xml.writeTextElement(QString("ColStyleBold%1").arg(c+1),       "x");
                            xml.writeTextElement(QString("ColStyleItalic%1").arg(c+1),     "x");
                            xml.writeTextElement(QString("ColStyleUnderline%1").arg(c+1),  "x");
                            xml.writeTextElement(QString("ColAlign%1").arg(c+1),           "x");
                        }
                        xml.writeEndElement();
                    }
                    // actual first row
                    xml.writeStartElement(tag + "_SHELLS");
                    xml.writeTextElement("TabID", QString("%1_%2").arg(prefix, underscored));
                    xml.writeTextElement("Order", QString("%1").arg(r-headerRows+1,3,10,QChar('0')));

                    bool rowHasMergedCells = false;
                    for (int c = 0; c < mtx[r].size(); ++c) {
                        auto own = findOwner(r, c);
                        if (mtx[own.first][own.second].colSpan > 1) {
                            rowHasMergedCells = true;
                            break;
                        }
                    }
                    if (rowHasMergedCells) {
                        xml.writeTextElement("commontext", "Y");
                    }

                    for (int c = 0; c < mtx[r].size(); ++c) {
                        auto own = findOwner(r, c);
                        QString cell = mtx[own.first][own.second].text;
                        xml.writeTextElement(QString("Col%1").arg(c+1), stripHtml(cell)); // Renamed here
                        writeCellStyles(xml, cell, "Col", c+1);
                    }
                    xml.writeEndElement();
                }
            } else { // GRATH
                if (firstGraph) {
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
                    xml.writeTextElement("font", "x");
                    xml.writeTextElement("fontsize", "x");
                    xml.writeEndElement();
                    firstGraph = false; // Mark as no longer the first
                }

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

                // Font and Fontsize for graphs
                QRegularExpression fontRe("font-family\\s*:\\s*([^;]+)");
                QRegularExpression fontSizeRe("font-size\\s*:\\s*([^;]+)");
                auto fontMatch = fontRe.match(progHtml);
                auto fontSizeMatch = fontSizeRe.match(progHtml);
                xml.writeTextElement("font", fontMatch.hasMatch() ? stripHtml(fontMatch.captured(1)) : QString());
                xml.writeTextElement("fontsize", fontSizeMatch.hasMatch() ? stripHtml(fontSizeMatch.captured(1)) : QString());

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
    QString plain = doc.toPlainText().trimmed();
    return plain.replace(QRegularExpression("[\\r\\n]+"), "~");
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


    xml.writeTextElement(QString("%1StyleBold%2").arg(tagBase).arg(colIndex),
                         bold ? "Y" : QString());
    xml.writeTextElement(QString("%1StyleItalic%2").arg(tagBase).arg(colIndex),
                         italic ? "Y" : QString());
    xml.writeTextElement(QString("%1StyleUnderline%2").arg(tagBase).arg(colIndex),
                         underline ? "Y" : QString());
    xml.writeTextElement(QString("%1Align%2").arg(tagBase).arg(colIndex), align);
}

