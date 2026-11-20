/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_FIRST_RUN_WIZARD_H
#define KIS_FIRST_RUN_WIZARD_H

#include <QDialog>
#include <kritaui_export.h>

class QStackedWidget;
class QButtonGroup;
class QComboBox;
class QPushButton;

class KRITAUI_EXPORT KisFirstRunWizard : public QDialog
{
    Q_OBJECT
public:
    explicit KisFirstRunWizard(QWidget *parent = nullptr);
    ~KisFirstRunWizard() override = default;

    static bool shouldRun();
    static void markCompleted();

private Q_SLOTS:
    void slotNextOrFinish();
    void slotBack();

private:
    void applyConfiguration();

    QStackedWidget *m_stack {nullptr};
    QButtonGroup *m_deviceGroup {nullptr};
    QButtonGroup *m_styleGroup {nullptr};
    QComboBox *m_pixelSizeCombo {nullptr};
    QPushButton *m_btnBack {nullptr};
    QPushButton *m_btnNext {nullptr};
};

#endif // KIS_FIRST_RUN_WIZARD_H
