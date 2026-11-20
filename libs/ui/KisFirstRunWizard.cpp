/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisFirstRunWizard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QComboBox>

#include <klocalizedstring.h>
#include <KSharedConfig>
#include <KConfigGroup>

#include "kis_config.h"

namespace {
enum InputDeviceType {
    DeviceMouse = 0,
    DeviceTouch = 1,
    DeviceTablet = 2
};

enum DrawingStyleType {
    StyleNormal = 0,
    StyleAnime = 1,
    StylePixelArt = 2,
    StylePainting = 3
};

QPushButton *makeOptionCard(const QString &title, const QString &subtitle, QWidget *parent)
{
    QPushButton *btn = new QPushButton(QString("%1\n%2").arg(title, subtitle), parent);
    btn->setCheckable(true);
    btn->setMinimumHeight(68);
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { text-align: left; padding: 10px 14px; border-radius: 10px; border: 1px solid palette(mid); font-size: 10pt; }"
        "QPushButton:checked { border: 2px solid palette(highlight); background-color: palette(highlight); color: palette(highlighted-text); }"
    ));
    return btn;
}
}

KisFirstRunWizard::KisFirstRunWizard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Welcome to Draworever"));
    setModal(true);
    resize(500, 420);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 16);
    root->setSpacing(14);

    QLabel *header = new QLabel(i18n("Let's set up Draworever for you"), this);
    QFont hf = header->font();
    hf.setPointSize(14);
    hf.setBold(true);
    header->setFont(hf);
    root->addWidget(header);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, 1);

    // Page 1: Input Device
    QWidget *page1 = new QWidget(this);
    QVBoxLayout *p1Layout = new QVBoxLayout(page1);
    p1Layout->setSpacing(10);
    p1Layout->addWidget(new QLabel(i18n("How do you draw?"), page1));

    m_deviceGroup = new QButtonGroup(page1);
    m_deviceGroup->setExclusive(true);

    QPushButton *btnMouse = makeOptionCard(i18n("Mouse"), i18n("Extra stroke smoothing for steady lines without a pen"), page1);
    QPushButton *btnTouch = makeOptionCard(i18n("Touchscreen"), i18n("Touch-friendly gestures and smooth finger strokes"), page1);
    QPushButton *btnTablet = makeOptionCard(i18n("Drawing Tablet"), i18n("Pen pressure smoothing and balanced stabilization"), page1);
    btnTablet->setChecked(true);

    m_deviceGroup->addButton(btnMouse, DeviceMouse);
    m_deviceGroup->addButton(btnTouch, DeviceTouch);
    m_deviceGroup->addButton(btnTablet, DeviceTablet);

    p1Layout->addWidget(btnMouse);
    p1Layout->addWidget(btnTouch);
    p1Layout->addWidget(btnTablet);
    p1Layout->addStretch();
    m_stack->addWidget(page1);

    // Page 2: Drawing Style
    QWidget *page2 = new QWidget(this);
    QVBoxLayout *p2Layout = new QVBoxLayout(page2);
    p2Layout->setSpacing(10);
    p2Layout->addWidget(new QLabel(i18n("What's your drawing style?"), page2));

    m_styleGroup = new QButtonGroup(page2);
    m_styleGroup->setExclusive(true);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);

    QPushButton *btnNormal = makeOptionCard(i18n("Normal"), i18n("Balanced smoothing & 4K canvas"), page2);
    QPushButton *btnAnime = makeOptionCard(i18n("Anime / Manga"), i18n("Crisp lineart smoothing & portrait canvas"), page2);
    QPushButton *btnPixel = makeOptionCard(i18n("Pixel Art"), i18n("Zero smoothing & pixel-art canvas"), page2);
    QPushButton *btnPaint = makeOptionCard(i18n("Concept / Paint"), i18n("Light smoothing & wide canvas"), page2);
    btnNormal->setChecked(true);

    m_styleGroup->addButton(btnNormal, StyleNormal);
    m_styleGroup->addButton(btnAnime, StyleAnime);
    m_styleGroup->addButton(btnPixel, StylePixelArt);
    m_styleGroup->addButton(btnPaint, StylePainting);

    grid->addWidget(btnNormal, 0, 0);
    grid->addWidget(btnAnime, 0, 1);
    grid->addWidget(btnPixel, 1, 0);
    grid->addWidget(btnPaint, 1, 1);
    p2Layout->addLayout(grid);

    QHBoxLayout *pxRow = new QHBoxLayout();
    QLabel *pxLbl = new QLabel(i18n("Pixel Art Canvas Size:"), page2);
    m_pixelSizeCombo = new QComboBox(page2);
    m_pixelSizeCombo->addItem(QStringLiteral("16 x 16"), 16);
    m_pixelSizeCombo->addItem(QStringLiteral("32 x 32"), 32);
    m_pixelSizeCombo->addItem(QStringLiteral("64 x 64"), 64);
    m_pixelSizeCombo->setCurrentIndex(1);
    pxLbl->setVisible(false);
    m_pixelSizeCombo->setVisible(false);
    pxRow->addWidget(pxLbl);
    pxRow->addWidget(m_pixelSizeCombo);
    pxRow->addStretch();
    p2Layout->addLayout(pxRow);
    p2Layout->addStretch();
    m_stack->addWidget(page2);

    connect(m_styleGroup, SIGNAL(idClicked(int)), this, SLOT(update()));
    connect(btnPixel, &QPushButton::toggled, this, [pxLbl, this](bool checked) {
        pxLbl->setVisible(checked);
        m_pixelSizeCombo->setVisible(checked);
    });

    // Footer buttons
    QHBoxLayout *footer = new QHBoxLayout();
    m_btnBack = new QPushButton(i18n("Back"), this);
    m_btnBack->setVisible(false);
    m_btnNext = new QPushButton(i18n("Next"), this);
    m_btnNext->setDefault(true);

    footer->addWidget(m_btnBack);
    footer->addStretch();
    footer->addWidget(m_btnNext);
    root->addLayout(footer);

    connect(m_btnBack, &QPushButton::clicked, this, &KisFirstRunWizard::slotBack);
    connect(m_btnNext, &QPushButton::clicked, this, &KisFirstRunWizard::slotNextOrFinish);
}

