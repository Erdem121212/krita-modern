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
#include <QCheckBox>

#include <klocalizedstring.h>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KisStorageModel.h>

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
    btn->setMinimumHeight(62);
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
    resize(540, 460);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 16);
    root->setSpacing(12);

    QLabel *header = new QLabel(i18n("Let's set up Draworever for you"), this);
    QFont hf = header->font();
    hf.setPointSize(14);
    hf.setBold(true);
    header->setFont(hf);
    root->addWidget(header);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, 1);

    // Page 0: Input Device
    QWidget *page0 = new QWidget(this);
    QVBoxLayout *p0Layout = new QVBoxLayout(page0);
    p0Layout->setSpacing(10);
    p0Layout->addWidget(new QLabel(i18n("How do you draw?"), page0));

    m_deviceGroup = new QButtonGroup(page0);
    m_deviceGroup->setExclusive(true);

    QPushButton *btnMouse = makeOptionCard(i18n("Mouse"), i18n("Extra stroke smoothing for steady lines without a pen"), page0);
    QPushButton *btnTouch = makeOptionCard(i18n("Touchscreen"), i18n("Touch-friendly gestures and smooth finger strokes"), page0);
    QPushButton *btnTablet = makeOptionCard(i18n("Drawing Tablet"), i18n("Pen pressure smoothing and balanced stabilization"), page0);
    btnTablet->setChecked(true);

    m_deviceGroup->addButton(btnMouse, DeviceMouse);
    m_deviceGroup->addButton(btnTouch, DeviceTouch);
    m_deviceGroup->addButton(btnTablet, DeviceTablet);

    p0Layout->addWidget(btnMouse);
    p0Layout->addWidget(btnTouch);
    p0Layout->addWidget(btnTablet);
    p0Layout->addStretch();
    m_stack->addWidget(page0);

    // Page 1: Drawing Style
    QWidget *page1 = new QWidget(this);
    QVBoxLayout *p1Layout = new QVBoxLayout(page1);
    p1Layout->setSpacing(10);
    p1Layout->addWidget(new QLabel(i18n("What's your drawing style?"), page1));

    m_styleGroup = new QButtonGroup(page1);
    m_styleGroup->setExclusive(true);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);

    QPushButton *btnNormal = makeOptionCard(i18n("Normal"), i18n("Balanced smoothing & 4K canvas"), page1);
    QPushButton *btnAnime = makeOptionCard(i18n("Anime / Manga"), i18n("Crisp lineart smoothing & portrait canvas"), page1);
    QPushButton *btnPixel = makeOptionCard(i18n("Pixel Art"), i18n("Zero smoothing & pixel-art canvas"), page1);
    QPushButton *btnPaint = makeOptionCard(i18n("Concept / Paint"), i18n("Light smoothing & wide canvas"), page1);
    btnNormal->setChecked(true);

    m_styleGroup->addButton(btnNormal, StyleNormal);
    m_styleGroup->addButton(btnAnime, StyleAnime);
    m_styleGroup->addButton(btnPixel, StylePixelArt);
    m_styleGroup->addButton(btnPaint, StylePainting);

    grid->addWidget(btnNormal, 0, 0);
    grid->addWidget(btnAnime, 0, 1);
    grid->addWidget(btnPixel, 1, 0);
    grid->addWidget(btnPaint, 1, 1);
    p1Layout->addLayout(grid);

    QHBoxLayout *pxRow = new QHBoxLayout();
    QLabel *pxLbl = new QLabel(i18n("Pixel Art Canvas Size:"), page1);
    m_pixelSizeCombo = new QComboBox(page1);
    m_pixelSizeCombo->addItem(QStringLiteral("16 x 16"), 16);
    m_pixelSizeCombo->addItem(QStringLiteral("32 x 32"), 32);
    m_pixelSizeCombo->addItem(QStringLiteral("64 x 64"), 64);
    m_pixelSizeCombo->setCurrentIndex(1);
    pxLbl->setVisible(false);
    m_pixelSizeCombo->setVisible(false);
    pxRow->addWidget(pxLbl);
    pxRow->addWidget(m_pixelSizeCombo);
    pxRow->addStretch();
    p1Layout->addLayout(pxRow);
    p1Layout->addStretch();
    m_stack->addWidget(page1);

    connect(btnPixel, &QPushButton::toggled, this, [pxLbl, this](bool checked) {
        pxLbl->setVisible(checked);
        m_pixelSizeCombo->setVisible(checked);
    });

    // Page 2: Starter Brush Sets
    QWidget *page2 = new QWidget(this);
    QVBoxLayout *p2Layout = new QVBoxLayout(page2);
    p2Layout->setSpacing(10);
    p2Layout->addWidget(new QLabel(i18n("Başlangıç Fırça Setleri (Brush Sets):"), page2));

    m_chkBuiltinBrushes = new QCheckBox(i18n("Draworever Yerleşik Fırçalar"), page2);
    m_chkKrita5Brushes = new QCheckBox(i18n("Krita 5 Fırçaları"), page2);
    m_chkKrita4Brushes = new QCheckBox(i18n("Krita 4 Fırçaları"), page2);
    m_chkKrita3Brushes = new QCheckBox(i18n("Krita 3 Fırçaları"), page2);

    m_chkBuiltinBrushes->setChecked(true);
    m_chkKrita5Brushes->setChecked(true);
    m_chkKrita4Brushes->setChecked(true);
    m_chkKrita3Brushes->setChecked(false);

    p2Layout->addWidget(m_chkBuiltinBrushes);
    p2Layout->addWidget(m_chkKrita5Brushes);
    p2Layout->addWidget(m_chkKrita4Brushes);
    p2Layout->addWidget(m_chkKrita3Brushes);
    p2Layout->addStretch();
    m_stack->addWidget(page2);

    // Page 3: Sidebar Complexity & Top Bar Mode + Inline Breeze Note
    QWidget *page3 = new QWidget(this);
    QVBoxLayout *p3Layout = new QVBoxLayout(page3);
    p3Layout->setSpacing(10);

    p3Layout->addWidget(new QLabel(i18n("Sidebar Zorluğu (Toolbox):"), page3));
    m_sidebarGroup = new QButtonGroup(page3);
    m_sidebarGroup->setExclusive(true);
    QHBoxLayout *sbRow = new QHBoxLayout();
    QPushButton *btnSimple = makeOptionCard(i18n("Basit Mod"), i18n("Yalnızca temel çizim araçları"), page3);
    QPushButton *btnAdv = makeOptionCard(i18n("Gelişmiş Mod"), i18n("Tüm araçlar açık"), page3);
    btnSimple->setChecked(true);
    m_sidebarGroup->addButton(btnSimple, 1);
    m_sidebarGroup->addButton(btnAdv, 0);
    sbRow->addWidget(btnSimple);
    sbRow->addWidget(btnAdv);
    p3Layout->addLayout(sbRow);

    p3Layout->addWidget(new QLabel(i18n("Üst Araç Çubuğu:"), page3));
    m_topBarGroup = new QButtonGroup(page3);
    m_topBarGroup->setExclusive(true);
    QHBoxLayout *tbRow = new QHBoxLayout();
    QPushButton *btnFixed = makeOptionCard(i18n("Sabit"), i18n("Butonlar yerinde sabit kalır"), page3);
    QPushButton *btnEditable = makeOptionCard(i18n("Düzenlenebilir"), i18n("Basılı tutup sürükleyerek düzenle"), page3);
    btnFixed->setChecked(true);
    m_topBarGroup->addButton(btnFixed, 0);
    m_topBarGroup->addButton(btnEditable, 1);
    tbRow->addWidget(btnFixed);
    tbRow->addWidget(btnEditable);
    p3Layout->addLayout(tbRow);

    m_breezeNoteLabel = new QLabel(QStringLiteral("Not : Breeze temaları şuanlık bozuk."), page3);
    m_breezeNoteLabel->setStyleSheet(QStringLiteral("color: palette(placeholder-text); font-size: 9pt; padding-top: 6px;"));
    p3Layout->addWidget(m_breezeNoteLabel);
    p3Layout->addStretch();
    m_stack->addWidget(page3);

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
    const int cur = m_stack->currentIndex();
    if (cur > 0) {
        m_stack->setCurrentIndex(cur - 1);
    }
    m_btnBack->setVisible(m_stack->currentIndex() > 0);
    m_btnNext->setText(i18n("Next"));
}

