/*
 * SPDX-FileCopyrightText: 2005 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
 * SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "widgets/kis_custom_image_widget.h"

#include <QMimeData>
#include <QSlider>
#include <QApplication>
#include <QScreen>
#include <QDialogButtonBox>
#include <QFile>
#include <QSpacerItem>

#include <QMessageBox>
#include <QPainter>
#include <QMouseEvent>
#include <QCheckBox>
#include <QTabBar>
#include <QVBoxLayout>
#include <KoResourcePaths.h>

#include <KFormat>
#include <kstandardguiitem.h>

#include <kis_debug.h>

#include <kis_icon.h>
#include <KoCompositeOp.h>
#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoID.h>
#include <KoColor.h>
#include <KoUnit.h>
#include <KoColorModelStandardIds.h>

#include <kis_fill_painter.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_group_layer.h>
#include <kis_paint_layer.h>
#include <kis_paint_device.h>
#include <kis_painter.h>

#include "kis_config.h"
#include "KisPart.h"
#include "KisDocument.h"
#include <KisSpinBoxI18nHelper.h>
#include <KisSqueezedComboBox.h>
#include "kis_signals_blocker.h"

static QString pixelsInchStr()
{
    static QString str = i18n("Pixels/Inch");
    return str;
}

static const QString pixelsCentimeterStr()
{
    static QString str = i18n("Pixels/Centimeter");
    return str;
}

namespace {
class CanvasPreviewWidget : public QWidget
{
public:
    explicit CanvasPreviewWidget(QDoubleSpinBox *widthBox, QDoubleSpinBox *heightBox, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_widthBox(widthBox)
        , m_heightBox(heightBox)
    {
        setMinimumHeight(175);
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

protected:
    enum Handle { None = 0, Left = 1, Right = 2, Top = 4, Bottom = 8 };

    QRectF canvasRect() const
    {
        const qreal maxBoxW = qMax<qreal>(40.0, width() - 48.0);
        const qreal maxBoxH = qMax<qreal>(40.0, height() - 36.0);
        const qreal w = qMax<qreal>(100.0, m_widthBox->value());
        const qreal h = qMax<qreal>(100.0, m_heightBox->value());
        const qreal maxDim = 8000.0;
        const qreal scale = qMin(maxBoxW / maxDim, maxBoxH / maxDim);
        const qreal rw = qBound<qreal>(28.0, w * scale, maxBoxW);
        const qreal rh = qBound<qreal>(28.0, h * scale, maxBoxH);
        return QRectF((width() - rw) * 0.5, (height() - rh) * 0.5, rw, rh);
    }

    int hitTest(const QPointF &pos) const
    {
        const QRectF r = canvasRect();
        const qreal margin = 10.0;
        if (!r.adjusted(-margin, -margin, margin, margin).contains(pos)) {
            return None;
        }
        int mask = None;
        if (qAbs(pos.x() - r.left()) <= margin) mask |= Left;
        if (qAbs(pos.x() - r.right()) <= margin) mask |= Right;
        if (qAbs(pos.y() - r.top()) <= margin) mask |= Top;
        if (qAbs(pos.y() - r.bottom()) <= margin) mask |= Bottom;
        return mask;
    }

    void updateCursor(int mask)
    {
        if ((mask == (Left | Top)) || (mask == (Right | Bottom))) {
            setCursor(Qt::SizeFDiagCursor);
        } else if ((mask == (Right | Top)) || (mask == (Left | Bottom))) {
            setCursor(Qt::SizeBDiagCursor);
        } else if (mask & (Left | Right)) {
            setCursor(Qt::SizeHorCursor);
        } else if (mask & (Top | Bottom)) {
            setCursor(Qt::SizeVerCursor);
        } else {
            unsetCursor();
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_activeHandle = hitTest(event->pos());
            if (m_activeHandle != None) {
                m_dragStart = event->pos();
                m_startW = m_widthBox->value();
                m_startH = m_heightBox->value();
                event->accept();
                return;
            }
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_activeHandle != None) {
            const qreal maxBoxW = qMax<qreal>(40.0, width() - 48.0);
            const qreal maxBoxH = qMax<qreal>(40.0, height() - 36.0);
            const qreal scale = qMin(maxBoxW / 8000.0, maxBoxH / 8000.0);
            const QPointF delta = event->pos() - m_dragStart;
            const qreal dxPx = (delta.x() * 2.0) / qMax(0.001, scale);
            const qreal dyPx = (delta.y() * 2.0) / qMax(0.001, scale);

            qreal newW = m_startW;
            qreal newH = m_startH;
            if (m_activeHandle & Right) newW += dxPx;
            if (m_activeHandle & Left)  newW -= dxPx;
            if (m_activeHandle & Bottom) newH += dyPx;
            if (m_activeHandle & Top)    newH -= dyPx;

            m_widthBox->setValue(qBound(16.0, qRound(newW / 10.0) * 10.0, 16000.0));
            m_heightBox->setValue(qBound(16.0, qRound(newH / 10.0) * 10.0, 16000.0));
            update();
            event->accept();
            return;
        }
        updateCursor(hitTest(event->pos()));
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_activeHandle != None) {
            m_activeHandle = None;
            updateCursor(hitTest(event->pos()));
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF bg = rect().adjusted(2, 2, -2, -2);
        QColor bgColor = palette().color(QPalette::Base);
        p.setPen(QPen(palette().color(QPalette::Mid), 1));
        p.setBrush(bgColor);
        p.drawRoundedRect(bg, 8, 8);

        // Subtle grid background
        p.save();
        p.setClipRect(bg.adjusted(1, 1, -1, -1));
        QColor gridColor = palette().color(QPalette::Mid);
        gridColor.setAlpha(60);
        p.setPen(QPen(gridColor, 1));
        const int step = 18;
        for (int x = int(bg.left()) + step; x < int(bg.right()); x += step) {
            p.drawLine(x, int(bg.top()), x, int(bg.bottom()));
        }
        for (int y = int(bg.top()) + step; y < int(bg.bottom()); y += step) {
            p.drawLine(int(bg.left()), y, int(bg.right()), y);
        }
        p.restore();

        // Canvas rectangle
        const QRectF r = canvasRect();
        QColor accent = palette().color(QPalette::Highlight);
        QColor fill = palette().color(QPalette::Window).lighter(115);
        p.setPen(QPen(accent, 2));
        p.setBrush(fill);
        p.drawRoundedRect(r, 4, 4);

        // Size label inside preview
        p.setPen(palette().color(QPalette::Text));
        const QString sizeLabel = QString("%1 x %2 px")
            .arg(qRound(m_widthBox->value()))
            .arg(qRound(m_heightBox->value()));
        p.drawText(r, Qt::AlignCenter, sizeLabel);

        // Draggable edge/corner handles
        p.setBrush(accent);
        p.setPen(Qt::NoPen);
        const qreal hs = 6.0;
        const QPointF handles[8] = {
            r.topLeft(), QPointF(r.center().x(), r.top()), r.topRight(),
            QPointF(r.right(), r.center().y()), r.bottomRight(),
            QPointF(r.center().x(), r.bottom()), r.bottomLeft(),
            QPointF(r.left(), r.center().y())
        };
        for (const QPointF &pt : handles) {
            p.drawEllipse(pt, hs * 0.55, hs * 0.55);
        }
    }

private:
    QDoubleSpinBox *m_widthBox;
    QDoubleSpinBox *m_heightBox;
    int m_activeHandle {None};
    QPointF m_dragStart;
    qreal m_startW {0};
    qreal m_startH {0};
};
}

KisCustomImageWidget::KisCustomImageWidget(QWidget* parent, const QString& imageName, const QLatin1String &dialogType)
    : WdgNewImage(parent)
    , m_dialogType(dialogType)
{
    setObjectName("KisCustomImageWidget");
    m_openPane = qobject_cast<KisOpenPane*>(parent);
    Q_ASSERT(m_openPane);

    KisConfig cfg(true);

    const qint32 defWidth = cfg.defImageWidth();
    const qint32 defHeight = cfg.defImageHeight();
    const double resolution = cfg.defImageResolution();
    const QString& defColorModel = cfg.defColorModel();
    const QString& defColorDepth = cfg.defaultColorDepth();
    const QString& defColorProfile = cfg.defColorProfile();

    txtName->setText(imageName);
    m_widthUnit = KoUnit(KoUnit::Pixel, resolution);
    doubleWidth->setValue(defWidth);
    doubleWidth->setDecimals(0);
    m_width = m_widthUnit.fromUserValue(defWidth);
    cmbWidthUnit->addItems(KoUnit::listOfUnitNameForUi(KoUnit::ListAll));
    cmbWidthUnit->setCurrentIndex(m_widthUnit.indexInListForUi(KoUnit::ListAll));

    m_heightUnit = KoUnit(KoUnit::Pixel, resolution);
    doubleHeight->setValue(defHeight);
    doubleHeight->setDecimals(0);
    m_height = m_heightUnit.fromUserValue(defHeight);
    cmbHeightUnit->addItems(KoUnit::listOfUnitNameForUi(KoUnit::ListAll));
    cmbHeightUnit->setCurrentIndex(m_heightUnit.indexInListForUi(KoUnit::ListAll));

    cmbResolutionUnit->addItem(pixelsInchStr());
    cmbResolutionUnit->addItem(pixelsCentimeterStr());

    doubleResolution->setValue(72.0 * resolution);
    doubleResolution->setDecimals(2);

    grpClipboard->hide();

    sliderOpacity->setRange(0, 100, 0);
    sliderOpacity->setValue(100);
    KisSpinBoxI18nHelper::setText(sliderOpacity, i18nc("{n} is the number value, % is the percent sign", "{n}%"));

    connect(cmbPredefined, SIGNAL(activated(int)), SLOT(predefinedClicked(int)));
    connect(doubleResolution, SIGNAL(valueChanged(double)),
            this, SLOT(resolutionChanged(double)));
    connect(cmbWidthUnit, SIGNAL(activated(int)),
            this, SLOT(widthUnitChanged(int)));
    connect(doubleWidth, SIGNAL(valueChanged(double)),
            this, SLOT(widthChanged(double)));
    connect(cmbHeightUnit, SIGNAL(activated(int)),
            this, SLOT(heightUnitChanged(int)));
    connect(doubleHeight, SIGNAL(valueChanged(double)),
            this, SLOT(heightChanged(double)));
    connect(cmbResolutionUnit, SIGNAL(currentIndexChanged(int)),
            this, SLOT(resolutionUnitChanged()));


    // Create image
    newDialogConfirmationButtonBox->button(QDialogButtonBox::Ok)->setText(i18n("&Create"));
    KGuiItem::assign(newDialogConfirmationButtonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());
    connect(newDialogConfirmationButtonBox, SIGNAL(accepted()), this, SLOT(createImage()));


    // Cancel Create image button
    connect(newDialogConfirmationButtonBox, SIGNAL(rejected()), m_openPane, SLOT(reject()));



    bnPortrait->setIcon(KisIconUtils::loadIcon("portrait"));
    connect(bnPortrait, SIGNAL(clicked()), SLOT(setPortrait()));
    connect(bnLandscape, SIGNAL(clicked()), SLOT(setLandscape()));
    bnLandscape->setIcon(KisIconUtils::loadIcon("landscape"));

    connect(doubleWidth, SIGNAL(valueChanged(double)), this, SLOT(switchPortraitLandscape()));
    connect(doubleHeight, SIGNAL(valueChanged(double)), this, SLOT(switchPortraitLandscape()));
    connect(bnSaveAsPredefined, SIGNAL(clicked()), this, SLOT(saveAsPredefined()));

    colorSpaceSelector->setCurrentColorModel(KoID(defColorModel));
    colorSpaceSelector->setCurrentColorDepth(KoID(defColorDepth));
    colorSpaceSelector->setCurrentProfile(defColorProfile);
    connect(colorSpaceSelector, SIGNAL(colorSpaceChanged(const KoColorSpace*)), this, SLOT(changeDocumentInfoLabel()));

    connect(colorSpaceSelector, SIGNAL(selectionChanged(bool)), newDialogConfirmationButtonBox->button(QDialogButtonBox::Ok), SLOT(setEnabled(bool)));

    intNumLayers->setValue(cfg.numDefaultLayers(m_dialogType));
    KoColor bcol(KoColorSpaceRegistry::instance()->rgb8());
    bcol.fromQColor(cfg.defaultBackgroundColor(m_dialogType));
    cmbColor->setColor(bcol);
    setBackgroundOpacity(cfg.defaultBackgroundOpacity(m_dialogType));

    KisConfig::BackgroundStyle bgStyle = cfg.defaultBackgroundStyle(m_dialogType);

    if (bgStyle == KisConfig::RASTER_LAYER) {
      radioBackgroundAsRaster->setChecked(true);
    } else if (bgStyle == KisConfig::FILL_LAYER) {
      radioBackgroundAsFill->setChecked(true);
    } else {
      radioBackgroundAsProjection->setChecked(true);
    }

    fillPredefined();
    switchPortraitLandscape();

    // this makes the portrait and landscape buttons more
    // obvious what is selected by changing the highlight color
    QPalette p = QApplication::palette();
    QPalette palette_highlight(p );
    QColor c = p.color(QPalette::Highlight);
    palette_highlight.setColor(QPalette::Button, c);
    bnLandscape->setPalette(palette_highlight);
    bnPortrait->setPalette(palette_highlight);
    changeDocumentInfoLabel();

    // Insert interactive grid-backed canvas preview at the top and hide advanced options by default
    CanvasPreviewWidget *preview = new CanvasPreviewWidget(doubleWidth, doubleHeight, this);
    connect(doubleWidth, SIGNAL(valueChanged(double)), preview, SLOT(update()));
    connect(doubleHeight, SIGNAL(valueChanged(double)), preview, SLOT(update()));

    QCheckBox *chkAdvanced = new QCheckBox(i18n("Show Advanced Settings"), this);
    chkAdvanced->setChecked(false);
    connect(chkAdvanced, SIGNAL(toggled(bool)), this, SLOT(toggleAdvancedOptions(bool)));

    if (QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout())) {
        mainLayout->insertWidget(0, preview);
        int btnIdx = mainLayout->indexOf(newDialogConfirmationButtonBox);
        if (btnIdx >= 0) {
            mainLayout->insertWidget(btnIdx, chkAdvanced);
        } else {
            mainLayout->addWidget(chkAdvanced);
        }
    }
    toggleAdvancedOptions(false);
}

void KisCustomImageWidget::toggleAdvancedOptions(bool show)
{
    tabWidget->tabBar()->setVisible(show);
    if (!show) {
        tabWidget->setCurrentIndex(0);
    }
    grpMode->setVisible(show);
    bnSaveAsPredefined->setVisible(show);
}

void KisCustomImageWidget::showEvent(QShowEvent *)
{
    fillPredefined();
    newDialogConfirmationButtonBox->button(QDialogButtonBox::Ok)->setFocus();
    newDialogConfirmationButtonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
}

KisCustomImageWidget::~KisCustomImageWidget()
{
    m_predefined.clear();
}

void KisCustomImageWidget::resolutionChanged(double res)
{  
    if (m_widthUnit.type() == KoUnit::Pixel) {
        m_widthUnit.setFactor(res / 72.0);
        m_width = m_widthUnit.fromUserValue(doubleWidth->value());
    }

    if (m_heightUnit.type() == KoUnit::Pixel) {
        m_heightUnit.setFactor(res / 72.0);
        m_height = m_heightUnit.fromUserValue(doubleHeight->value());
    }
    changeDocumentInfoLabel();
}

void KisCustomImageWidget::resolutionUnitChanged()
{
    qreal resolution = doubleResolution->value();

    if (cmbResolutionUnit->currentText() == pixelsInchStr()) {
        resolution = KoUnit::convertFromUnitToUnit(resolution, KoUnit(KoUnit::Inch), KoUnit(KoUnit::Centimeter));
    } else {
        resolution = KoUnit::convertFromUnitToUnit(resolution, KoUnit(KoUnit::Centimeter), KoUnit(KoUnit::Inch));
    }

    {
        KisSignalsBlocker b(doubleResolution);
        doubleResolution->setValue(resolution);
    }
}

void KisCustomImageWidget::widthUnitChanged(int index)
{
    doubleWidth->blockSignals(true);

    m_widthUnit = KoUnit::fromListForUi(index, KoUnit::ListAll);
    if (m_widthUnit.type() == KoUnit::Pixel) {
        doubleWidth->setDecimals(0);
        m_widthUnit.setFactor(doubleResolution->value() / 72.0);
    } else {
        doubleWidth->setDecimals(2);
    }

    doubleWidth->setValue(m_widthUnit.toUserValuePrecise(m_width));

    doubleWidth->blockSignals(false);
    changeDocumentInfoLabel();
}

void KisCustomImageWidget::widthChanged(double value)
{
    m_width = m_widthUnit.fromUserValue(value);
    changeDocumentInfoLabel();
}

void KisCustomImageWidget::heightUnitChanged(int index)
{
    doubleHeight->blockSignals(true);

    m_heightUnit = KoUnit::fromListForUi(index, KoUnit::ListAll);
    if (m_heightUnit.type() == KoUnit::Pixel) {
        doubleHeight->setDecimals(0);
        m_heightUnit.setFactor(doubleResolution->value() / 72.0);
    } else {
        doubleHeight->setDecimals(2);
    }

    doubleHeight->setValue(m_heightUnit.toUserValuePrecise(m_height));

    doubleHeight->blockSignals(false);
    changeDocumentInfoLabel();
}

void KisCustomImageWidget::heightChanged(double value)
{
    m_height = m_heightUnit.fromUserValue(value);
    changeDocumentInfoLabel();
}

void KisCustomImageWidget::createImage()
{
    double resolution = 1.0;
    if (cmbResolutionUnit->currentText() == pixelsInchStr()) {
        resolution = doubleResolution->value() / 72.0;
    } else if (cmbResolutionUnit->currentText() == pixelsCentimeterStr()) {
        resolution = doubleResolution->value() / 28.34;
    }
    const qint32 pxW = static_cast<qint32>(0.5 + KoUnit(KoUnit::Pixel, resolution).toUserValuePrecise(m_width));
    const qint32 pxH = static_cast<qint32>(0.5 + KoUnit(KoUnit::Pixel, resolution).toUserValuePrecise(m_height));

    if (pxW < 400 || pxH < 400) {
        const QMessageBox::StandardButton answer = QMessageBox::warning(
            this,
            i18n("Low Resolution Warning"),
            i18n("This canvas is very small (%1 x %2 px), so your brush strokes may look pixelated! Are you sure you want to continue? (Great for pixel art, though!)",
                 pxW, pxH),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    if (static_cast<qint64>(pxW) * static_cast<qint64>(pxH) > 36000000LL || pxW > 8000 || pxH > 8000) {
        const QMessageBox::StandardButton answer = QMessageBox::warning(
            this,
            i18n("Huge Canvas Warning"),
            QStringLiteral("Hey buddy the bigger resolution , the lower performance!! Be careful.. You may crash your PC with that big resolution! Well i cant give guarantee if yours an good PC:.. Idk.. yeah."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    newDialogConfirmationButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    KisDocument *doc = createNewImage();
    if (doc) {
        doc->setModified(false);
        Q_EMIT m_openPane->documentSelected(doc);
        m_openPane->accept();
    } else {
        newDialogConfirmationButtonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}

KisDocument* KisCustomImageWidget::createNewImage()
{
    const KoColorSpace * cs = colorSpaceSelector->currentColorSpace();

    if (cs->colorModelId() == RGBAColorModelID &&
        cs->colorDepthId() == Integer8BitsColorDepthID) {

        const KoColorProfile *profile = cs->profile();

        if (profile->name().contains("linear") ||
            profile->name().contains("scRGB") ||
            profile->info().contains("linear") ||
            profile->info().contains("scRGB")) {

            int result =
                QMessageBox::warning(this,
                                     i18nc("@title:window", "Krita"),
                                     i18n("Linear gamma RGB color spaces are not supposed to be used "
                                          "in 8-bit integer modes. It is suggested to use 16-bit integer "
                                          "or any floating point colorspace for linear profiles.\n\n"
                                          "Press \"Ok\" to create a 8-bit integer linear RGB color space "
                                          "or \"Cancel\" to return to the settings dialog."),
                                     QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);

            if (result == QMessageBox::Cancel) {
                dbgKrita << "Model RGB8" << "NOT SUPPORTED";
                dbgKrita << ppVar(cs->name());
                dbgKrita << ppVar(cs->profile()->name());
                dbgKrita << ppVar(cs->profile()->info());
                return 0;
            }
        }
    }
    KisDocument *doc = static_cast<KisDocument*>(KisPart::instance()->createDocument());

    qint32 width, height;
    double resolution ;
    KIS_SAFE_ASSERT_RECOVER(cmbResolutionUnit->currentText() == pixelsInchStr() || cmbResolutionUnit->currentText() == pixelsCentimeterStr()) { resolution = 1.0; }
    if (cmbResolutionUnit->currentText() == pixelsInchStr()) {
        resolution = doubleResolution->value() / 72.0;  // internal resolution is in pixels per pt
    } else if (cmbResolutionUnit->currentText() == pixelsCentimeterStr()) {
        resolution = doubleResolution->value() / 28.34;
    } else {
        resolution = 1.0;
    }
    width = static_cast<qint32>(0.5  + KoUnit(KoUnit::Pixel, resolution).toUserValuePrecise(m_width));
    height = static_cast<qint32>(0.5 + KoUnit(KoUnit::Pixel, resolution).toUserValuePrecise(m_height));

    // Automatically supersample small non-pixel-art canvases so raster brushes stay smooth and crisp
    if (width > 128 && height > 128) {
        const qint32 minSide = qMin(width, height);
        int superScale = 1;
        if (minSide < 800) {
            superScale = 3;
        } else if (minSide < 1600) {
            superScale = 2;
        }
        if (superScale > 1) {
            width *= superScale;
            height *= superScale;
            resolution *= superScale;
        }
    }

    QColor qc = cmbColor->color().toQColor();
    qc.setAlpha(backgroundOpacity());
    KoColor bgColor(qc, cs);

    KisConfig::BackgroundStyle bgStyle = KisConfig::CANVAS_COLOR;
    if( radioBackgroundAsRaster->isChecked() ){
        bgStyle = KisConfig::RASTER_LAYER;
    } else if( radioBackgroundAsFill->isChecked() ){
        bgStyle = KisConfig::FILL_LAYER;
    }

    doc->newImage(txtName->text(), width, height, cs, bgColor, bgStyle, intNumLayers->value(), txtDescription->toPlainText(), resolution);

    KisConfig cfg(false);
    cfg.setNumDefaultLayers(m_dialogType, intNumLayers->value());
    cfg.setDefaultBackgroundOpacity(m_dialogType, backgroundOpacity());
    cfg.setDefaultBackgroundColor(m_dialogType, cmbColor->color().toQColor());
    cfg.setDefaultBackgroundStyle(m_dialogType, bgStyle);

    if (shouldSaveImageDimensions()) {
        cfg.defImageWidth(width);
        cfg.defImageHeight(height);
        cfg.defImageResolution(resolution);
        if (!cfg.useDefaultColorSpace()) {
            cfg.defColorModel(cs->colorModelId().id());
            cfg.setDefaultColorDepth(cs->colorDepthId().id());
            cfg.defColorProfile(cs->profile() ? cs->profile()->name() : "");
        }
    }

    return doc;
}

bool KisCustomImageWidget::shouldSaveImageDimensions() const
{
    return true;
}

void KisCustomImageWidget::setNumberOfLayers(int layers)
{
    intNumLayers->setValue(layers);
}

quint8 KisCustomImageWidget::backgroundOpacity() const
{
    qint32 opacity = sliderOpacity->value();

    if (!opacity)
        return 0;

    return (opacity * 255) / 100;
}

void KisCustomImageWidget::setBackgroundOpacity(quint8 value) {
  sliderOpacity->setValue((value * 100) / 255);
}

void KisCustomImageWidget::fillPredefined()
{
    cmbPredefined->clear();
    m_predefined.clear();

    cmbPredefined->addItem("");

    QStringList definitions = KoResourcePaths::findAllAssets("data", "predefined_image_sizes/*.predefinedimage", KoResourcePaths::Recursive);
    definitions.sort();

    if (!definitions.empty()) {

        Q_FOREACH (const QString &definition, definitions) {
            QFile f(definition);
            if (f.open(QIODevice::ReadOnly)) {
                QString xml = QString::fromUtf8(f.readAll());
                KisPropertiesConfigurationSP predefined = new KisPropertiesConfiguration;
                predefined->fromXML(xml);
                if (predefined->hasProperty("name")
                        && predefined->hasProperty("width")
                        && predefined->hasProperty("height")
                        && predefined->hasProperty("resolution")
                        && predefined->hasProperty("x-unit")
                        && predefined->hasProperty("y-unit")) {
                    m_predefined << predefined;
                    cmbPredefined->addItem(predefined->getString("name"));
                }
            }
        }
    }

    cmbPredefined->setCurrentIndex(0);

}


void KisCustomImageWidget::predefinedClicked(int index)
{
    if (index < 1 || index > m_predefined.size()) return;

    KisPropertiesConfigurationSP predefined = m_predefined[index - 1];
    txtPredefinedName->setText(predefined->getString("name"));
    if (cmbResolutionUnit->currentText() == pixelsInchStr()) {
        doubleResolution->setValue(predefined->getDouble("resolution"));
    } else {
        doubleResolution->setValue(KoUnit::convertFromUnitToUnit(predefined->getDouble("resolution"), KoUnit(KoUnit::Centimeter), KoUnit(KoUnit::Inch)));
    }
    cmbWidthUnit->setCurrentIndex(predefined->getInt("x-unit"));
    cmbHeightUnit->setCurrentIndex(predefined->getInt("y-unit"));

    widthUnitChanged(cmbWidthUnit->currentIndex());
    heightUnitChanged(cmbHeightUnit->currentIndex());

    doubleWidth->setValue(predefined->getDouble("width"));
    doubleHeight->setValue(predefined->getDouble("height"));
    changeDocumentInfoLabel();

}

void KisCustomImageWidget::saveAsPredefined()
{
    QString fileName = txtPredefinedName->text();
    if (fileName.isEmpty()) {
        return;
    }
    QString saveLocation = KoResourcePaths::saveLocation("data", "predefined_image_sizes/", true);
    QFile f(saveLocation + '/' + fileName.replace(' ', '_').replace('(', '_').replace(')', '_').replace(':', '_') + ".predefinedimage");

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, i18nc("@title:window", "Krita"),
            i18n("Could not save %1.\nReason: %2.", f.fileName(), f.errorString()));
        return;
    }
    KisPropertiesConfigurationSP predefined = new KisPropertiesConfiguration();
    predefined->setProperty("name", txtPredefinedName->text());
    predefined->setProperty("width", doubleWidth->value());
    predefined->setProperty("height", doubleHeight->value());
    predefined->setProperty("resolution", doubleResolution->value());
    predefined->setProperty("x-unit", cmbWidthUnit->currentIndex());
    predefined->setProperty("y-unit", cmbHeightUnit->currentIndex());

    QString xml = predefined->toXML();

    f.write(xml.toUtf8());
    f.flush();
    f.close();

    int i = 0;
    bool found = false;
    Q_FOREACH (KisPropertiesConfigurationSP pr, m_predefined) {
        if (pr->getString("name") == txtPredefinedName->text()) {
            found = true;
            break;
        }
        ++i;
    }
    if (found) {
        m_predefined[i] = predefined;
    }
    else {
        m_predefined.append(predefined);
        cmbPredefined->addItem(txtPredefinedName->text());
    }

}

void KisCustomImageWidget::setLandscape()
{
    if (doubleWidth->value() < doubleHeight->value()) {
        switchWidthHeight();
    }
}

void KisCustomImageWidget::setPortrait()
{
    if (doubleWidth->value() > doubleHeight->value()) {
        switchWidthHeight();
    }
}

void KisCustomImageWidget::switchWidthHeight()
{
    double width = doubleWidth->value();
    double height = doubleHeight->value();

    doubleHeight->clearFocus();
    doubleWidth->clearFocus();

    doubleHeight->blockSignals(true);
    doubleWidth->blockSignals(true);
    cmbWidthUnit->blockSignals(true);
    cmbHeightUnit->blockSignals(true);

    doubleWidth->setValue(height);
    doubleHeight->setValue(width);
    cmbWidthUnit->setCurrentIndex(m_heightUnit.indexInListForUi(KoUnit::ListAll));
    cmbHeightUnit->setCurrentIndex(m_widthUnit.indexInListForUi(KoUnit::ListAll));

    doubleHeight->blockSignals(false);
    doubleWidth->blockSignals(false);
    cmbWidthUnit->blockSignals(false);
    cmbHeightUnit->blockSignals(false);
    switchPortraitLandscape();

    widthChanged(doubleWidth->value());
    heightChanged(doubleHeight->value());
    changeDocumentInfoLabel();
}

void KisCustomImageWidget::switchPortraitLandscape()
{
    if(doubleWidth->value() > doubleHeight->value())
        bnLandscape->setChecked(true);
    else
        bnPortrait->setChecked(true);
}

void KisCustomImageWidget::changeDocumentInfoLabel()
{

    qint64 width, height;
    double resolution;
    resolution = doubleResolution->value() / 72.0;  // internal resolution is in pixels per pt

    width = static_cast<qint64>(0.5  + KoUnit(KoUnit::Pixel, resolution).toUserValuePrecise(m_width));
    height = static_cast<qint64>(0.5 + KoUnit(KoUnit::Pixel, resolution).toUserValuePrecise(m_height));

    qint64 layerSize = width * height;
    const KoColorSpace *cs = colorSpaceSelector->currentColorSpace();
    int bitSize = 8 * cs->pixelSize(); //pixelsize is in bytes.
    layerSize = layerSize * cs->pixelSize();
    QString text = i18nc("arg1: width. arg2: height. arg3: colorspace name. arg4: size of a channel in bits. arg5: image size",
                         "This document will be %1 pixels by %2 pixels in %3. The pixel size is %4 bit. A single paint layer will use %5 of RAM.",
                         width,
                         height,
                         cs->name(),
                         bitSize,
                         KFormat().formatByteSize(layerSize));
    lblDocumentInfo->setText(text);
}

