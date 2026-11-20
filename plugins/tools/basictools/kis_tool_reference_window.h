/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_REFERENCE_WINDOW_H_
#define KIS_TOOL_REFERENCE_WINDOW_H_

#include <kis_tool.h>
#include <KoToolFactoryBase.h>
#include <QPointer>

class KisFloatingReferenceWindow;

class KisToolReferenceWindow : public KisTool
{
    Q_OBJECT
public:
    KisToolReferenceWindow(KoCanvasBase *canvas);
    ~KisToolReferenceWindow() override;

    void activate(const QSet<KoShape*> &shapes) override;
    void deactivate() override;
    void paint(QPainter &painter, const KoViewConverter &converter) override;

private:
    QPointer<KisFloatingReferenceWindow> m_window;
};

class KisToolReferenceWindowFactory : public KoToolFactoryBase
{
public:
    KisToolReferenceWindowFactory();
    ~KisToolReferenceWindowFactory() override;

    KoToolBase *createTool(KoCanvasBase *canvas) override;
};

#endif // KIS_TOOL_REFERENCE_WINDOW_H_
