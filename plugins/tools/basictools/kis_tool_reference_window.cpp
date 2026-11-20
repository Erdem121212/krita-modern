/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_reference_window.h"

#include <QDialog>
#include <QPushButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QTimer>
#include <QSizeGrip>
#include <QApplication>

#include <klocalizedstring.h>
#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoColor.h>
#include <KoIcon.h>
#include <kis_icon.h>
#include <kis_image.h>
#include <kis_paint_device.h>
#include <canvas/kis_canvas2.h>

class KisFloatingReferenceWindow : public QWidget
{
    Q_OBJECT
public:
    enum Mode { CanvasMode = 0, ImageMode = 1 };

    explicit KisFloatingReferenceWindow(KoCanvasBase *canvas, QWidget *parent = nullptr)
        : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
        , m_canvas(canvas)
    {
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAcceptDrops(true);
        resize(340, 320);
        setMinimumSize(220, 200);

        setStyleSheet(QStringLiteral(
            "KisFloatingReferenceWindow { border: 1px solid palette(mid); border-radius: 10px; background: palette(window); }"
            "QPushButton { border-radius: 6px; padding: 4px 10px; }"
        ));

        QVBoxLayout *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(8, 8, 8, 8);
        rootLayout->setSpacing(6);

        // Top bar: Canvas / Image tabs, Load Image, X close
        m_topBar = new QWidget(this);
        QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
        topLayout->setContentsMargins(2, 2, 2, 2);
        topLayout->setSpacing(6);

        m_modeGroup = new QButtonGroup(this);
        m_modeGroup->setExclusive(true);

        m_btnCanvas = new QPushButton(i18n("Canvas"), m_topBar);
        m_btnCanvas->setCheckable(true);
        m_btnCanvas->setChecked(true);
        m_modeGroup->addButton(m_btnCanvas, CanvasMode);

        m_btnImage = new QPushButton(i18n("Image"), m_topBar);
        m_btnImage->setCheckable(true);
        m_modeGroup->addButton(m_btnImage, ImageMode);

        m_btnLoadImage = new QPushButton(i18n("Open..."), m_topBar);
        m_btnLoadImage->setVisible(false);

        QPushButton *btnClose = new QPushButton(QStringLiteral("X"), m_topBar);
        btnClose->setFixedSize(26, 26);

        topLayout->addWidget(m_btnCanvas);
        topLayout->addWidget(m_btnImage);
        topLayout->addWidget(m_btnLoadImage);
        topLayout->addStretch();
        topLayout->addWidget(btnClose);

        rootLayout->addWidget(m_topBar);

        // Viewport area
        m_viewport = new QWidget(this);
        m_viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_viewport->setMouseTracking(true);
        m_viewport->installEventFilter(this);
        rootLayout->addWidget(m_viewport, 1);

        // Bottom resize grip
        QHBoxLayout *bottomLayout = new QHBoxLayout();
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->addStretch();
        bottomLayout->addWidget(new QSizeGrip(this), 0, Qt::AlignBottom | Qt::AlignRight);
        rootLayout->addLayout(bottomLayout);

        // Long-press timer for eyedropper
        m_longPressTimer = new QTimer(this);
        m_longPressTimer->setSingleShot(true);
        m_longPressTimer->setInterval(350);
        connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
            m_pickingColor = true;
            m_viewport->setCursor(Qt::CrossCursor);
            pickColorAt(m_pressPos);
            m_viewport->update();
        });

        // Periodic live canvas refresh while in Canvas mode
        m_canvasRefreshTimer = new QTimer(this);
        m_canvasRefreshTimer->setInterval(300);
        connect(m_canvasRefreshTimer, &QTimer::timeout, this, [this]() {
            if (isVisible() && m_mode == CanvasMode) {
                refreshCanvasSnapshot();
            }
        });
        m_canvasRefreshTimer->start();

        connect(m_modeGroup, SIGNAL(idClicked(int)), this, SLOT(setMode(int)));
        connect(m_btnLoadImage, &QPushButton::clicked, this, &KisFloatingReferenceWindow::loadReferenceImage);
        connect(btnClose, &QPushButton::clicked, this, &QWidget::hide);

        refreshCanvasSnapshot();
    }

