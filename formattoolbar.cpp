#include "formattoolbar.h"
#include <QApplication>
#include <QActionGroup>
#include <QColorDialog>
#include <QCompleter>
#include <QFontDatabase>
#include <QIntValidator>
#include <QLineEdit>
#include <QTextCursor>


FormatToolBar::FormatToolBar(QWidget *parent)
    : QToolBar(parent) {
    setObjectName("FormatToolBar");
    setWindowTitle(tr("Formatting Panel"));
    setFocusPolicy(Qt::NoFocus);

    // 1. Выбор шрифта
    fontCombo = new QFontComboBox();
    fontCombo->setToolTip("Font Style");
    fontCombo->setEditable(true);  // Разрешаем ввод текста
    fontCombo->setFocusPolicy(Qt::ClickFocus);  // Получать фокус только по клику
    addWidget(fontCombo);

    auto completer = new QCompleter(fontCombo->model(), this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setFilterMode(Qt::MatchContains);
    fontCombo->setCompleter(completer);

    fontCombo->lineEdit()->installEventFilter(this);
    connect(fontCombo->lineEdit(), &QLineEdit::editingFinished,
            this, [this] () {
                applyFontFamily(fontCombo->currentFont());
            });

    // 2. Выбор размера шрифта
    sizeCombo = new QComboBox();
    sizeCombo->setToolTip("Font Size");
    sizeCombo->setEditable(true);
    sizeCombo->setFocusPolicy(Qt::ClickFocus);
    sizeCombo->setInsertPolicy(QComboBox::NoInsert);

    auto validator = new QIntValidator(1, 999, this);
    sizeCombo->setValidator(validator);

    // Заполняем стандартными размерами
    for (int size : QFontDatabase::standardSizes()) {
        sizeCombo->addItem(QString::number(size));
    }
    addWidget(sizeCombo);

    sizeCombo->lineEdit()->installEventFilter(this);
    connect(sizeCombo->lineEdit(), &QLineEdit::editingFinished,
            this, [this] () {
                applyFontSize(sizeCombo->currentText());
            });

    QFont defaultFont("Courier New", 8);
    defaultFont.setStyleHint(QFont::Monospace);
    fontCombo->setCurrentFont(defaultFont);
    sizeCombo->setCurrentText("8");
    if (activeTextEdit) {
        activeTextEdit->setFont(defaultFont);
        activeTextEdit->setFontPointSize(8);
    }

    // 3. Кнопка "Жирный"
    boldAction = new QAction(QIcon(":/icons/bold.png"),"", this);
    boldAction->setCheckable(true);
    boldAction->setToolTip("Bold");
    QFont boldFont = boldAction->font();
    boldFont.setBold(true);
    boldAction->setFont(boldFont);
    addAction(boldAction);
    connect(boldAction, &QAction::triggered, this, &FormatToolBar::toggleBold);

    // 4. Кнопка "Курсив"
    italicAction = new QAction(QIcon(":/icons/italic.png"),"", this);
    italicAction->setCheckable(true);
    italicAction->setToolTip("Italics");
    QFont italicFont = italicAction->font();
    italicFont.setItalic(true);
    italicAction->setFont(italicFont);
    addAction(italicAction);
    connect(italicAction, &QAction::triggered, [this](){
        toggleItalic(italicAction->isChecked());
    });

    // 5. Кнопка "Подчеркнутый"
    underlineAction = new QAction(QIcon(":/icons/underline.png"),"", this);
    underlineAction->setCheckable(true);
    underlineAction->setToolTip("Underlined");
    QFont underlineFont = underlineAction->font();
    underlineFont.setUnderline(true);
    underlineAction->setFont(underlineFont);
    addAction(underlineAction);
    connect(underlineAction, &QAction::triggered, [this](){
        toggleUnderline(underlineAction->isChecked());
    });

    // 6. Кнопки выравнивания
    QActionGroup *alignGroup = new QActionGroup(this);

    leftAlignAction = new QAction(QIcon(":/icons/align_left.png"),"", alignGroup);
    leftAlignAction->setCheckable(true);
    leftAlignAction->setToolTip("Align to the left");
    addAction(leftAlignAction);
    connect(leftAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
    });

    centerAlignAction = new QAction(QIcon(":/icons/align_center.png"),"", alignGroup);
    centerAlignAction->setCheckable(true);
    centerAlignAction->setToolTip("Align to the center");
    addAction(centerAlignAction);
    connect(centerAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignHCenter);
    });

    rightAlignAction = new QAction(QIcon(":/icons/align_right.png"),"", alignGroup);
    rightAlignAction->setCheckable(true);
    rightAlignAction->setToolTip("Align to the right");
    addAction(rightAlignAction);
    connect(rightAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
    });

    justifyAlignAction = new QAction(QIcon(":/icons/align_justify.png"),"", alignGroup);
    justifyAlignAction->setCheckable(true);
    justifyAlignAction->setToolTip("Align to the width");
    addAction(justifyAlignAction);
    connect(justifyAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignJustify);
    });


    // 7. Кнопка "Цвет текста"
    textColorAction = new QAction(QIcon(":/icons/text_color.png"),"", this);
    textColorAction->setToolTip("Font Color");
    addAction(textColorAction);
    connect(textColorAction, &QAction::triggered, [this]() {
        QColor color = QColorDialog::getColor(Qt::black, this, "Выберите цвет текста");
        if (color.isValid()) setTextColor(color);
    });


    // 8. Кнопка "Выделение"
    textFillColorAction = new QAction(QIcon(":/icons/text_highlight.png"),"", this);
    textFillColorAction->setToolTip("Text selection color");
    addAction(textFillColorAction);
    connect(textFillColorAction, &QAction::triggered, [this]() {
        QColor color = QColorDialog::getColor(Qt::white, this, "Выберите цвет выделения текста");
        if (color.isValid()) setTextFillColor(color);
    });

    // 9. Кнопка "Заливка"
    cellFillColorAction = new QAction(QIcon(":/icons/cell_fill.png"),"", this);
    cellFillColorAction->setToolTip("Filling");
    addAction(cellFillColorAction);
    connect(cellFillColorAction, &QAction::triggered,
            this, &FormatToolBar::setCellFillColor);

    // 10. Выбор стиля таблиц
    styleCombo = new QComboBox(this);
    styleCombo->setToolTip("Template Style");
    styleCombo->setEditable(false);
    addWidget(styleCombo);
    QStringList styleNames = {"MyStyle", "Daisy1", "Moonflower", "Pearl", "Printer",
    "Sapphire", "RTF", "PowerPointDark", "PowerPointLight",
    "EGDefault", "HTMLBlue", "Plateau", "Listing", "Minimal",
    "BlockPrint", "Default", "Dove", "HighContrast", "Journal",
    "Journal2", "Journal3", "Raven", "Statistical"};
    styleCombo->addItems(styleNames);
    styleCombo->setCurrentText("MyStyle");
    connect(styleCombo, QOverload<int>::of(&QComboBox::activated),
            this, [this](int index){
                QString styleName = styleCombo->currentText();
                emit styleSelected(styleName);
            });

}