void KisFirstRunWizard::slotNextOrFinish()
{
    const int cur = m_stack->currentIndex();
    const int lastIdx = m_stack->count() - 1;
    if (cur < lastIdx) {
        m_stack->setCurrentIndex(cur + 1);
        m_btnBack->setVisible(true);
        m_btnNext->setText(cur + 1 == lastIdx ? i18n("Start Drawing") : i18n("Next"));
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

    if (dev == DeviceMouse) {
        cfg.setLineSmoothingType(2);
        cfg.setLineSmoothingDistanceMin(70.0);
        cfg.setLineSmoothingDistanceMax(70.0);
        cfg.setLineSmoothingSmoothPressure(false);
    } else if (dev == DeviceTouch) {
        cfg.setTouchPainting(KisConfig::TOUCH_PAINTING_ENABLED);
        cfg.setLineSmoothingType(2);
        cfg.setLineSmoothingDistanceMin(55.0);
        cfg.setLineSmoothingDistanceMax(55.0);
    } else {
        cfg.setLineSmoothingType(2);
        cfg.setLineSmoothingDistanceMin(45.0);
        cfg.setLineSmoothingDistanceMax(45.0);
        cfg.setLineSmoothingSmoothPressure(true);
    }

    if (style == StyleAnime) {
        cfg.setLineSmoothingType(2);
        cfg.setLineSmoothingDistanceMin(65.0);
        cfg.setLineSmoothingDistanceMax(65.0);
        cfg.defImageWidth(2480);
        cfg.defImageHeight(3508);
        cfg.defImageResolution(300.0);
    } else if (style == StylePixelArt) {
        cfg.setLineSmoothingType(0);
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

    KConfigGroup drawGrp(KSharedConfig::openConfig(), "Draworever");
    drawGrp.writeEntry("SidebarSimpleMode", m_sidebarGroup->checkedId() == 1);
    drawGrp.writeEntry("EditableTopBar", m_topBarGroup->checkedId() == 1);
    drawGrp.writeEntry("EnableBuiltinBrushes", m_chkBuiltinBrushes->isChecked());
    drawGrp.writeEntry("EnableKrita5Brushes", m_chkKrita5Brushes->isChecked());
    drawGrp.writeEntry("EnableKrita4Brushes", m_chkKrita4Brushes->isChecked());
    drawGrp.writeEntry("EnableKrita3Brushes", m_chkKrita3Brushes->isChecked());
    drawGrp.sync();

    // Toggle Krita 3/4/5 resource bundles in KisStorageModel
    if (KisStorageModel *sm = KisStorageModel::instance()) {
        for (int i = 0; i < sm->rowCount(); ++i) {
            const QModelIndex idx = sm->index(i, 0);
            const QString loc = idx.data(Qt::UserRole + KisStorageModel::Location).toString();
            if (loc.contains(QStringLiteral("Krita_5"), Qt::CaseInsensitive)) {
                sm->setData(idx, m_chkKrita5Brushes->isChecked() ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
            } else if (loc.contains(QStringLiteral("Krita_4"), Qt::CaseInsensitive)) {
                sm->setData(idx, m_chkKrita4Brushes->isChecked() ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
            } else if (loc.contains(QStringLiteral("Krita_3"), Qt::CaseInsensitive)) {
                sm->setData(idx, m_chkKrita3Brushes->isChecked() ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
            } else if (loc.contains(QStringLiteral("Kgaia_brushes"), Qt::CaseInsensitive) ||
                       loc.contains(QStringLiteral("Rakurri_Brush_Set"), Qt::CaseInsensitive) ||
                       loc.contains(QStringLiteral("stupid_idiot_bundle"), Qt::CaseInsensitive)) {
                sm->setData(idx, m_chkBuiltinBrushes->isChecked() ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
            }
        }
    }
}
