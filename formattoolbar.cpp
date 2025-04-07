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
    setWindowTitle(tr("Панель форматирования"));
    setFocusPolicy(Qt::NoFocus);

    // 1. Выбор шрифта
    fontCombo = new QFontComboBox();
    fontCombo->setToolTip("Стиль шрифта");
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
    sizeCombo->setToolTip("Размер шрифта");
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

    // 3. Кнопка "Жирный"
    boldAction = new QAction(QIcon(":/icons/bold.png"),"", this);
    boldAction->setCheckable(true);
    boldAction->setToolTip("Жирный");
    QFont boldFont = boldAction->font();
    boldFont.setBold(true);
    boldAction->setFont(boldFont);
    addAction(boldAction);
    connect(boldAction, &QAction::triggered, this, &FormatToolBar::toggleBold);

    // 4. Кнопка "Курсив"
    italicAction = new QAction(QIcon(":/icons/italic.png"),"", this);
    italicAction->setCheckable(true);
    italicAction->setToolTip("Курсив");
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
    underlineAction->setToolTip("Подчёркнутый");
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
    leftAlignAction->setToolTip("Выровнять по левому краю");
    addAction(leftAlignAction);
    connect(leftAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
    });

    centerAlignAction = new QAction(QIcon(":/icons/align_center.png"),"", alignGroup);
    centerAlignAction->setCheckable(true);
    centerAlignAction->setToolTip("Выровнять по центру");
    addAction(centerAlignAction);
    connect(centerAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignHCenter);
    });

    rightAlignAction = new QAction(QIcon(":/icons/align_right.png"),"", alignGroup);
    rightAlignAction->setCheckable(true);
    rightAlignAction->setToolTip("Выровнять по правому краю");
    addAction(rightAlignAction);
    connect(rightAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
    });

    justifyAlignAction = new QAction(QIcon(":/icons/align_justify.png"),"", alignGroup);
    justifyAlignAction->setCheckable(true);
    justifyAlignAction->setToolTip("Выровнять по ширине");
    addAction(justifyAlignAction);
    connect(justifyAlignAction, &QAction::triggered, [this](){
        this->setAlignment(Qt::AlignJustify);
    });


    // 7. Кнопка "Цвет текста"
    textColorAction = new QAction(QIcon(":/icons/text_color.png"),"", this);
    textColorAction->setToolTip("Цвет шрифта");
    addAction(textColorAction);
    connect(textColorAction, &QAction::triggered, [this]() {
        QColor color = QColorDialog::getColor(Qt::black, this, "Выберите цвет текста");
        if (color.isValid()) setTextColor(color);
    });


    // 8. Кнопка "Выделение"
    textFillColorAction = new QAction(QIcon(":/icons/text_highlight.png"),"", this);
    textFillColorAction->setToolTip("Цвет выделения текста");
    addAction(textFillColorAction);
    connect(textFillColorAction, &QAction::triggered, [this]() {
        QColor color = QColorDialog::getColor(Qt::white, this, "Выберите цвет выделения текста");
        if (color.isValid()) setTextFillColor(color);
    });

    // 9. Кнопка "Заливка"
    cellFillColorAction = new QAction(QIcon(":/icons/cell_fill.png"),"", this);
    cellFillColorAction->setToolTip("Заливка");
    addAction(cellFillColorAction);
    connect(cellFillColorAction, &QAction::triggered,
            this, &FormatToolBar::setCellFillColor);

    // 10. Выбор стиля таблиц
    styleCombo = new QComboBox(this);
    styleCombo->setToolTip("Стиль таблиц");
    styleCombo->setEditable(false);
    addWidget(styleCombo);
    QStringList styleNames = {"Daisy1", "Moonflower", "Pearl", "Printer",
    "Sapphire", "RTF", "PowerPointDark", "PowerPointLight",
    "EGDefault", "HTMLBlue", "Plateau", "Listing", "Minimal",
    "BlockPrint", "Default", "Dove", "HighContrast", "Journal",
    "Journal2", "Journal3", "Raven", "Statistical"};
    styleCombo->addItems(styleNames);
    styleCombo->setCurrentText("Default");
    connect(styleCombo, QOverload<int>::of(&QComboBox::activated),
            this, [this](int index){
                QString styleName = styleCombo->currentText();
                emit styleSelected(styleName);
            });

}

FormatToolBar::~FormatToolBar() {}

// Слоты
void FormatToolBar::applyFontFamily(const QFont &font) {
    // Устанавливаем выбранный шрифт в выделенный текст
    QTextCharFormat format;
    format.setFontFamilies(QStringList() << font.family());
    mergeFormatOnWordOrSelection(format);
}
void FormatToolBar::applyFontSize(const QString &sizeText) {
    bool ok = false;
    int size = sizeText.toInt(&ok);
    if (ok && size > 0) {
        QTextCharFormat format;
        format.setFontPointSize(size);
        mergeFormatOnWordOrSelection(format);
    }
}
void FormatToolBar::toggleBold() {
    QTextEdit* editor = qobject_cast<QTextEdit*>(QApplication::focusWidget());
    if (!editor || !editor->textCursor().hasSelection()) {
        boldAction->setChecked(false);
        return;
    }
    // Включаем/выключаем жирный
    QTextCharFormat format;
    format.setFontWeight(boldAction->isChecked() ? QFont::Bold : QFont::Normal);
    mergeFormatOnWordOrSelection(format);
}
void FormatToolBar::toggleItalic(bool checked) {
    QTextEdit* editor = qobject_cast<QTextEdit*>(QApplication::focusWidget());
    if (!editor || !editor->textCursor().hasSelection()) {
        italicAction->setChecked(false);
        return;
    }
    QTextCharFormat format;
    format.setFontItalic(italicAction->isChecked());
    mergeFormatOnWordOrSelection(format);
}
void FormatToolBar::toggleUnderline(bool checked) {
    QTextEdit* editor = qobject_cast<QTextEdit*>(QApplication::focusWidget());
    if (!editor || !editor->textCursor().hasSelection()) {
        underlineAction->setChecked(false);
        return;
    }
    QTextCharFormat format;
    format.setFontUnderline(underlineAction->isChecked());
    mergeFormatOnWordOrSelection(format);
}
void FormatToolBar::setAlignment(Qt::Alignment alignment) {
    if (!activeTextEdit)
        return; // Игнорируем, если нет активного редактора

    activeTextEdit->setAlignment(alignment);
}
void FormatToolBar::setTextColor(const QColor &color) {
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
    format.setForeground(QBrush(color));
    mergeFormatOnWordOrSelection(format);
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