bool KisFirstRunWizard::shouldRun()
{
    KConfigGroup grp(KSharedConfig::openConfig(), "Draworever");
    return !grp.readEntry("FirstRunDone", false);
}

void KisFirstRunWizard::markCompleted()
{
    KConfigGroup grp(KSharedConfig::openConfig(), "Draworever");
    grp.writeEntry("FirstRunDone", true);
    grp.sync();
}

void KisFirstRunWizard::slotBack()
{
    m_stack->setCurrentIndex(0);
    m_btnBack->setVisible(false);
    m_btnNext->setText(i18n("Next"));
}

void KisFirstRunWizard::slotNextOrFinish()
{
    if (m_stack->currentIndex() == 0) {
        m_stack->setCurrentIndex(1);
        m_btnBack->setVisible(true);
        m_btnNext->setText(i18n("Start Drawing"));
        return;
    }

    applyConfiguration();
    markCompleted();
    accept();
}

void KisFirstRunWizard::applyConfiguration()
{
    KisConfig cfg(false);
    const int dev = m_deviceGroup->checkedId();
    const int style = m_styleGroup->checkedId();

    // Base device configuration
    if (dev == DeviceMouse) {
        cfg.setLineSmoothingType(2); // Weighted
        cfg.setLineSmoothingDistanceMin(70.0);
        cfg.setLineSmoothingDistanceMax(70.0);
        cfg.setLineSmoothingSmoothPressure(false);
    } else if (dev == DeviceTouch) {
        cfg.setDisableTouchOnCanvas(false);
        cfg.setLineSmoothingType(2);
        cfg.setLineSmoothingDistanceMin(55.0);
        cfg.setLineSmoothingDistanceMax(55.0);
    } else {
        cfg.setLineSmoothingType(2);
        cfg.setLineSmoothingDistanceMin(45.0);
        cfg.setLineSmoothingDistanceMax(45.0);
        cfg.setLineSmoothingSmoothPressure(true);
    }

    // Drawing style adjustments
    if (style == StyleAnime) {
        cfg.setLineSmoothingType(2);
        cfg.setLineSmoothingDistanceMin(65.0);
        cfg.setLineSmoothingDistanceMax(65.0);
        cfg.defImageWidth(2480);
        cfg.defImageHeight(3508);
        cfg.defImageResolution(300.0);
    } else if (style == StylePixelArt) {
        cfg.setLineSmoothingType(0); // None / crisp pixel
        const int sz = m_pixelSizeCombo->currentData().toInt();
        cfg.defImageWidth(sz > 0 ? sz : 32);
        cfg.defImageHeight(sz > 0 ? sz : 32);
        cfg.defImageResolution(72.0);
    } else if (style == StylePainting) {
        cfg.setLineSmoothingDistanceMin(30.0);
        cfg.setLineSmoothingDistanceMax(30.0);
        cfg.defImageWidth(3840);
        cfg.defImageHeight(2560);
        cfg.defImageResolution(300.0);
    } else {
        cfg.defImageWidth(3840);
        cfg.defImageHeight(2560);
        cfg.defImageResolution(300.0);
    }
}
