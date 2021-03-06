/***************************************************************************
                          kcurrencyeditdlg.h  -  description
                             -------------------
    begin                : Wed Mar 24 2004
    copyright            : (C) 2000-2004 by Michael Edwardes
    email                : mte@users.sourceforge.net
                           Javier Campos Morales <javi_c@users.sourceforge.net>
                           Felix Rodriguez <frodriguez@users.sourceforge.net>
                           John C <thetacoturtle@users.sourceforge.net>
                           Thomas Baumgart <ipwizard@users.sourceforge.net>
                           Kevin Tambascio <ktambascio@users.sourceforge.net>
                           Alvaro Soliverez <asoliverez@gmail.com>
                           (C) 2017 Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KCURRENCYEDITDLG_H
#define KCURRENCYEDITDLG_H

// ----------------------------------------------------------------------------
// QT Includes

// ----------------------------------------------------------------------------
// KDE Includes

#include <QDialog>

// ----------------------------------------------------------------------------
// Project Includes

class QTreeWidgetItem;
class KAvailableCurrencyDlg;
class KCurrencyEditorDlg;
class KTreeWidgetSearchLineWidget;

class MyMoneySecurity;
/**
  * @author Thomas Baumgart
  */
class KCurrencyEditDlgPrivate;
class KCurrencyEditDlg : public QDialog
{
  Q_OBJECT
  Q_DISABLE_COPY(KCurrencyEditDlg)

public:
  explicit KCurrencyEditDlg(QWidget *parent = nullptr);
  ~KCurrencyEditDlg();

public Q_SLOTS:
  void slotSelectCurrency(const QString& id);

protected Q_SLOTS:
  void slotSelectCurrency(QTreeWidgetItem *citem, QTreeWidgetItem *pitem);
  void slotSelectCurrency(QTreeWidgetItem *item);
  void slotItemSelectionChanged();
  void slotShowCurrencyMenu(const QPoint& p);
  void slotLoadCurrencies();
  void slotUpdateCurrency(QTreeWidgetItem* citem, int column);
  void slotUpdateCurrency(QTreeWidgetItem *citem, QTreeWidgetItem *pitem);

private:
  KCurrencyEditDlgPrivate * const d_ptr;
  Q_DECLARE_PRIVATE(KCurrencyEditDlg)

private Q_SLOTS:
  void timerDone();
  void slotSelectBaseCurrency();
  void slotAddCurrency();
  void slotRemoveCurrency();
  void slotRemoveUnusedCurrency();
  void slotEditCurrency();

  void slotNewCurrency();
  void slotRenameCurrency();
  void slotDeleteCurrency();
  void slotSetBaseCurrency();
};

#endif
