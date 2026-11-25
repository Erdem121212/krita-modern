/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisBrushShopDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

#include <klocalizedstring.h>
#include <KisStorageModel.h>
#include <KisResourceServerProvider.h>
#include <KisResourceModel.h>
#include <KisResourceModelProvider.h>
#include <KisTagModel.h>
#include <KisTagResourceModel.h>
#include <brushengine/kis_paintop_preset.h>

KisBrushShopDialog::KisBrushShopDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Brushes — Import"));
    setAcceptDrops(true);
    resize(560, 440);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    QHBoxLayout *topRow = new QHBoxLayout();
    QLabel *titleLbl = new QLabel(i18n("Brush Importer"), this);
    QFont tf = titleLbl->font();
    tf.setPointSize(13);
    tf.setBold(true);
    titleLbl->setFont(tf);

    QPushButton *btnImport = new QPushButton(i18n("Import .bundle / .kpp"), this);
    btnImport->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3b82f6; color: #ffffff; border: none; border-radius: 7px; padding: 6px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2563eb; }"
    ));

    topRow->addWidget(titleLbl);
    topRow->addStretch();
    topRow->addWidget(btnImport);
    root->addLayout(topRow);

    QLabel *dropHint = new QLabel(i18n("Drag and drop .bundle or .kpp files here, or click Import above.\nImported brushes are automatically added to the 'Imported' category."), this);
    dropHint->setAlignment(Qt::AlignCenter);
    dropHint->setMinimumHeight(64);
    dropHint->setStyleSheet(QStringLiteral(
        "QLabel { border: 2px dashed #2a2d3a; border-radius: 10px; padding: 10px; background: #121319; color: #9aa0b2; }"
    ));
    root->addWidget(dropHint);

    root->addWidget(new QLabel(i18n("Installed Brush Packs (Bundles):"), this));

    m_listWidget = new QListWidget(this);
    m_listWidget->setSpacing(4);
    m_listWidget->setAlternatingRowColors(true);
    root->addWidget(m_listWidget, 1);

    m_statusLabel = new QLabel(i18n("Ready to import brushes."), this);
    root->addWidget(m_statusLabel);

    connect(btnImport, &QPushButton::clicked, this, &KisBrushShopDialog::slotImportLocalFiles);

    refreshBundleList();
}

