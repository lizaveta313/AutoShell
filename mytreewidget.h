#ifndef MYTREEWIDGET_H
#define MYTREEWIDGET_H

#include <QTreeWidget>
#include <QDropEvent>

class MyTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit MyTreeWidget(QWidget *parent = nullptr);

signals:
    // Сигнал, который будет испускаться после завершения операции drop
    void dropped();

protected:
    // Переопределяем dropEvent, чтобы добавить свою обработку
    void dropEvent(QDropEvent *event) override;
};

#endif // MYTREEWIDGET_H