public Q_SLOTS:
    void setMode(int mode)
    {
        m_mode = static_cast<Mode>(mode);
        m_btnLoadImage->setVisible(m_mode == ImageMode);
        if (m_mode == CanvasMode) {
            refreshCanvasSnapshot();
        } else if (m_loadedImage.isNull()) {
            loadReferenceImage();
        }
        m_viewport->update();
    }

    void loadReferenceImage()
    {
        const QString fileName = QFileDialog::getOpenFileName(
            this,
            i18n("Select Reference Image"),
            QString(),
            i18n("Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)"));
        if (!fileName.isEmpty()) {
            QImage img(fileName);
            if (!img.isNull()) {
                m_loadedImage = img.convertToFormat(QImage::Format_ARGB32);
                m_viewport->update();
            }
        }
    }

protected:
    void refreshCanvasSnapshot()
    {
        KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2*>(m_canvas);
        if (!kisCanvas || !kisCanvas->image()) {
            return;
        }
        KisImageSP img = kisCanvas->image();
        if (img->width() > 0 && img->height() > 0) {
            m_canvasSnapshot = img->projection()->convertToQImage(0, 0, 0, img->width(), img->height());
            m_viewport->update();
        }
    }

    QRect targetImageRect(const QImage &src) const
    {
        if (src.isNull() || m_viewport->width() <= 0 || m_viewport->height() <= 0) {
            return QRect();
        }
        QSize scaled = src.size().scaled(m_viewport->size() - QSize(8, 8), Qt::KeepAspectRatio);
        int x = (m_viewport->width() - scaled.width()) / 2;
        int y = (m_viewport->height() - scaled.height()) / 2;
        return QRect(QPoint(x, y), scaled);
    }

    void pickColorAt(const QPoint &widgetPos)
    {
        const QImage &src = (m_mode == CanvasMode) ? m_canvasSnapshot : m_loadedImage;
        if (src.isNull()) {
            return;
        }
        const QRect dst = targetImageRect(src);
        if (!dst.contains(widgetPos)) {
            return;
        }
        const int ix = qBound(0, ((widgetPos.x() - dst.x()) * src.width()) / qMax(1, dst.width()), src.width() - 1);
        const int iy = qBound(0, ((widgetPos.y() - dst.y()) * src.height()) / qMax(1, dst.height()), src.height() - 1);
        m_lastPickedColor = src.pixelColor(ix, iy);
        m_lastPickPos = widgetPos;

        KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2*>(m_canvas);
        if (kisCanvas && kisCanvas->image() && m_canvas->resourceManager()) {
            KoColor koCol(m_lastPickedColor, kisCanvas->image()->colorSpace());
            m_canvas->resourceManager()->setForegroundColor(koCol);
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_topBar->geometry().contains(event->pos())) {
            m_draggingWindow = true;
            m_windowDragOffset = event->globalPos() - frameGeometry().topLeft();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_draggingWindow) {
            move(event->globalPos() - m_windowDragOffset);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_draggingWindow && event->button() == Qt::LeftButton) {
            m_draggingWindow = false;
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override
    {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QImage img(urls.first().toLocalFile());
            if (!img.isNull()) {
                m_loadedImage = img.convertToFormat(QImage::Format_ARGB32);
                m_btnImage->setChecked(true);
                setMode(ImageMode);
            }
        }
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_viewport) {
            if (event->type() == QEvent::Paint) {
                QPainter p(m_viewport);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                p.fillRect(m_viewport->rect(), palette().color(QPalette::Base));

                const QImage &src = (m_mode == CanvasMode) ? m_canvasSnapshot : m_loadedImage;
                if (src.isNull()) {
                    p.setPen(palette().color(QPalette::Text));
                    p.drawText(m_viewport->rect(), Qt::AlignCenter,
                               m_mode == ImageMode ? i18n("Click 'Open...' or drop an image here") : i18n("No active canvas"));
                } else {
                    const QRect dst = targetImageRect(src);
                    p.drawImage(dst, src);
                }

                if (m_pickingColor && m_lastPickedColor.isValid()) {
                    p.setPen(QPen(Qt::white, 2));
                    p.setBrush(m_lastPickedColor);
                    p.drawEllipse(m_lastPickPos, 14, 14);
                }
                return true;
            } else if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    m_pressPos = me->pos();
                    m_pickingColor = false;
                    m_longPressTimer->start();
                    return true;
                }
            } else if (event->type() == QEvent::MouseMove) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (m_pickingColor) {
                    pickColorAt(me->pos());
                    m_viewport->update();
                    return true;
                } else if (m_longPressTimer->isActive() && (me->pos() - m_pressPos).manhattanLength() > 10) {
                    // Dragging from viewport before long-press triggers moves the window
                    m_longPressTimer->stop();
                    m_draggingWindow = true;
                    m_windowDragOffset = me->globalPos() - frameGeometry().topLeft();
                    return true;
                }
            } else if (event->type() == QEvent::MouseButtonRelease) {
                m_longPressTimer->stop();
                m_pickingColor = false;
                m_draggingWindow = false;
                m_viewport->unsetCursor();
                m_viewport->update();
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    KoCanvasBase *m_canvas {nullptr};
    Mode m_mode {CanvasMode};
    QWidget *m_topBar {nullptr};
    QWidget *m_viewport {nullptr};
    QButtonGroup *m_modeGroup {nullptr};
    QPushButton *m_btnCanvas {nullptr};
    QPushButton *m_btnImage {nullptr};
    QPushButton *m_btnLoadImage {nullptr};
    QTimer *m_longPressTimer {nullptr};
    QTimer *m_canvasRefreshTimer {nullptr};
    QImage m_canvasSnapshot;
    QImage m_loadedImage;
    bool m_pickingColor {false};
    bool m_draggingWindow {false};
    QPoint m_pressPos;
    QPoint m_lastPickPos;
    QPoint m_windowDragOffset;
    QColor m_lastPickedColor;
};

