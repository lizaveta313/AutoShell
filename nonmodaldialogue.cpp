#include "nonmodaldialogue.h"

DialogEditName::DialogEditName(const QString &currentName, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Editing the name"));

    nameEdit = new QLineEdit(this);
    nameEdit->setText(currentName);

    okButton = new QPushButton(tr("OK"), this);
    cancelButton = new QPushButton(tr("Cancel"), this);

    connect(okButton, &QPushButton::clicked, this, &DialogEditName::accept);
    connect(cancelButton, &QPushButton::clicked, this, &DialogEditName::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(nameEdit);
    layout->addWidget(okButton);
    layout->addWidget(cancelButton);
}

QString DialogEditName::getNewName() const {
    return nameEdit->text();
}