void KisBrushShopDialog::refreshBundleList()
{
    m_listWidget->clear();
    if (KisStorageModel *sm = KisStorageModel::instance()) {
        for (int i = 0; i < sm->rowCount(); ++i) {
            const QModelIndex idx = sm->index(i, 0);
            const QString loc = idx.data(Qt::UserRole + KisStorageModel::Location).toString();
            const bool active = idx.data(Qt::CheckStateRole).toInt() == Qt::Checked;
            if (loc.endsWith(QStringLiteral(".bundle"), Qt::CaseInsensitive)) {
                const QString name = QFileInfo(loc).fileName();
                QListWidgetItem *item = new QListWidgetItem(m_listWidget);
                item->setSizeHint(QSize(0, 40));

                QWidget *rowWidget = new QWidget(m_listWidget);
                QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
                rowLayout->setContentsMargins(10, 4, 10, 4);
                rowLayout->setSpacing(8);

                QLabel *nameLabel = new QLabel(name, rowWidget);
                nameLabel->setStyleSheet(QStringLiteral("QLabel { color: #f5f7fa; background: transparent; }"));
                rowLayout->addWidget(nameLabel, 1);

                QPushButton *toggleBtn = new QPushButton(active ? i18n("Active") : i18n("Off"), rowWidget);
                toggleBtn->setCheckable(true);
                toggleBtn->setChecked(active);
                toggleBtn->setFixedSize(64, 24);
                toggleBtn->setCursor(Qt::PointingHandCursor);
                toggleBtn->setStyleSheet(QStringLiteral(
                    "QPushButton { background-color: #252836; color: #9aa0b2; border: 1px solid #32364a; border-radius: 12px; font-size: 11px; font-weight: bold; padding: 0px; }"
                    "QPushButton:checked { background-color: #3b82f6; color: #ffffff; border: none; }"
                    "QPushButton:hover { border-color: #4c6ef5; }"
                ));

                connect(toggleBtn, &QPushButton::toggled, this, [this, toggleBtn, name, i](bool checked) {
                    if (KisStorageModel *model = KisStorageModel::instance()) {
                        const QModelIndex storageIdx = model->index(i, 0);
                        model->setData(storageIdx, checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
                    }
                    toggleBtn->setText(checked ? i18n("Active") : i18n("Off"));
                    m_statusLabel->setText(checked ? i18n("Enabled %1", name) : i18n("Disabled %1", name));
                });

                rowLayout->addWidget(toggleBtn);
                m_listWidget->setItemWidget(item, rowWidget);
            }
        }
    }
}

void KisBrushShopDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void KisBrushShopDialog::dropEvent(QDropEvent *event)
{
    int importedCount = 0;
    for (const QUrl &url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (path.endsWith(QStringLiteral(".bundle"), Qt::CaseInsensitive) ||
            path.endsWith(QStringLiteral(".kpp"), Qt::CaseInsensitive)) {
            if (importDownloadedFile(path)) {
                ++importedCount;
            }
        }
    }
    if (importedCount > 0) {
        refreshBundleList();
        m_statusLabel->setText(i18n("Imported %1 brush file(s) into 'Imported'!", importedCount));
    }
}

bool KisBrushShopDialog::importDownloadedFile(const QString &filePath)
{
    KisTagModel tagModel(ResourceType::PaintOpPresets);
    KisTagSP importedTag = tagModel.tagForUrl(QStringLiteral("Imported"));
    if (!importedTag) {
        importedTag = tagModel.addTag(QStringLiteral("Imported"), false, {});
    }

    if (filePath.endsWith(QStringLiteral(".bundle"), Qt::CaseInsensitive)) {
        KisResourceModel resModel(ResourceType::PaintOpPresets);
        QSet<int> beforeIds;
        for (int i = 0; i < resModel.rowCount(); ++i) {
            beforeIds.insert(resModel.data(resModel.index(i, 0), Qt::UserRole + KisAbstractResourceModel::Id).toInt());
        }
        const bool ok = KisStorageModel::instance()->importStorage(filePath, KisStorageModel::Overwrite);
        if (ok && importedTag) {
            QVector<int> newIds;
            for (int i = 0; i < resModel.rowCount(); ++i) {
                const int id = resModel.data(resModel.index(i, 0), Qt::UserRole + KisAbstractResourceModel::Id).toInt();
                if (!beforeIds.contains(id)) {
                    newIds.append(id);
                }
            }
            if (!newIds.isEmpty()) {
                if (auto *tagResModel = KisResourceModelProvider::tagResourceModel(ResourceType::PaintOpPresets)) {
                    tagResModel->tagResources(importedTag, newIds);
                }
            }
        }
        return ok;
    } else if (filePath.endsWith(QStringLiteral(".kpp"), Qt::CaseInsensitive)) {
        KoResourceSP res = KisResourceServerProvider::instance()->paintOpPresetServer()->importResourceFile(filePath, true);
        if (!res.isNull() && importedTag) {
            if (auto *tagResModel = KisResourceModelProvider::tagResourceModel(ResourceType::PaintOpPresets)) {
                tagResModel->tagResources(importedTag, {res->resourceId()});
            }
        }
        return !res.isNull();
    }
    return false;
}

void KisBrushShopDialog::slotImportLocalFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        i18n("Import Brushes"),
        QString(),
        i18n("Krita Brush Files (*.bundle *.kpp)"));
    if (files.isEmpty()) {
        return;
    }
    int importedCount = 0;
    for (const QString &f : files) {
        if (importDownloadedFile(f)) {
            ++importedCount;
        }
    }
    refreshBundleList();
    if (importedCount > 0) {
        m_statusLabel->setText(i18n("Imported %1 brush file(s) into 'Imported'!", importedCount));
    } else {
        m_statusLabel->setText(i18n("Failed to import selected brush file(s)."));
    }
}
