#ifndef NONMODALDIALOGUE_H
#define NONMODALDIALOGUE_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

class DialogEditName : public QDialog {
    Q_OBJECT

public:
    explicit DialogEditName(const QString &currentName, QWidget *parent = nullptr);

    QString getNewName() const;

private:
    QLineEdit *nameEdit;
    QPushButton *okButton;
    QPushButton *cancelButton;
};

#endif // NONMODALDIALOGUE_H
