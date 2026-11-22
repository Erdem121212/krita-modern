/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_BRUSH_SHOP_DIALOG_H
#define KIS_BRUSH_SHOP_DIALOG_H

#include <QDialog>
#include <kritaui_export.h>

class QListWidget;
class QLabel;
class QDragEnterEvent;
class QDropEvent;

class KRITAUI_EXPORT KisBrushShopDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KisBrushShopDialog(QWidget *parent = nullptr);
    ~KisBrushShopDialog() override = default;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private Q_SLOTS:
    void slotImportLocalFiles();
    void refreshBundleList();

private:
    bool importDownloadedFile(const QString &filePath);

    QListWidget *m_listWidget {nullptr};
    QLabel *m_statusLabel {nullptr};
};

#endif // KIS_BRUSH_SHOP_DIALOG_H
