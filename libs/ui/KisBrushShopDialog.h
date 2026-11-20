/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_BRUSH_SHOP_DIALOG_H
#define KIS_BRUSH_SHOP_DIALOG_H

#include <QDialog>
#include <kritaui_export.h>

class QListWidget;
class QLabel;
class QNetworkAccessManager;
class QPushButton;
class QLineEdit;

class KRITAUI_EXPORT KisBrushShopDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KisBrushShopDialog(QWidget *parent = nullptr);
    ~KisBrushShopDialog() override = default;

private Q_SLOTS:
    void fetchBrushCatalog();
    void slotInstallSelected();
    void slotOpenSelectedPage();
    void slotImportLocalFile();
    void slotFilterChanged(const QString &text);

private:
    void downloadAndInstallUrl(const QUrl &url, const QString &suggestedName);
    bool importDownloadedFile(const QString &filePath);

    QNetworkAccessManager *m_nam {nullptr};
    QLineEdit *m_searchEdit {nullptr};
    QListWidget *m_listWidget {nullptr};
    QLabel *m_statusLabel {nullptr};
    QPushButton *m_btnInstall {nullptr};
    QPushButton *m_btnOpenPage {nullptr};
};

#endif // KIS_BRUSH_SHOP_DIALOG_H
