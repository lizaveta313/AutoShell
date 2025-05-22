#include "richtextdelegate.h"
#include "templatepanel.h"
#include <QTextEdit>
#include <QTextDocument>
#include <QApplication>
#include <QPainter>

RichTextDelegate::RichTextDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {
}
RichTextDelegate::~RichTextDelegate() {}


QWidget *RichTextDelegate::createEditor(QWidget *parent,
                                        const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const {
    QTextEdit *editor = new QTextEdit(parent);
    editor->setAcceptRichText(true);
    editor->setFocusPolicy(Qt::StrongFocus);

    TemplatePanel *panel = qobject_cast<TemplatePanel*>(this->parent());
    if (panel) {
        // Устанавливаем eventFilter на TemplatePanel,
        // где и находится ваш eventFilter(...)
        editor->installEventFilter(panel);
    }
    editor->installEventFilter(const_cast<RichTextDelegate*>(this));
    return editor;
}

bool RichTextDelegate::eventFilter(QObject *obj, QEvent *event)
{
    // если уход фокуса из редактора
    if (event->type() == QEvent::FocusOut) {
        if (auto *editor = qobject_cast<QWidget*>(obj)) {
            // говорим модели: вытащи данные из редактора...
            emit commitData(editor);
            // ...и закрой этот редактор
            emit closeEditor(editor, QAbstractItemDelegate::NoHint);
        }
    }
    // дальше стандартная логика фильтрации
    return QStyledItemDelegate::eventFilter(obj, event);
}

void RichTextDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {

    QTextEdit *textEdit = qobject_cast<QTextEdit*>(editor);
    if (!textEdit) return;

    QString html = index.model()->data(index, Qt::EditRole).toString();
    textEdit->setHtml(html);

    // Выделяем весь текст в документе
    QTextCursor cursor = textEdit->textCursor();
    cursor.select(QTextCursor::Document);
    textEdit->setTextCursor(cursor);
}

void RichTextDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                    const QModelIndex &index) const {
    QTextEdit *textEdit = qobject_cast<QTextEdit*>(editor);
    if (textEdit)
    {
        model->setData(index, textEdit->toHtml(), Qt::EditRole);
    }
}

void RichTextDelegate::updateEditorGeometry(QWidget *editor,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {
    editor->setGeometry(option.rect);
}

void RichTextDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {

    painter->save();
    painter->setClipRect(option.rect);

    // Инициализируем стиль
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    // Очищаем строку, чтобы стиль НЕ выводил сырой HTML
    opt.text.clear();

    // Отрисовываем фон, выделение и т.п. (но без текста)
    QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

    // Теперь рисуем сам HTML
    QString html = index.data(Qt::DisplayRole).toString();
    QTextDocument doc;
    doc.setHtml(html);

    // Устанавливаем ширину, чтобы QTextDocument знал,
    // как «переносить» строки при отрисовке
    doc.setTextWidth(option.rect.width());

    // Смещаем «начало координат» на левый верхний угол ячейки
    painter->translate(option.rect.topLeft());

    // Рисуем текст внутри прямоугольника
    doc.drawContents(painter, QRectF(0, 0, option.rect.width(), option.rect.height()));

    painter->restore();
}

QSize RichTextDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const {
    QString html = index.data(Qt::DisplayRole).toString();
    QTextDocument doc;
    doc.setHtml(html);
    // Задаём ширину для правильного вычисления высоты
    doc.setTextWidth(option.rect.width());

    return QSize(doc.idealWidth(), int(doc.size().height()));
}
