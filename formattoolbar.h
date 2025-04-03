#ifndef FORMATTOOLBAR_H
#define FORMATTOOLBAR_H

#include <QToolBar>
#include <QColor>
#include <QFontComboBox>
#include <QComboBox>
#include <QAction>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QPointer>


class FormatToolBar : public QToolBar
{
    Q_OBJECT

public:

    explicit FormatToolBar(QWidget *parent = nullptr);
    ~FormatToolBar();


public slots:
    void setActiveTextEdit(QTextEdit *editor);
    void setStyleComboText(const QString &styleName);

private slots:

    void applyFontFamily(const QFont &font);
    void applyFontSize(const QString &sizeText);
    void toggleBold();
    void toggleItalic(bool checked);
    void toggleUnderline(bool checked);
    void setAlignment(Qt::Alignment alignment);
    void setTextColor(const QColor &color);
    void setTextFillColor(const QColor &color);
    void setCellFillColor();

signals:
    void cellFillRequested(const QColor &color);
    void styleSelected(const QString &styleName);

private:

    void mergeFormatOnWordOrSelection(const QTextCharFormat &format);
    void updateFormatActions();

private:

    // Элементы форматирования
    QFontComboBox *fontCombo;
    QComboBox     *sizeCombo;
    QComboBox *styleCombo;
    QAction *boldAction;
    QAction *italicAction;
    QAction *underlineAction;
    QAction *leftAlignAction;
    QAction *centerAlignAction;
    QAction *rightAlignAction;
    QAction *justifyAlignAction;
    QAction *textColorAction;
    QAction *textFillColorAction;
    QAction *cellFillColorAction;

    QPointer<QTextEdit> activeTextEdit;
};

#endif // FORMATTOOLBAR_H