FormatToolBar::~FormatToolBar() {}

// Слоты
void FormatToolBar::applyFontFamily(const QFont &font) {
    QTextCharFormat format;
    format.setFontFamilies(QStringList() << font.family());
    mergeFormatOnWordOrSelection(format);
    emit cellFontFamilyRequested(font);
}
void FormatToolBar::applyFontSize(const QString &sizeText) {
    bool ok = false;
    int size = sizeText.toInt(&ok);
    if (ok && size > 0) {
        QTextCharFormat format;
        format.setFontPointSize(size);
        mergeFormatOnWordOrSelection(format);
        emit cellFontSizeRequested(size);
    }
}
void FormatToolBar::toggleBold() {
    bool on = boldAction->isChecked();
    // если есть активный QTextEdit с выделением — применим в нём
    if (auto *editor = qobject_cast<QTextEdit*>(QApplication::focusWidget())) {
        if (editor->textCursor().hasSelection()) {
            QTextCharFormat fmt;
            fmt.setFontWeight(on ? QFont::Bold : QFont::Normal);
            mergeFormatOnWordOrSelection(fmt);
        }
    }
    // но сигналы кидаем всегда, чтобы TemplatePanel их отловил
    emit cellBoldToggled(on);
}
void FormatToolBar::toggleItalic(bool checked) {
    bool on = italicAction->isChecked();
    if (auto *editor = qobject_cast<QTextEdit*>(QApplication::focusWidget())) {
        if (editor->textCursor().hasSelection()) {
            QTextCharFormat fmt;
            fmt.setFontItalic(on);
            mergeFormatOnWordOrSelection(fmt);
        }
    }
    emit cellItalicToggled(on);
}
void FormatToolBar::toggleUnderline(bool checked) {
    bool on = underlineAction->isChecked();
    if (auto *editor = qobject_cast<QTextEdit*>(QApplication::focusWidget())) {
        if (editor->textCursor().hasSelection()) {
            QTextCharFormat fmt;
            fmt.setFontUnderline(on);
            mergeFormatOnWordOrSelection(fmt);
        }
    }
    emit cellUnderlineToggled(on);
}
void FormatToolBar::setAlignment(Qt::Alignment alignment) {
    // даже если нет activeTextEdit, сигнал кидаем всегда:
    if (auto *ed = activeTextEdit.data()) {
        ed->setAlignment(alignment);
    }
    emit cellAlignmentRequested(alignment);
}
void FormatToolBar::setTextColor(const QColor &color) {
    // Меняем цвет в редакторе, если выделен текст…
    if (auto *ed = activeTextEdit.data()) {
        QTextCursor cur = ed->textCursor();
        if (cur.hasSelection()) {
            ed->setTextCursor(cur);
            QTextCharFormat fmt; fmt.setForeground(QBrush(color));
            mergeFormatOnWordOrSelection(fmt);
        }
    }
    // А сигнал кидаем всегда
    emit cellTextColorRequested(color);
}
void FormatToolBar::setTextFillColor(const QColor &color) {
    // Если нет активного редактора – выходим
    if (!activeTextEdit) return;

    // Сохраняем курсор (со всеми выделениями)
    QTextCursor cursor = activeTextEdit->textCursor();
    if (!cursor.hasSelection()) {
        // Если нет выделения – можно сразу выйти или разрешить менять цвет курсора
        // Но в вашем случае вы хотели, чтобы без выделения цвет не менялся
        return;
    }

    // Возвращаем фокус в редактор (на случай, если диалог сбил фокус)
    activeTextEdit->setFocus(Qt::OtherFocusReason);

    // Восстанавливаем сохранённый курсор (на случай, если диалог сбил выделение)
    activeTextEdit->setTextCursor(cursor);

    QTextCharFormat format;
    format.setBackground(QBrush(color));
    mergeFormatOnWordOrSelection(format);
    emit cellFillRequested(color);
}
void FormatToolBar::setCellFillColor() {
    QColor color = QColorDialog::getColor(Qt::white, this, tr("Выберите цвет заливки"));
    if (color.isValid()) {
        emit cellFillRequested(color);
    }
}

