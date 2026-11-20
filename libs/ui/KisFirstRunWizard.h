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
class QLabel;

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
    int nextPageIndex(int current) const;
    int prevPageIndex(int current) const;

    QStackedWidget *m_stack {nullptr};
    QButtonGroup *m_deviceGroup {nullptr};
    QButtonGroup *m_styleGroup {nullptr};
    QButtonGroup *m_sidebarGroup {nullptr};
    QButtonGroup *m_topBarGroup {nullptr};
    QComboBox *m_pixelSizeCombo {nullptr};
    QComboBox *m_penBtnCountCombo {nullptr};
    QComboBox *m_penBtn1Action {nullptr};
    QComboBox *m_penBtn2Action {nullptr};
    QComboBox *m_penBtn3Action {nullptr};
    QLabel *m_breezeNoteLabel {nullptr};
    QPushButton *m_btnBack {nullptr};
    QPushButton *m_btnNext {nullptr};
};

#endif // KIS_FIRST_RUN_WIZARD_H