KisToolReferenceWindow::KisToolReferenceWindow(KoCanvasBase *canvas)
    : KisTool(canvas, QCursor(Qt::ArrowCursor))
{
    setObjectName("tool_reference_window");
}

KisToolReferenceWindow::~KisToolReferenceWindow()
{
    if (m_window) {
        m_window->deleteLater();
    }
}

void KisToolReferenceWindow::activate(const QSet<KoShape*> &shapes)
{
    KisTool::activate(shapes);
    if (!m_window) {
        m_window = new KisFloatingReferenceWindow(canvas(), qApp->activeWindow());
    }
    m_window->show();
    m_window->raise();
}

void KisToolReferenceWindow::deactivate()
{
    KisTool::deactivate();
}

void KisToolReferenceWindow::paint(QPainter &painter, const KoViewConverter &converter)
{
    Q_UNUSED(painter);
    Q_UNUSED(converter);
}

KisToolReferenceWindowFactory::KisToolReferenceWindowFactory()
    : KoToolFactoryBase("KritaShape/KisToolReferenceWindow")
{
    setToolTip(i18n("Reference Window"));
    setSection(ToolBoxSection::View);
    setIconName(koIconNameCStr("view-preview"));
    setPriority(10);
    setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
}

KisToolReferenceWindowFactory::~KisToolReferenceWindowFactory()
{
}

KoToolBase *KisToolReferenceWindowFactory::createTool(KoCanvasBase *canvas)
{
    return new KisToolReferenceWindow(canvas);
}

#include "kis_tool_reference_window.moc"