void FormatToolBar::setActiveTextEdit(QTextEdit *editor) {
    activeTextEdit = editor;
    editor->disconnect(this);
    connect(editor, &QTextEdit::cursorPositionChanged, this, &FormatToolBar::updateFormatActions);
    connect(editor, &QTextEdit::currentCharFormatChanged, this, &FormatToolBar::updateFormatActions);
    if (activeTextEdit) {
        QFont f = fontCombo->currentFont();
        f.setPointSize(sizeCombo->currentText().toInt());
        activeTextEdit->setFont(f);
        activeTextEdit->document()->setDefaultFont(f);
        updateFormatActions();
    }

}
void FormatToolBar::setStyleComboText(const QString &styleName) {
    // если в ComboBox есть такой пункт, установим его
    int index = styleCombo->findText(styleName);
    if (index >= 0) {
        styleCombo->setCurrentIndex(index);
    } else {
        // если вдруг нет в списке – можем добавить или fallback’нуть на "Default"
        styleCombo->setCurrentText(styleName);
    }
}
void FormatToolBar::resetState() {
    // Сбросим состояния toggle-кнопок
    if(boldAction) boldAction->setChecked(false);
    if(italicAction) italicAction->setChecked(false);
    if(underlineAction) underlineAction->setChecked(false);
}

