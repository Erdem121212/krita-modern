/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_EFFECTS_MENU_DIALOG_H
#define KIS_EFFECTS_MENU_DIALOG_H

#include <QDialog>
#include <kritaui_export.h>

class KisMainWindow;

class KRITAUI_EXPORT KisEffectsMenuDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KisEffectsMenuDialog(KisMainWindow *mainWindow, QWidget *parent = nullptr);
    ~KisEffectsMenuDialog() override = default;
};

#endif // KIS_EFFECTS_MENU_DIALOG_H
