/***************************************************************************
                          kinstitutionsview.cpp
                             -------------------
    copyright            : (C) 2007 by Thomas Baumgart <ipwizard@users.sourceforge.net>
                           (C) 2017 by Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kinstitutionsview_p.h"

#include <typeinfo>

// ----------------------------------------------------------------------------
// QT Includes

#include <QTimer>
#include <QMenu>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KMessageBox>

// ----------------------------------------------------------------------------
// Project Includes

#include "kmymoneyglobalsettings.h"
#include "mymoneyexception.h"
#include "knewbankdlg.h"
#include "menuenums.h"

using namespace Icons;

KInstitutionsView::KInstitutionsView(QWidget *parent) :
    KMyMoneyAccountsViewBase(*new KInstitutionsViewPrivate(this), parent)
{
  connect(pActions[eMenu::Action::NewInstitution],    &QAction::triggered, this, &KInstitutionsView::slotNewInstitution);
  connect(pActions[eMenu::Action::EditInstitution],   &QAction::triggered, this, &KInstitutionsView::slotEditInstitution);
  connect(pActions[eMenu::Action::DeleteInstitution], &QAction::triggered, this, &KInstitutionsView::slotDeleteInstitution);
}

KInstitutionsView::~KInstitutionsView()
{
}

void KInstitutionsView::setDefaultFocus()
{
  Q_D(KInstitutionsView);
  QTimer::singleShot(0, d->ui->m_accountTree, SLOT(setFocus()));
}

void KInstitutionsView::refresh()
{
  Q_D(KInstitutionsView);
  if (!isVisible()) {
    d->m_needsRefresh = true;
    return;
  }
  d->m_needsRefresh = false;

  d->m_proxyModel->invalidate();
  d->m_proxyModel->setHideEquityAccounts(!KMyMoneyGlobalSettings::expertMode());
  d->m_proxyModel->setHideClosedAccounts(KMyMoneyGlobalSettings::hideClosedAccounts());
}

void KInstitutionsView::showEvent(QShowEvent * event)
{
  Q_D(KInstitutionsView);
  if (!d->m_proxyModel)
    d->init();

  emit aboutToShow(View::Institutions);

  if (d->m_needsRefresh)
    refresh();

  // don't forget base class implementation
  QWidget::showEvent(event);
}

void KInstitutionsView::updateActions(const MyMoneyObject& obj)
{
  Q_D(KInstitutionsView);
  if (typeid(obj) != typeid(MyMoneyInstitution) ||
      (obj.id().isEmpty() && d->m_currentInstitution.id().isEmpty())) // do not disable actions that were already disabled
    return;

  const auto& inst = static_cast<const MyMoneyInstitution&>(obj);

  pActions[eMenu::Action::NewInstitution]->setEnabled(true);
  auto b = inst.id().isEmpty() ? false : true;
  pActions[eMenu::Action::EditInstitution]->setEnabled(b);
  pActions[eMenu::Action::DeleteInstitution]->setEnabled(b && !MyMoneyFile::instance()->isReferenced(inst));
  d->m_currentInstitution = inst;
}

void KInstitutionsView::slotNetWorthChanged(const MyMoneyMoney &netWorth)
{
  Q_D(KInstitutionsView);
  d->netBalProChanged(netWorth, d->ui->m_totalProfitsLabel, View::Institutions);
}

void KInstitutionsView::slotNewInstitution()
{
  Q_D(KInstitutionsView);
  d->m_currentInstitution.clearId();
  QPointer<KNewBankDlg> dlg = new KNewBankDlg(d->m_currentInstitution);
  if (dlg->exec() == QDialog::Accepted && dlg != 0) {
    d->m_currentInstitution = dlg->institution();

    const auto file = MyMoneyFile::instance();
    MyMoneyFileTransaction ft;

    try {
      file->addInstitution(d->m_currentInstitution);
      ft.commit();

    } catch (const MyMoneyException &e) {
      KMessageBox::information(this, i18n("Cannot add institution: %1", e.what()));
    }
  }
  delete dlg;
}

void KInstitutionsView::slotShowInstitutionsMenu(const MyMoneyInstitution& inst)
{
  Q_UNUSED(inst);
  pMenus[eMenu::Menu::Institution]->exec(QCursor::pos());
}

void KInstitutionsView::slotEditInstitution()
{
  Q_D(KInstitutionsView);

  // make sure the selected object has an id
  if (d->m_currentInstitution.id().isEmpty())
    return;

  try {
    const auto file = MyMoneyFile::instance();

    //grab a pointer to the view, regardless of it being a account or institution view.
    auto institution = file->institution(d->m_currentInstitution.id());

    // bankSuccess is not checked anymore because d->m_file->institution will throw anyway
    QPointer<KNewBankDlg> dlg = new KNewBankDlg(institution);
    if (dlg->exec() == QDialog::Accepted && dlg != 0) {
      MyMoneyFileTransaction ft;
      try {
        file->modifyInstitution(dlg->institution());
        ft.commit();
        emit objectSelected(dlg->institution());
      } catch (const MyMoneyException &e) {
        KMessageBox::information(this, i18n("Unable to store institution: %1", e.what()));
      }
    }
    delete dlg;

  } catch (const MyMoneyException &e) {
    KMessageBox::information(this, i18n("Unable to edit institution: %1", e.what()));
  }
}

void KInstitutionsView::slotDeleteInstitution()
{
  Q_D(KInstitutionsView);
  const auto file = MyMoneyFile::instance();
  try {
    auto institution = file->institution(d->m_currentInstitution.id());
    if ((KMessageBox::questionYesNo(this, i18n("<p>Do you really want to delete the institution <b>%1</b>?</p>", institution.name()))) == KMessageBox::No)
      return;
    MyMoneyFileTransaction ft;

    try {
      file->removeInstitution(institution);
      emit objectSelected(MyMoneyInstitution());
      ft.commit();
    } catch (const MyMoneyException &e) {
      KMessageBox::information(this, i18n("Unable to delete institution: %1", e.what()));
    }
  } catch (const MyMoneyException &e) {
    KMessageBox::information(this, i18n("Unable to delete institution: %1", e.what()));
  }
}