// Вспомогательные методы
void FormatToolBar::mergeFormatOnWordOrSelection(const QTextCharFormat &format) {
    // Динамически обновляем activeTextEdit на основе текущего фокуса:
    QWidget *w = QApplication::focusWidget();
    QTextEdit *editor = qobject_cast<QTextEdit*>(w);
    if (editor)
        activeTextEdit = editor;
    else {
        qDebug() << "Нет активного текстового редактора";
        return;  // Если нет редактора – ничего не делаем
    }

    // Если редактор не имеет выделения – просто выходим
    QTextCursor cursor = activeTextEdit->textCursor();
    if (!cursor.hasSelection()) {
        qDebug() << "Нет выделения текста";
        return;
    }

    // Применяем форматирование
    cursor.mergeCharFormat(format);
    activeTextEdit->mergeCurrentCharFormat(format);
}
void FormatToolBar::updateFormatActions() {
    // Если активный редактор отсутствует, выходим
    if (!activeTextEdit)
        return;

    // Получаем текущий формат текста из активного редактора.
    // Если ничего не выделено, берётся формат текущего курсора.
    QTextCharFormat currentFormat = activeTextEdit->currentCharFormat();

    // Обновляем состояние кнопки "Жирный":
    // Если вес шрифта равен QFont::Bold, считаем, что текст жирный.
    bool isBold = (currentFormat.fontWeight() == QFont::Bold);
    boldAction->setChecked(isBold);

    // Обновляем состояние кнопки "Курсив":
    bool isItalic = currentFormat.fontItalic();
    italicAction->setChecked(isItalic);

    // Обновляем состояние кнопки "Подчёркнутый":
    bool isUnderline = currentFormat.fontUnderline();
    underlineAction->setChecked(isUnderline);

    // Обновляем состояние кнопок выравнивания на основе текущего выравнивания редактора.
    // Здесь используются битовые операции, поскольку выравнивание может комбинироваться.
    Qt::Alignment alignment = activeTextEdit->alignment();
    leftAlignAction->setChecked(alignment & Qt::AlignLeft);
    centerAlignAction->setChecked(alignment & Qt::AlignHCenter);
    rightAlignAction->setChecked(alignment & Qt::AlignRight);
    justifyAlignAction->setChecked(alignment & Qt::AlignJustify);

    // Дополнительно: обновляем комбобоксы для выбора шрифта и размера
    // Обновляем выбор шрифта: ищем в fontCombo текущий шрифт редактора
    QFont currentFont = currentFormat.font();
    int fontIndex = fontCombo->findText(currentFont.family());
    if (fontIndex != -1) {
        fontCombo->setCurrentIndex(fontIndex);
    }
    // Обновляем выбор размера шрифта: ищем в sizeCombo размер шрифта в пунктах
    QString fontSizeStr = QString::number(currentFont.pointSize());
    int sizeIndex = sizeCombo->findText(fontSizeStr);
    if (sizeIndex != -1) {
        sizeCombo->setCurrentIndex(sizeIndex);
    }
}
