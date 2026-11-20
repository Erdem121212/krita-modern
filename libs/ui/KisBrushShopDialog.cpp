/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisBrushShopDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMessageBox>

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
    , m_nam(new QNetworkAccessManager(this))
{
    setWindowTitle(i18n("Draworever Brush Shop"));
    resize(680, 520);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    QHBoxLayout *topRow = new QHBoxLayout();
    QLabel *titleLbl = new QLabel(i18n("Krita Artists — Brushes & Bundles"), this);
    QFont tf = titleLbl->font();
    tf.setPointSize(12);
    tf.setBold(true);
    titleLbl->setFont(tf);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(i18n("Search brushes..."));
    m_searchEdit->setMaximumWidth(220);

    QPushButton *btnRefresh = new QPushButton(i18n("Refresh"), this);
    QPushButton *btnLocal = new QPushButton(i18n("Import .bundle/.kpp"), this);

    topRow->addWidget(titleLbl);
    topRow->addStretch();
    topRow->addWidget(m_searchEdit);
    topRow->addWidget(btnRefresh);
    topRow->addWidget(btnLocal);
    root->addLayout(topRow);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSpacing(4);
    m_listWidget->setAlternatingRowColors(true);
    root->addWidget(m_listWidget, 1);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    m_statusLabel = new QLabel(i18n("Loading brush catalog from krita-artists.org..."), this);
    m_btnOpenPage = new QPushButton(i18n("Open Page"), this);
    m_btnInstall = new QPushButton(i18n("Install"), this);
    m_btnInstall->setDefault(true);
    m_btnOpenPage->setEnabled(false);
    m_btnInstall->setEnabled(false);

    bottomRow->addWidget(m_statusLabel, 1);
    bottomRow->addWidget(m_btnOpenPage);
    bottomRow->addWidget(m_btnInstall);
    root->addLayout(bottomRow);

    connect(btnRefresh, &QPushButton::clicked, this, &KisBrushShopDialog::fetchBrushCatalog);
    connect(btnLocal, &QPushButton::clicked, this, &KisBrushShopDialog::slotImportLocalFile);
    connect(m_btnInstall, &QPushButton::clicked, this, &KisBrushShopDialog::slotInstallSelected);
    connect(m_btnOpenPage, &QPushButton::clicked, this, &KisBrushShopDialog::slotOpenSelectedPage);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &KisBrushShopDialog::slotFilterChanged);
    connect(m_listWidget, &QListWidget::currentRowChanged, this, [this](int row) {
        const bool hasSel = (row >= 0);
        m_btnOpenPage->setEnabled(hasSel);
        m_btnInstall->setEnabled(hasSel);
    });
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &KisBrushShopDialog::slotInstallSelected);

    fetchBrushCatalog();
}

void KisBrushShopDialog::fetchBrushCatalog()
{
    m_statusLabel->setText(i18n("Fetching latest brush packs from krita-artists.org..."));
    m_listWidget->clear();

    QNetworkRequest req(QUrl(QStringLiteral("https://krita-artists.org/c/resources/brushes-and-bundles/16.json")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Draworever/1.0"));
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(i18n("Network error: %1", reply->errorString()));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray topics = doc.object().value("topic_list").toObject().value("topics").toArray();
        int count = 0;
        for (const QJsonValue &val : topics) {
            const QJsonObject obj = val.toObject();
            const int id = obj.value("id").toInt();
            const QString title = obj.value("title").toString();
            const QString slug = obj.value("slug").toString();
            if (id <= 0 || title.isEmpty()) {
                continue;
            }
            QListWidgetItem *item = new QListWidgetItem(title, m_listWidget);
            item->setData(Qt::UserRole, id);
            item->setData(Qt::UserRole + 1, QStringLiteral("https://krita-artists.org/t/%1/%2").arg(slug).arg(id));
            ++count;
        }
        m_statusLabel->setText(i18n("Loaded %1 brush packs. Select one and click Install.", count));
    });
}

