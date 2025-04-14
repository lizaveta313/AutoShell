#ifndef RICHHEADERVIEW_H
#define RICHHEADERVIEW_H

#include <QHeaderView>
#include <QTextEdit>
#include "formattoolbar.h"

class RichHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit RichHeaderView(Qt::Orientation orientation, FormatToolBar *fmtBar, QWidget *parent = nullptr);
    ~RichHeaderView();

protected:
    // Отрисовка заголовков как раньше
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    QSize sectionSizeFromContents(int logicalIndex) const override;

    // Новые обработчики для inline-редактирования
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void beginEditSection(int logicalIndex);
    void finishEditing(bool accept);

    QTextEdit* editor = nullptr;
    int editingSection = -1;

    FormatToolBar *formatToolBar;
};

#endif // RICHHEADERVIEW_H
