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
#include <QMouseEvent>
#include <QTabletEvent>

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
    btn->setMinimumHeight(62);
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { text-align: left; padding: 10px 14px; border-radius: 10px; border: 1px solid palette(mid); font-size: 10pt; }"
        "QPushButton:checked { border: 2px solid palette(highlight); background-color: palette(highlight); color: palette(highlighted-text); }"
    ));
    return btn;
}

class PenButtonDetectPad : public QLabel
{
public:
    explicit PenButtonDetectPad(QComboBox *countCombo, QWidget *parent = nullptr)
        : QLabel(parent), m_countCombo(countCombo)
    {
        setAlignment(Qt::AlignCenter);
        setMinimumHeight(64);
        setWordWrap(true);
        setText(i18n("Wacom / Tablet Pen Test Area: Press your pen's side buttons here to detect them"));
        setStyleSheet(QStringLiteral(
            "QLabel { border: 2px dashed palette(mid); border-radius: 10px; padding: 8px; background: palette(base); }"
        ));
    }

protected:
    void mousePressEvent(QMouseEvent *ev) override
    {
        if (ev->button() == Qt::MiddleButton || ev->button() == Qt::RightButton || ev->button() == Qt::ExtraButton1) {
            recordDetectedButton(int(ev->button()));
        }
        QLabel::mousePressEvent(ev);
    }

    void tabletEvent(QTabletEvent *ev) override
    {
        if (ev->buttons() != Qt::NoButton && ev->buttons() != Qt::LeftButton) {
            recordDetectedButton(int(ev->buttons()));
        }
        QLabel::tabletEvent(ev);
    }

private:
    void recordDetectedButton(int btnCode)
    {
        m_seenButtons.insert(btnCode);
        const int detected = qBound(1, m_seenButtons.size(), 3);
        m_countCombo->setCurrentIndex(detected - 1);
        setText(i18n("Detected %1 pen button(s)!", detected));
    }

    QComboBox *m_countCombo;
    QSet<int> m_seenButtons;
};
}

KisFirstRunWizard::KisFirstRunWizard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Welcome to Draworever"));
    setModal(true);
    resize(540, 470);

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

    // Page 1: Tablet Pen Buttons (only shown if Drawing Tablet is selected)
    QWidget *page1 = new QWidget(this);
    QVBoxLayout *p1Layout = new QVBoxLayout(page1);
    p1Layout->setSpacing(10);
    p1Layout->addWidget(new QLabel(i18n("Tablet Pen Buttons"), page1));

    m_penBtnCountCombo = new QComboBox(page1);
    m_penBtnCountCombo->addItem(i18n("1 Button"), 1);
    m_penBtnCountCombo->addItem(i18n("2 Buttons (Standard Wacom)"), 2);
    m_penBtnCountCombo->addItem(i18n("3 Buttons (Pro Pen)"), 3);
    m_penBtnCountCombo->setCurrentIndex(1);

    PenButtonDetectPad *detectPad = new PenButtonDetectPad(m_penBtnCountCombo, page1);
    p1Layout->addWidget(detectPad);

    QHBoxLayout *cntRow = new QHBoxLayout();
    cntRow->addWidget(new QLabel(i18n("Pen Button Count:"), page1));
    cntRow->addWidget(m_penBtnCountCombo);
    cntRow->addStretch();
    p1Layout->addLayout(cntRow);

    auto makeActionCombo = [page1](int defaultIdx) {
        QComboBox *cb = new QComboBox(page1);
        cb->addItem(i18n("Popup Palette / Right Click"), QStringLiteral("popup_palette"));
        cb->addItem(i18n("Pan Canvas"), QStringLiteral("pan"));
        cb->addItem(i18n("Color Picker (Eyedropper)"), QStringLiteral("color_picker"));
        cb->addItem(i18n("Toggle Eraser"), QStringLiteral("eraser"));
        cb->addItem(i18n("Undo (Ctrl+Z)"), QStringLiteral("undo"));
        cb->setCurrentIndex(defaultIdx);
        return cb;
    };

    m_penBtn1Action = makeActionCombo(1);
    m_penBtn2Action = makeActionCombo(0);
    m_penBtn3Action = makeActionCombo(2);

    QGridLayout *mapGrid = new QGridLayout();
    QLabel *lblB1 = new QLabel(i18n("Button 1:"), page1);
    QLabel *lblB2 = new QLabel(i18n("Button 2:"), page1);
    QLabel *lblB3 = new QLabel(i18n("Button 3:"), page1);
    mapGrid->addWidget(lblB1, 0, 0);
    mapGrid->addWidget(m_penBtn1Action, 0, 1);
    mapGrid->addWidget(lblB2, 1, 0);
    mapGrid->addWidget(m_penBtn2Action, 1, 1);
    mapGrid->addWidget(lblB3, 2, 0);
    mapGrid->addWidget(m_penBtn3Action, 2, 1);
    p1Layout->addLayout(mapGrid);

    auto updateBtnRows = [this, lblB2, lblB3]() {
        const int cnt = m_penBtnCountCombo->currentData().toInt();
        lblB2->setVisible(cnt >= 2);
        m_penBtn2Action->setVisible(cnt >= 2);
        lblB3->setVisible(cnt >= 3);
        m_penBtn3Action->setVisible(cnt >= 3);
    };
    connect(m_penBtnCountCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(update()));
    connect(m_penBtnCountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [updateBtnRows](int) {
        updateBtnRows();
    });
    updateBtnRows();
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

    connect(btnPixel, &QPushButton::toggled, this, [pxLbl, this](bool checked) {
        pxLbl->setVisible(checked);
        m_pixelSizeCombo->setVisible(checked);
    });

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

int KisFirstRunWizard::nextPageIndex(int current) const
{
    if (current == 0) {
        return (m_deviceGroup->checkedId() == DeviceTablet) ? 1 : 2;
    }
    return current + 1;
}

int KisFirstRunWizard::prevPageIndex(int current) const
{
    if (current == 2 && m_deviceGroup->checkedId() != DeviceTablet) {
        return 0;
    }
    return current - 1;
}

void KisFirstRunWizard::slotBack()
{
    const int prev = prevPageIndex(m_stack->currentIndex());
    if (prev >= 0) {
        m_stack->setCurrentIndex(prev);
    }
    m_btnBack->setVisible(m_stack->currentIndex() > 0);
    m_btnNext->setText(i18n("Next"));
}

void KisFirstRunWizard::slotNextOrFinish()
{
    const int cur = m_stack->currentIndex();
    const int lastIdx = m_stack->count() - 1;
    if (cur < lastIdx) {
        const int nxt = nextPageIndex(cur);
        m_stack->setCurrentIndex(nxt);
        m_btnBack->setVisible(true);
        m_btnNext->setText(nxt == lastIdx ? i18n("Start Drawing") : i18n("Next"));
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
    if (dev == DeviceTablet) {
        drawGrp.writeEntry("PenButtonCount", m_penBtnCountCombo->currentData().toInt());
        drawGrp.writeEntry("PenButton1Action", m_penBtn1Action->currentData().toString());
        drawGrp.writeEntry("PenButton2Action", m_penBtn2Action->currentData().toString());
        drawGrp.writeEntry("PenButton3Action", m_penBtn3Action->currentData().toString());
    }
    drawGrp.sync();
}
