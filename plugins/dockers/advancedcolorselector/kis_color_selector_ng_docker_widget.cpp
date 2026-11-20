/*
 *  SPDX-FileCopyrightText: 2010 Adam Celarek <kdedev at xibo dot at>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_color_selector_ng_docker_widget.h"
#include "ui_wdg_color_selector_settings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLineEdit>
#include <QLabel>
#include <QLinearGradient>
#include <KoColor.h>
#include <KoColorSpaceRegistry.h>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>
#include <kis_icon_utils.h>

#include <QAction>
#include <kactioncollection.h>

#include "kis_canvas2.h"
#include "KisViewManager.h"
#include "kis_node_manager.h"
#include "kis_canvas_resource_provider.h"
#include "kis_color_space_selector.h"
#include "kis_config_notifier.h"
#include "kis_preference_set_registry.h"
#include "kis_node.h"
#include "kis_paint_device.h"
#include "KisMainWindow.h"

#include "kis_color_history.h"
#include "kis_common_colors.h"
#include "kis_color_selector_settings.h"
#include "kis_color_selector_container.h"

#include "kis_action_registry.h"

namespace {
class ModernFigmaColorSelector : public QWidget
{
public:
    explicit ModernFigmaColorSelector(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(160, 190);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(6, 6, 6, 6);
        root->setSpacing(8);

        m_svBox = new QWidget(this);
        m_svBox->setMinimumHeight(120);
        m_svBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_svBox->installEventFilter(this);

        m_hueBar = new QWidget(this);
        m_hueBar->setFixedHeight(18);
        m_hueBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_hueBar->installEventFilter(this);

        QHBoxLayout *hexRow = new QHBoxLayout();
        hexRow->setContentsMargins(0, 0, 0, 0);
        hexRow->setSpacing(6);

        m_swatch = new QLabel(this);
        m_swatch->setFixedSize(22, 22);

        QLabel *hexLbl = new QLabel(QStringLiteral("Hex"), this);
        m_hexEdit = new QLineEdit(this);
        m_hexEdit->setMaxLength(7);
        m_hexEdit->setFixedHeight(24);

        hexRow->addWidget(m_swatch);
        hexRow->addWidget(hexLbl);
        hexRow->addWidget(m_hexEdit, 1);

        root->addWidget(m_svBox, 1);
        root->addWidget(m_hueBar);
        root->addLayout(hexRow);

        connect(m_hexEdit, &QLineEdit::editingFinished, this, [this]() {
            QString txt = m_hexEdit->text().trimmed();
            if (!txt.startsWith('#')) {
                txt.prepend('#');
            }
            QColor c(txt);
            if (c.isValid()) {
                setColorFromExternal(c);
                emitColorChanged();
            }
        });

        setColorFromExternal(QColor(255, 0, 0));
    }

    void setCanvas(KisCanvas2 *canvas)
    {
        m_canvas = canvas;
        if (m_canvas && m_canvas->resourceManager()) {
            setColorFromExternal(m_canvas->resourceManager()->foregroundColor().toQColor());
        }
    }

    void setColorFromExternal(const QColor &c)
    {
        if (!c.isValid() || m_internalUpdate) {
            return;
        }
        float h, s, v;
        c.getHsvF(&h, &s, &v);
        if (h >= 0.0f) {
            m_hue = qBound(0.0f, h, 1.0f);
        }
        m_sat = qBound(0.0f, s, 1.0f);
        m_val = qBound(0.0f, v, 1.0f);
        updateDisplays();
    }

protected:
    QColor currentColor() const
    {
        return QColor::fromHsvF(m_hue, m_sat, m_val);
    }

    void updateDisplays()
    {
        const QColor c = currentColor();
        m_swatch->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 5px; border: 1px solid palette(mid);").arg(c.name()));
        if (!m_hexEdit->hasFocus()) {
            m_hexEdit->setText(c.name().mid(1).toUpper());
        }
        m_svBox->update();
        m_hueBar->update();
    }

    void emitColorChanged()
    {
        if (!m_canvas || !m_canvas->resourceManager()) {
            return;
        }
        m_internalUpdate = true;
        const KoColorSpace *cs = m_canvas->image()
            ? m_canvas->image()->colorSpace()
            : KoColorSpaceRegistry::instance()->rgb8();
        m_canvas->resourceManager()->setForegroundColor(KoColor(currentColor(), cs));
        m_internalUpdate = false;
    }

    void pickSV(const QPoint &pos)
    {
        const qreal w = qMax(1, m_svBox->width() - 1);
        const qreal h = qMax(1, m_svBox->height() - 1);
        m_sat = qBound(0.0f, float(pos.x() / w), 1.0f);
        m_val = qBound(0.0f, float(1.0 - (pos.y() / h)), 1.0f);
        updateDisplays();
        emitColorChanged();
    }

    void pickHue(const QPoint &pos)
    {
        const qreal pad = 8.0;
        const qreal w = qMax(1.0, m_hueBar->width() - 2.0 * pad);
        m_hue = qBound(0.0f, float((pos.x() - pad) / w), 1.0f);
        updateDisplays();
        emitColorChanged();
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_svBox) {
            if (event->type() == QEvent::Paint) {
                QPainter p(m_svBox);
                p.setRenderHint(QPainter::Antialiasing, true);
                QPainterPath clip;
                clip.addRoundedRect(m_svBox->rect(), 8, 8);
                p.setClipPath(clip);

                QColor pureHue = QColor::fromHsvF(m_hue, 1.0f, 1.0f);
                QLinearGradient satGrad(0, 0, m_svBox->width(), 0);
                satGrad.setColorAt(0.0, Qt::white);
                satGrad.setColorAt(1.0, pureHue);
                p.fillRect(m_svBox->rect(), satGrad);

                QLinearGradient valGrad(0, 0, 0, m_svBox->height());
                valGrad.setColorAt(0.0, QColor(0, 0, 0, 0));
                valGrad.setColorAt(1.0, QColor(0, 0, 0, 255));
                p.fillRect(m_svBox->rect(), valGrad);

                p.setClipping(false);
                const qreal cx = m_sat * m_svBox->width();
                const qreal cy = (1.0 - m_val) * m_svBox->height();
                p.setPen(QPen(Qt::white, 2.5));
                p.setBrush(currentColor());
                p.drawEllipse(QPointF(cx, cy), 7, 7);
                return true;
            } else if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    m_draggingSV = true;
                    pickSV(me->pos());
                    return true;
                }
            } else if (event->type() == QEvent::MouseMove && m_draggingSV) {
                pickSV(static_cast<QMouseEvent*>(event)->pos());
                return true;
            } else if (event->type() == QEvent::MouseButtonRelease) {
                m_draggingSV = false;
                return true;
            }
        } else if (watched == m_hueBar) {
            if (event->type() == QEvent::Paint) {
                QPainter p(m_hueBar);
                p.setRenderHint(QPainter::Antialiasing, true);
                QRectF barRect = m_hueBar->rect().adjusted(2, 2, -2, -2);
                QLinearGradient hueGrad(barRect.left(), 0, barRect.right(), 0);
                hueGrad.setColorAt(0.0 / 6.0, QColor(255, 0, 0));
                hueGrad.setColorAt(1.0 / 6.0, QColor(255, 255, 0));
                hueGrad.setColorAt(2.0 / 6.0, QColor(0, 255, 0));
                hueGrad.setColorAt(3.0 / 6.0, QColor(0, 255, 255));
                hueGrad.setColorAt(4.0 / 6.0, QColor(0, 0, 255));
                hueGrad.setColorAt(5.0 / 6.0, QColor(255, 0, 255));
                hueGrad.setColorAt(6.0 / 6.0, QColor(255, 0, 0));
                p.setPen(Qt::NoPen);
                p.setBrush(hueGrad);
                p.drawRoundedRect(barRect, 7, 7);

                const qreal pad = 8.0;
                const qreal tx = pad + m_hue * qMax(1.0, m_hueBar->width() - 2.0 * pad);
                const qreal ty = m_hueBar->height() * 0.5;
                p.setPen(QPen(Qt::white, 2.5));
                p.setBrush(QColor::fromHsvF(m_hue, 1.0f, 1.0f));
                p.drawEllipse(QPointF(tx, ty), 7, 7);
                return true;
            } else if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    m_draggingHue = true;
                    pickHue(me->pos());
                    return true;
                }
            } else if (event->type() == QEvent::MouseMove && m_draggingHue) {
                pickHue(static_cast<QMouseEvent*>(event)->pos());
                return true;
            } else if (event->type() == QEvent::MouseButtonRelease) {
                m_draggingHue = false;
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    QPointer<KisCanvas2> m_canvas;
    QWidget *m_svBox {nullptr};
    QWidget *m_hueBar {nullptr};
    QLabel *m_swatch {nullptr};
    QLineEdit *m_hexEdit {nullptr};
    float m_hue {0.0f};
    float m_sat {1.0f};
    float m_val {1.0f};
    bool m_draggingSV {false};
    bool m_draggingHue {false};
    bool m_internalUpdate {false};
};
}

KisColorSelectorNgDockerWidget::KisColorSelectorNgDockerWidget(QWidget *parent) :
    QWidget(parent),
    m_colorHistoryAction(0),
    m_commonColorsAction(0),
    m_widgetLayout(0),
    m_mainLayout(0),
    m_horizontalPatchesContainer(0),
    m_sidebarLayout(0),
    m_verticalColorPatchesLayout(0),
    m_horizontalColorPatchesLayout(0),
    m_fallbackSettingsButton(new QToolButton(this)),
    m_canvas(0)
{
    setAutoFillBackground(true);

    m_colorSelectorContainer = new KisColorSelectorContainer(this);
    m_colorHistoryWidget = new KisColorHistory(this);
    m_commonColorsWidget = new KisCommonColors(this);

    //default settings
    //remember to also change the default in the ui file

    //shade selector

    // fallback settings button when the color selector is disabled
    m_fallbackSettingsButton->setIcon(KisIconUtils::loadIcon("configure"));
    m_fallbackSettingsButton->setIconSize(QSize(22,22));
    m_fallbackSettingsButton->setAutoRaise(true);
    m_fallbackSettingsButton->hide();

    //layout
    m_widgetLayout = new QHBoxLayout(this);
    m_widgetLayout->setSpacing(0);
    m_widgetLayout->setContentsMargins(0, 0, 0, 0);

    m_mainLayout = new QVBoxLayout();
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    m_horizontalPatchesContainer = new QHBoxLayout();
    m_horizontalPatchesContainer->setSpacing(0);
    m_horizontalPatchesContainer->setContentsMargins(0, 0, 0, 0);

    m_sidebarLayout = new QVBoxLayout();
    m_sidebarLayout->setSpacing(0);
    m_sidebarLayout->setContentsMargins(0, 0, 0, 0);

    m_verticalColorPatchesLayout = new QHBoxLayout();
    m_verticalColorPatchesLayout->setSpacing(0);
    m_verticalColorPatchesLayout->setContentsMargins(0, 0, 0, 0);

    m_horizontalColorPatchesLayout = new QVBoxLayout();
    m_horizontalColorPatchesLayout->setSpacing(0);
    m_horizontalColorPatchesLayout->setContentsMargins(0, 0, 0, 0);

    m_horizontalPatchesContainer->addLayout(m_horizontalColorPatchesLayout);

    m_mainLayout->addWidget(m_colorSelectorContainer);
    m_colorSelectorContainer->hide();
    ModernFigmaColorSelector *modernPicker = new ModernFigmaColorSelector(this);
    m_modernSelector = modernPicker;
    m_mainLayout->addWidget(m_modernSelector);
    m_mainLayout->addLayout(m_horizontalPatchesContainer);

    m_sidebarLayout->addLayout(m_verticalColorPatchesLayout);

    m_widgetLayout->addLayout(m_mainLayout);
    m_widgetLayout->addLayout(m_sidebarLayout);

    updateLayout();

    connect(m_colorSelectorContainer, SIGNAL(openSettings()), this, SLOT(openSettings()));

    //Q_EMIT settingsChanged() if the settings are changed in krita preferences
    KisPreferenceSetRegistry *preferenceSetRegistry = KisPreferenceSetRegistry::instance();
    KisColorSelectorSettingsFactory* factory =
            dynamic_cast<KisColorSelectorSettingsFactory*>(preferenceSetRegistry->get("KisColorSelectorSettingsFactory"));
    Q_ASSERT(factory);
    connect(&(factory->repeater), SIGNAL(settingsUpdated()), this, SIGNAL(settingsChanged()), Qt::UniqueConnection);
    connect(this,     SIGNAL(settingsChanged()), this,                     SLOT(updateLayout()), Qt::UniqueConnection);
    connect(this,     SIGNAL(settingsChanged()), m_commonColorsWidget,     SLOT(updateSettings()), Qt::UniqueConnection);
    connect(this,     SIGNAL(settingsChanged()), m_colorHistoryWidget,     SLOT(updateSettings()), Qt::UniqueConnection);
    connect(this,     SIGNAL(settingsChanged()), m_colorSelectorContainer, SIGNAL(settingsChanged()), Qt::UniqueConnection);
    connect(this,     SIGNAL(settingsChanged()), this,                     SLOT(update()), Qt::UniqueConnection);


    Q_EMIT settingsChanged();

    m_colorHistoryAction = KisActionRegistry::instance()->makeQAction("show_color_history", this);
    connect(m_colorHistoryAction, SIGNAL(triggered()), m_colorHistoryWidget, SLOT(showPopup()), Qt::UniqueConnection);

    m_commonColorsAction = KisActionRegistry::instance()->makeQAction("show_common_colors", this);
    connect(m_commonColorsAction, SIGNAL(triggered()), m_commonColorsWidget, SLOT(showPopup()), Qt::UniqueConnection);

    connect(m_fallbackSettingsButton, SIGNAL(clicked()), this, SLOT(openSettings()));
}

void KisColorSelectorNgDockerWidget::unsetCanvas()
{
    m_canvas = 0;
    m_commonColorsWidget->unsetCanvas();
    m_colorHistoryWidget->unsetCanvas();
    m_colorSelectorContainer->unsetCanvas();
}

void KisColorSelectorNgDockerWidget::setCanvas(KisCanvas2 *canvas)
{
    if (m_canvas) {
        m_canvas->disconnect(this);
        KisKActionCollection *ac = m_canvas->viewManager()->actionCollection();
        ac->takeAction(ac->action("show_color_history"));
        ac->takeAction(ac->action("show_common_colors"));
    }

    m_canvas = canvas;

    m_commonColorsWidget->setCanvas(canvas);
    m_colorHistoryWidget->setCanvas(canvas);
    m_colorSelectorContainer->setCanvas(canvas);
    if (ModernFigmaColorSelector *picker = static_cast<ModernFigmaColorSelector*>(m_modernSelector)) {
        picker->setCanvas(canvas);
        if (m_canvas && m_canvas->viewManager() && m_canvas->viewManager()->canvasResourceProvider()) {
            connect(m_canvas->viewManager()->canvasResourceProvider(), &KisCanvasResourceProvider::sigFGColorChanged,
                    picker, [picker](const KoColor &c) {
                        picker->setColorFromExternal(c.toQColor());
                    }, Qt::UniqueConnection);
        }
    }

    if (m_canvas && m_canvas->viewManager()) {
        KisKActionCollection* actionCollection = canvas->viewManager()->actionCollection();

        actionCollection->addAction("show_color_history", m_colorHistoryAction);
        actionCollection->addAction("show_common_colors", m_commonColorsAction);

        connect(m_canvas->viewManager()->mainWindow(), SIGNAL(themeChanged()), m_colorSelectorContainer, SLOT(slotUpdateIcons()), Qt::UniqueConnection);
    }

}

void KisColorSelectorNgDockerWidget::openSettings()
{
    if (!m_canvas) return;

    KisColorSelectorSettingsDialog settings;
    if(settings.exec()==QDialog::Accepted) {
        Q_EMIT settingsChanged();
        KisConfigNotifier::instance()->notifyConfigChanged();
    }
}


void KisColorSelectorNgDockerWidget::updateLayout()
{
    KConfigGroup cfg =  KSharedConfig::openConfig()->group("advancedColorSelector");

    bool showColorSelector = (bool) cfg.readEntry("showColorSelector", true);

    //color patches
    bool m_lastColorsShow = cfg.readEntry("lastUsedColorsShow", true);
    KisColorPatches::Direction m_lastColorsDirection;
    if(cfg.readEntry("lastUsedColorsAlignment", false))
        m_lastColorsDirection=KisColorPatches::Vertical;
    else
        m_lastColorsDirection=KisColorPatches::Horizontal;

    bool m_commonColorsShow = cfg.readEntry("commonColorsShow", true);
    KisColorPatches::Direction m_commonColorsDirection;
    if(cfg.readEntry("commonColorsAlignment", false))
        m_commonColorsDirection=KisColorPatches::Vertical;
    else
        m_commonColorsDirection=KisColorPatches::Horizontal;

    m_verticalColorPatchesLayout->removeWidget(m_colorHistoryWidget);
    m_verticalColorPatchesLayout->removeWidget(m_commonColorsWidget);
    m_horizontalColorPatchesLayout->removeWidget(m_colorHistoryWidget);
    m_horizontalColorPatchesLayout->removeWidget(m_commonColorsWidget);

    m_sidebarLayout->removeWidget(m_fallbackSettingsButton);
    m_mainLayout->removeWidget(m_fallbackSettingsButton);

    if (m_lastColorsShow == false) {
        m_colorHistoryWidget->hide();
    } else {
        m_colorHistoryWidget->show();
    }

    if(m_commonColorsShow==false) {
        m_commonColorsWidget->hide();
    }
    else {
        m_commonColorsWidget->show();
    }


    bool fallbackSettingsButtonVertical = true;

    if(m_lastColorsShow && m_lastColorsDirection==KisColorPatches::Vertical) {
         m_verticalColorPatchesLayout->addWidget(m_colorHistoryWidget);
    }

    if(m_commonColorsShow && m_commonColorsDirection==KisColorPatches::Vertical) {
        m_verticalColorPatchesLayout->addWidget(m_commonColorsWidget);
    }

    if(m_lastColorsShow && m_lastColorsDirection==KisColorPatches::Horizontal) {
        m_horizontalColorPatchesLayout->addWidget(m_colorHistoryWidget);
        fallbackSettingsButtonVertical = false;
    }

    if(m_commonColorsShow && m_commonColorsDirection==KisColorPatches::Horizontal) {
        m_horizontalColorPatchesLayout->addWidget(m_commonColorsWidget);
        fallbackSettingsButtonVertical = false;
    }

    // prefer the vertical column, if patch components have different layout
    if (m_commonColorsDirection != m_lastColorsDirection) {
        fallbackSettingsButtonVertical = true;
    }

    if (!showColorSelector) {
        if (fallbackSettingsButtonVertical) {
            m_sidebarLayout->addWidget(m_fallbackSettingsButton);
        } else {
            m_horizontalPatchesContainer->addWidget(m_fallbackSettingsButton);
        }

        m_fallbackSettingsButton->show();
    } else {
        m_fallbackSettingsButton->hide();
    }

    updateGeometry();
}
