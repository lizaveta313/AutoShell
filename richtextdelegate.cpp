#include "richtextdelegate.h"
#include "mainwindow.h"
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
    // editor->setFrameStyle(QFrame::NoFrame);
    // editor->setStyleSheet("QTextEdit { border: none; }");
    editor->installEventFilter(const_cast<MainWindow*>(qobject_cast<MainWindow*>(this->parent())));
    return editor;
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

    // QString html = index.model()->data(index, Qt::EditRole).toString();
    // QTextEdit *textEdit = qobject_cast<QTextEdit*>(editor);
    // if (textEdit)
    //     textEdit->setHtml(html);
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


    // // 1) Убираем состояние «фокуса» и «выделения», чтобы не рисовалась чёрная рамка
    // opt.state &= ~QStyle::State_HasFocus;
    // opt.state &= ~QStyle::State_Selected;

    // // 2) Если у ячейки задан фон (BackgroundRole), зальём им всю ячейку
    // QVariant bgData = index.data(Qt::BackgroundRole);
    // if (bgData.canConvert<QBrush>()) {
    //     QBrush bgBrush = qvariant_cast<QBrush>(bgData);
    //     painter->fillRect(opt.rect, bgBrush);
    // } else {
    //     // Если фон не задан, можно вызвать стандартную отрисовку фона
    //     // но без текста (opt.text.clear()), если вы планируете рисовать текст сами
    //     opt.text.clear();
    //     QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter);
    // }

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
