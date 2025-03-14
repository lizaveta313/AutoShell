#include "mytreewidget.h"

MyTreeWidget::MyTreeWidget(QWidget *parent)
    : QTreeWidget(parent) {

}

void MyTreeWidget::dropEvent(QDropEvent *event)
{
    // Вызываем базовую реализацию, чтобы обеспечить корректное перемещение элементов
    QTreeWidget::dropEvent(event);
    // После завершения перетаскивания испускаем сигнал, который MainWindow сможет обработать
    emit dropped();
}
