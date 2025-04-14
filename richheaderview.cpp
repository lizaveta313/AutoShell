#include "richheaderview.h"
#include <QTextDocument>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QDebug>
#include <QTextLine>
#include <QTextBlock>

RichHeaderView::RichHeaderView(Qt::Orientation orientation, FormatToolBar *fmtBar, QWidget *parent)
    : QHeaderView(orientation, parent),
    formatToolBar(fmtBar){
    setSectionsClickable(true);
    setSectionResizeMode(QHeaderView::Interactive);
    setSectionsMovable(true);
    // setSectionResizeMode(QHeaderView::ResizeToContents);
}

RichHeaderView::~RichHeaderView() {
    if (editor) {
        editor->removeEventFilter(this);
        editor->deleteLater();
    }
}

void RichHeaderView::paintSection(QPainter *painter,
                                  const QRect &rect,
                                  int logicalIndex) const {

    QStyleOptionHeader opt;
    initStyleOption(&opt);
    opt.rect = rect;
    opt.section = logicalIndex;
    opt.text.clear();

    style()->drawControl(QStyle::CE_HeaderSection, &opt, painter, this);

    QVariant data = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole);
    if (!data.isValid())
        return;
    QString html = data.toString();
    if (html.isEmpty())
        return;

    QTextDocument doc;
    doc.setHtml(html);

    QTextOption to = doc.defaultTextOption();
    to.setWrapMode(QTextOption::NoWrap);
    doc.setDefaultTextOption(to);
    // doc.setTextWidth(rect.width());
    doc.setTextWidth(-1);
    doc.adjustSize();

    painter->save();
    painter->translate(rect.topLeft());

    doc.drawContents(painter, QRectF(0, 0, doc.size().width(), doc.size().height()));
    painter->restore();
}

QSize RichHeaderView::sectionSizeFromContents(int logicalIndex) const {

    QSize baseSize = QHeaderView::sectionSizeFromContents(logicalIndex);

    QVariant data = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole);
    if (!data.isValid())
        return baseSize;

    QString html = data.toString();
    if (html.isEmpty())
        return baseSize;

    // Создаём QTextDocument и задаём HTML
    QTextDocument doc;
    doc.setHtml(html);
    // Чтобы избежать авто-переноса по ширине, задаём очень большое значение:
    doc.setTextWidth(10000);
    doc.adjustSize();

    // Перебираем все текстовые блоки и вычисляем ширину каждой строки
    qreal maxWidth = 0;
    for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
        QTextLayout *layout = block.layout();
        if (!layout)
            continue;
        for (int i = 0; i < layout->lineCount(); ++i) {
            QTextLine line = layout->lineAt(i);
            // naturalTextWidth возвращает ширину, необходимую для строки без переносов
            maxWidth = qMax(maxWidth, line.naturalTextWidth());
        }
    }

    // Добавляем небольшой отступ
    int extraMargin = 4;
    int neededWidth = qCeil(maxWidth + extraMargin);

    // Рассчитываем высоту на основе всего документа с обтеканием (если нужно ограничить высоту)
    // Можно оставить вычисление высоты как раньше или задать фиксированное значение.
    QTextOption opt = doc.defaultTextOption();
    opt.setWrapMode(QTextOption::WordWrap);
    doc.setDefaultTextOption(opt);
    // Здесь можно задать doc.setTextWidth(rect.width()) при отрисовке;
    qreal docHeight = doc.size().height();
    int extraHeight = 4;
    int neededHeight = qCeil(docHeight + extraHeight);
    int maxHeight = 100; // ограничение, если нужно
    neededHeight = qMin(neededHeight, maxHeight);

    return QSize(qMax(baseSize.width(), neededWidth),
                 qMax(baseSize.height(), neededHeight));
}

void RichHeaderView::mouseDoubleClickEvent(QMouseEvent *event) {
    int section = logicalIndexAt(event->pos());
    if (section >= 0) {
        beginEditSection(section);
        return;
    }
    QHeaderView::mouseDoubleClickEvent(event);
}

void RichHeaderView::beginEditSection(int logicalIndex) {
    // Если уже идёт редактирование, завершаем его
    if (editor) {
        finishEditing(true);
    }

    editingSection = logicalIndex;

    // Координаты секции заголовка
    int xPos = sectionPosition(logicalIndex);
    int width = sectionSize(logicalIndex);
    QRect sectionRect(xPos, 0, width, height());

    // Создаём текстовый редактор поверх заголовка
    editor = new QTextEdit(this);
    editor->setGeometry(sectionRect);
    editor->setAcceptRichText(true);  // позволяет использовать форматирование
    editor->installEventFilter(this);

    if (formatToolBar) {
        formatToolBar->setActiveTextEdit(editor);
    }

    // Загружаем текущее содержимое (HTML) из модели
    QVariant var = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole);
    editor->setHtml(var.toString());

    editor->setFocus();
    editor->show();
}

// Обработка событий редактора: ключевые события и потеря фокуса
bool RichHeaderView::eventFilter(QObject *obj, QEvent *event) {
    if (obj == editor) {
        if (event->type() == QEvent::FocusOut) {
            finishEditing(true);
            return true;
        } else if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            // Если нажато Enter и не зажат Shift – завершаем редактирование
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
                !(keyEvent->modifiers() & Qt::ShiftModifier)) {
                finishEditing(true);
                return true;
            }
            // Если нажато Shift+Enter, позволяйте вставлять перенос строки
            else if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
                     (keyEvent->modifiers() & Qt::ShiftModifier)) {
                // Не перехватываем событие, чтобы QTextEdit сам вставил перенос строки
                return false;
            }
            else if (keyEvent->key() == Qt::Key_Escape) {
                finishEditing(false);
                return true;
            }
        }
    }
    return QHeaderView::eventFilter(obj, event);
}

// Завершение редактирования: если accept==true, сохраняем новое значение
void RichHeaderView::finishEditing(bool accept) {
    if (!editor)
        return;

    if (accept) {
        QString fullHtml = editor->toHtml();

        // Используем регулярное выражение для извлечения содержимого между <body> и </body>
        QRegularExpression re("<body[^>]*>(.*)</body>", QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(fullHtml);
        QString newHtml;
        if (match.hasMatch()) {
            // Извлекаем содержимое тега <body> и обрезаем лишние пробелы
            newHtml = match.captured(1).trimmed();
        } else {
            newHtml = fullHtml;
        }

        // Обновляем модель, используя роли EditRole и DisplayRole
        model()->setHeaderData(editingSection, orientation(), newHtml, Qt::EditRole);
        model()->setHeaderData(editingSection, orientation(), newHtml, Qt::DisplayRole);
    }

    if (formatToolBar) {
        formatToolBar->setActiveTextEdit(nullptr);
    }

    editor->removeEventFilter(this);
    editor->deleteLater();
    editor = nullptr;
    editingSection = -1;
    update();  // Перерисовываем header
}
