#ifndef TAGCREATEDIALOG_H
#define TAGCREATEDIALOG_H

#include "../../../../TagManager/tagmanager.h"

#include "../Controller/tag-type-list-controller.h"

#include <QDialog>
#include <QItemSelection>

namespace Ui {
class TagCreateDialog;
}

class TagCreateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TagCreateDialog(QWidget *parent = nullptr);
    ~TagCreateDialog();

    QString tagID();
    QString tagName();
    QString tagData();

private slots:

    void on_tagTypeListSelectionChanged(const QItemSelection& newSelection, const QItemSelection& oldSelection);

private:
    Ui::TagCreateDialog *ui;

    QStringList tagTypeIDList;
    TagTypeListController tagTypeListController;
};

#endif // TAGCREATEDIALOG_H
