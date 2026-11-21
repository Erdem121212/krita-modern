/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisEffectsMenuDialog.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QAction>
#include <QImage>
#include <QPixmap>

#include <klocalizedstring.h>
#include <kactioncollection.h>

#include "KisMainWindow.h"
#include "KisViewManager.h"
#include "KisView.h"
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_paint_device.h>
#include <filter/kis_filter_registry.h>
#include <filter/kis_filter.h>
#include <filter/kis_filter_configuration.h>
#include <kis_processing_information.h>

namespace {
QPixmap makeFilterPreview(const QString &filterId, const QImage &baseThumb, KisImageSP img)
{
    if (baseThumb.isNull() || !img) {
        return QPixmap();
    }
    KisFilterSP filter = KisFilterRegistry::instance()->value(filterId);
    if (!filter) {
        return QPixmap::fromImage(baseThumb);
    }

    KisPaintDeviceSP dev = new KisPaintDevice(img->colorSpace());
    dev->convertFromQImage(baseThumb, nullptr);
    KisFilterConfigurationSP cfg = filter->defaultConfiguration(KisGlobalResourcesInterface::instance());
    if (cfg) {
        filter->process(dev, QRect(0, 0, baseThumb.width(), baseThumb.height()), cfg.data());
    }
    QImage out = dev->convertToQImage(nullptr, 0, 0, baseThumb.width(), baseThumb.height());
    return QPixmap::fromImage(out);
}
}

KisEffectsMenuDialog::KisEffectsMenuDialog(KisMainWindow *mainWindow, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Effects & Filters"));
    resize(620, 460);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    QLabel *title = new QLabel(i18n("Effects & Live Preview"), this);
    QFont tf = title->font();
    tf.setPointSize(13);
    tf.setBold(true);
    title->setFont(tf);
    root->addWidget(title);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *container = new QWidget(scroll);
    QGridLayout *grid = new QGridLayout(container);
    grid->setSpacing(10);

    KisImageSP activeImage;
    QImage baseThumb;
    if (mainWindow && mainWindow->activeView() && mainWindow->activeView()->image()) {
        activeImage = mainWindow->activeView()->image();
        QImage full = activeImage->projection()->convertToQImage(nullptr, 0, 0, activeImage->width(), activeImage->height());
        if (!full.isNull()) {
            baseThumb = full.scaled(110, 82, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    const struct {
        const char *filterId;
        QString label;
    } effects[] = {
        { "gaussian blur", i18n("Gaussian Blur") },
        { "sharpen", i18n("Sharpen") },
        { "brightnesscontrast", i18n("Brightness / Contrast") },
        { "hsvadjustment", i18n("Hue / Saturation") },
        { "colorbalance", i18n("Color Balance") },
        { "posterize", i18n("Posterize") },
        { "oilpaint", i18n("Oil Paint") },
        { "pixelize", i18n("Pixelize") },
        { "halftone", i18n("Halftone") },
        { "invert", i18n("Invert") },
        { "desaturate", i18n("Desaturate") },
        { "edge detection", i18n("Edge Detection") }
    };

    int idx = 0;
    const int cols = 4;
    for (const auto &eff : effects) {
        const QString fid = QString::fromLatin1(eff.filterId);
        QPushButton *card = new QPushButton(container);
        card->setFixedSize(132, 118);
        card->setStyleSheet(QStringLiteral(
            "QPushButton { border-radius: 10px; border: 1px solid palette(mid); padding: 6px; text-align: center; }"
            "QPushButton:hover { border-color: palette(highlight); background: palette(midlight); }"
        ));

        QVBoxLayout *cl = new QVBoxLayout(card);
        cl->setContentsMargins(4, 4, 4, 4);
        cl->setSpacing(4);

        QLabel *imgLbl = new QLabel(card);
        imgLbl->setAlignment(Qt::AlignCenter);
        imgLbl->setFixedHeight(78);
        QPixmap pm = makeFilterPreview(fid, baseThumb, activeImage);
        if (!pm.isNull()) {
            imgLbl->setPixmap(pm);
        } else {
            imgLbl->setText(i18n("Preview"));
        }

        QLabel *nameLbl = new QLabel(eff.label, card);
        nameLbl->setAlignment(Qt::AlignCenter);
        nameLbl->setWordWrap(true);

        cl->addWidget(imgLbl, 1);
        cl->addWidget(nameLbl);

        connect(card, &QPushButton::clicked, this, [this, mainWindow, fid]() {
            accept();
            if (mainWindow && mainWindow->viewManager() && mainWindow->viewManager()->actionCollection()) {
                QAction *act = mainWindow->viewManager()->actionCollection()->action(QStringLiteral("krita_filter_") + fid);
                if (act) {
                    act->trigger();
                }
            }
        });

        grid->addWidget(card, idx / cols, idx % cols);
        ++idx;
    }

    scroll->setWidget(container);
    root->addWidget(scroll, 1);
}