void KisBrushShopDialog::slotFilterChanged(const QString &text)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        item->setHidden(!text.isEmpty() && !item->text().contains(text, Qt::CaseInsensitive));
    }
}

void KisBrushShopDialog::slotOpenSelectedPage()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;
    QDesktopServices::openUrl(QUrl(item->data(Qt::UserRole + 1).toString()));
}

void KisBrushShopDialog::slotInstallSelected()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    const int topicId = item->data(Qt::UserRole).toInt();
    const QString topicPage = item->data(Qt::UserRole + 1).toString();
    m_statusLabel->setText(i18n("Checking %1 for downloadable brush files...", item->text()));
    m_btnInstall->setEnabled(false);

    QNetworkRequest req(QUrl(QStringLiteral("https://krita-artists.org/t/%1.json").arg(topicId)));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Draworever/1.0"));
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, topicPage]() {
        reply->deleteLater();
        m_btnInstall->setEnabled(true);
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(i18n("Failed to read topic: %1", reply->errorString()));
            return;
        }

        const QByteArray raw = reply->readAll();
        const QString text = QString::fromUtf8(raw);

        // Look for direct .bundle or .kpp attachment URLs in the post
        QRegularExpression rx(QStringLiteral("https?://[^\"'\\s<>]+\\.(?:bundle|kpp)"), QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = rx.match(text);
        if (match.hasMatch()) {
            const QUrl fileUrl(match.captured(0));
            downloadAndInstallUrl(fileUrl, fileUrl.fileName());
            return;
        }

        // Also check relative /uploads/.../*.bundle or .kpp
        QRegularExpression relRx(QStringLiteral("/uploads/[^\"'\\s<>]+\\.(?:bundle|kpp)"), QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch relMatch = relRx.match(text);
        if (relMatch.hasMatch()) {
            const QUrl fileUrl(QStringLiteral("https://krita-artists.org") + relMatch.captured(0));
            downloadAndInstallUrl(fileUrl, fileUrl.fileName());
            return;
        }

        m_statusLabel->setText(i18n("This pack is hosted externally — opening download page in your browser."));
        QDesktopServices::openUrl(QUrl(topicPage));
    });
}

void KisBrushShopDialog::downloadAndInstallUrl(const QUrl &url, const QString &suggestedName)
{
    m_statusLabel->setText(i18n("Downloading %1...", suggestedName));
    m_btnInstall->setEnabled(false);

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Draworever/1.0"));
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, suggestedName]() {
        reply->deleteLater();
        m_btnInstall->setEnabled(true);
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(i18n("Download failed: %1", reply->errorString()));
            return;
        }

        const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString outPath = QDir(tmpDir).filePath(suggestedName.isEmpty() ? QStringLiteral("downloaded.bundle") : suggestedName);
        QFile f(outPath);
        if (!f.open(QIODevice::WriteOnly)) {
            m_statusLabel->setText(i18n("Could not save temporary file: %1", outPath));
            return;
        }
        f.write(reply->readAll());
        f.close();

        if (importDownloadedFile(outPath)) {
            m_statusLabel->setText(i18n("Installed %1! Brushes are ready to use.", suggestedName));
            QMessageBox::information(this, i18n("Brush Shop"), i18n("Successfully installed %1!", suggestedName));
        } else {
            m_statusLabel->setText(i18n("Downloaded to %1, but import failed.", outPath));
        }
    });
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

void KisBrushShopDialog::slotImportLocalFile()
{
    const QString file = QFileDialog::getOpenFileName(
        this,
        i18n("Import Brush Pack"),
        QString(),
        i18n("Krita Brush Files (*.bundle *.kpp)"));
    if (file.isEmpty()) {
        return;
    }
    if (importDownloadedFile(file)) {
        m_statusLabel->setText(i18n("Installed %1!", QFileInfo(file).fileName()));
    } else {
        m_statusLabel->setText(i18n("Failed to import %1.", QFileInfo(file).fileName()));
    }
}
