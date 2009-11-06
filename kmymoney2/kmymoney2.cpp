/***************************************************************************
                          kmymoney2.cpp
                             -------------------
    copyright            : (C) 2000 by Michael Edwardes <mte@users.sourceforge.net>
                           (C) 2007 by Thomas Baumgart <ipwizard@users.sourceforge.net>

****************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <config-kmymoney.h>



// ----------------------------------------------------------------------------
// Std C++ / STL Includes

#include <typeinfo>
#include <cstdio>
#include <iostream>

// ----------------------------------------------------------------------------
// QT Includes

#include <QDir>
#include <QPrinter>
#include <QLayout>
#include <QSignalMapper>
#include <QClipboard>        // temp for problem 1105503
#include <QDateTime>         // only for performance tests
#include <QTimer>
#include <q3sqlpropertymap.h>
#include <q3vbox.h>
#include <QEventLoop>
#include <QByteArray>
#include <QBoxLayout>
#include <Q3Frame>
#include <QResizeEvent>
#include <QLabel>
#include <KMenu>
#include <QProgressBar>
#include <QList>

// ----------------------------------------------------------------------------
// KDE Includes

#include <kdebug.h>
#include <kapplication.h>
#include <kshortcut.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kmenubar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstandardaction.h>
#include <ktoggleaction.h>
#include <kactioncollection.h>
#include <kactionmenu.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kstatusbar.h>
#include <ktip.h>
#include <kprogressdialog.h>
#include <kio/netaccess.h>
#include <kstartupinfo.h>
#include <kparts/componentfactory.h>
#include <krun.h>
#include <kconfigdialog.h>
#include <kinputdialog.h>
#include <kxmlguifactory.h>
#include <krecentfilesaction.h>

// ----------------------------------------------------------------------------
// Project Includes

#include "kmymoney2.h"
#include "kmymoneyglobalsettings.h"
#include "kmymoneyadaptor.h"

#include "dialogs/kstartdlg.h"
#include "dialogs/settings/ksettingsgeneral.h"
#include "dialogs/settings/ksettingsregister.h"
#include "dialogs/settings/ksettingsgpg.h"
#include "dialogs/settings/ksettingscolors.h"
#include "dialogs/settings/ksettingsfonts.h"
#include "dialogs/settings/ksettingsschedules.h"
#include "dialogs/settings/ksettingsonlinequotes.h"
#include "dialogs/settings/ksettingshome.h"
#include "dialogs/settings/ksettingsforecast.h"
#include "dialogs/settings/ksettingsplugins.h"
#include "dialogs/kbackupdlg.h"
#include "dialogs/kexportdlg.h"
#include "dialogs/kimportdlg.h"
#include "dialogs/mymoneyqifprofileeditor.h"
#include "dialogs/kenterscheduledlg.h"
#include "dialogs/kconfirmmanualenterdlg.h"
#include "dialogs/kmymoneypricedlg.h"
#include "dialogs/kcurrencyeditdlg.h"
#include "dialogs/kequitypriceupdatedlg.h"
#include "dialogs/ksecuritylisteditor.h"
#include "dialogs/kmymoneyfileinfodlg.h"
#include "dialogs/kfindtransactiondlg.h"
#include "dialogs/knewbankdlg.h"
#include "dialogs/knewinvestmentwizard.h"
#include "dialogs/knewaccountdlg.h"
#include "dialogs/knewfiledlg.h"
#include "dialogs/kselectdatabasedlg.h"
#include "dialogs/kcurrencycalculator.h"
#include "dialogs/keditscheduledlg.h"
#include "dialogs/knewloanwizard.h"
#include "dialogs/keditloanwizard.h"
#include "dialogs/kpayeereassigndlg.h"
#include "dialogs/kcategoryreassigndlg.h"
#include "dialogs/kmergetransactionsdlg.h"
#include "dialogs/kendingbalancedlg.h"
#include "dialogs/kbalancechartdlg.h"
#include "ui_kplugindlg.h"
#include "dialogs/kloadtemplatedlg.h"
#include "dialogs/kgpgkeyselectiondlg.h"
#include "dialogs/transactionmatcher.h"
#include "wizards/newuserwizard/knewuserwizard.h"
#include "wizards/newaccountwizard/knewaccountwizard.h"
#include "dialogs/kbalancewarning.h"

#include "widgets/kmymoneycombo.h"
#include "widgets/kmymoneycompletion.h"

#include "views/kmymoneyview.h"

#include "mymoney/mymoneyutils.h"
#include "mymoney/mymoneystatement.h"
#include "mymoney/storage/mymoneystoragedump.h"
#include "mymoney/mymoneyforecast.h"

#include "converter/mymoneyqifwriter.h"
#include "converter/mymoneyqifreader.h"
#include "converter/mymoneystatementreader.h"
#include "converter/mymoneytemplate.h"

#include "plugins/interfaces/kmmviewinterface.h"
#include "plugins/interfaces/kmmstatementinterface.h"
#include "plugins/interfaces/kmmimportinterface.h"
#include "plugins/pluginloader.h"

#include <libkgpgfile/kgpgfile.h>

#include <transactioneditor.h>
#include <kmymoneylistviewitem.h>
#include <ktoolinvocation.h>

#include "kmymoneyutils.h"


#define RECOVER_KEY_ID        "59B0F826D2B08440"
#define ID_STATUS_MSG 1

enum backupStateE {
  BACKUP_IDLE = 0,
  BACKUP_MOUNTING,
  BACKUP_COPYING,
  BACKUP_UNMOUNTING
};

class KMyMoney2App::Private
{
public:
  Private() :
    m_ft(0),
    m_moveToAccountSelector(0),
    m_statementXMLindex(0),
    m_collectingStatements(false),
    m_myMoneyView(0),
    m_searchDlg(0),
    m_autoSaveTimer(0),
    m_inAutoSaving(false),
    m_transactionEditor(0),
    m_endingBalanceDlg(0),
    m_saveEncrypted(0)
  {}
  void unlinkStatementXML(void);
  void moveInvestmentTransaction(const QString& fromId,
                                 const QString& toId,
                                 const MyMoneyTransaction& t);

  MyMoneyFileTransaction*       m_ft;
  kMyMoneyAccountSelector*      m_moveToAccountSelector;
  int                           m_statementXMLindex;
  KBalanceWarning*              m_balanceWarning;

  bool                          m_collectingStatements;
  QStringList                   m_statementResults;
  KMyMoneyPlugin::PluginLoader* m_pluginLoader;
  QString                       m_lastPayeeEntered;

  /** the configuration object of the application */
  KSharedConfigPtr m_config;

  QMap<QString,KMyMoneyPlugin::ImporterPlugin*> m_importerPlugins;

  QMap<QString, KMyMoneyPlugin::OnlinePlugin*> m_onlinePlugins;

  /**
    * The following variable represents the state while crafting a backup.
    * It can have the following values
    *
    * - IDLE: the default value if not performing a backup
    * - MOUNTING: when a mount command has been issued
    * - COPYING:  when a copy command has been issued
    * - UNMOUNTING: when an unmount command has been issued
    */
  backupStateE   m_backupState;

  /**
    * This variable keeps the result of the backup operation.
    */
  int     m_backupResult;

  /**
    * This variable is set, when the user selected to mount/unmount
    * the backup volume.
    */
  bool    m_backupMount;

  KProcess m_proc;

  /// A pointer to the view holding the tabs.
  KMyMoneyView *m_myMoneyView;

  /// The URL of the file currently being edited when open.
  KUrl  m_fileName;

  bool m_startDialog;
  QString m_mountpoint;

  QProgressBar* m_progressBar;

  QString m_statusMsg;

  int m_progressUpdate;
  int m_nextUpdate;

  MyMoneyQifReader* m_qifReader;
  MyMoneyStatementReader* m_smtReader;
  KFindTransactionDlg* m_searchDlg;

  bool m_bCheckSchedules;
  QObject*              m_pluginInterface;

  MyMoneyAccount        m_selectedAccount;
  MyMoneyAccount        m_reconciliationAccount;
  MyMoneyAccount        m_selectedInvestment;
  MyMoneyInstitution    m_selectedInstitution;
  MyMoneySchedule       m_selectedSchedule;
  MyMoneySecurity       m_selectedCurrency;
  QList<MyMoneyPayee>   m_selectedPayees;
  QList<MyMoneyBudget>  m_selectedBudgets;
  KMyMoneyRegister::SelectedTransactions m_selectedTransactions;

  // This is Auto Saving related
  bool                  m_autoSaveEnabled;
  QTimer*               m_autoSaveTimer;
  int                   m_autoSavePeriod;
  bool                  m_inAutoSaving;

  // pointer to the current transaction editor
  TransactionEditor*    m_transactionEditor;

  // Reconciliation dialog
  KEndingBalanceDlg*    m_endingBalanceDlg;

  // Pointer to the combo box used for key selection during
  // File/Save as
  KComboBox*            m_saveEncrypted;

  // id's that need to be remembered
  QString               m_accountGoto, m_payeeGoto;

  QStringList           m_additionalGpgKeys;
  QLabel*               m_additionalKeyLabel;
  KPushButton*          m_additionalKeyButton;

  KRecentFilesAction*   m_recentFiles;
};

KMyMoney2App::KMyMoney2App(QWidget* parent) :
  KXmlGuiWindow(0),
  d(new Private)
{
  new KmymoneyAdaptor(this);
  QDBusConnection::sessionBus().registerObject("/KMymoney", this);
  QDBusConnection::sessionBus().interface()->registerService(
      "org.kde.kmymoney", QDBusConnectionInterface::DontQueueService );

  ::timetrace("start kmymoney2app constructor");
  // preset the pointer because we need it during the course of this constructor
  kmymoney2 = this;
  d->m_config = KGlobal::config();

  MyMoneyTransactionFilter::setFiscalYearStart(KMyMoneyGlobalSettings::firstFiscalMonth(), KMyMoneyGlobalSettings::firstFiscalDay());

  updateCaption(true);

  QFrame* frame = new QFrame(this);
  frame->setFrameStyle(QFrame::NoFrame);
  // values for margin (11) and spacing(6) taken from KDialog implementation
  QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom, frame);
  layout->setContentsMargins(2,2,2,2);
  layout->setSpacing(6);

  ::timetrace("init statusbar");
  initStatusBar();
  ::timetrace("init actions");
  initActions();

  initDynamicMenus();

  ::timetrace("create view");
  d->m_myMoneyView = new KMyMoneyView(frame, "KMyMoneyView");
  layout->addWidget(d->m_myMoneyView, 10);
  //FIXME: Port to KDE4
  //connect(myMoneyView, SIGNAL(aboutToShowPage(QWidget*)), this, SLOT(slotResetSelections()));

  ///////////////////////////////////////////////////////////////////
  // call inits to invoke all other construction parts
  ::timetrace("init options");
  readOptions();

#if 0
  m_pluginSignalMapper = new QSignalMapper( this );
  connect( m_pluginSignalMapper, SIGNAL( mapped( const QString& ) ), this, SLOT( slotPluginImport( const QString& ) ) );
#endif

  // now initialize the plugin structure
  ::timetrace("load plugins");
  createInterfaces();
  loadPlugins();

  setCentralWidget(frame);

  ::timetrace("done");

  connect(&d->m_proc, SIGNAL(finished (int, QProcess::ExitStatus)), this, SLOT(slotProcessExited()));

  // force to show the home page if the file is closed
  connect(action("view_show_transaction_detail"), SIGNAL(toggled(bool)), d->m_myMoneyView, SLOT(slotShowTransactionDetail(bool)));

  d->m_backupState = BACKUP_IDLE;

  d->m_qifReader = 0;
  d->m_smtReader = 0;

  d->m_autoSaveEnabled = KMyMoneyGlobalSettings::autoSaveFile();
  d->m_autoSavePeriod = KMyMoneyGlobalSettings::autoSavePeriod();

  d->m_autoSaveTimer = new QTimer(this);
  connect(d->m_autoSaveTimer, SIGNAL(timeout()), this, SLOT(slotAutoSave()));

  // make sure, we get a note when the engine changes state
  connect(MyMoneyFile::instance(), SIGNAL(dataChanged()), this, SLOT(slotDataChanged()));

  // make sure we have a balance warning object
  d->m_balanceWarning = new KBalanceWarning(this);

  // kickstart date change timer
  slotDateChanged();
}

KMyMoney2App::~KMyMoney2App()
{
  delete d->m_searchDlg;
  delete d->m_qifReader;
  delete d->m_transactionEditor;
  delete d->m_endingBalanceDlg;
  delete d->m_moveToAccountSelector;
  delete d->m_myMoneyView;
  delete d;
}

const KUrl KMyMoney2App::lastOpenedURL(void)
{
  KUrl url = d->m_startDialog ? KUrl() : d->m_fileName;

  if(!url.isValid())
  {
    url = readLastUsedFile();
  }

  ready();

  return url;
}

void KMyMoney2App::initDynamicMenus(void)
{
  QWidget* w = factory()->container("transaction_move_menu", this);
  KMenu *menu = dynamic_cast<KMenu*>(w);
  if(menu) {
    QWidgetAction *accountSelectorAction = new QWidgetAction(menu);
    d->m_moveToAccountSelector = new kMyMoneyAccountSelector(menu, 0, 0, false);
    accountSelectorAction->setDefaultWidget(d->m_moveToAccountSelector);
    menu->addAction(accountSelectorAction);
    connect(d->m_moveToAccountSelector, SIGNAL(itemSelected(const QString&)), this, SLOT(slotMoveToAccount(const QString&)));
    connect(this, SIGNAL(accountSelected(const MyMoneyAccount&)), this, SLOT(slotUpdateMoveToAccountMenu()));
    connect(this, SIGNAL(transactionsSelected(const KMyMoneyRegister::SelectedTransactions&)), this, SLOT(slotUpdateMoveToAccountMenu()));
    connect(MyMoneyFile::instance(), SIGNAL(dataChanged()), this, SLOT(slotUpdateMoveToAccountMenu()));
  }
}

void KMyMoney2App::initActions(void)
{
  // *************
  // The File menu
  // *************

  actionCollection()->addAction( KStandardAction::New, this, SLOT(slotFileNew()) );
  actionCollection()->addAction( KStandardAction::Open, this, SLOT(slotFileOpen()) );
  d->m_recentFiles = KStandardAction::openRecent(this, SLOT(slotFileOpenRecent(const KUrl&)), actionCollection());
  actionCollection()->addAction( KStandardAction::Save, this, SLOT(slotFileSave()) );
  actionCollection()->addAction( KStandardAction::SaveAs, this, SLOT(slotFileSaveAs()) );
  actionCollection()->addAction( KStandardAction::Close, this, SLOT(slotFileClose()) );
  actionCollection()->addAction( KStandardAction::Quit, this, SLOT(slotFileQuit()) );
  actionCollection()->addAction( KStandardAction::Print, this, SLOT(slotPrintView()) );

  KAction *open_database = actionCollection()->addAction("open_database");
  open_database->setText(i18n("Open database..."));
  connect(open_database, SIGNAL(triggered()), this, SLOT(slotOpenDatabase()));

  KAction *saveas_database = actionCollection()->addAction("saveas_database");
  saveas_database->setText(i18n("Save as database..."));
  connect(saveas_database, SIGNAL(triggered()), this, SLOT(slotSaveAsDatabase()));

  KAction *file_backup = actionCollection()->addAction("file_backup");
  file_backup->setText(i18n("Backup..."));
  file_backup->setIcon(KIcon("backup"));
  connect(file_backup, SIGNAL(triggered()), this, SLOT(slotFileBackup()));

  KAction *file_import_qif = actionCollection()->addAction("file_import_qif");
  file_import_qif->setText(i18n("QIF..."));
  connect(file_import_qif, SIGNAL(triggered()), this, SLOT(slotQifImport()));

  KAction *file_import_gnc = actionCollection()->addAction("file_import_gnc");
  file_import_gnc->setText(i18n("Gnucash..."));
  connect(file_import_gnc, SIGNAL(triggered()), this, SLOT(slotGncImport()));

  KAction *file_import_statement = actionCollection()->addAction("file_import_statement");
  file_import_statement->setText(i18n("Statement file..."));
  connect(file_import_statement, SIGNAL(triggered()), this, SLOT(slotStatementImport()));

  KAction *file_import_template = actionCollection()->addAction("file_import_template");
  file_import_template->setText(i18n("Account Template..."));
  connect(file_import_template, SIGNAL(triggered()), this, SLOT(slotLoadAccountTemplates()));

  KAction *file_export_template = actionCollection()->addAction("file_export_template");
  file_export_template->setText(i18n("Account Template..."));
  connect(file_export_template, SIGNAL(triggered()), this, SLOT(slotSaveAccountTemplates()));

  KAction *file_export_qif = actionCollection()->addAction("file_export_qif");
  file_export_qif->setText(i18n("QIF..."));
  connect(file_export_qif, SIGNAL(triggered()), this, SLOT(slotQifExport()));

  KAction *view_personal_data = actionCollection()->addAction("view_personal_data");
  view_personal_data->setText(i18n("Personal Data..."));
  view_personal_data->setIcon(KIcon("user-properties"));
  connect(view_personal_data, SIGNAL(triggered()), this, SLOT(slotFileViewPersonal()));

#if KMM_DEBUG
  KAction *file_dump = actionCollection()->addAction("file_dump");
  file_dump->setText(i18n("Dump Memory"));
  connect(file_dump, SIGNAL(triggered()), this, SLOT(slotFileFileInfo()));
#endif

  KAction *view_file_info = actionCollection()->addAction("view_file_info");
  view_file_info->setText(i18n("File-Information..."));
  view_file_info->setIcon(KIcon("document-properties"));
  connect(view_file_info, SIGNAL(triggered()), this, SLOT(slotFileInfoDialog()));

  // *************
  // The Edit menu
  // *************
  KAction *edit_find_transaction = actionCollection()->addAction("edit_find_transaction");
  edit_find_transaction->setText(i18n("Find transaction..."));
  edit_find_transaction->setIcon(KIcon("transaction_find"));
  edit_find_transaction->setShortcut(KShortcut("Ctrl+F"));
  connect(edit_find_transaction, SIGNAL(triggered()), this, SLOT(slotFindTransaction()));

  // *************
  // The View menu
  // *************
  KToggleAction *view_show_transaction_detail = new KToggleAction(this);
  actionCollection()->addAction("view_show_transaction_detail", view_show_transaction_detail);
  view_show_transaction_detail->setIcon(KIcon("zoom-in"));
  view_show_transaction_detail->setText(i18n("Show Transaction Detail"));
  view_show_transaction_detail->setShortcut(KShortcut("Ctrl+T"));

  KToggleAction *view_hide_reconciled_transactions = new KToggleAction(this);
  actionCollection()->addAction("view_hide_reconciled_transactions", view_hide_reconciled_transactions);
  view_hide_reconciled_transactions->setText(i18n("Hide reconciled transactions"));
  view_hide_reconciled_transactions->setIcon(KIcon("hide_reconciled"));
  view_hide_reconciled_transactions->setShortcut(KShortcut("Ctrl+R"));
  connect(view_hide_reconciled_transactions, SIGNAL(triggered()), this, SLOT(slotHideReconciledTransactions()));

  KToggleAction *view_hide_unused_categories = new KToggleAction(this);
  actionCollection()->addAction("view_hide_unused_categories", view_hide_unused_categories);
  view_hide_unused_categories->setText(i18n("Hide unused categories"));
  view_hide_unused_categories->setIcon(KIcon("hide_categories"));
  view_hide_unused_categories->setShortcut(KShortcut("Ctrl+U"));
  connect(view_hide_unused_categories, SIGNAL(triggered()), this, SLOT(slotHideUnusedCategories()));

  KToggleAction *view_show_all_accounts = new KToggleAction(this);
  actionCollection()->addAction("view_show_all_accounts", view_show_all_accounts);
  view_show_all_accounts->setText(i18n("Show all accounts"));
  view_show_all_accounts->setShortcut(KShortcut("Ctrl+Shift+A"));
  connect(view_show_all_accounts, SIGNAL(triggered()), this, SLOT(slotShowAllAccounts()));

  // *********************
  // The institutions menu
  // *********************
  KAction *institution_new = actionCollection()->addAction("institution_new");
  institution_new->setText(i18n("New institution..."));
  institution_new->setIcon(KIcon("institution_add"));
  connect(institution_new, SIGNAL(triggered()), this, SLOT(slotInstitutionNew()));

  KAction *institution_edit = actionCollection()->addAction("institution_edit");
  institution_edit->setText(i18n("Edit institution..."));
  institution_edit->setIcon(KIcon("document-properties"));
  connect(institution_edit, SIGNAL(triggered()), this, SLOT(slotInstitutionEdit()));

  KAction *institution_delete = actionCollection()->addAction("institution_delete");
  institution_delete->setText(i18n("Delete institution..."));
  institution_delete->setIcon(KIcon("edit-delete"));
  connect(institution_delete, SIGNAL(triggered()), this, SLOT(slotInstitutionDelete()));

  // *****************
  // The accounts menu
  // *****************
  KAction *account_new = actionCollection()->addAction("account_new");
  account_new->setText(i18n("New account..."));
  account_new->setIcon(KIcon("account_add"));
  connect(account_new, SIGNAL(triggered()), this, SLOT(slotAccountNew()));

  // note : action "category_new" is included in this menu but defined below
  KAction *account_open = actionCollection()->addAction("account_open");
  account_open->setText(i18n("Open ledger"));
  account_open->setIcon(KIcon("ledger"));
  connect(account_open, SIGNAL(triggered()), this, SLOT(slotAccountOpen()));

  KAction *account_reconcile = actionCollection()->addAction("account_reconcile");
  account_reconcile->setText(i18n("Reconcile..."));
  account_reconcile->setIcon(KIcon("reconcile"));
  account_reconcile->setShortcut(KShortcut("Ctrl+Shift+R"));
  connect(account_reconcile, SIGNAL(triggered()), this, SLOT(slotAccountReconcileStart()));

  KAction *account_reconcile_finish = actionCollection()->addAction("account_reconcile_finish");
  account_reconcile_finish->setText(i18nc("Finish reconciliation", "Finish"));
  account_reconcile_finish->setIcon(KIcon("media-skip-forward"));
  connect(account_reconcile_finish, SIGNAL(triggered()), this, SLOT(slotAccountReconcileFinish()));

  KAction *account_reconcile_postpone = actionCollection()->addAction("account_reconcile_postpone");
  account_reconcile_postpone->setText(i18n("Postpone reconciliation"));
  account_reconcile_postpone->setIcon(KIcon("media-playback-pause"));
  connect(account_reconcile_postpone, SIGNAL(triggered()), this, SLOT(slotAccountReconcilePostpone()));

  KAction *account_edit = actionCollection()->addAction("account_edit");
  account_edit->setText(i18n("Edit account..."));
  account_edit->setIcon(KIcon("document-properties"));
  connect(account_edit, SIGNAL(triggered()), this, SLOT(slotAccountEdit()));

  KAction *account_delete = actionCollection()->addAction("account_delete");
  account_delete->setText(i18n("Delete account..."));
  connect(account_delete, SIGNAL(triggered()), this, SLOT(slotAccountDelete()));

  KAction *account_close = actionCollection()->addAction("account_close");
  account_close->setText(i18n("Close account"));
  connect(account_close, SIGNAL(triggered()), this, SLOT(slotAccountClose()));

  KAction *account_reopen = actionCollection()->addAction("account_reopen");
  account_reopen->setText(i18n("Reopen account"));
  connect(account_reopen, SIGNAL(triggered()), this, SLOT(slotAccountReopen()));

  KAction *account_transaction_report = actionCollection()->addAction("account_transaction_report");
  account_transaction_report->setText(i18n("Transaction report"));
  account_transaction_report->setIcon(KIcon("view_info"));
  connect(account_transaction_report, SIGNAL(triggered()), this, SLOT(slotAccountTransactionReport()));

  KAction *account_chart = actionCollection()->addAction("account_chart");
  account_chart->setText(i18n("Show balance chart..."));
  account_chart->setIcon(KIcon("report"));
  connect(account_chart, SIGNAL(triggered()), this, SLOT(slotAccountChart()));

  KAction *account_online_map = actionCollection()->addAction("account_online_map");
  account_online_map->setText(i18n("Map to online account"));
  account_online_map->setIcon(KIcon("news-subscribe"));
  connect(account_online_map, SIGNAL(triggered()), this, SLOT(slotAccountMapOnline()));

  KAction *account_online_unmap = actionCollection()->addAction("account_online_unmap");
  account_online_unmap->setText(i18n("Unmap account..."));
  connect(account_online_unmap, SIGNAL(triggered()), this, SLOT(slotAccountUnmapOnline()));

  KActionMenu* menu = new KActionMenu(KIcon("view-refresh"), i18nc("Update online accounts menu", "Update"), actionCollection());
  // activating the menu button is the same as selecting the current account
  connect( menu, SIGNAL( activated() ), this, SLOT(slotAccountUpdateOnline()));

  KAction *account_online_update = actionCollection()->addAction("account_online_update");
  account_online_update->setText(i18n("Update account..."));
  connect(account_online_update, SIGNAL(triggered()), this, SLOT(slotAccountUpdateOnline()));
  menu->addAction(account_online_update);

  KAction *account_online_update_all = actionCollection()->addAction("account_online_update_all");
  account_online_update_all->setText(i18n("Update all accounts..."));
  connect(account_online_update_all, SIGNAL(triggered()), this, SLOT(slotAccountUpdateOnlineAll()));
  menu->addAction(account_online_update_all);

  // *******************
  // The categories menu
  // *******************
  KAction *category_new = actionCollection()->addAction("category_new");
  category_new->setText(i18n("New category..."));
  category_new->setIcon(KIcon("account_add"));
  connect(category_new, SIGNAL(triggered()), this, SLOT(slotCategoryNew()));

  KAction *category_edit = actionCollection()->addAction("category_edit");
  category_edit->setText(i18n("Edit category..."));
  category_edit->setIcon(KIcon("document-properties"));
  connect(category_edit, SIGNAL(triggered()), this, SLOT(slotAccountEdit()));

  KAction *category_delete = actionCollection()->addAction("category_delete");
  category_delete->setText(i18n("Delete category..."));
  category_delete->setIcon(KIcon("edit-delete"));
  connect(category_delete, SIGNAL(triggered()), this, SLOT(slotAccountDelete()));

  // **************
  // The tools menu
  // **************
  KAction *tools_qif_editor = actionCollection()->addAction("tools_qif_editor");
  tools_qif_editor->setText(i18n("QIF Profile Editor..."));
  tools_qif_editor->setIcon(KIcon("document-properties"));
  connect(tools_qif_editor, SIGNAL(triggered()), this, SLOT(slotQifProfileEditor()));

  KAction *tools_security_editor = actionCollection()->addAction("tools_security_editor");
  tools_security_editor->setText(i18n("Securities..."));
  connect(tools_security_editor, SIGNAL(triggered()), this, SLOT(slotSecurityEditor()));

  KAction *tools_currency_editor = actionCollection()->addAction("tools_currency_editor");
  tools_currency_editor->setText(i18n("Currencies..."));
  connect(tools_currency_editor, SIGNAL(triggered()), this, SLOT(slotCurrencyDialog()));

  KAction *tools_price_editor = actionCollection()->addAction("tools_price_editor");
  tools_price_editor->setText(i18n("Prices..."));
  connect(tools_price_editor, SIGNAL(triggered()), this, SLOT(slotPriceDialog()));

  KAction *tools_update_prices = actionCollection()->addAction("tools_update_prices");
  tools_update_prices->setText(i18n("Update Stock and Currency Prices..."));
  connect(tools_update_prices, SIGNAL(triggered()), this, SLOT(slotEquityPriceUpdate()));

  KAction *tools_consistency_check = actionCollection()->addAction("tools_consistency_check");
  tools_consistency_check->setText(i18n("Consistency Check"));
  connect(tools_consistency_check, SIGNAL(triggered()), this, SLOT(slotFileConsitencyCheck()));

  KAction *tools_performancetest = actionCollection()->addAction("tools_performancetest");
  tools_performancetest->setText(i18n("Performance-Test"));
  tools_performancetest->setIcon(KIcon("fork"));
  connect(tools_performancetest, SIGNAL(triggered()), this, SLOT(slotPerformanceTest()));

  KAction *tools_kcalc = actionCollection()->addAction("tools_kcalc");
  tools_kcalc->setText(i18n("KCalc..."));
  tools_kcalc->setIcon(KIcon("accessories-calculator"));
  connect(tools_kcalc, SIGNAL(triggered()), this, SLOT(slotToolsStartKCalc()));

  // *****************
  // The settings menu
  // *****************
  actionCollection()->addAction( KStandardAction::Preferences, this, SLOT(slotSettings()) );

  KAction *settings_enable_messages = actionCollection()->addAction("settings_enable_messages");
  settings_enable_messages->setText(i18n("Enable all messages"));
  connect(settings_enable_messages, SIGNAL(triggered()), this, SLOT(slotEnableMessages()));

  KAction *settings_language = actionCollection()->addAction("settings_language");
  settings_language->setText(i18n("KDE language settings..."));
  connect(settings_language, SIGNAL(triggered()), this, SLOT(slotKDELanguageSettings()));

  // *************
  // The help menu
  // *************
  KAction *help_show_tip = actionCollection()->addAction("help_show_tip");
  help_show_tip->setText(i18n("&Show tip of the day"));
  help_show_tip->setIcon(KIcon("ktip"));
  connect(help_show_tip, SIGNAL(triggered()), this, SLOT(slotShowTipOfTheDay()));

  // ***************************
  // Actions w/o main menu entry
  // ***************************
  KAction *transaction_new = actionCollection()->addAction("transaction_new");
  transaction_new->setText(i18nc("New transaction button", "New"));
  transaction_new->setIcon(KIcon("document-new"));
  transaction_new->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Insert));
  connect(transaction_new, SIGNAL(triggered()), this, SLOT(slotTransactionsNew()));


  // we use Return as the same shortcut for Edit and Enter. Therefore, we don't allow
  // to change them (The standard KDE dialog complains anyway if you want to assign
  // the same shortcut to two actions)
  KAction *transaction_edit = actionCollection()->addAction("transaction_edit");
  transaction_edit->setText(i18nc("Edit transaction button", "Edit"));
  transaction_edit->setIcon(KIcon("document-properties"));
  transaction_edit->setShortcutConfigurable(false);
  connect(transaction_edit, SIGNAL(triggered()), this, SLOT(slotTransactionsEdit()));

  KAction *transaction_enter = actionCollection()->addAction("transaction_enter");
  transaction_enter->setText(i18nc("Enter transaction", "Enter"));
  transaction_enter->setIcon(KIcon("dialog-ok"));
  transaction_enter->setShortcutConfigurable(false);
  connect(transaction_enter, SIGNAL(triggered()), this, SLOT(slotTransactionsEnter()));

  KAction *transaction_editsplits = actionCollection()->addAction("transaction_editsplits");
  transaction_editsplits->setText(i18nc("Edit split button", "Edit splits"));
  transaction_editsplits->setIcon(KIcon("split_transaction"));
  connect(transaction_editsplits, SIGNAL(triggered()), this, SLOT(slotTransactionsEditSplits()));

  KAction *transaction_cancel = actionCollection()->addAction("transaction_cancel");
  transaction_cancel->setText(i18nc("Cancel transaction edit", "Cancel"));
  transaction_cancel->setIcon(KIcon("dialog-cancel"));
  connect(transaction_cancel, SIGNAL(triggered()), this, SLOT(slotTransactionsCancel()));

  KAction *transaction_delete = actionCollection()->addAction("transaction_delete");
  transaction_delete->setText(i18nc("Delete transaction", "Delete"));
  transaction_delete->setIcon(KIcon("edit-delete"));
  connect(transaction_delete, SIGNAL(triggered()), this, SLOT(slotTransactionsDelete()));

  KAction *transaction_duplicate = actionCollection()->addAction("transaction_duplicate");
  transaction_duplicate->setText(i18nc("Duplicate transaction", "Duplicate"));
  transaction_duplicate->setIcon(KIcon("edit-copy"));
  connect(transaction_duplicate, SIGNAL(triggered()), this, SLOT(slotTransactionDuplicate()));

  KAction *transaction_match = actionCollection()->addAction("transaction_match");
  transaction_match->setText(i18nc("Button text for match transaction", "Match"));
  transaction_match->setIcon(KIcon("process-stop"));
  connect(transaction_match, SIGNAL(triggered()), this, SLOT(slotTransactionMatch()));

  KAction *transaction_accept = actionCollection()->addAction("transaction_accept");
  transaction_accept->setText(i18nc("Accept 'imported' and 'matched' transaction", "Accept"));
  transaction_accept->setIcon(KIcon("dialog-ok-apply"));
  connect(transaction_accept, SIGNAL(triggered()), this, SLOT(slotTransactionsAccept()));

  KAction *transaction_mark_toggle = actionCollection()->addAction("transaction_mark_toggle");
  transaction_mark_toggle->setText(i18nc("Toggle reconciliation flag", "Toggle"));
  transaction_mark_toggle->setShortcut(KShortcut("Ctrl+Space"));
  connect(transaction_mark_toggle, SIGNAL(triggered()), this, SLOT(slotToggleReconciliationFlag()));

  KAction *transaction_mark_cleared = actionCollection()->addAction("transaction_mark_cleared");
  transaction_mark_cleared->setText(i18nc("Mark transaction cleared", "Cleared"));
  transaction_mark_cleared->setShortcut(KShortcut("Ctrl+Alt+Space"));
  connect(transaction_mark_cleared, SIGNAL(triggered()), this, SLOT(slotMarkTransactionCleared()));

  KAction *transaction_mark_reconciled = actionCollection()->addAction("transaction_mark_reconciled");
  transaction_mark_reconciled->setText(i18nc("Mark transaction reconciled", "Reconciled"));
  connect(transaction_mark_reconciled, SIGNAL(triggered()), this, SLOT(slotMarkTransactionReconciled()));

  KAction *transaction_mark_notreconciled = actionCollection()->addAction("transaction_mark_notreconciled");
  transaction_mark_notreconciled->setText(i18nc("Mark transaction not reconciled", "Not reconciled"));
  connect(transaction_mark_notreconciled, SIGNAL(triggered()), this, SLOT(slotMarkTransactionNotReconciled()));

  KAction *transaction_select_all = actionCollection()->addAction("transaction_select_all");
  transaction_select_all->setText(i18nc("Select all transactions", "Select all"));
  transaction_select_all->setShortcut(KShortcut("Ctrl+A"));
  connect(transaction_select_all, SIGNAL(triggered()), this, SIGNAL(selectAllTransactions()));

  KAction *transaction_goto_account = actionCollection()->addAction("transaction_goto_account");
  transaction_goto_account->setText(i18n("Goto account"));
  transaction_goto_account->setIcon(KIcon("go-jump"));
  connect(transaction_goto_account, SIGNAL(triggered()), this, SLOT(slotTransactionGotoAccount()));

  KAction *transaction_goto_payee = actionCollection()->addAction("transaction_goto_payee");
  transaction_goto_payee->setText(i18n("Goto payee"));
  transaction_goto_payee->setIcon(KIcon("go-jump"));
  connect(transaction_goto_payee, SIGNAL(triggered()), this, SLOT(slotTransactionGotoPayee()));

  KAction *transaction_create_schedule = actionCollection()->addAction("transaction_create_schedule");
  transaction_create_schedule->setText(i18n("Create scheduled transaction..."));
  transaction_create_schedule->setIcon(KIcon("bookmark-new"));
  connect(transaction_create_schedule, SIGNAL(triggered()), this, SLOT(slotTransactionCreateSchedule()));

  KAction *transaction_assign_number = actionCollection()->addAction("transaction_assign_number");
  transaction_assign_number->setText(i18n("Assign next number"));
  transaction_assign_number->setShortcut(KShortcut("Ctrl+Shift+N"));
  connect(transaction_assign_number, SIGNAL(triggered()), this, SLOT(slotTransactionAssignNumber()));

  KAction *transaction_combine = actionCollection()->addAction("transaction_combine");
  transaction_combine->setText(i18nc("Combine transactions", "Combine"));
  connect(transaction_combine, SIGNAL(triggered()), this, SLOT(slotTransactionCombine()));

  //Investment
  KAction *investment_new = actionCollection()->addAction("investment_new");
  investment_new->setText(i18n("New investment..."));
  investment_new->setIcon(KIcon("document-new"));
  connect(investment_new, SIGNAL(triggered()), this, SLOT(slotInvestmentNew()));

  KAction *investment_edit = actionCollection()->addAction("investment_edit");
  investment_edit->setText(i18n("Edit investment..."));
  investment_edit->setIcon(KIcon("document-properties"));
  connect(investment_edit, SIGNAL(triggered()), this, SLOT(slotInvestmentEdit()));

  KAction *investment_delete = actionCollection()->addAction("investment_delete");
  investment_delete->setText(i18n("Delete investment..."));
  investment_delete->setIcon(KIcon("edit-delete"));
  connect(investment_delete, SIGNAL(triggered()), this, SLOT(slotInvestmentDelete()));

  KAction *investment_online_price_update = actionCollection()->addAction("investment_online_price_update");
  investment_online_price_update->setText(i18n("Online price update..."));
  connect(investment_online_price_update, SIGNAL(triggered()), this, SLOT(slotOnlinePriceUpdate()));

  KAction *investment_manual_price_update = actionCollection()->addAction("investment_manual_price_update");
  investment_manual_price_update->setText(i18n("Manual price update..."));
  connect(investment_manual_price_update, SIGNAL(triggered()), this, SLOT(slotManualPriceUpdate()));

  //Schedule
  KAction *schedule_new = actionCollection()->addAction("schedule_new");
  schedule_new->setText(i18n("New scheduled transaction"));
  schedule_new->setIcon(KIcon("appointment-new"));
  connect(schedule_new, SIGNAL(triggered()), this, SLOT(slotScheduleNew()));

  KAction *schedule_edit = actionCollection()->addAction("schedule_edit");
  schedule_edit->setText(i18n("Edit scheduled transaction"));
  schedule_edit->setIcon(KIcon("document-properties"));
  connect(schedule_edit, SIGNAL(triggered()), this, SLOT(slotScheduleEdit()));

  KAction *schedule_delete = actionCollection()->addAction("schedule_delete");
  schedule_delete->setText(i18n("Delete scheduled transaction"));
  schedule_delete->setIcon(KIcon("edit-delete"));
  connect(schedule_delete, SIGNAL(triggered()), this, SLOT(slotScheduleDelete()));

  KAction *schedule_duplicate = actionCollection()->addAction("schedule_duplicate");
  schedule_duplicate->setText(i18n("Duplicate scheduled transaction"));
  schedule_duplicate->setIcon(KIcon("edit-copy"));
  connect(schedule_duplicate, SIGNAL(triggered()), this, SLOT(slotScheduleDuplicate()));

  KAction *schedule_enter = actionCollection()->addAction("schedule_enter");
  schedule_enter->setText(i18n("Enter next transaction..."));
  schedule_enter->setIcon(KIcon("go-jump-locationbar"));
  connect(schedule_enter, SIGNAL(triggered()), this, SLOT(slotScheduleEnter()));

  KAction *schedule_skip = actionCollection()->addAction("schedule_skip");
  schedule_skip->setText(i18n("Skip next transaction..."));
  schedule_skip->setIcon(KIcon("media-seek-forward"));
  connect(schedule_skip, SIGNAL(triggered()), this, SLOT(slotScheduleSkip()));

  //Payees
  KAction *payee_new = actionCollection()->addAction("payee_new");
  payee_new->setText(i18n("New payee"));
  payee_new->setIcon(KIcon("document-new"));
  connect(payee_new, SIGNAL(triggered()), this, SLOT(slotPayeeNew()));

  KAction *payee_rename = actionCollection()->addAction("payee_rename");
  payee_rename->setText(i18n("Rename payee"));
  payee_rename->setIcon(KIcon("edit-delete"));
  connect(payee_rename, SIGNAL(triggered()), this, SIGNAL(payeeRename()));

  KAction *payee_delete = actionCollection()->addAction("payee_delete");
  payee_delete->setText(i18n("Delete payee"));
  payee_delete->setIcon(KIcon("edit-delete"));
  connect(payee_delete, SIGNAL(triggered()), this, SLOT(slotPayeeDelete()));

  //Budget
  KAction *budget_new = actionCollection()->addAction("budget_new");
  budget_new->setText(i18n("New budget"));
  budget_new->setIcon(KIcon("document-new"));
  connect(budget_new, SIGNAL(triggered()), this, SLOT(slotBudgetNew()));

  KAction *budget_rename = actionCollection()->addAction("budget_rename");
  budget_rename->setText(i18n("Rename budget"));
  budget_rename->setIcon(KIcon("document-properties"));
  connect(budget_rename, SIGNAL(triggered()), this, SIGNAL(budgetRename()));

  KAction *budget_delete = actionCollection()->addAction("budget_delete");
  budget_delete->setText(i18n("Delete budget"));
  budget_delete->setIcon(KIcon("edit-delete"));
  connect(budget_delete, SIGNAL(triggered()), this, SLOT(slotBudgetDelete()));

  KAction *budget_copy = actionCollection()->addAction("budget_copy");
  budget_copy->setText(i18n("Copy budget"));
  budget_copy->setIcon(KIcon("edit-copy"));
  connect(budget_copy, SIGNAL(triggered()), this, SLOT(slotBudgetCopy()));

  KAction *budget_change_year = actionCollection()->addAction("budget_change_year");
  budget_change_year->setText(i18n("Change budget year"));
  //budget_change_year->setIcon(KIcon(""));
  connect(budget_change_year, SIGNAL(triggered()), this, SLOT(slotBudgetChangeYear()));

  KAction *budget_forecast = actionCollection()->addAction("budget_forecast");
  budget_forecast->setText(i18n("Budget based on forecast"));
  budget_forecast->setIcon(KIcon("forcast"));
  connect(budget_forecast, SIGNAL(triggered()), this, SLOT(slotBudgetForecast()));

  // ************************
  // Currency actions
  // ************************
  KAction *currency_new = actionCollection()->addAction("currency_new");
  currency_new->setText(i18n("New currency"));
  currency_new->setIcon(KIcon("document-new"));
  connect(currency_new, SIGNAL(triggered()), this, SLOT(slotCurrencyNew()));

  KAction *currency_rename = actionCollection()->addAction("currency_rename");
  currency_rename->setText(i18n("Rename currency"));
  currency_rename->setIcon(KIcon("edit-delete"));
  connect(currency_rename, SIGNAL(triggered()), this, SIGNAL(currencyRename()));

  KAction *currency_delete = actionCollection()->addAction("currency_delete");
  currency_delete->setText(i18n("Delete currency"));
  currency_delete->setIcon(KIcon("edit-delete"));
  connect(currency_delete, SIGNAL(triggered()), this, SLOT(slotCurrencyDelete()));

  KAction *currency_setbase = actionCollection()->addAction("currency_setbase");
  currency_setbase->setText(i18n("Select as base currency"));
  currency_setbase->setIcon(KIcon("kmymoney2"));
  connect(currency_setbase, SIGNAL(triggered()), this, SLOT(slotCurrencySetBase()));

#ifdef KMM_DEBUG
  KAction *new_user_wizard = actionCollection()->addAction("new_user_wizard");
  new_user_wizard->setText(i18n("Test new feature"));
  new_user_wizard->setShortcut(KShortcut("Ctrl+G"));
  connect(new_user_wizard, SIGNAL(triggered()), this, SLOT(slotNewFeature()));

  KToggleAction *debug_traces = new KToggleAction(this);
  actionCollection()->addAction("debug_traces", debug_traces);
  debug_traces->setText(i18n("Debug Traces"));
  connect(debug_traces, SIGNAL(triggered()), this, SLOT(slotToggleTraces()));

  KToggleAction *debug_timers = new KToggleAction(this);
  actionCollection()->addAction("debug_timers", debug_timers);
  debug_timers->setText(i18n("Debug Timers"));
  connect(debug_timers, SIGNAL(triggered()), this, SLOT(slotToggleTimers()));
#endif

  // ************************
  // Currently unused actions
  // ************************
#if 0
  new KToolBarPopupAction(i18n("View back"), "go-previous", 0, this, SLOT(slotShowPreviousView()), actionCollection(), "go_back");
  new KToolBarPopupAction(i18n("View forward"), "go-next", 0, this, SLOT(slotShowNextView()), actionCollection(), "go_forward");

  action("go_back")->setEnabled(false);
  action("go_forward")->setEnabled(false);
#endif

  // Setup transaction detail switch
  toggleAction("view_show_transaction_detail")->setChecked(KMyMoneyGlobalSettings::showRegisterDetailed());
  toggleAction("view_hide_reconciled_transactions")->setChecked(KMyMoneyGlobalSettings::hideReconciledTransactions());
  toggleAction("view_hide_unused_categories")->setChecked(KMyMoneyGlobalSettings::hideUnusedCategory());
  toggleAction("view_show_all_accounts")->setChecked(false);

  // use the absolute path to your kmymoney2ui.rc file for testing purpose in createGUI();
  // createGUI(QString::null, false);
  setupGUI();
}

void KMyMoney2App::dumpActions(void) const
{
  const QList<QAction*> list = actionCollection()->actions();
  QList<QAction*>::const_iterator it;
  for(it = list.begin(); it != list.end(); ++it) {
    //FIXME: Port to KDE4
    #if 0
    std::cout << (*it)->name() << ": " << (*it)->text() << std::endl;
    #endif
  }
}

QAction* KMyMoney2App::action(const QString& actionName) const
{
  //static KShortcut shortcut("");
  //FIXME: Port to KDE4
  //static QAction dummyAction(QString("Dummy"), static_cast<QObject*>(this));

  //FIXME: Port to KDE4
  QAction* p = actionCollection()->action(QString(actionName.toLatin1()));
  if(p)
    return p;

  qWarning("Action with name '%s' not found!", qPrintable(actionName));
  return p;
}

KToggleAction* KMyMoney2App::toggleAction(const QString& actionName) const
{
  //FIXME: Port to KDE4
  //static KShortcut shortcut("");
  //static KToggleAction dummyAction(QString("Dummy"), QString(), shortcut, static_cast<const QObject*>(this), 0, static_cast<KActionCollection*>(0), "");

  QAction* q = actionCollection()->action(QString(actionName.toLatin1()));

  if(q) {
    KToggleAction* p = dynamic_cast<KToggleAction*>(q);
    if(!p) {
      qWarning("Action '%s' is not of type KToggleAction", qPrintable(actionName));
      //p = &dummyAction;
    }
    return p;
  }

  qWarning("Action with name '%s' not found!", qPrintable(actionName));
  return dynamic_cast<KToggleAction*>(q);
}


void KMyMoney2App::initStatusBar(void)
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR

  statusBar()->insertItem("", ID_STATUS_MSG);
  ready();

  // Initialization of progress bar taken from KDevelop ;-)
  d->m_progressBar = new QProgressBar(statusBar());
  statusBar()->addWidget(d->m_progressBar);
  d->m_progressBar->setFixedHeight(d->m_progressBar->sizeHint().height() - 8);

  // hide the progress bar for now
  slotStatusProgressBar(-1, -1);
}

void KMyMoney2App::saveOptions(void)
{
  //FIXME: Port to KDE4
  KConfigGroup grp = d->m_config->group("General Options");
  grp.writeEntry("Geometry", size());

  grp.writeEntry("Show Statusbar", toggleAction("options_show_statusbar")->isChecked());
  //toolBar("mainToolBar")->saveSettings(grp, "mainToolBar");

  d->m_recentFiles->saveEntries(d->m_config->group("Recent Files"));

}


void KMyMoney2App::readOptions(void)
{
  KConfigGroup grp = d->m_config->group("General Options");

  toggleAction("view_hide_reconciled_transactions")->setChecked(KMyMoneyGlobalSettings::hideReconciledTransactions());
  toggleAction("view_hide_unused_categories")->setChecked(KMyMoneyGlobalSettings::hideUnusedCategory());

  d->m_recentFiles->loadEntries(d->m_config->group("Recent Files"));

  // Startdialog is written in the settings dialog
  d->m_startDialog = grp.readEntry("StartDialog", true);

  KConfigGroup schedGrp = d->m_config->group("Schedule Options");
  d->m_bCheckSchedules = schedGrp.readEntry("CheckSchedules", true);
}

void KMyMoney2App::resizeEvent(QResizeEvent* ev)
{
  KMainWindow::resizeEvent(ev);
  updateCaption(true);
}

bool KMyMoney2App::queryClose(void)
{
  if(!isReady())
    return false;

  if (d->m_myMoneyView->dirty()) {
    int ans = KMessageBox::warningYesNoCancel(this, i18n("KMyMoney file needs saving.  Save ?"));
    if (ans==KMessageBox::Cancel)
      return false;
    else if (ans==KMessageBox::Yes)
      return slotFileSave();
  }
  if (d->m_myMoneyView->isDatabase())
    slotFileClose(); // close off the database
  return true;
}

bool KMyMoney2App::queryExit(void)
{
  saveOptions();

  return true;
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////
void KMyMoney2App::slotFileInfoDialog(void)
{
  KMyMoneyFileInfoDlg dlg(0);
  dlg.exec();
}

void KMyMoney2App::slotPerformanceTest(void)
{
  // dump performance report to stderr

  int measurement[2];
  QTime timer;
  MyMoneyAccount acc;

  qDebug("--- Starting performance tests ---");

  // AccountList
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  timer.start();
  for(int i = 0; i < 1000; ++i) {
    QList<MyMoneyAccount> list;

    MyMoneyFile::instance()->accountList(list);
    measurement[i != 0] = timer.elapsed();
  }
  std::cerr << "accountList()" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Total time: " << (measurement[0] + measurement[1]) << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // Balance of asset account(s)
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  acc = MyMoneyFile::instance()->asset();
  for(int i = 0; i < 1000; ++i) {
    timer.start();
    MyMoneyMoney result = MyMoneyFile::instance()->balance(acc.id());
    measurement[i != 0] += timer.elapsed();
  }
  std::cerr << "balance(Asset)" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // total balance of asset account
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  acc = MyMoneyFile::instance()->asset();
  for(int i = 0; i < 1000; ++i) {
    timer.start();
    MyMoneyMoney result = MyMoneyFile::instance()->totalBalance(acc.id());
    measurement[i != 0] += timer.elapsed();
  }
  std::cerr << "totalBalance(Asset)" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // Balance of expense account(s)
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  acc = MyMoneyFile::instance()->expense();
  for(int i = 0; i < 1000; ++i) {
    timer.start();
    MyMoneyMoney result = MyMoneyFile::instance()->balance(acc.id());
    measurement[i != 0] += timer.elapsed();
  }
  std::cerr << "balance(Expense)" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // total balance of expense account
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  acc = MyMoneyFile::instance()->expense();
  timer.start();
  for(int i = 0; i < 1000; ++i) {
    MyMoneyMoney result = MyMoneyFile::instance()->totalBalance(acc.id());
    measurement[i != 0] = timer.elapsed();
  }
  std::cerr << "totalBalance(Expense)" << std::endl;
  std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
  std::cerr << "Total time: " << (measurement[0] + measurement[1]) << " msec" << std::endl;
  std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 1000 << " msec" << std::endl;

  // transaction list
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  if(MyMoneyFile::instance()->asset().accountCount()) {
    MyMoneyTransactionFilter filter(MyMoneyFile::instance()->asset().accountList()[0]);
    filter.setDateFilter(QDate(), QDate::currentDate());
    QList<MyMoneyTransaction> list;

    timer.start();
    for(int i = 0; i < 100; ++i) {
      list = MyMoneyFile::instance()->transactionList(filter);
      measurement[i != 0] = timer.elapsed();
    }
    std::cerr << "transactionList()" << std::endl;
    std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
    std::cerr << "Total time: " << (measurement[0] + measurement[1]) << " msec" << std::endl;
    std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 100 << " msec" << std::endl;
  }

  // transaction list
  MyMoneyFile::instance()->preloadCache();
  measurement[0] = measurement[1] = 0;
  if(MyMoneyFile::instance()->asset().accountCount()) {
    MyMoneyTransactionFilter filter(MyMoneyFile::instance()->asset().accountList()[0]);
    filter.setDateFilter(QDate(), QDate::currentDate());
    QList<MyMoneyTransaction> list;

    timer.start();
    for(int i = 0; i < 100; ++i) {
      MyMoneyFile::instance()->transactionList(list, filter);
      measurement[i != 0] = timer.elapsed();
    }
    std::cerr << "transactionList(list)" << std::endl;
    std::cerr << "First time: " << measurement[0] << " msec" << std::endl;
    std::cerr << "Total time: " << (measurement[0] + measurement[1]) << " msec" << std::endl;
    std::cerr << "Average   : " << (measurement[0] + measurement[1]) / 100 << " msec" << std::endl;
  }
  MyMoneyFile::instance()->preloadCache();
}

void KMyMoney2App::slotFileNew(void)
{
  KMSTATUS(i18n("Creating new document..."));

  slotFileClose();

  if(!d->m_myMoneyView->fileOpen()) {
    // next line required until we move all file handling out of KMyMoneyView
    d->m_myMoneyView->newFile();

    d->m_fileName = KUrl();
    updateCaption();

    // before we create the wizard, we need to preload the currencies
    MyMoneyFileTransaction ft;
    d->m_myMoneyView->loadDefaultCurrencies();
    d->m_myMoneyView->loadAncientCurrencies();
    ft.commit();

    NewUserWizard::Wizard *wizard = new NewUserWizard::Wizard();

    if(wizard->exec() == QDialog::Accepted) {
      MyMoneyFile* file = MyMoneyFile::instance();
      ft.restart();
      try {
        // store the user info
        file->setUser(wizard->user());

        // setup base currency
        file->setBaseCurrency(wizard->baseCurrency());

        // create a possible institution
        MyMoneyInstitution inst = wizard->institution();
        if(inst.name().length()) {
          file->addInstitution(inst);
        }

        // create a possible checking account
        MyMoneyAccount acc = wizard->account();
        if(acc.name().length()) {
          acc.setInstitutionId(inst.id());
          MyMoneyAccount asset = file->asset();
          file->addAccount(acc, asset);

          // create possible opening balance transaction
          if(!wizard->openingBalance().isZero()) {
            file->createOpeningBalanceTransaction(acc, wizard->openingBalance());
          }
        }

        // import the account templates
        Q3ValueList<MyMoneyTemplate> templates = wizard->templates();
        Q3ValueList<MyMoneyTemplate>::iterator it_t;
        for(it_t = templates.begin(); it_t != templates.end(); ++it_t) {
          (*it_t).importTemplate(&progressCallback);
        }

        d->m_fileName = KUrl(wizard->url());
        ft.commit();
        KMyMoneyGlobalSettings::setFirstTimeRun(false);

        // FIXME This is a bit clumsy. We re-read the freshly
        // created file to be able to run through all the
        // fixup logic and then save it to keep the modified
        // flag off.
        slotFileSave();
        d->m_myMoneyView->readFile(d->m_fileName);
        slotFileSave();

        // now keep the filename in the recent files used list
        //KRecentFilesAction *p = dynamic_cast<KRecentFilesAction*>(action(KStandardAction::name(KStandardAction::OpenRecent)));
        //if(p)
        d->m_recentFiles->addUrl( d->m_fileName );
        writeLastUsedFile(d->m_fileName.url());

      } catch(MyMoneyException* e) {
        delete e;
        // next line required until we move all file handling out of KMyMoneyView
        d->m_myMoneyView->closeFile();
      }

    } else {
      // next line required until we move all file handling out of KMyMoneyView
      d->m_myMoneyView->closeFile();
    }
    delete wizard;
    updateCaption();

    emit fileLoaded(d->m_fileName);
  }
}

KUrl KMyMoney2App::selectFile(const QString& /*title*/, const QString& _path, const QString& mask, KFile::Mode mode)
{
  KUrl url;
  QString path(_path);

  if(path.isEmpty())
    path = KGlobalSettings::documentPath();

  QPointer<KFileDialog> dialog = new KFileDialog(KUrl(path), mask, this);
  dialog->setMode(mode);

  if(dialog->exec() == QDialog::Accepted) {
    url = dialog->selectedUrl();
  }
  delete dialog;

  return url;
}

// General open
void KMyMoney2App::slotFileOpen(void)
{
  KMSTATUS(i18n("Open a file."));

  QPointer<KFileDialog> dialog = new KFileDialog(KUrl(KGlobalSettings::documentPath()),
                            i18n("*.kmy *.xml|KMyMoney files\n *.*|All files (*.*)"),
                            this);
  dialog->setMode(KFile::File | KFile::ExistingOnly);

  if(dialog->exec() == QDialog::Accepted) {
    slotFileOpenRecent(dialog->selectedUrl());
  }
  delete dialog;
}

void KMyMoney2App::slotOpenDatabase(void)
{
  KMSTATUS(i18n("Open a file."));
  QPointer<KSelectDatabaseDlg> dialog = new KSelectDatabaseDlg(QIODevice::ReadWrite);
  if (!dialog->checkDrivers()) {
    delete dialog;
    return;
  }

  if(dialog->exec() == QDialog::Accepted) {
      slotFileOpenRecent(dialog->selectedURL());
  }
  delete dialog;
}

bool KMyMoney2App::isImportableFile( const KUrl& url )
{
  bool result = false;

  // Iterate through the plugins and see if there's a loaded plugin who can handle it
  QMap<QString,KMyMoneyPlugin::ImporterPlugin*>::const_iterator it_plugin = d->m_importerPlugins.constBegin();
  while ( it_plugin != d->m_importerPlugins.constEnd() )
  {
    if ( (*it_plugin)->isMyFormat(url.path()) )
    {
      result = true;
      break;
    }
    ++it_plugin;
  }

  // If we did not find a match, try importing it as a KMM statement file,
  // which is really just for testing.  the statement file is not exposed
  // to users.
  if ( it_plugin == d->m_importerPlugins.constEnd() )
    if ( MyMoneyStatement::isStatementFile( url.path() ) )
      result = true;

  // Place code here to test for QIF and other locally-supported formats
  // (i.e. not a plugin). If you add them here, be sure to add it to
  // the webConnect function.

  return result;
}

void KMyMoney2App::slotFileOpenRecent(const KUrl& url)
{
  KMSTATUS(i18n("Loading file..."));
  KUrl lastFile = d->m_fileName;

  // check if there are other instances which might have this file open
  QList<QString> list = instanceList();
  QList<QString>::ConstIterator it;
  bool duplicate = false;
  for(it = list.constBegin(); duplicate == false && it != list.constEnd(); ++it) {
    QDBusInterface remoteApp(*it, "/KMymoney", "org.kde.kmymoney");
    QDBusReply<QString> reply = remoteApp.call("filename");
    if(!reply.isValid()) {
      qDebug("D-Bus error while calling app->filename()");
    } else {
      if(reply.value() == url.url()) {
        duplicate = true;
      }
    }
  }
  if(!duplicate) {
    KUrl newurl = url;
    if((newurl.protocol() == "sql")) {
      if (newurl.queryItem("driver") == "QMYSQL3") { // fix any old urls
          newurl.removeQueryItem("driver");
          newurl.addQueryItem("driver", "QMYSQL");
      }
      if (newurl.queryItem("driver") == "QSQLITE3") {
          newurl.removeQueryItem("driver");
          newurl.addQueryItem("driver", "QSQLITE");
      }
      // check if a password is needed. it may be if the URL came from the last/recent file list
      QPointer<KSelectDatabaseDlg> dialog = new KSelectDatabaseDlg(QIODevice::ReadWrite, newurl);
      if (!dialog->checkDrivers()) {
        delete dialog;
        return;
      }
      // if we need to supply a password, then show the dialog
      // otherwise it isn't needed
      if ((newurl.queryItem("secure") == "yes") && newurl.pass().isEmpty()) {
        if(dialog->exec() == QDialog::Accepted) newurl = dialog->selectedURL();
      }
      delete dialog;
    }
    if ((newurl.protocol() == "sql") || (newurl.isValid() && KIO::NetAccess::exists(newurl, true, this))) {
      slotFileClose();
      if(!d->m_myMoneyView->fileOpen()) {
        if(d->m_myMoneyView->readFile(newurl)) {
          if((d->m_myMoneyView->isNativeFile())) {
            d->m_fileName = newurl;
            d->m_recentFiles->addUrl(newurl.pathOrUrl());
            writeLastUsedFile(newurl.pathOrUrl());
          } else {
            d->m_fileName = KUrl(); // imported files have no filename
          }
          ::timetrace("Start checking schedules");
          // Check the schedules
          slotCheckSchedules();
          ::timetrace("Done checking schedules");
        }
        updateCaption();
        ::timetrace("Announcing new filename");
        emit fileLoaded(d->m_fileName);
        ::timetrace("Announcing new filename done");
      } else {
          /*fileOpen failed - should we do something
           or maybe fileOpen puts out the message... - it does for database*/
      }
    } else { // newurl invalid
      slotFileClose();
      KMessageBox::sorry(this, QString("<p>")+i18n("<b>%1</b> is either an invalid filename or the file does not exist. You can open another file or create a new one.", url.pathOrUrl()), i18n("File not found"));
    }
  } else { // isDuplicate
    KMessageBox::sorry(this, QString("<p>")+i18n("File <b>%1</b> is already opened in another instance of KMyMoney", url.pathOrUrl()), i18n("Duplicate open"));
  }
}

bool KMyMoney2App::slotFileSave(void)
{
  // if there's nothing changed, there's no need to save anything
  if(!d->m_myMoneyView->dirty())
    return true;

  bool rc = false;

  KMSTATUS(i18n("Saving file..."));

  if (d->m_fileName.isEmpty())
    return slotFileSaveAs();

  /*if (myMoneyView->isDatabase()) {
    rc = myMoneyView->saveDatabase(m_fileName);
    // the 'save' function is no longer relevant for a database*/
  setEnabled(false);
  rc = d->m_myMoneyView->saveFile(d->m_fileName, MyMoneyFile::instance()->value("kmm-encryption-key"));
  setEnabled(true);

  d->m_autoSaveTimer->stop();

  updateCaption();
  return rc;
}

void KMyMoney2App::slotFileSaveAsFilterChanged(const QString& filter)
{
  if (!d->m_saveEncrypted)
    return;
  if(filter != "*.kmy") {
    d->m_saveEncrypted->setCurrentItem(0);
    d->m_saveEncrypted->setEnabled(false);
  } else {
    d->m_saveEncrypted->setEnabled(true);
  }
}

void KMyMoney2App::slotManageGpgKeys(void)
{
  QPointer<KGpgKeySelectionDlg> dlg = new KGpgKeySelectionDlg(this);
  dlg->setKeys(d->m_additionalGpgKeys);
  if(dlg->exec() == QDialog::Accepted) {
    d->m_additionalGpgKeys = dlg->keys();
    d->m_additionalKeyLabel->setText(i18n("Additional encryption key(s) to be used: %1", d->m_additionalGpgKeys.count()));
  }
  delete dlg;
}

void KMyMoney2App::slotKeySelected(int idx)
{
  int cnt = 0;
  if(idx != 0) {
    cnt = d->m_additionalGpgKeys.count();
  }
  d->m_additionalKeyLabel->setEnabled(idx != 0);
  d->m_additionalKeyButton->setEnabled(idx != 0);
  d->m_additionalKeyLabel->setText(i18n("Additional encryption key(s) to be used: %1", cnt));
}

bool KMyMoney2App::slotFileSaveAs(void)
{
  bool rc = false;
  // in event of it being a database, ensure that all data is read into storage for saveas
  if (d->m_myMoneyView->isDatabase())
        dynamic_cast<IMyMoneySerialize*> (MyMoneyFile::instance()->storage())->fillStorage();
  KMSTATUS(i18n("Saving file with a new filename..."));
  QString prevDir= ""; // don't prompt file name if not a native file
  if (d->m_myMoneyView->isNativeFile())
    prevDir = readLastUsedDir();

  // fill the additional key list with the default
  d->m_additionalGpgKeys = KMyMoneyGlobalSettings::gpgRecipientList();

  Q3VBox* vbox = new Q3VBox();
  if(KGPGFile::GPGAvailable()) {
    Q3HBox* keyBox = new Q3HBox(vbox);
    new QLabel(i18n("Encryption key to be used"), keyBox);
    d->m_saveEncrypted = new KComboBox(keyBox);

    Q3HBox* labelBox = new Q3HBox(vbox);
    d->m_additionalKeyLabel = new QLabel(i18n("Additional encryption key(s) to be used: %1", d->m_additionalGpgKeys.count()), labelBox);
    d->m_additionalKeyButton = new KPushButton(i18n("Manage additional keys"), labelBox);
    connect(d->m_additionalKeyButton, SIGNAL(clicked()), this, SLOT(slotManageGpgKeys()));
    connect(d->m_saveEncrypted, SIGNAL(activated(int)), this, SLOT(slotKeySelected(int)));

    // fill the secret key list and combo box
    QStringList keyList;
    KGPGFile::secretKeyList(keyList);
    d->m_saveEncrypted->addItem(i18n("No encryption"));

    for(QStringList::iterator it = keyList.begin(); it != keyList.end(); ++it) {
      QStringList fields = (*it).split(':', QString::SkipEmptyParts);
      if(fields[0] != RECOVER_KEY_ID) {
        // replace parenthesis in name field with brackets
        QString name = fields[1];
        name.replace('(', "[");
        name.replace(')', "]");
        name = QString("%1 (0x%2)").arg(name).arg(fields[0]);
        d->m_saveEncrypted->addItem(name);
        if(name.contains(KMyMoneyGlobalSettings::gpgRecipient())) {
          d->m_saveEncrypted->setCurrentItem(name);
        }
      }
    }
  }

  // the following code is copied from KFileDialog::getSaveFileName,
  // adjust to our local needs (filetypes etc.) and
  // enhanced to show the d->m_saveEncrypted combo box
  bool specialDir = prevDir.at(0) == ':';
  QPointer<KFileDialog> dlg =
    new KFileDialog( specialDir ? prevDir : QString::null,
                   QString("%1|%2\n").arg("*.kmy").arg(i18nc("KMyMoney (Filefilter)", "KMyMoney files")) +
                   QString("%1|%2\n").arg("*.xml").arg(i18nc("XML (Filefilter)", "XML files")) +
                   QString("%1|%2\n").arg("*.anon.xml").arg(i18nc("Anonymous (Filefilter)", "Anonymous files")) +
                   QString("%1|%2\n").arg("*").arg(i18nc("All files (Filefilter)", "All files")),
                   this);
  connect(dlg, SIGNAL(filterChanged(const QString&)), this, SLOT(slotFileSaveAsFilterChanged(const QString&)));

  if ( !specialDir )
    dlg->setSelection( prevDir ); // may also be a filename

  dlg->setOperationMode( KFileDialog::Saving );
  dlg->setCaption(i18n("Save As"));

  if(dlg->exec() == QDialog::Accepted) {

    KUrl newURL = dlg->selectedUrl();

    delete dlg;

    if (!newURL.isEmpty()) {
      QString newName = newURL.pathOrUrl();

  // end of copy

      // find last . delimiter
      int nLoc = newName.lastIndexOf('.');
      if(nLoc != -1)
      {
        QString strExt, strTemp;
        strTemp = newName.left(nLoc + 1);
        strExt = newName.right(newName.length() - (nLoc + 1));
        if((strExt.indexOf("kmy", 0, Qt::CaseInsensitive) == -1) && (strExt.indexOf("xml", 0, Qt::CaseInsensitive) == -1))
        {

          strTemp.append("kmy");
          //append to make complete file name
          newName = strTemp;
        }
      }
      else
      {
        newName.append(".kmy");
      }

      if(okToWriteFile(newName)) {
        //KRecentFilesAction *p = dynamic_cast<KRecentFilesAction*>(action("file_open_recent"));
        //if(p)
          d->m_recentFiles->addUrl( newName );

        setEnabled(false);
        // If this is the anonymous file export, just save it, don't actually take the
        // name, or remember it! Don't even try to encrypt it
        if (newName.right(9).toLower() == ".anon.xml")
        {
          rc = d->m_myMoneyView->saveFile(newName);
        }
        else
        {

          d->m_fileName = newName;
          QString encryptionKeys;
          if(d->m_saveEncrypted && d->m_saveEncrypted->currentIndex() != 0) {
            QRegExp keyExp(".* \\((.*)\\)");
            if(keyExp.indexIn(d->m_saveEncrypted->currentText()) != -1) {
              encryptionKeys = keyExp.cap(1);
            }
            if(!d->m_additionalGpgKeys.isEmpty()) {
              if(!encryptionKeys.isEmpty())
                encryptionKeys += ',';
              encryptionKeys += d->m_additionalGpgKeys.join(",");
            }
          }
          rc = d->m_myMoneyView->saveFile(newName, encryptionKeys);
          //write the directory used for this file as the default one for next time.
          writeLastUsedDir(newName);
          writeLastUsedFile(newName);
        }
        d->m_autoSaveTimer->stop();
        setEnabled(true);
      }
    }
  } else {
    delete dlg;
  }

  updateCaption();
  return rc;
}

bool KMyMoney2App::slotSaveAsDatabase(void)
{
  bool rc = false;
  KUrl oldUrl;
  // in event of it being a database, ensure that all data is read into storage for saveas
  if (d->m_myMoneyView->isDatabase()) {
    dynamic_cast<IMyMoneySerialize*> (MyMoneyFile::instance()->storage())->fillStorage();
    oldUrl = d->m_fileName.isEmpty() ? lastOpenedURL() : d->m_fileName;
  }
  KMSTATUS(i18n("Saving file to database..."));
  QPointer<KSelectDatabaseDlg> dialog = new KSelectDatabaseDlg(QIODevice::WriteOnly);
  KUrl url = oldUrl;
  if (!dialog->checkDrivers()) {
    delete dialog;
    return (false);
  }

  while (oldUrl == url && dialog->exec() == QDialog::Accepted) {
      url = dialog->selectedURL();
    // If the protocol is SQL for the old and new, and the hostname and database names match
    // Let the user know that the current database cannot be saved on top of itself.
    if (url.protocol() == "sql" && oldUrl.protocol() == "sql"
      && oldUrl.host() == url.host()
      && oldUrl.queryItem("driver") == url.queryItem("driver")
      && oldUrl.path().right(oldUrl.path().length() - 1) == url.path().right(url.path().length() - 1)) {
      KMessageBox::sorry(this, i18n("Cannot save to current database."));
    } else {
      try {
        rc = d->m_myMoneyView->saveAsDatabase(url);
      } catch (MyMoneyException* e) {
        KMessageBox::sorry(this, i18n("Cannot save to current database: %1", e->what()));
        delete e;
      }
    }
  }
  delete dialog;

  if (rc) {
    //KRecentFilesAction *p = dynamic_cast<KRecentFilesAction*>(action("file_open_recent"));
    //if(p)
    d->m_recentFiles->addUrl(url.pathOrUrl());
    writeLastUsedFile(url.pathOrUrl());
  }
  d->m_autoSaveTimer->stop();
  updateCaption();
  return rc;
}

void KMyMoney2App::slotFileCloseWindow(void)
{
  KMSTATUS(i18n("Closing window..."));

  if (d->m_myMoneyView->dirty()) {
    int answer = KMessageBox::warningYesNoCancel(this, i18n("The file has been changed, save it ?"));
    if (answer == KMessageBox::Cancel)
      return;
    else if (answer == KMessageBox::Yes)
      slotFileSave();
  }
  close();
}

void KMyMoney2App::slotFileClose(void)
{
  bool okToSelect = true;

  // check if transaction editor is open and ask user what he wants to do
  slotTransactionsCancelOrEnter(okToSelect);

  if(!okToSelect)
    return;

  // no update status here, as we might delete the status too early.
  if (d->m_myMoneyView->dirty()) {
    int answer = KMessageBox::warningYesNoCancel(this, i18n("The file has been changed, save it ?"));
    if (answer == KMessageBox::Cancel)
      return;
    else if (answer == KMessageBox::Yes)
      slotFileSave();
  }

  slotSelectAccount();
  slotSelectInstitution();
  slotSelectInvestment();
  slotSelectSchedule();
  slotSelectCurrency();
  slotSelectBudget(QList<MyMoneyBudget>());
  slotSelectPayees(QList<MyMoneyPayee>());
  slotSelectTransactions(KMyMoneyRegister::SelectedTransactions());

  d->m_reconciliationAccount = MyMoneyAccount();
  d->m_myMoneyView->finishReconciliation(d->m_reconciliationAccount);

  d->m_myMoneyView->closeFile();
  d->m_fileName = KUrl();
  updateCaption();

  // just create a new balance warning object
  delete d->m_balanceWarning;
  d->m_balanceWarning = new KBalanceWarning(this);

  emit fileLoaded(d->m_fileName);
}

void KMyMoney2App::slotFileQuit(void)
{
  // don't modify the status message here as this will prevent quit from working!!
  // See the beginning of queryClose() and isReady() why. Thomas Baumgart 2005-10-17

  KMainWindow* w = 0;


  QList<KMainWindow*> memberList = KMainWindow::memberList();
  if(!memberList.isEmpty()) {

    QList<KMainWindow*>::const_iterator w_it = memberList.constBegin();
    for(; w_it != memberList.constEnd(); ++w_it) {
      // only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,

      // the window and the application stay open.
      if(!(*w_it)->close()) {
        w = (*w_it);
        break;
      }
    }
    // We will only quit if all windows were processed and not cancelled
    if(w == 0)
      kapp->quit();

  } else
      kapp->quit();
}

void KMyMoney2App::slotHideReconciledTransactions(void)
{
  KMyMoneyGlobalSettings::setHideReconciledTransactions(toggleAction("view_hide_reconciled_transactions")->isChecked());
  d->m_myMoneyView->slotRefreshViews();
}

void KMyMoney2App::slotHideUnusedCategories(void)
{
  KMyMoneyGlobalSettings::setHideUnusedCategory(toggleAction("view_hide_unused_categories")->isChecked());
  d->m_myMoneyView->slotRefreshViews();
}

void KMyMoney2App::slotShowAllAccounts(void)
{
  d->m_myMoneyView->slotRefreshViews();
}

void KMyMoney2App::slotToggleTraces(void)
{
  MyMoneyTracer::onOff(toggleAction("debug_traces")->isChecked() ? 1 : 0);
}

void KMyMoney2App::slotToggleTimers(void)
{
  extern bool timersOn; // main.cpp
  timersOn = toggleAction("debug_timers")->isChecked();
}

const QString KMyMoney2App::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  QString msg = d->m_statusMsg;

  d->m_statusMsg = text;
  if(d->m_statusMsg.isEmpty())
    d->m_statusMsg = i18nc("Application is ready to use", "Ready.");
  statusBar()->clearMessage();
  statusBar()->changeItem(text, ID_STATUS_MSG);
  return msg;
}

void KMyMoney2App::ready(void)
{
  slotStatusMsg(QString());
}

bool KMyMoney2App::isReady(void)
{
  return d->m_statusMsg == i18nc("Application is ready to use", "Ready.");
}

void KMyMoney2App::slotStatusProgressBar(int current, int total)
{
  if(total == -1 && current == -1) {      // reset
    d->m_progressBar->reset();
    d->m_progressBar->hide();
    d->m_nextUpdate = 0;

  } else if(total != 0) {                 // init
    d->m_progressBar->setMaximum(total);
    d->m_progressBar->show();

    // make sure, we don't waste too much time for updateing the screen.
    // if we have more than 1000 steps, we update the progress bar
    // every 100 steps. If we have less, we allow to update
    // every 10 steps.
    d->m_progressUpdate = 1;
    if(total > 100)
      d->m_progressUpdate = 10;
    if(total > 1000)
      d->m_progressUpdate = 100;
    d->m_nextUpdate = 0;

  } else {                                // update
    if(current > d->m_nextUpdate) {
      d->m_progressBar->setValue(current);
      //QApplication::eventLoop()->processEvents(QEventLoop::ExcludeUserInput, 10);
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInput, 10);
      d->m_nextUpdate += d->m_progressUpdate;
    }
  }
}

void KMyMoney2App::progressCallback(int current, int total, const QString& msg)
{
  if(!msg.isEmpty())
    kmymoney2->slotStatusMsg(msg);

  kmymoney2->slotStatusProgressBar(current, total);
}

void KMyMoney2App::slotFileViewPersonal(void)
{
  if ( !d->m_myMoneyView->fileOpen() ) {
    KMessageBox::information(this, i18n("No KMyMoneyFile open"));
    return;
  }

  KMSTATUS(i18n("Viewing personal data..."));

  MyMoneyFile* file = MyMoneyFile::instance();
  MyMoneyPayee user = file->user();

  QPointer<KNewFileDlg> newFileDlg = new KNewFileDlg(user.name(), user.address(),
    user.city(), user.state(), user.postcode(), user.telephone(),
    user.email(), this, "NewFileDlg", i18n("Edit Personal Data"));

  if (newFileDlg->exec() == QDialog::Accepted)
  {
    user.setName(newFileDlg->userNameText);
    user.setAddress(newFileDlg->userStreetText);
    user.setCity(newFileDlg->userTownText);
    user.setState(newFileDlg->userCountyText);
    user.setPostcode(newFileDlg->userPostcodeText);
    user.setTelephone(newFileDlg->userTelephoneText);
    user.setEmail(newFileDlg->userEmailText);
    MyMoneyFileTransaction ft;
    try {
      file->setUser(user);
      ft.commit();
    } catch(MyMoneyException *e) {
      KMessageBox::information(this, i18n("Unable to store user information: %1",e->what()));
      delete e;
    }
  }
  delete newFileDlg;
}

void KMyMoney2App::slotFileFileInfo(void)
{
  if ( !d->m_myMoneyView->fileOpen() ) {
    KMessageBox::information(this, i18n("No KMyMoneyFile open"));
    return;
  }

  QFile g( "kmymoney2.dump" );
  g.open( QIODevice::WriteOnly );
  QDataStream st(&g);
  MyMoneyStorageDump dumper;
  dumper.writeStream(st, dynamic_cast<IMyMoneySerialize*> (MyMoneyFile::instance()->storage()));
  g.close();
}

void KMyMoney2App::slotLoadAccountTemplates(void)
{
  KMSTATUS(i18n("Importing account templates."));

  int rc;
  QPointer<KLoadTemplateDlg> dlg = new KLoadTemplateDlg();
  if((rc = dlg->exec()) == QDialog::Accepted) {
    MyMoneyFileTransaction ft;
    try {
    // import the account templates
      Q3ValueList<MyMoneyTemplate> templates = dlg->templates();
      Q3ValueList<MyMoneyTemplate>::iterator it_t;
      for(it_t = templates.begin(); it_t != templates.end(); ++it_t) {
        (*it_t).importTemplate(&progressCallback);
      }
      ft.commit();
    } catch(MyMoneyException* e) {
      KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to import template(s): %1, thrown in %2:%3",e->what(),e->file(),e->line()));
      delete e;
    }
  }
  delete dlg;
}

void KMyMoney2App::slotSaveAccountTemplates(void)
{
  KMSTATUS(i18n("Exporting account templates."));

  QString newName = KFileDialog::getSaveFileName(KGlobalSettings::documentPath(),
                                               i18n("*.kmt|KMyMoney template files\n"
                                               "*.*|All files"), this, i18n("Save as..."));
  //
  // If there is no file extension, then append a .kmt at the end of the file name.
  // If there is a file extension, make sure it is .kmt, delete any others.
  //
  if(!newName.isEmpty())
  {
    // find last . delimiter
    int nLoc = newName.lastIndexOf('.');
    if(nLoc != -1)
    {
      QString strExt, strTemp;
      strTemp = newName.left(nLoc + 1);
      strExt = newName.right(newName.length() - (nLoc + 1));
      if((strExt.indexOf("kmt", 0, Qt::CaseInsensitive) == -1))
      {
        strTemp.append("kmt");
        //append to make complete file name
        newName = strTemp;
      }
    }
    else
    {
      newName.append(".kmt");
    }

    if(okToWriteFile(newName)) {
      MyMoneyTemplate templ;
      templ.exportTemplate(&progressCallback);
      templ.saveTemplate(newName);
    }
  }
}

void KMyMoney2App::slotQifImport(void)
{
  if(d->m_qifReader == 0) {
    // FIXME: the menu entry for qif import should be disabled here

    QPointer<KImportDlg> dlg = new KImportDlg(0);

    if(dlg->exec()) {
      KMSTATUS(i18n("Importing file..."));
      d->m_qifReader = new MyMoneyQifReader;

      // remove all kmm-statement-#.txt files
      d->unlinkStatementXML();

      connect(d->m_qifReader, SIGNAL(importFinished()), this, SLOT(slotQifImportFinished()));

      d->m_qifReader->setURL(dlg->filename());

      d->m_qifReader->setProfile(dlg->profile());
      d->m_qifReader->setCategoryMapping(dlg->m_typeComboBox->currentIndex() == 0);
      d->m_qifReader->setProgressCallback(&progressCallback);

      // disable all standard widgets during the import
      setEnabled(false);

      d->m_ft = new MyMoneyFileTransaction();
      d->m_collectingStatements = true;
      d->m_statementResults.clear();
      d->m_qifReader->startImport();
    }
    delete dlg;

    slotUpdateActions();
  }
}

void KMyMoney2App::slotQifImportFinished(void)
{
  if(d->m_qifReader != 0) {
    d->m_qifReader->finishImport();
    d->m_ft->commit();
    d->m_collectingStatements = false;

    KMessageBox::informationList(this, i18n("The statements have been processed with the following results:"), d->m_statementResults, i18n("Statement stats"));

#if 0
    // fixme: re-enable the QIF import menu options
    if(d->m_qifReader->finishImport()) {
      if(verifyImportedData(d->m_qifReader->account())) {
        // keep the new data set, destroy the backup copy
        delete d->m_engineBackup;
        d->m_engineBackup = 0;
      }
    }

    if(d->m_engineBackup != 0) {
      // user cancelled, destroy the updated set and keep the backup copy
      IMyMoneyStorage* data = file->storage();


      if(data != 0) {
        file->detachStorage(data);
        delete data;
      }
      file->attachStorage(d->m_engineBackup);
      d->m_engineBackup = 0;

    }
#endif

    // update the views as they might still contain invalid data
    // from the import session. The same applies for the window caption
    d->m_myMoneyView->slotRefreshViews();
    updateCaption();

    delete d->m_qifReader;
    d->m_qifReader = 0;
  }
  delete d->m_ft;
  d->m_ft = 0;

  slotStatusProgressBar(-1, -1);
  ready();

  // re-enable all standard widgets
  setEnabled(true);
  slotUpdateActions();
}

void KMyMoney2App::slotGncImport(void)
{
  if (d->m_myMoneyView->fileOpen()) {
    switch (KMessageBox::questionYesNoCancel (0,
          i18n("You cannot import GnuCash data into an existing file. Do you wish to save this file?"), PACKAGE)) {
    case KMessageBox::Yes:
      slotFileSave();
      break;
    case KMessageBox::No:
      d->m_myMoneyView->closeFile();
      d->m_fileName = KUrl();
      break;
    default:
      return;
    }
  }

  KMSTATUS(i18n("Importing a Gnucash file."));

  QPointer<KFileDialog> dialog = new KFileDialog(KUrl(KGlobalSettings::documentPath()),
                            i18n(" * |Gnucash files\n * |All files (*.*)"),
                            this);
  dialog->setMode(KFile::File | KFile::ExistingOnly);

  if(dialog->exec() == QDialog::Accepted) {
//    slotFileClose();
//    if(d->myMoneyView->fileOpen())
//      return;

    // call the importer
    d->m_myMoneyView->readFile(dialog->selectedUrl());
    // imported files don't have a name
    d->m_fileName = KUrl();

    updateCaption();
    emit fileLoaded(d->m_fileName);
  }
  delete dialog;

}

void KMyMoney2App::slotAccountChart(void)
{
  if(!d->m_selectedAccount.id().isEmpty()) {
    QPointer<KBalanceChartDlg> dlg = new KBalanceChartDlg(d->m_selectedAccount, this);
    dlg->exec();
    delete dlg;
  }
}


//
// KMyMoney2App::slotStatementImport() is for testing only.  The MyMoneyStatement
// is not intended to be exposed to users in XML form.
//

void KMyMoney2App::slotStatementImport(void)
{
  bool result = false;
  KMSTATUS(i18n("Importing an XML Statement."));

  QPointer<KFileDialog> dialog = new KFileDialog(KUrl(KGlobalSettings::documentPath()),
                            i18n("*.xml|XML files\n *.*|All files (*.*)"),
                            this);
  dialog->setMode(KFile::File | KFile::ExistingOnly);

  if(dialog->exec() == QDialog::Accepted)
  {
    result = slotStatementImport(dialog->selectedUrl().path());

/*    QFile f( dialog->selectedURL().path() );
    f.open( QIODevice::ReadOnly );
    QString error = "Unable to parse file";
    QDomDocument* doc = new QDomDocument;
    if(doc->setContent(&f, FALSE))
    {
      if ( doc->doctype().name() == "KMYMONEY-STATEMENT" )
      {
        QDomElement rootElement = doc->documentElement();
        if(!rootElement.isNull())
        {
          QDomNode child = rootElement.firstChild();
          if(!child.isNull() && child.isElement())
          {
            MyMoneyStatement s;
            if ( s.read(child.toElement()) )
              result = slotStatementImport(s);
            else
              error = "File does not contain any statements";
          }
        }
      }
      else
        error = "File is not a KMyMoney Statement";
    }
    delete doc;

    if ( !result )
    {
      QMessageBox::critical( this, i18n("Critical Error"), i18n("Unable to read file %1: %2").arg( dialog->selectedURL().path()).arg(error), QMessageBox::Ok, 0 );

    }*/
  }
  delete dialog;

  if ( !result )
  {
    // re-enable all standard widgets
    setEnabled(true);
  }
}

bool KMyMoney2App::slotStatementImport(const QString& url)
{
  bool result = false;
  MyMoneyStatement s;
  if ( MyMoneyStatement::readXMLFile( s, url ) )
    result = slotStatementImport(s);
  else
    KMessageBox::error(this, i18n("Error importing %1: This file is not a valid KMM statement file.",url), i18n("Invalid Statement"));

  return result;
}

bool KMyMoney2App::slotStatementImport(const MyMoneyStatement& s)
{
  bool result = false;

  // keep a copy of the statement
  MyMoneyStatement::writeXMLFile(s, QString("/home/thb/kmm-statement-%1.txt").arg(d->m_statementXMLindex++));

  // we use an object on the heap here, so that we can check the presence
  // of it during slotUpdateActions() by looking at the pointer.
  d->m_smtReader = new MyMoneyStatementReader;
  d->m_smtReader->setAutoCreatePayee(true);
  d->m_smtReader->setProgressCallback(&progressCallback);

  // disable all standard widgets during the import
  setEnabled(false);

  QStringList messages;
  result = d->m_smtReader->import(s, messages);

  // get rid of the statement reader and tell everyone else
  // about the destruction by setting the pointer to zero
  delete d->m_smtReader;
  d->m_smtReader = 0;

  slotStatusProgressBar(-1, -1);
  ready();

  // re-enable all standard widgets
  setEnabled(true);

  if(!d->m_collectingStatements)
    KMessageBox::informationList(this, i18n("The statement has been processed with the following results:"), messages, i18n("Statement stats"));
  else
    d->m_statementResults += messages;

  return result;
}

void KMyMoney2App::slotQifExport(void)
{
  KMSTATUS(i18n("Exporting file..."));

  QPointer<KExportDlg> dlg = new KExportDlg(0);

  if(dlg->exec()) {
    if(okToWriteFile(dlg->filename())) {
      MyMoneyQifWriter writer;
      connect(&writer, SIGNAL(signalProgress(int, int)), this, SLOT(slotStatusProgressBar(int, int)));

      writer.write(dlg->filename(), dlg->profile(), dlg->accountId(),
            dlg->accountSelected(), dlg->categorySelected(),
            dlg->startDate(), dlg->endDate());
    }
  }
  delete dlg;
}

bool KMyMoney2App::okToWriteFile(const KUrl& url)
{
  // check if the file exists and warn the user
  bool reallySaveFile = true;

  if(KIO::NetAccess::exists(url, true, this)) {
    if(KMessageBox::warningYesNo(this, QString("<qt>")+i18n("The file <b>%1</b> already exists. Do you really want to override it?",url.pathOrUrl())+QString("</qt>"), i18n("File already exists")) != KMessageBox::Yes)
      reallySaveFile = false;
  }
  return reallySaveFile;
}

void KMyMoney2App::slotSettings(void)
{
  // if we already have an instance of the settings dialog, then use it
  if(KConfigDialog::showDialog("KMyMoney-Settings"))
    return;

  // otherwise, we have to create it
  KConfigDialog* dlg = new KConfigDialog(this, "KMyMoney-Settings", KMyMoneyGlobalSettings::self());

  // create the pages ...
  KSettingsGeneral* generalPage = new KSettingsGeneral();
  KSettingsRegister* registerPage = new KSettingsRegister();
  KSettingsHome* homePage = new KSettingsHome();
  KSettingsSchedules* schedulesPage = new KSettingsSchedules();
  KSettingsGpg* encryptionPage = new KSettingsGpg();
  KSettingsColors* colorsPage = new KSettingsColors();
  KSettingsFonts* fontsPage = new KSettingsFonts();
  KSettingsOnlineQuotes* onlineQuotesPage = new KSettingsOnlineQuotes();
  KSettingsForecast* forecastPage = new KSettingsForecast();
  KSettingsPlugins* pluginsPage = new KSettingsPlugins();

  dlg->addPage(generalPage, i18nc("General settings", "General"), "system-run");
  dlg->addPage(registerPage, i18nc("Register view settings", "Register"), "ledger");
  dlg->addPage(homePage, i18n("Home"), "go-home");
  dlg->addPage(schedulesPage, i18n("Scheduled\ntransactions"), "view-pim-calendar");
  dlg->addPage(encryptionPage, i18n("Encryption"), "kgpg");
  dlg->addPage(colorsPage, i18n("Colors"), "preferences-desktop-color");
  dlg->addPage(fontsPage, i18n("Fonts"), "preferences-desktop-font");
  dlg->addPage(onlineQuotesPage, i18n("Online Quotes"), "preferences-system-network");
  dlg->addPage(forecastPage, i18nc("Forecast settings", "Forecast"), "forcast");
  dlg->addPage(pluginsPage, i18n("Plugins"), "network-disconnect");

  connect(dlg, SIGNAL(settingsChanged(const QString&)), this, SLOT(slotUpdateConfiguration()));
  connect(dlg, SIGNAL(okClicked()), pluginsPage, SLOT(slotSavePlugins()));
  connect(dlg, SIGNAL(defaultClicked()), pluginsPage, SLOT(slotDefaultsPlugins()));

  dlg->show();
}

void KMyMoney2App::slotUpdateConfiguration(void)
{
  MyMoneyTransactionFilter::setFiscalYearStart(KMyMoneyGlobalSettings::firstFiscalMonth(), KMyMoneyGlobalSettings::firstFiscalDay());

  d->m_myMoneyView->slotRefreshViews();

  // re-read autosave configuration
  d->m_autoSaveEnabled = KMyMoneyGlobalSettings::autoSaveFile();
  d->m_autoSavePeriod = KMyMoneyGlobalSettings::autoSavePeriod();

  // stop timer if turned off but running
  if(d->m_autoSaveTimer->isActive() && !d->m_autoSaveEnabled) {
    d->m_autoSaveTimer->stop();
  }
  // start timer if turned on and needed but not running
  if(!d->m_autoSaveTimer->isActive() && d->m_autoSaveEnabled && d->m_myMoneyView->dirty()) {
    d->m_autoSaveTimer->setSingleShot(true);
    d->m_autoSaveTimer->start(d->m_autoSavePeriod * 60 * 1000);
  }
}

/** Init wizard dialog */
bool KMyMoney2App::initWizard(void)
{
  KStartDlg start;
  if (start.exec()) {
    slotFileClose();
    if (start.isNewFile()) {
      slotFileNew();
    } else if (start.isOpenFile()) {
      KUrl url;
      url = start.getURL();

      d->m_fileName = url.url();
      slotFileOpenRecent(url);
    } else { // Wizard / Template
      d->m_fileName = start.getURL();
    }

    //save off directory as the last one used.
    if(d->m_fileName.isLocalFile() && d->m_fileName.hasPath())
    {
      writeLastUsedDir(d->m_fileName.toLocalFile(KUrl::LeaveTrailingSlash));
      writeLastUsedFile(d->m_fileName.toLocalFile(KUrl::LeaveTrailingSlash));
    }

    return true;

  } else {
    // cancel clicked so post an exit call
    return false;
  }
}

/** No descriptions */
void KMyMoney2App::slotFileBackup(void)
{
  // Save the file first so isLocalFile() works
  if (d->m_myMoneyView && d->m_myMoneyView->dirty())

  {
    if (KMessageBox::questionYesNo(this, i18n("The file must be saved first "
        "before it can be backed up.  Do you want to continue?")) == KMessageBox::No)
    {
      return;

    }

    slotFileSave();
  }



  if ( d->m_fileName.isEmpty() )
      return;

  if(!d->m_fileName.isLocalFile()) {
    KMessageBox::sorry(this,
                       i18n("The current implementation of the backup functionality only supports local files as source files! Your current source file is '%1'.",d->m_fileName.url()),

                       i18n("Local files only"));
    return;
  }

  QPointer<KBackupDlg> backupDlg = new KBackupDlg(this);
  int returncode = backupDlg->exec();
  if(returncode)
  {

    d->m_backupMount = backupDlg->mountCheckBox->isChecked();
    d->m_proc.clearProgram();
    d->m_backupState = BACKUP_MOUNTING;
    d->m_mountpoint = backupDlg->txtMountPoint->text();

    if (d->m_backupMount) {
      progressCallback(0, 300, i18n("Mounting %1",d->m_mountpoint));
      d->m_proc.setProgram("mount");
      d->m_proc << d->m_mountpoint;
      d->m_proc.start();

    } else {
      // If we don't have to mount a device, we just issue
      // a dummy command to start the copy operation
      progressCallback(0, 300, "");
      d->m_proc.setProgram("echo");
      d->m_proc.start();
    }

  }

  delete backupDlg;
}


/** No descriptions */
void KMyMoney2App::slotProcessExited(void)
{
  switch(d->m_backupState) {
    case BACKUP_MOUNTING:

      if(d->m_proc.exitStatus() == QProcess::NormalExit && d->m_proc.exitCode() == 0) {
        d->m_proc.clearProgram();
        QString today;
        today.sprintf("-%04d-%02d-%02d.kmy",
          QDate::currentDate().year(),
          QDate::currentDate().month(),
          QDate::currentDate().day());
        QString backupfile = d->m_mountpoint + '/' + d->m_fileName.fileName();
        KMyMoneyUtils::appendCorrectFileExt(backupfile, today);

        // check if file already exists and ask what to do
        d->m_backupResult = 0;
        QFile f(backupfile);
        if (f.exists()) {
          int answer = KMessageBox::warningContinueCancel(this, i18n("Backup file for today exists on that device.  Replace ?"), i18n("Backup"), KGuiItem(i18n("&Replace")));
          if (answer == KMessageBox::Cancel) {
            d->m_backupResult = 1;

            if (d->m_backupMount) {
              progressCallback(250, 0, i18n("Unmounting %1",d->m_mountpoint));
              d->m_proc.clearProgram();
              d->m_proc.setProgram("umount");
              d->m_proc << d->m_mountpoint;
              d->m_backupState = BACKUP_UNMOUNTING;
              d->m_proc.start();
            } else {
              d->m_backupState = BACKUP_IDLE;
              progressCallback(-1, -1, QString());
              ready();
            }
          }
        }

        if(d->m_backupResult == 0) {
          progressCallback(50, 0, i18n("Writing %1",backupfile));
//FIXME: FIX on windows
          d->m_proc << "cp" << "-f" << d->m_fileName.path(KUrl::LeaveTrailingSlash) << backupfile;
          d->m_backupState = BACKUP_COPYING;
          d->m_proc.start();
        }

      } else {
        KMessageBox::information(this, i18n("Error mounting device"), i18n("Backup"));
        d->m_backupResult = 1;
        if (d->m_backupMount) {
          progressCallback(250, 0, i18n("Unmounting %1",d->m_mountpoint));
          d->m_proc.clearProgram();
          d->m_proc.setProgram("umount");
          d->m_proc << d->m_mountpoint;
          d->m_backupState = BACKUP_UNMOUNTING;
          d->m_proc.start();

        } else {
          d->m_backupState = BACKUP_IDLE;
          progressCallback(-1, -1, QString());
          ready();
        }
      }
      break;

    case BACKUP_COPYING:
      if(d->m_proc.exitStatus() == QProcess::NormalExit && d->m_proc.exitCode() == 0) {

        if (d->m_backupMount) {
          progressCallback(250, 0, i18n("Unmounting %1",d->m_mountpoint));
          d->m_proc.clearProgram();
          d->m_proc.setProgram("umount");
          d->m_proc << d->m_mountpoint;
          d->m_backupState = BACKUP_UNMOUNTING;
          d->m_proc.start();
        } else {
          progressCallback(300, 0, i18nc("Backup done", "Done"));
          KMessageBox::information(this, i18n("File successfully backed up"), i18n("Backup"));
          d->m_backupState = BACKUP_IDLE;
          progressCallback(-1, -1, QString());
          ready();
        }
      } else {
        qDebug("cp exit code is %d", d->m_proc.exitCode());
        d->m_backupResult = 1;
        KMessageBox::information(this, i18n("Error copying file to device"), i18n("Backup"));

        if (d->m_backupMount) {
          progressCallback(250, 0, i18n("Unmounting %1",d->m_mountpoint));
          d->m_proc.clearProgram();
          d->m_proc.setProgram("umount");
          d->m_proc << d->m_mountpoint;
          d->m_backupState = BACKUP_UNMOUNTING;
          d->m_proc.start();


        } else {
          d->m_backupState = BACKUP_IDLE;
          progressCallback(-1, -1, QString());
          ready();
        }
      }
      break;


    case BACKUP_UNMOUNTING:
      if(d->m_proc.exitStatus() == QProcess::NormalExit && d->m_proc.exitCode() == 0) {

        progressCallback(300, 0, i18nc("Backup done", "Done"));
        if(d->m_backupResult == 0)
          KMessageBox::information(this, i18n("File successfully backed up"), i18n("Backup"));
      } else {
        KMessageBox::information(this, i18n("Error unmounting device"), i18n("Backup"));
      }
      d->m_backupState = BACKUP_IDLE;
      progressCallback(-1, -1, QString());
      ready();
      break;

    default:
      qWarning("Unknown state for backup operation!");
      progressCallback(-1, -1, QString());
      ready();
      break;
  }
}

void KMyMoney2App::slotFileNewWindow(void)
{
  KMyMoney2App *newWin = new KMyMoney2App;

  newWin->show();
}

void KMyMoney2App::slotShowTipOfTheDay(void)
{
  KTipDialog::showTip(d->m_myMoneyView, "", true);
}

void KMyMoney2App::slotShowPreviousView(void)
{

}

void KMyMoney2App::slotShowNextView(void)
{

}

void KMyMoney2App::slotQifProfileEditor(void)
{
    QPointer<MyMoneyQifProfileEditor> editor = new MyMoneyQifProfileEditor(true, this );
    editor->setObjectName( "QIF Profile Editor");


  editor->exec();

  delete editor;

}

void KMyMoney2App::slotToolsStartKCalc(void)
{
  KRun::runCommand("kcalc", this);
}

void KMyMoney2App::slotFindTransaction(void)
{
  if(d->m_searchDlg == 0) {
    d->m_searchDlg = new KFindTransactionDlg();
    connect(d->m_searchDlg, SIGNAL(destroyed()), this, SLOT(slotCloseSearchDialog()));
    connect(d->m_searchDlg, SIGNAL(transactionSelected(const QString&, const QString&)),
            d->m_myMoneyView, SLOT(slotLedgerSelected(const QString&, const QString&)));
  }
  d->m_searchDlg->show();
  d->m_searchDlg->raise();
  d->m_searchDlg->activateWindow();
}

void KMyMoney2App::slotCloseSearchDialog(void)
{
  if(d->m_searchDlg)
    d->m_searchDlg->deleteLater();
  d->m_searchDlg = 0;
}

void KMyMoney2App::createInstitution(MyMoneyInstitution& institution)
{
  MyMoneyFile* file = MyMoneyFile::instance();

  MyMoneyFileTransaction ft;

  try {
    file->addInstitution(institution);
    ft.commit();

  } catch (MyMoneyException *e) {
    KMessageBox::information(this, i18n("Cannot add institution: %1",e->what()));
    delete e;
  }
}

void KMyMoney2App::slotInstitutionNew(void)
{
  MyMoneyInstitution institution;
  slotInstitutionNew(institution);
}

void KMyMoney2App::slotInstitutionNew(MyMoneyInstitution& institution)
{
  institution.clearId();
  QPointer<KNewBankDlg> dlg = new KNewBankDlg(institution);
  if (dlg->exec()) {
    institution = dlg->institution();
    createInstitution(institution);
  }
  delete dlg;
}

void KMyMoney2App::slotInstitutionEdit(const MyMoneyObject& obj)
{
  if(typeid(obj) != typeid(MyMoneyInstitution))
    return;

  try {
    MyMoneyFile* file = MyMoneyFile::instance();

    //grab a pointer to the view, regardless of it being a account or institution view.
    MyMoneyInstitution institution = file->institution(d->m_selectedInstitution.id());

    // bankSuccess is not checked anymore because d->m_file->institution will throw anyway
    QPointer<KNewBankDlg> dlg = new KNewBankDlg(institution);
    if (dlg->exec()) {
      MyMoneyFileTransaction ft;
      try {
        file->modifyInstitution(dlg->institution());
        ft.commit();
        slotSelectInstitution(file->institution(dlg->institution().id()));

      } catch(MyMoneyException *e) {
        KMessageBox::information(this, i18n("Unable to store institution: %1",e->what()));
        delete e;
      }
    }
    delete dlg;

  } catch(MyMoneyException *e) {
    if(!obj.id().isEmpty())
      KMessageBox::information(this, i18n("Unable to edit institution: %1",e->what()));
    delete e;
  }
}

void KMyMoney2App::slotInstitutionDelete(void)
{
  MyMoneyFile *file = MyMoneyFile::instance();
  try {

    MyMoneyInstitution institution = file->institution(d->m_selectedInstitution.id());
    if ((KMessageBox::questionYesNo(this, QString("<p>")+i18n("Do you really want to delete institution <b>%1</b> ?",institution.name()))) == KMessageBox::No)
      return;
    MyMoneyFileTransaction ft;

    try {
      file->removeInstitution(institution);
      ft.commit();
    } catch (MyMoneyException *e) {
      KMessageBox::information(this, i18n("Unable to delete institution: %1",e->what()));
      delete e;
    }
  } catch (MyMoneyException *e) {
    KMessageBox::information(this, i18n("Unable to delete institution: %1",e->what()));
    delete e;
  }
}

const MyMoneyAccount& KMyMoney2App::findAccount(const MyMoneyAccount& acc, const MyMoneyAccount& parent) const
{
  static MyMoneyAccount nullAccount;

  MyMoneyFile* file = MyMoneyFile::instance();
  QList<MyMoneyAccount> parents;
  try {
    // search by id
    if(!acc.id().isEmpty()) {
      return file->account(acc.id());
    }
    // collect the parents. in case parent does not have an id, we scan the all top-level accounts
    if(parent.id().isEmpty()) {
      parents << file->asset();
      parents << file->liability();
      parents << file->income();
      parents << file->expense();
      parents << file->equity();
    } else {
      parents << parent;
    }
    QList<MyMoneyAccount>::const_iterator it_p;
    for(it_p = parents.constBegin(); it_p != parents.constEnd(); ++it_p) {
      MyMoneyAccount parentAccount = *it_p;
      // search by name (allow hierarchy)
      int pos;
      // check for ':' in the name and use it as separator for a hierarchy
      QString name = acc.name();
      while((pos = name.indexOf(MyMoneyFile::AccountSeperator)) != -1) {
        QString part = name.left(pos);
        QString remainder = name.mid(pos+1);
        const MyMoneyAccount& existingAccount = file->subAccountByName(parentAccount, part);
        if(existingAccount.id().isEmpty()) {
          return existingAccount;
        }
        parentAccount = existingAccount;
        name = remainder;
      }
      const MyMoneyAccount& existingAccount = file->subAccountByName(parentAccount, name);
      if(!existingAccount.id().isEmpty()) {
        if(acc.accountType() != MyMoneyAccount::UnknownAccountType) {
          if(acc.accountType() != existingAccount.accountType())
            continue;
        }
        return existingAccount;
      }
    }
  } catch (MyMoneyException *e) {
    KMessageBox::error(0, i18n("Unable to find account: %1",e->what()));
    delete e;
  }
  return nullAccount;
}

void KMyMoney2App::createAccount(MyMoneyAccount& newAccount, MyMoneyAccount& parentAccount, MyMoneyAccount& brokerageAccount, MyMoneyMoney openingBal)
{
  MyMoneyFile* file = MyMoneyFile::instance();

  // make sure we have a currency. If none is assigned, we assume base currency
  if(newAccount.currencyId().isEmpty())
    newAccount.setCurrencyId(file->baseCurrency().id());

  MyMoneyFileTransaction ft;
  try
  {
    int pos;
    // check for ':' in the name and use it as separator for a hierarchy
    while((pos = newAccount.name().indexOf(MyMoneyFile::AccountSeperator)) != -1) {
      QString part = newAccount.name().left(pos);
      QString remainder = newAccount.name().mid(pos+1);
      const MyMoneyAccount& existingAccount = file->subAccountByName(parentAccount, part);
      if(existingAccount.id().isEmpty()) {
        newAccount.setName(part);

        file->addAccount(newAccount, parentAccount);
        parentAccount = newAccount;
      } else {
        parentAccount = existingAccount;
      }
      newAccount.setParentAccountId(QString());  // make sure, there's no parent
      newAccount.clearId();                       // and no id set for adding
      newAccount.removeAccountIds();              // and no sub-account ids
      newAccount.setName(remainder);
    }

    const MyMoneySecurity& sec = file->security(newAccount.currencyId());
    // Check the opening balance
    if (openingBal.isPositive() && newAccount.accountGroup() == MyMoneyAccount::Liability)
    {
      QString message = i18n("This account is a liability and if the "
          "opening balance represents money owed, then it should be negative.  "
          "Negate the amount?\n\n"
          "Please click Yes to change the opening balance to %1,\n"
          "Please click No to leave the amount as %2,\n"
          "Please click Cancel to abort the account creation."
          , (-openingBal).formatMoney(newAccount, sec)
          , openingBal.formatMoney(newAccount, sec));

      int ans = KMessageBox::questionYesNoCancel(this, message);
      if (ans == KMessageBox::Yes) {
        openingBal = -openingBal;

      } else if (ans == KMessageBox::Cancel)
        return;
    }

    file->addAccount(newAccount, parentAccount);

    // in case of a loan account, we add the initial payment
    if((newAccount.accountType() == MyMoneyAccount::Loan
    || newAccount.accountType() == MyMoneyAccount::AssetLoan)
    && !newAccount.value("kmm-loan-payment-acc").isEmpty()
    && !newAccount.value("kmm-loan-payment-date").isEmpty()) {
      MyMoneyAccountLoan acc(newAccount);
      MyMoneyTransaction t;
      MyMoneySplit a, b;
      a.setAccountId(acc.id());
      b.setAccountId(acc.value("kmm-loan-payment-acc").toLatin1());
      a.setValue(acc.loanAmount());
      if(acc.accountType() == MyMoneyAccount::Loan)
        a.setValue(-a.value());

      a.setShares(a.value());
      b.setValue(-a.value());
      b.setShares(b.value());
      a.setMemo(i18n("Loan payout"));
      b.setMemo(i18n("Loan payout"));
      t.setPostDate(QDate::fromString(acc.value("kmm-loan-payment-date"), Qt::ISODate));
      newAccount.deletePair("kmm-loan-payment-acc");
      newAccount.deletePair("kmm-loan-payment-date");
      MyMoneyFile::instance()->modifyAccount(newAccount);

      t.addSplit(a);
      t.addSplit(b);
      file->addTransaction(t);
      file->createOpeningBalanceTransaction(newAccount, openingBal);

    // in case of an investment account we check if we should create
    // a brokerage account
    } else if(newAccount.accountType() == MyMoneyAccount::Investment
            && !brokerageAccount.name().isEmpty()) {
      file->addAccount(brokerageAccount, parentAccount);

      // set a link from the investment account to the brokerage account
      file->modifyAccount(newAccount);
      file->createOpeningBalanceTransaction(brokerageAccount, openingBal);

    } else
      file->createOpeningBalanceTransaction(newAccount, openingBal);

    ft.commit();
  }
  catch (MyMoneyException *e)
  {
    KMessageBox::information(this, i18n("Unable to add account: %1",e->what()));
    delete e;
  }
}

void KMyMoney2App::slotCategoryNew(const QString& name, QString& id)
{
  MyMoneyAccount account;
  account.setName(name);

  slotCategoryNew(account, MyMoneyFile::instance()->expense());

  id = account.id();
}

void KMyMoney2App::slotCategoryNew(MyMoneyAccount& account, const MyMoneyAccount& parent)
{
  if(KMessageBox::questionYesNo(this,
    QString("<qt>%1</qt>").arg(i18n("<p>The category <b>%1</b> currently does not exist. Do you want to create it?</p><p><i>The parent account will default to <b>%2</b> but can be changed in the following dialog</i>.</p>",account.name(),parent.name())), i18n("Create category"),
    KStandardGuiItem::yes(), KStandardGuiItem::no(), "CreateNewCategories") == KMessageBox::Yes) {
    createCategory(account, parent);
  }
}

void KMyMoney2App::slotCategoryNew(void)
{
  MyMoneyAccount parent;
  MyMoneyAccount account;

  // Preselect the parent account by looking at the current selected account/category
  if(!d->m_selectedAccount.id().isEmpty() && d->m_selectedAccount.isIncomeExpense()) {
    MyMoneyFile* file = MyMoneyFile::instance();
    try {
      parent = file->account(d->m_selectedAccount.id());
    } catch(MyMoneyException *e) {
      delete e;
    }
  }

  createCategory(account, parent);
}

void KMyMoney2App::createCategory(MyMoneyAccount& account, const MyMoneyAccount& parent)
{
  if(!parent.id().isEmpty()) {
    try {
      // make sure parent account exists
      MyMoneyFile::instance()->account(parent.id());
      account.setParentAccountId(parent.id());
      account.setAccountType( parent.accountType() );
    } catch(MyMoneyException *e) {
      delete e;
    }
  }

  QPointer<KNewAccountDlg> dialog =
    new KNewAccountDlg(account, false, true, 0, 0, i18n("Create a new Category"));

  if(dialog->exec() == QDialog::Accepted) {
    MyMoneyAccount parentAccount, brokerageAccount;
    account = dialog->account();
    parentAccount = dialog->parentAccount();

    createAccount(account, parentAccount, brokerageAccount, MyMoneyMoney(0,1));
  }
  delete dialog;
}

void KMyMoney2App::slotAccountNew(void)
{
  MyMoneyAccount acc;
  acc.setOpeningDate(QDate::currentDate());

  slotAccountNew(acc);
}

void KMyMoney2App::slotAccountNew(MyMoneyAccount& account)
{
  NewAccountWizard::Wizard* wizard = new NewAccountWizard::Wizard();
  connect(wizard, SIGNAL(createInstitution(MyMoneyInstitution&)), this, SLOT(slotInstitutionNew(MyMoneyInstitution&)));
  connect(wizard, SIGNAL(createAccount(MyMoneyAccount&)), this, SLOT(slotAccountNew(MyMoneyAccount&)));
  connect(wizard, SIGNAL(createPayee(const QString&, QString&)), this, SLOT(slotPayeeNew(const QString&, QString&)));
  connect(wizard, SIGNAL(createCategory(MyMoneyAccount&, const MyMoneyAccount&)), this, SLOT(slotCategoryNew(MyMoneyAccount&, const MyMoneyAccount&)));

  wizard->setAccount(account);

  if(wizard->exec() == QDialog::Accepted) {
    MyMoneyAccount acc = wizard->account();
    if(!(acc == MyMoneyAccount())) {
      MyMoneyFileTransaction ft;
      MyMoneyFile* file = MyMoneyFile::instance();
      try {
        // create the account
        MyMoneyAccount parent = wizard->parentAccount();
        file->addAccount(acc, parent);

        // tell the wizard about the account id which it
        // needs to create a possible schedule and transactions
        wizard->setAccount(acc);

        // store a possible conversion rate for the currency
        if(acc.currencyId() != file->baseCurrency().id()) {
          file->addPrice(wizard->conversionRate());
        }

        // create the opening balance transaction if any
        file->createOpeningBalanceTransaction(acc, wizard->openingBalance());
        // create the payout transaction for loans if any
        MyMoneyTransaction payoutTransaction = wizard->payoutTransaction();
        if(payoutTransaction.splits().count() > 0) {
          file->addTransaction(payoutTransaction);
        }

        // create a brokerage account if selected
        MyMoneyAccount brokerageAccount = wizard->brokerageAccount();
        if(!(brokerageAccount == MyMoneyAccount())) {
          file->addAccount(brokerageAccount, parent);
        }

        // create a possible schedule
        MyMoneySchedule sch = wizard->schedule();
        if(!(sch == MyMoneySchedule())) {
          MyMoneyFile::instance()->addSchedule(sch);
          if(acc.isLoan()) {
            MyMoneyAccountLoan accLoan = MyMoneyFile::instance()->account(acc.id());
            accLoan.setSchedule(sch.id());
            acc = accLoan;
            MyMoneyFile::instance()->modifyAccount(acc);
          }
        }
        ft.commit();
        account = acc;
      } catch (MyMoneyException *e) {
        KMessageBox::error(this, i18n("Unable to create account: %1",e->what()));
      }
    }
  }
  delete wizard;
}

void KMyMoney2App::slotInvestmentNew(MyMoneyAccount& account, const MyMoneyAccount& parent)
{
  QString dontShowAgain = "CreateNewInvestments";
  if(KMessageBox::questionYesNo(this,
    QString("<qt>")+i18n("The security <b>%1</b> currently does not exist as sub-account of <b>%2</b>. "
          "Do you want to create it?",account.name(),parent.name())+QString("</qt>"), i18n("Create security"),
    KStandardGuiItem::yes(), KStandardGuiItem::no(), dontShowAgain) == KMessageBox::Yes) {
    KNewInvestmentWizard dlg;
    dlg.setName(account.name());
    if(dlg.exec() == QDialog::Accepted) {
      dlg.createObjects(parent.id());
      account = dlg.account();
    }
  } else {
    // in case the user said no but turned on the don't show again selection, we will enable
    // the message no matter what. Otherwise, the user is not able to use this feature
    // in the future anymore.
    KMessageBox::enableMessage(dontShowAgain);
  }
}

void KMyMoney2App::slotInvestmentNew(void)
{
  KNewInvestmentWizard dlg;
  if(dlg.exec() == QDialog::Accepted) {
    dlg.createObjects(d->m_selectedAccount.id());
  }
}

void KMyMoney2App::slotInvestmentEdit(void)
{
  KNewInvestmentWizard dlg(d->m_selectedInvestment);
  if(dlg.exec() == QDialog::Accepted) {
    dlg.createObjects(d->m_selectedAccount.id());
  }
}

void KMyMoney2App::slotInvestmentDelete(void)
{
  if(KMessageBox::questionYesNo(this, QString("<p>")+i18n("Do you really want to delete the investment <b>%1</b>?",d->m_selectedInvestment.name()), i18n("Delete investment"), KStandardGuiItem::yes(), KStandardGuiItem::no(), "DeleteInvestment") == KMessageBox::Yes) {
    MyMoneyFile* file = MyMoneyFile::instance();
    MyMoneyFileTransaction ft;
    try {
      file->removeAccount(d->m_selectedInvestment);
      ft.commit();
    } catch(MyMoneyException *e) {
      KMessageBox::information(this, i18n("Unable to delete investment: %1",e->what()));
      delete e;
    }
  }
}

void KMyMoney2App::slotOnlinePriceUpdate(void)
{
  if(!d->m_selectedInvestment.id().isEmpty()) {
    QPointer<KEquityPriceUpdateDlg> dlg =
      new KEquityPriceUpdateDlg(0, d->m_selectedInvestment.currencyId());
    if(dlg->exec() == QDialog::Accepted)
      dlg->storePrices();

    delete dlg;
  }
}

void KMyMoney2App::slotManualPriceUpdate(void)
{
  if(!d->m_selectedInvestment.id().isEmpty()) {
    try {
      MyMoneySecurity security = MyMoneyFile::instance()->security(d->m_selectedInvestment.currencyId());
      MyMoneySecurity currency = MyMoneyFile::instance()->security(security.tradingCurrency());
      MyMoneyPrice price = MyMoneyFile::instance()->price(security.id(), currency.id());
      signed64 fract = MyMoneyMoney::precToDenom(KMyMoneyGlobalSettings::pricePrecision());

      QPointer<KCurrencyCalculator> calc =
        new KCurrencyCalculator(security, currency, MyMoneyMoney(1,1), price.rate(currency.id()), price.date(), fract);
      calc->setupPriceEditor();

      // The dialog takes care of adding the price if necessary
      calc->exec();
    } catch(MyMoneyException* e) {
      qDebug("Error in price update: %s", qPrintable(e->what()));
      delete e;
    }
  }
}

void KMyMoney2App::createSchedule(MyMoneySchedule newSchedule, MyMoneyAccount& newAccount)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  // Add the schedule only if one exists
  //
  // Remember to modify the first split to reference the newly created account
  if (!newSchedule.name().isEmpty())
  {
    try
    {
      // We assume at least 2 splits in the transaction
      MyMoneyTransaction t = newSchedule.transaction();
      if(t.splitCount() < 2) {
        throw new MYMONEYEXCEPTION("Transaction for schedule has less than 2 splits!");
      }
#if 0
      // now search the split that does not have an account reference
      // and set it up to be the one of the account we just added
      // to the account pool. Note: the schedule code used to leave
      // this always the first split, but the loan code leaves it as
      // the second one. So I thought, searching is a good alternative ....
      Q3ValueList<MyMoneySplit>::ConstIterator it_s;
      for(it_s = t.splits().begin(); it_s != t.splits().end(); ++it_s) {
        if((*it_s).accountId().isEmpty()) {
          MyMoneySplit s = (*it_s);
          s.setAccountId(newAccount.id());
          t.modifySplit(s);
          break;
        }
      }
      newSchedule.setTransaction(t);
#endif

      MyMoneyFileTransaction ft;
      try {
        file->addSchedule(newSchedule);

        // in case of a loan account, we keep a reference to this
        // schedule in the account
        if(newAccount.accountType() == MyMoneyAccount::Loan
        || newAccount.accountType() == MyMoneyAccount::AssetLoan) {
          newAccount.setValue("schedule", newSchedule.id());
          file->modifyAccount(newAccount);
        }
        ft.commit();
      } catch (MyMoneyException *e) {
        KMessageBox::information(this, i18n("Unable to add scheduled transaction: "), e->what());
        delete e;
      }
    }
    catch (MyMoneyException *e)
    {
      KMessageBox::information(this, i18n("Unable to add scheduled transaction: "), e->what());
      delete e;
    }
  }
}

bool KMyMoney2App::exchangeAccountInTransaction(MyMoneyTransaction& _t, const QString& fromId, const QString& toId)
{
  bool rc = false;
  MyMoneyTransaction t(_t);
  QList<MyMoneySplit>::iterator it_s;
  for(it_s = t.splits().begin(); it_s != t.splits().end(); ++it_s) {
    if((*it_s).accountId() == fromId) {
      (*it_s).setAccountId(toId);
      _t.modifySplit(*it_s);
      rc = true;
    }
  }
  return rc;
}

void KMyMoney2App::slotAccountDelete(void)
{
  if (d->m_selectedAccount.id().isEmpty())
    return;  // need an account ID

  MyMoneyFile* file = MyMoneyFile::instance();
  // can't delete standard accounts or account which still have transactions assigned
  if (file->isStandardAccount(d->m_selectedAccount.id()))
    return;

  // check if the account is referenced by a transaction or schedule
  MyMoneyFileBitArray skip(IMyMoneyStorage::MaxRefCheckBits);
  skip.fill(false);
  skip.setBit(IMyMoneyStorage::RefCheckAccount);
  skip.setBit(IMyMoneyStorage::RefCheckInstitution);
  skip.setBit(IMyMoneyStorage::RefCheckPayee);
  skip.setBit(IMyMoneyStorage::RefCheckSecurity);
  skip.setBit(IMyMoneyStorage::RefCheckCurrency);
  skip.setBit(IMyMoneyStorage::RefCheckPrice);
  bool hasReference = file->isReferenced(d->m_selectedAccount, skip);

  // make sure we only allow transactions in a 'category' (income/expense account)
  switch(d->m_selectedAccount.accountType()) {
    case MyMoneyAccount::Income:
    case MyMoneyAccount::Expense:
      break;

    default:
      // if the account is still referenced
      if(hasReference) {
        return;
      }
      break;
  }

  // if we get here and still have transactions referencing the account, we
  // need to check with the user to possibly re-assign them to a different account
  bool needAskUser = true;
  bool exit = false;

  MyMoneyFileTransaction ft;

  if(hasReference) {
    // show transaction reassignment dialog

    needAskUser = false;
    KCategoryReassignDlg* dlg = new KCategoryReassignDlg(this);
    QString categoryId = dlg->show(d->m_selectedAccount);
    delete dlg; // and kill the dialog
    if (categoryId.isEmpty())
      return; // the user aborted the dialog, so let's abort as well

    MyMoneyAccount newCategory = file->account(categoryId);
    try {
      {
        KMSTATUS(i18n("Adjusting transactions..."));
        /*
          d->m_selectedAccount.id() is the old id, categoryId the new one
          Now search all transactions and schedules that reference d->m_selectedAccount.id()
          and replace that with categoryId.
        */
        // get the list of all transactions that reference the old account
        MyMoneyTransactionFilter filter(d->m_selectedAccount.id());
        filter.setReportAllSplits(false);
        QList<MyMoneyTransaction> tlist;
        QList<MyMoneyTransaction>::iterator it_t;
        file->transactionList(tlist, filter);

        slotStatusProgressBar(0, tlist.count());
        int cnt = 0;
        for(it_t = tlist.begin(); it_t != tlist.end(); ++it_t) {
          slotStatusProgressBar(++cnt, 0);
          MyMoneyTransaction t = (*it_t);
          if(exchangeAccountInTransaction(t, d->m_selectedAccount.id(), categoryId))
            file->modifyTransaction(t);
        }
        slotStatusProgressBar(tlist.count(), 0);
      }
      // now fix all schedules
      {
        KMSTATUS(i18n("Adjusting scheduled transactions..."));
        QList<MyMoneySchedule> slist = file->scheduleList(d->m_selectedAccount.id());
        QList<MyMoneySchedule>::iterator it_s;

        int cnt = 0;
        slotStatusProgressBar(0, slist.count());
        for(it_s = slist.begin(); it_s != slist.end(); ++it_s) {
          slotStatusProgressBar(++cnt, 0);
          MyMoneyTransaction t = (*it_s).transaction();
          if(exchangeAccountInTransaction(t, d->m_selectedAccount.id(), categoryId)) {
            (*it_s).setTransaction(t);
            file->modifySchedule(*it_s);
          }
        }
        slotStatusProgressBar(slist.count(), 0);
      }
      // now fix all budgets
      {
        KMSTATUS(i18n("Adjusting budgets..."));
        QList<MyMoneyBudget> blist = file->budgetList();
        QList<MyMoneyBudget>::const_iterator it_b;
        for(it_b = blist.constBegin(); it_b != blist.constEnd(); ++it_b) {
          if((*it_b).hasReferenceTo(d->m_selectedAccount.id())) {
            MyMoneyBudget b = (*it_b);
            MyMoneyBudget::AccountGroup fromBudget = b.account(d->m_selectedAccount.id());
            MyMoneyBudget::AccountGroup toBudget = b.account(categoryId);
            toBudget += fromBudget;
            b.setAccount(toBudget, toBudget.id());
            b.removeReference(d->m_selectedAccount.id());
            file->modifyBudget(b);

          }
        }
        slotStatusProgressBar(blist.count(), 0);
      }
    } catch(MyMoneyException  *e) {
      KMessageBox::error( this, i18n("Unable to exchange category <b>%1</b> with category <b>%2</b>. Reason: %3",d->m_selectedAccount.name(),newCategory.name(),e->what()));
      delete e;
      exit = true;
    }
    slotStatusProgressBar(-1, -1);
  }

  if(exit)
    return;

  // at this point, we must not have a reference to the account
  // to be deleted anymore
  switch(d->m_selectedAccount.accountGroup()) {
    // special handling for categories to allow deleting of empty subcategories
    case MyMoneyAccount::Income:
    case MyMoneyAccount::Expense:
    { // open a compound statement here to be able to declare variables
      // which would otherwise not work within a case label.

      // case A - only a single, unused category without subcats selected
      if (d->m_selectedAccount.accountList().isEmpty()) {
        if (!needAskUser || (KMessageBox::questionYesNo(this, QString("<qt>")+i18n("Do you really want to delete category <b>%1</b>?",d->m_selectedAccount.name())+QString("</qt>")) == KMessageBox::Yes)) {
          try {
            file->removeAccount(d->m_selectedAccount);
            d->m_selectedAccount.clearId();
            slotUpdateActions();
            ft.commit();
          } catch(MyMoneyException* e) {
            KMessageBox::error( this, QString("<qt>")+i18n("Unable to delete category <b>%1</b>. Cause: %2",d->m_selectedAccount.name(),e->what())+QString("</qt>"));
            delete e;
          }
        }
        return;
      }
      // case B - we have some subcategories, maybe the user does not want to
      //          delete them all, but just the category itself?
      MyMoneyAccount parentAccount = file->account(d->m_selectedAccount.parentAccountId());

      QStringList accountsToReparent;
      int result = KMessageBox::questionYesNoCancel(this, QString("<qt>")+
          i18n("Do you want to delete category <b>%1</b> with all its sub-categories or only "
               "the category itself? If you only delete the category itself, all its sub-categories "
               "will be made sub-categories of <b>%2</b>.",d->m_selectedAccount.name(),parentAccount.name())+QString("</qt>"),
          QString::null,
          KGuiItem(i18n("Delete all")),
          KGuiItem(i18n("Just the category")));
      if (result == KMessageBox::Cancel)
        return; // cancel pressed? ok, no delete then...
      // "No" means "Just the category" and that means we need to reparent all subaccounts
      bool need_confirmation = false;
      // case C - User only wants to delete the category itself
      if (result == KMessageBox::No)
        accountsToReparent = d->m_selectedAccount.accountList();
      else {
        // case D - User wants to delete all subcategories, now check all subcats of
        //          d->m_selectedAccount and remember all that cannot be deleted and
        //          must be "reparented"
        for (QStringList::const_iterator it = d->m_selectedAccount.accountList().begin();
          it != d->m_selectedAccount.accountList().end(); ++it)
        {
          // reparent account if a transaction is assigned
          if (file->transactionCount(*it)!=0)
            accountsToReparent.push_back(*it);
          else if (!file->account(*it).accountList().isEmpty()) {
            // or if we have at least one sub-account that is used for transactions
            if (!file->hasOnlyUnusedAccounts(file->account(*it).accountList())) {
              accountsToReparent.push_back(*it);
              //kDebug() << "subaccount not empty";
            }
          }
        }
        if (!accountsToReparent.isEmpty())
          need_confirmation = true;
      }
      if (!accountsToReparent.isEmpty() && need_confirmation) {
        if (KMessageBox::questionYesNo(this, QString("<p>")+i18n("Some sub-categories of category <b>%1</b> cannot "
          "be deleted, because they are still used. They will be made sub-categories of <b>%2</b>. Proceed?",d->m_selectedAccount.name(),parentAccount.name())) != KMessageBox::Yes) {
          return; // user gets wet feet...
        }
      }
      // all good, now first reparent selected sub-categories
      try {
        MyMoneyAccount parent = file->account(d->m_selectedAccount.parentAccountId());
        for (QStringList::const_iterator it = accountsToReparent.constBegin(); it != accountsToReparent.constEnd(); ++it) {
          MyMoneyAccount child = file->account(*it);
          file->reparentAccount(child, parent);
        }
        // reload the account because the sub-account list might have changed
        d->m_selectedAccount = file->account(d->m_selectedAccount.id());
        // now recursively delete remaining sub-categories
        file->removeAccountList(d->m_selectedAccount.accountList());
        // don't forget to update d->m_selectedAccount, because we still have a copy of
        // the old account list, which is no longer valid
        d->m_selectedAccount = file->account(d->m_selectedAccount.id());
      } catch(MyMoneyException* e) {
        KMessageBox::error( this, QString("<qt>")+i18n("Unable to delete a sub-category of category <b>%1</b>. Reason: %2",d->m_selectedAccount.name(),e->what())+QString("</qt>"));
        delete e;
        return;
      }
    }
    break; // the category/account is deleted after the switch

    default:
      if (!d->m_selectedAccount.accountList().isEmpty())
        return; // can't delete accounts which still have subaccounts

      if (KMessageBox::questionYesNo(this, QString("<p>")+i18n("Do you really want to "
          "delete account <b>%1</b>?",d->m_selectedAccount.name())) != KMessageBox::Yes) {
        return; // ok, you don't want to? why did you click then, hmm?
      }
  } // switch;

  try {
    file->removeAccount(d->m_selectedAccount);
    d->m_selectedAccount.clearId();
    slotUpdateActions();
    ft.commit();
  } catch(MyMoneyException* e) {
    KMessageBox::error( this, i18n("Unable to delete account '%1'. Cause: %2",d->m_selectedAccount.name(),e->what()));
    delete e;
  }
}

void KMyMoney2App::slotAccountEdit(void)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  if(!d->m_selectedAccount.id().isEmpty()) {
    if(!file->isStandardAccount(d->m_selectedAccount.id())) {
      if(d->m_selectedAccount.accountType() != MyMoneyAccount::Loan
      && d->m_selectedAccount.accountType() != MyMoneyAccount::AssetLoan) {
        QString caption;
        bool category = false;
        switch(MyMoneyAccount::accountGroup(d->m_selectedAccount.accountType())) {
          default:
            caption = i18n("Edit account '%1'",d->m_selectedAccount.name());
            break;

          case MyMoneyAccount::Expense:
          case MyMoneyAccount::Income:
            caption = i18n("Edit category '%1'",d->m_selectedAccount.name());
            category = true;
            break;
        }
        QString tid = file->openingBalanceTransaction(d->m_selectedAccount);
        MyMoneyTransaction t;
        MyMoneySplit s0, s1;
        QPointer<KNewAccountDlg> dlg =
          new KNewAccountDlg(d->m_selectedAccount, true, category, 0, 0, caption);

        if(category || d->m_selectedAccount.accountType() == MyMoneyAccount::Investment) {
          dlg->setOpeningBalanceShown(false);
        } else {
          if(!tid.isEmpty()) {
            try {
              t = file->transaction(tid);
              s0 = t.splitByAccount(d->m_selectedAccount.id());
              s1 = t.splitByAccount(d->m_selectedAccount.id(), false);
              dlg->setOpeningBalance(s0.shares());
              if(d->m_selectedAccount.accountGroup() == MyMoneyAccount::Liability) {
                dlg->setOpeningBalance(-s0.shares());
              }
            } catch(MyMoneyException *e) {
              kDebug(2) << "Error retrieving opening balance transaction " << tid << ": " << e->what() << "\n";
              tid = QString();
              delete e;
            }
          }
        }

        // check for online modules
        QMap<QString, KMyMoneyPlugin::OnlinePlugin *>::const_iterator it_plugin = d->m_onlinePlugins.constEnd();
        const MyMoneyKeyValueContainer& kvp = d->m_selectedAccount.onlineBankingSettings();
        if(!kvp["provider"].isEmpty()) {
          // if we have an online provider for this account, we need to check
          // that we have the corresponding plugin. If that exists, we ask it
          // to provide an additional tab for the account editor.
          it_plugin = d->m_onlinePlugins.constFind(kvp["provider"]);
          if(it_plugin != d->m_onlinePlugins.constEnd()) {
            QString name;
            QWidget *w = 0;
            w = (*it_plugin)->accountConfigTab(d->m_selectedAccount, name);
            dlg->addTab(w, name);
          }
        }

        if (dlg->exec() == QDialog::Accepted) {
          try {
            MyMoneyFileTransaction ft;

            MyMoneyAccount account = dlg->account();
            MyMoneyAccount parent = dlg->parentAccount();
            if(it_plugin != d->m_onlinePlugins.constEnd()) {
              account.setOnlineBankingSettings((*it_plugin)->onlineBankingSettings(account.onlineBankingSettings()));
            }
            MyMoneyMoney bal = dlg->openingBalance();
            if(d->m_selectedAccount.accountGroup() == MyMoneyAccount::Liability) {
              bal = -bal;
            }

            // we need to modify first, as reparent would override all other changes
            file->modifyAccount(account);
            if(account.parentAccountId() != parent.id()) {
              file->reparentAccount(account, parent);
            }
            if(!tid.isEmpty() && dlg->openingBalance().isZero()) {
              file->removeTransaction(t);

            } else if(!tid.isEmpty() && !dlg->openingBalance().isZero()) {
              s0.setShares(bal);
              s0.setValue(bal);
              t.modifySplit(s0);
              s1.setShares(-bal);
              s1.setValue(-bal);
              t.modifySplit(s1);
              file->modifyTransaction(t);

            } else if(tid.isEmpty() && !dlg->openingBalance().isZero()){
              file->createOpeningBalanceTransaction(d->m_selectedAccount, bal);
            }

            ft.commit();

            // reload the account object as it might have changed in the meantime
            slotSelectAccount(file->account(account.id()));

          } catch(MyMoneyException* e) {
            KMessageBox::error( this, i18n("Unable to modify account '%1'. Cause: %2",d->m_selectedAccount.name(),e->what()));
            delete e;
          }
        }
        delete dlg;
      } else {
        QPointer<KEditLoanWizard> wizard = new KEditLoanWizard(d->m_selectedAccount);
        connect(wizard, SIGNAL(newCategory(MyMoneyAccount&)), this, SLOT(slotCategoryNew(MyMoneyAccount&)));
        connect(wizard, SIGNAL(createPayee(const QString&, QString&)), this, SLOT(slotPayeeNew(const QString&, QString&)));
        if(wizard->exec() == QDialog::Accepted) {
          MyMoneySchedule sch = file->schedule(d->m_selectedAccount.value("schedule").toLatin1());
          if(!(d->m_selectedAccount == wizard->account())
          || !(sch == wizard->schedule())) {
            MyMoneyFileTransaction ft;
            try {
              file->modifyAccount(wizard->account());
              sch = wizard->schedule();
              try {
                file->schedule(sch.id());
                file->modifySchedule(sch);
                ft.commit();
              } catch (MyMoneyException *e) {
                try {
                  file->addSchedule(sch);
                  ft.commit();
                } catch (MyMoneyException *f) {
                  qDebug("Cannot add schedule: '%s'", qPrintable(f->what()));
                  delete f;
                }
                delete e;
              }
            } catch(MyMoneyException *e) {
              qDebug("Unable to modify account %s: '%s'", qPrintable(d->m_selectedAccount.name()),
                  qPrintable(e->what()));
              delete e;
            }
          }
        }
        delete wizard;
      }
    }
  }
}

void KMyMoney2App::slotAccountReconcileStart(void)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  MyMoneyAccount account;

  // we cannot reconcile standard accounts
  if(!file->isStandardAccount(d->m_selectedAccount.id())) {
    // check if we can reconcile this account
    // it make's sense for asset and liability accounts
    try {
      // check if we have overdue schedules for this account
      QList<MyMoneySchedule> schedules = file->scheduleList(d->m_selectedAccount.id(), MyMoneySchedule::TYPE_ANY, MyMoneySchedule::OCCUR_ANY, MyMoneySchedule::STYPE_ANY, QDate(), QDate(), true);
      if(schedules.count() > 0) {
        if(KMessageBox::questionYesNo(this, i18n("KMyMoney has detected some overdue scheduled transactions for this account. Do you want to enter those scheduled transactions now?"), i18n("Scheduled transactions found")) == KMessageBox::Yes) {

          QMap<QString, bool> skipMap;
          bool processedOne;
          KMyMoneyUtils::EnterScheduleResultCodeE rc = KMyMoneyUtils::Enter;
          do {
            processedOne = false;
            QList<MyMoneySchedule>::const_iterator it_sch;
            for(it_sch = schedules.constBegin(); (rc != KMyMoneyUtils::Cancel) && (it_sch != schedules.constEnd()); ++it_sch) {
              MyMoneySchedule sch(*(it_sch));

              // and enter it if it is not on the skip list
              if(skipMap.find((*it_sch).id()) == skipMap.end()) {
                rc = enterSchedule(sch, false, true);
                if(rc == KMyMoneyUtils::Ignore) {
                  skipMap[(*it_sch).id()] = true;
                }
              }
            }

            // reload list (maybe this schedule needs to be added again)
            schedules = file->scheduleList(d->m_selectedAccount.id(), MyMoneySchedule::TYPE_ANY, MyMoneySchedule::OCCUR_ANY, MyMoneySchedule::STYPE_ANY, QDate(), QDate(), true);
          } while(processedOne);
        }
      }

      account = file->account(d->m_selectedAccount.id());
      // get rid of previous run.
      delete d->m_endingBalanceDlg;
      d->m_endingBalanceDlg = new KEndingBalanceDlg(account, this);
      if(account.isAssetLiability()) {
        connect(d->m_endingBalanceDlg, SIGNAL(createPayee(const QString&, QString&)), this, SLOT(slotPayeeNew(const QString&, QString&)));
        connect(d->m_endingBalanceDlg, SIGNAL(createCategory(MyMoneyAccount&, const MyMoneyAccount&)), this, SLOT(slotCategoryNew(MyMoneyAccount&, const MyMoneyAccount&)));

        if(d->m_endingBalanceDlg->exec() == QDialog::Accepted) {
          if(d->m_myMoneyView->startReconciliation(account, d->m_endingBalanceDlg->statementDate(), d->m_endingBalanceDlg->endingBalance())) {

            // check if the user requests us to create interest
            // or charge transactions.
            MyMoneyTransaction ti = d->m_endingBalanceDlg->interestTransaction();
            MyMoneyTransaction tc = d->m_endingBalanceDlg->chargeTransaction();
            MyMoneyFileTransaction ft;
            try {
              if(ti != MyMoneyTransaction()) {
                MyMoneyFile::instance()->addTransaction(ti);
              }
              if(tc != MyMoneyTransaction()) {
                MyMoneyFile::instance()->addTransaction(tc);
              }
              ft.commit();

            } catch(MyMoneyException *e) {
              qWarning("interest transaction not stored: '%s'", qPrintable(e->what()));
              delete e;
            }

            // reload the account object as it might have changed in the meantime
            d->m_reconciliationAccount = file->account(account.id());
            slotUpdateActions();
          }
        }
      }
    } catch(MyMoneyException *e) {
      delete e;
    }
  }
}

void KMyMoney2App::slotAccountReconcileFinish(void)
{
  MyMoneyFile* file = MyMoneyFile::instance();

  if(!d->m_reconciliationAccount.id().isEmpty()) {
    // retrieve list of all transactions that are not reconciled or cleared
    QList<QPair<MyMoneyTransaction, MyMoneySplit> > transactionList;
    MyMoneyTransactionFilter filter(d->m_reconciliationAccount.id());
    filter.addState(MyMoneyTransactionFilter::cleared);
    filter.addState(MyMoneyTransactionFilter::notReconciled);
    filter.setDateFilter(QDate(), d->m_endingBalanceDlg->statementDate());
    filter.setConsiderCategory(false);
    filter.setReportAllSplits(true);
    file->transactionList(transactionList, filter);

    MyMoneyMoney balance = MyMoneyFile::instance()->balance(d->m_reconciliationAccount.id(), d->m_endingBalanceDlg->statementDate());
    MyMoneyMoney actBalance, clearedBalance;
    actBalance = clearedBalance = balance;

    // walk the list of transactions to figure out the balance(s)
    QList<QPair<MyMoneyTransaction, MyMoneySplit> >::const_iterator it;
    for(it = transactionList.constBegin(); it != transactionList.constEnd(); ++it) {
      if((*it).second.reconcileFlag() == MyMoneySplit::NotReconciled) {
        clearedBalance -= (*it).second.shares();
      }
    }

    if(d->m_endingBalanceDlg->endingBalance() != clearedBalance) {
      QString message = i18n("You are about to finish the reconciliation of this account with a difference between your bank statement and the transactions marked as cleared.\n"
                             "Are you sure you want to finish the reconciliation ?");
      if (KMessageBox::questionYesNo(this, message, i18n("Confirm end of reconciliation"), KStandardGuiItem::yes(), KStandardGuiItem::no()) == KMessageBox::No)
        return;
    }

    MyMoneyFileTransaction ft;

    // refresh object
    d->m_reconciliationAccount = file->account(d->m_reconciliationAccount.id());

    // Turn off reconciliation mode
    d->m_myMoneyView->finishReconciliation(d->m_reconciliationAccount);

    d->m_reconciliationAccount.setValue("lastStatementBalance", d->m_endingBalanceDlg->endingBalance().toString());
    d->m_reconciliationAccount.setLastReconciliationDate(d->m_endingBalanceDlg->statementDate());

    d->m_reconciliationAccount.deletePair("lastReconciledBalance");
    d->m_reconciliationAccount.deletePair("statementBalance");
    d->m_reconciliationAccount.deletePair("statementDate");

    try {
      // update the account data
      file->modifyAccount(d->m_reconciliationAccount);

      /*
      // collect the list of cleared splits for this account
      filter.clear();
      filter.addAccount(d->m_reconciliationAccount.id());
      filter.addState(MyMoneyTransactionFilter::cleared);
      filter.setConsiderCategory(false);
      filter.setReportAllSplits(true);
      file->transactionList(transactionList, filter);
      */

      // walk the list of transactions/splits and mark the cleared ones as reconciled
      QList<QPair<MyMoneyTransaction, MyMoneySplit> >::iterator it;

      for(it = transactionList.begin(); it != transactionList.end(); ++it) {
        MyMoneySplit sp = (*it).second;
        // skip the ones that are not marked cleared
        if(sp.reconcileFlag() != MyMoneySplit::Cleared)
          continue;

        // always retrieve a fresh copy of the transaction because we
        // might have changed it already with another split
        MyMoneyTransaction t = file->transaction((*it).first.id());
        sp.setReconcileFlag(MyMoneySplit::Reconciled);
        sp.setReconcileDate(d->m_endingBalanceDlg->statementDate());
        t.modifySplit(sp);

        // update the engine ...
        file->modifyTransaction(t);

        // ... and the list
        (*it) = qMakePair(t, sp);
      }
      ft.commit();

      // reload account data from engine as the data might have changed in the meantime
      d->m_reconciliationAccount = file->account(d->m_reconciliationAccount.id());
      emit accountReconciled(d->m_reconciliationAccount,
                             d->m_endingBalanceDlg->statementDate(),
                             d->m_endingBalanceDlg->previousBalance(),
                             d->m_endingBalanceDlg->endingBalance(),
                             transactionList);

    } catch(MyMoneyException *e) {
      qDebug("Unexpected exception when setting cleared to reconcile");
      delete e;
    }
  }
  // Turn off reconciliation mode
  d->m_reconciliationAccount = MyMoneyAccount();
  slotUpdateActions();
}

void KMyMoney2App::slotAccountReconcilePostpone(void)
{
  MyMoneyFileTransaction ft;
  MyMoneyFile* file = MyMoneyFile::instance();

  if(!d->m_reconciliationAccount.id().isEmpty()) {
    // refresh object
    d->m_reconciliationAccount = file->account(d->m_reconciliationAccount.id());

    // Turn off reconciliation mode
    d->m_myMoneyView->finishReconciliation(d->m_reconciliationAccount);

    d->m_reconciliationAccount.setValue("lastReconciledBalance", d->m_endingBalanceDlg->previousBalance().toString());
    d->m_reconciliationAccount.setValue("statementBalance", d->m_endingBalanceDlg->endingBalance().toString());
    d->m_reconciliationAccount.setValue("statementDate", d->m_endingBalanceDlg->statementDate().toString(Qt::ISODate));

    try {
      file->modifyAccount(d->m_reconciliationAccount);
      ft.commit();
      d->m_reconciliationAccount = MyMoneyAccount();
      slotUpdateActions();
    } catch(MyMoneyException *e) {
      qDebug("Unexpected exception when setting last reconcile info into account");
      delete e;
      ft.rollback();
      d->m_reconciliationAccount = file->account(d->m_reconciliationAccount.id());
    }
  }
}

void KMyMoney2App::slotAccountOpen(const MyMoneyObject& obj)
{
  if(typeid(obj) != typeid(MyMoneyAccount))
    return;

  MyMoneyFile* file = MyMoneyFile::instance();
  QString id = d->m_selectedAccount.id();

  // if the caller passed a non-empty object, we need to select that
  if(!obj.id().isEmpty()) {
    id = obj.id();
  }

  // we cannot reconcile standard accounts
  if(!file->isStandardAccount(id)) {
    // check if we can open this account
    // currently it make's sense for asset and liability accounts
    try {
      MyMoneyAccount account = file->account(id);
      d->m_myMoneyView->slotLedgerSelected(account.id());
    } catch(MyMoneyException *e) {
      delete e;
    }
  }
}

bool KMyMoney2App::canCloseAccount(const MyMoneyAccount& acc) const
{
  // balance must be zero
  if(!acc.balance().isZero())
    return false;

  // all children must be already closed
  QStringList::const_iterator it_a;
  for(it_a = acc.accountList().constBegin(); it_a != acc.accountList().constEnd(); ++it_a) {
    MyMoneyAccount a = MyMoneyFile::instance()->account(*it_a);
    if(!a.isClosed()) {
      return false;
    }
  }

  // there must be no unfinished schedule referencing the account
  QList<MyMoneySchedule> list = MyMoneyFile::instance()->scheduleList();
  QList<MyMoneySchedule>::const_iterator it_l;
  for(it_l = list.constBegin(); it_l != list.constEnd(); ++it_l) {
    if((*it_l).isFinished())
      continue;
    if((*it_l).hasReferenceTo(acc.id()))
      return false;
  }
  return true;
}

void KMyMoney2App::slotAccountClose(void)
{
  MyMoneyAccount a;
  if(!d->m_selectedInvestment.id().isEmpty())
    a = d->m_selectedInvestment;
  else if(!d->m_selectedAccount.id().isEmpty())
    a = d->m_selectedAccount;
  if(a.id().isEmpty())
    return;  // need an account ID

  MyMoneyFileTransaction ft;
  try {
    a.setClosed(true);
    MyMoneyFile::instance()->modifyAccount(a);
    ft.commit();
    if(KMyMoneyGlobalSettings::hideClosedAccounts()) {
      KMessageBox::information(this, QString("<qt>")+i18n("You have closed this account. It remains in the system because you have transactions which still refer to it, but is not shown in the views. You can make it visible again by going to the View menu and selecting <b>Show all accounts</b> or by unselecting the <b>Don't show closed accounts</b> setting.")+QString("</qt>"), i18n("Information"), "CloseAccountInfo");
    }
  } catch(MyMoneyException* e) {
    delete e;
  }
}

void KMyMoney2App::slotAccountReopen(void)
{
  MyMoneyAccount a;
  if(!d->m_selectedInvestment.id().isEmpty())
    a = d->m_selectedInvestment;
  else if(!d->m_selectedAccount.id().isEmpty())
    a = d->m_selectedAccount;
  if(a.id().isEmpty())
    return;  // need an account ID

  MyMoneyFile* file = MyMoneyFile::instance();
  MyMoneyFileTransaction ft;
  try {
    while(a.isClosed()) {
      a.setClosed(false);
      file->modifyAccount(a);
      a = file->account(a.parentAccountId());
    }
    ft.commit();
  } catch(MyMoneyException* e) {
    delete e;
  }
}

void KMyMoney2App::slotReparentAccount(const MyMoneyAccount& _src, const MyMoneyInstitution& _dst)
{
  MyMoneyAccount src(_src);
  src.setInstitutionId(_dst.id());
  MyMoneyFileTransaction ft;
  try {
    MyMoneyFile::instance()->modifyAccount(src);
    ft.commit();
  } catch(MyMoneyException* e) {
    KMessageBox::sorry(this, QString("<p>")+i18n("<b>%1</b> cannot be moved to institution <b>%2</b>. Reason: %3",src.name(),_dst.name(),e->what()));
    delete e;
  }
}

void KMyMoney2App::slotReparentAccount(const MyMoneyAccount& _src, const MyMoneyAccount& _dst)
{
  MyMoneyAccount src(_src);
  MyMoneyAccount dst(_dst);
  MyMoneyFileTransaction ft;
  try {
    MyMoneyFile::instance()->reparentAccount(src, dst);
    ft.commit();
  } catch(MyMoneyException* e) {
    KMessageBox::sorry(this, QString("<p>")+i18n("<b>%1</b> cannot be moved to <b>%2</b>. Reason: %3",src.name(),dst.name(),e->what()));
    delete e;
  }
}

void KMyMoney2App::slotAccountTransactionReport(void)
{
  // Generate a transaction report that contains transactions for only the
  // currently selected account.
  if(!d->m_selectedAccount.id().isEmpty()) {
    MyMoneyReport report(
        MyMoneyReport::eAccount,
        MyMoneyReport::eQCnumber|MyMoneyReport::eQCpayee|MyMoneyReport::eQCcategory,
        MyMoneyTransactionFilter::yearToDate,
        MyMoneyReport::eDetailAll,
        i18n("%1 YTD Account Transactions",d->m_selectedAccount.name()),
        i18n("Generated Report")
      );
    report.setGroup(i18n("Transactions"));
    report.addAccount(d->m_selectedAccount.id());

    d->m_myMoneyView->slotShowReport(report);
  }
}

void KMyMoney2App::slotScheduleNew(void)
{
  slotScheduleNew(MyMoneyTransaction());
}

void KMyMoney2App::slotScheduleNew(const MyMoneyTransaction& _t, MyMoneySchedule::occurrenceE occurrence)
{
  MyMoneySchedule schedule;
  schedule.setOccurrence(occurrence);

  // if the schedule is based on an existing transaction,
  // we take the post date and project it to the next
  // schedule in a month.
  if(_t != MyMoneyTransaction()) {
    MyMoneyTransaction t(_t);
    schedule.setTransaction(t);
    if(occurrence != MyMoneySchedule::OCCUR_ONCE)
      schedule.setNextDueDate(schedule.nextPayment(t.postDate()));
  }

  QPointer<KEditScheduleDlg> dlg = new KEditScheduleDlg(schedule, this);
  TransactionEditor* transactionEditor = dlg->startEdit();
  if(transactionEditor) {
    if(dlg->exec() == QDialog::Accepted) {
      MyMoneyFileTransaction ft;
      try {
        schedule = dlg->schedule();
        MyMoneyFile::instance()->addSchedule(schedule);
        ft.commit();

      } catch (MyMoneyException *e) {
        KMessageBox::error(this, i18n("Unable to add scheduled transaction: %1",e->what()), i18n("Add scheduled transaction"));
        delete e;
      }
    }
  }
  delete transactionEditor;
  delete dlg;
}

void KMyMoney2App::slotScheduleEdit(void)
{
  if (!d->m_selectedSchedule.id().isEmpty()) {
    try {
      MyMoneySchedule schedule = MyMoneyFile::instance()->schedule(d->m_selectedSchedule.id());

      KEditScheduleDlg* sched_dlg = 0;
      KEditLoanWizard* loan_wiz = 0;


      switch (schedule.type()) {
        case MyMoneySchedule::TYPE_BILL:
        case MyMoneySchedule::TYPE_DEPOSIT:
        case MyMoneySchedule::TYPE_TRANSFER:
          sched_dlg = new KEditScheduleDlg(schedule, this);
          d->m_transactionEditor = sched_dlg->startEdit();
          if(d->m_transactionEditor) {
            if(sched_dlg->exec() == QDialog::Accepted) {
              MyMoneyFileTransaction ft;
              try {
                MyMoneySchedule sched = sched_dlg->schedule();
                // Check whether the new Schedule Date
                // is at or before the lastPaymentDate
                // If it is, ask the user whether to clear the
                // lastPaymentDate
                const QDate& next = sched.nextDueDate();
                const QDate& last = sched.lastPayment();
                if ( next.isValid() && last.isValid() && next <= last ) {
                  // Entered a date effectively no later
                  // than previous payment.  Date would be
                  // updated automatically so we probably
                  // want to clear it.  Let's ask the user.
                  if(KMessageBox::questionYesNo(this, QString("<qt>")+i18n("You have entered a scheduled transaction date of <b>%1</b>.  Because the scheduled transaction was last paid on <b>%2</b>, KMyMoney will automatically adjust the scheduled transaction date to the next date unless the last payment date is reset.  Do you want to reset the last payment date?",KGlobal::locale()->formatDate(next, KLocale::ShortDate),KGlobal::locale()->formatDate(last, KLocale::ShortDate))+QString("</qt>"),i18n("Reset Last Payment Date" ), KStandardGuiItem::yes(), KStandardGuiItem::no()) == KMessageBox::Yes) {
                    sched.setLastPayment( QDate() );
                  }
                }
                MyMoneyFile::instance()->modifySchedule(sched);
                // delete the editor before we emit the dataChanged() signal from the
                // engine. Calling this twice in a row does not hurt.
                deleteTransactionEditor();
                ft.commit();
              } catch (MyMoneyException *e) {
                KMessageBox::detailedSorry(this, i18n("Unable to modify scheduled transaction '%1'",d->m_selectedSchedule.name()), e->what());
                delete e;
              }
            }
            deleteTransactionEditor();
          }
          delete sched_dlg;
          break;

        case MyMoneySchedule::TYPE_LOANPAYMENT:
          loan_wiz = new KEditLoanWizard(schedule.account(2));
          connect(loan_wiz, SIGNAL(newCategory(MyMoneyAccount&)), this, SLOT(slotCategoryNew(MyMoneyAccount&)));
          connect(loan_wiz, SIGNAL(createPayee(const QString&, QString&)), this, SLOT(slotPayeeNew(const QString&, QString&)));
          if (loan_wiz->exec() == QDialog::Accepted) {
            MyMoneyFileTransaction ft;
            try {
              MyMoneyFile::instance()->modifySchedule(loan_wiz->schedule());
              MyMoneyFile::instance()->modifyAccount(loan_wiz->account());
              ft.commit();
            } catch (MyMoneyException *e) {
              KMessageBox::detailedSorry(this, i18n("Unable to modify scheduled transaction '%1'",d->m_selectedSchedule.name()), e->what());
              delete e;
            }
          }
          delete loan_wiz;
          break;

        case MyMoneySchedule::TYPE_ANY:
          break;
      }

    } catch (MyMoneyException *e) {
      KMessageBox::detailedSorry(this, i18n("Unable to modify scheduled transaction '%1'",d->m_selectedSchedule.name()), e->what());
      delete e;
    }
  }
}

void KMyMoney2App::slotScheduleDelete(void)
{
  if (!d->m_selectedSchedule.id().isEmpty()) {
    MyMoneyFileTransaction ft;
    try {
      MyMoneySchedule sched = MyMoneyFile::instance()->schedule(d->m_selectedSchedule.id());
      QString msg = QString("<p>")+i18n("Are you sure you want to delete the scheduled transaction <b>%1</b>?",d->m_selectedSchedule.name());
      if(sched.type() == MyMoneySchedule::TYPE_LOANPAYMENT) {
        msg += QString(" ");
        msg += i18n("In case of loan payments it is currently not possible to recreate the scheduled transaction.");
      }
      if (KMessageBox::questionYesNo(this, msg) == KMessageBox::No)
        return;

      MyMoneyFile::instance()->removeSchedule(sched);
      ft.commit();

    } catch (MyMoneyException *e) {
      KMessageBox::detailedSorry(this, i18n("Unable to remove scheduled transaction '%1'",d->m_selectedSchedule.name()), e->what());
      delete e;
    }
  }
}

void KMyMoney2App::slotScheduleDuplicate(void)
{
  // since we may jump here via code, we have to make sure to react only
  // if the action is enabled
  if(kmymoney2->action("schedule_duplicate")->isEnabled()) {
    MyMoneySchedule sch = d->m_selectedSchedule;
    sch.clearId();
    sch.setLastPayment(QDate());
    sch.setName(i18nc("Copy of scheduled transaction name", "Copy of %1", sch.name()));

    MyMoneyFileTransaction ft;
    try {
      MyMoneyFile::instance()->addSchedule(sch);
      ft.commit();

      // select the new schedule in the view
      if(!d->m_selectedSchedule.id().isEmpty())
        d->m_myMoneyView->slotScheduleSelected(sch.id());

    } catch(MyMoneyException* e) {
      KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to duplicate transaction(s): %1, thrown in %2:%3",e->what(),e->file(),e->line()));
      delete e;
    }
  }
}

void KMyMoney2App::slotScheduleSkip(void)
{
  if (!d->m_selectedSchedule.id().isEmpty()) {
    try {
      MyMoneySchedule schedule = MyMoneyFile::instance()->schedule(d->m_selectedSchedule.id());
      if(!schedule.isFinished()) {
        if(schedule.occurrence() != MyMoneySchedule::OCCUR_ONCE) {
          QDate next = schedule.nextDueDate();
          if(!schedule.isFinished() && (KMessageBox::questionYesNo(this, QString("<qt>")+i18n("Do you really want to skip the <b>%1</b> transaction scheduled for <b>%2</b>?",schedule.name(), KGlobal::locale()->formatDateTime(QDateTime(next), KLocale::ShortDate, false))+QString("</qt>"))) == KMessageBox::Yes) {
            MyMoneyFileTransaction ft;
            schedule.setLastPayment(next);
            schedule.setNextDueDate(schedule.nextPayment(next));
            MyMoneyFile::instance()->modifySchedule(schedule);
            ft.commit();
          }
        }
      }
    } catch (MyMoneyException *e) {
      KMessageBox::detailedSorry(this, QString("<qt>")+i18n("Unable to skip scheduled transaction <b>%1</b>.",d->m_selectedSchedule.name())+QString("</qt>"), e->what());
      delete e;
    }
  }
}

void KMyMoney2App::slotScheduleEnter(void)
{
  if (!d->m_selectedSchedule.id().isEmpty()) {
    try {
      MyMoneySchedule schedule = MyMoneyFile::instance()->schedule(d->m_selectedSchedule.id());
      enterSchedule(schedule);
    } catch (MyMoneyException *e) {
      KMessageBox::detailedSorry(this, i18n("Unknown scheduled transaction '%1'",d->m_selectedSchedule.name()), e->what());
      delete e;
    }
  }
}

KMyMoneyUtils::EnterScheduleResultCodeE KMyMoney2App::enterSchedule(MyMoneySchedule& schedule, bool autoEnter, bool extendedKeys)
{
  KMyMoneyUtils::EnterScheduleResultCodeE rc = KMyMoneyUtils::Cancel;
  if (!schedule.id().isEmpty()) {
    try {
      schedule = MyMoneyFile::instance()->schedule(schedule.id());
    } catch (MyMoneyException *e) {
      KMessageBox::detailedSorry(this, i18n("Unable to enter scheduled transaction '%1'",d->m_selectedSchedule.name()), e->what());
      delete e;
      return rc;
    }

    QPointer<KEnterScheduleDlg> dlg = new KEnterScheduleDlg(this, schedule);

    try {
      QDate origDueDate = schedule.nextDueDate();

      dlg->showExtendedKeys(extendedKeys);

      d->m_transactionEditor = dlg->startEdit();
      if(d->m_transactionEditor) {
        MyMoneyTransaction torig, taccepted;
        d->m_transactionEditor->createTransaction(torig, dlg->transaction(), schedule.transaction().splits()[0], true);
        // force actions to be available no matter what (will be updated according to the state during
        // slotTransactionsEnter or slotTransactionsCancel)
        kmymoney2->action("transaction_cancel")->setEnabled(true);
        kmymoney2->action("transaction_enter")->setEnabled(true);

        KConfirmManualEnterDlg::Action action = KConfirmManualEnterDlg::ModifyOnce;
        if(!autoEnter || !schedule.isFixed()) {
          for(;;) {
            rc = KMyMoneyUtils::Cancel;
            if(dlg->exec() == QDialog::Accepted) {
              rc = dlg->resultCode();
              if(rc == KMyMoneyUtils::Enter) {
                d->m_transactionEditor->createTransaction(taccepted, torig, torig.splits()[0], true);
                // make sure to suppress comparison of some data: postDate
                torig.setPostDate(taccepted.postDate());
                if(torig != taccepted) {
                  QPointer<KConfirmManualEnterDlg> cdlg =
                    new KConfirmManualEnterDlg(schedule, this);
                  cdlg->loadTransactions(torig, taccepted);
                  if(cdlg->exec() == QDialog::Accepted) {
                    action = cdlg->action();
                    delete cdlg;
                    break;
                  }
                  delete cdlg;
                  // the user has chosen 'cancel' during confirmation,
                  // we go back to the editor
                  continue;
                }
              } else if(rc == KMyMoneyUtils::Skip) {
                slotTransactionsCancel();
                slotScheduleSkip();
              } else {
                slotTransactionsCancel();
              }
            } else {
              if(autoEnter) {
                if(KMessageBox::warningYesNo(this, i18n("Are you sure you wish to stop this scheduled transaction from being entered into the register?\n\nKMyMoney will prompt you again next time it starts unless you manually enter it later.")) == KMessageBox::No) {
                  // the user has chosen 'No' for the above question,
                  // we go back to the editor
                  continue;
                }
              }
              slotTransactionsCancel();
            }
            break;
          }
        }

        // if we still have the editor around here, the user did not cancel
        if(d->m_transactionEditor) {
          MyMoneyFileTransaction ft;
          try {
            MyMoneyTransaction t;
            // add the new transaction
            switch(action) {
              case KConfirmManualEnterDlg::UseOriginal:
                // setup widgets with original transaction data
                d->m_transactionEditor->setTransaction(dlg->transaction(), dlg->transaction().splits()[0]);
                // and create a transaction based on that data
                taccepted = MyMoneyTransaction();
                d->m_transactionEditor->createTransaction(taccepted, dlg->transaction(), dlg->transaction().splits()[0], true);
                break;

              case KConfirmManualEnterDlg::ModifyAlways:
                schedule.setTransaction(taccepted);
                break;

              case KConfirmManualEnterDlg::ModifyOnce:
                break;
            }

            QString newId;
            connect(d->m_transactionEditor, SIGNAL(balanceWarning(QWidget*, const MyMoneyAccount&, const QString&)), d->m_balanceWarning, SLOT(slotShowMessage(QWidget*, const MyMoneyAccount&, const QString&)));
            if(d->m_transactionEditor->enterTransactions(newId, false)) {
              if(!newId.isEmpty()) {
                MyMoneyTransaction t = MyMoneyFile::instance()->transaction(newId);
                schedule.setLastPayment(t.postDate());
              }
              schedule.setNextDueDate(schedule.nextPayment(origDueDate));
              MyMoneyFile::instance()->modifySchedule(schedule);
              rc = KMyMoneyUtils::Enter;

              // delete the editor before we emit the dataChanged() signal from the
              // engine. Calling this twice in a row does not hurt.
              deleteTransactionEditor();
              ft.commit();
            }
          } catch (MyMoneyException *e) {
            KMessageBox::detailedSorry(this, i18n("Unable to enter scheduled transaction '%1'",d->m_selectedSchedule.name()), e->what());
            delete e;
          }
          deleteTransactionEditor();
        }
      }
    } catch (MyMoneyException *e) {
      KMessageBox::detailedSorry(this, i18n("Unable to enter scheduled transaction '%1'",d->m_selectedSchedule.name()), e->what());
      delete e;
    }
    delete dlg;
  }
  return rc;
}

void KMyMoney2App::slotPayeeNew(const QString& newnameBase, QString& id)
{
  bool doit = true;

  if(newnameBase != i18n("New Payee")) {
    // Ask the user if that is what he intended to do?
    QString msg = QString("<qt>") + i18n("Do you want to add <b>%1</b> as payer/receiver ?",newnameBase) + QString("</qt>");

    if(KMessageBox::questionYesNo(this, msg, i18n("New payee/receiver"), KStandardGuiItem::yes(), KStandardGuiItem::no(), "NewPayee") == KMessageBox::No)
      doit = false;
  }

  if(doit) {
    MyMoneyFileTransaction ft;
    try {
      QString newname(newnameBase);
      // adjust name until a unique name has been created
      int count = 0;
      for(;;) {
        try {
          MyMoneyFile::instance()->payeeByName(newname);
          newname = QString("%1 [%2]").arg(newnameBase).arg(++count);
        } catch(MyMoneyException* e) {
          delete e;
          break;
        }
      }

      MyMoneyPayee p;
      p.setName(newname);
      MyMoneyFile::instance()->addPayee(p);
      id = p.id();
      ft.commit();
    } catch (MyMoneyException *e) {
      KMessageBox::detailedSorry(this, i18n("Unable to add payee"),
        QString("%1 thrown in %2:%3").arg(e->what()).arg(e->file()).arg(e->line()));
      delete e;
    }
  }
}

void KMyMoney2App::slotPayeeNew(void)
{
  QString id;
  slotPayeeNew(i18n("New Payee"), id);

  // the callbacks should have made sure, that the payees view has been
  // updated already. So we search for the id in the list of items
  // and select it.
  emit payeeCreated(id);
}

bool KMyMoney2App::payeeInList(const QList<MyMoneyPayee>& list, const QString& id) const
{
  bool rc = false;
  QList<MyMoneyPayee>::const_iterator it_p = list.begin();
  while(it_p != list.end()) {
    if((*it_p).id() == id) {
      rc = true;
      break;
    }
    ++it_p;
  }
  return rc;
}

void KMyMoney2App::slotPayeeDelete(void)
{
  if(d->m_selectedPayees.isEmpty())
    return; // shouldn't happen

  MyMoneyFile * file = MyMoneyFile::instance();

  // first create list with all non-selected payees
  QList<MyMoneyPayee> remainingPayees = file->payeeList();
  QList<MyMoneyPayee>::iterator it_p;
  for(it_p = remainingPayees.begin(); it_p != remainingPayees.end(); ) {
    if(d->m_selectedPayees.contains(*it_p)) {
      it_p = remainingPayees.erase(it_p);
    } else {
      ++it_p;
    }
  }

  // get confirmation from user
  QString prompt;
  if (d->m_selectedPayees.size() == 1)
    prompt = QString("<p>")+i18n("Do you really want to remove the payee <b>%1</b>?",d->m_selectedPayees.front().name());
  else
    prompt = i18n("Do you really want to remove all selected payees?");

  if (KMessageBox::questionYesNo(this, prompt, i18n("Remove Payee"))==KMessageBox::No)
    return;

  MyMoneyFileTransaction ft;
  try {
    // create a transaction filter that contains all payees selected for removal
    MyMoneyTransactionFilter f = MyMoneyTransactionFilter();
    for (QList<MyMoneyPayee>::const_iterator it = d->m_selectedPayees.constBegin();
         it != d->m_selectedPayees.constEnd(); ++it) {
      f.addPayee( (*it).id() );
    }
    // request a list of all transactions that still use the payees in question
    QList<MyMoneyTransaction> translist = file->transactionList(f);
//     kDebug() << "[KPayeesView::slotDeletePayee]  " << translist.count() << " transaction still assigned to payees";

    // now get a list of all schedules that make use of one of the payees
    QList<MyMoneySchedule> all_schedules = file->scheduleList();
    QList<MyMoneySchedule> used_schedules;
    for (QList<MyMoneySchedule>::ConstIterator it = all_schedules.constBegin();
         it != all_schedules.constEnd(); ++it)
    {
      // loop over all splits in the transaction of the schedule
      for (QList<MyMoneySplit>::ConstIterator s_it = (*it).transaction().splits().constBegin();
           s_it != (*it).transaction().splits().constEnd(); ++s_it)
      {
        // is the payee in the split to be deleted?
        if(payeeInList(d->m_selectedPayees, (*s_it).payeeId())) {
          used_schedules.push_back(*it); // remember this schedule
          break;
        }
      }
    }
//     kDebug() << "[KPayeesView::slotDeletePayee]  " << used_schedules.count() << " schedules use one of the selected payees";

    MyMoneyPayee newPayee;
    bool addToMatchList = false;
    // if at least one payee is still referenced, we need to reassign its transactions first
    if (!translist.isEmpty() || !used_schedules.isEmpty()) {
      // show error message if no payees remain
      if (remainingPayees.isEmpty()) {
        KMessageBox::sorry(this, i18n("At least one transaction/scheduled transaction is still referenced by a payee. "
          "Currently you have all payees selected. However, at least one payee must remain so "
          "that the transaction/scheduled transaction can be reassigned."));
        return;
      }

      // show transaction reassignment dialog
      KPayeeReassignDlg * dlg = new KPayeeReassignDlg(this);
      QString payee_id = dlg->show(remainingPayees);
      addToMatchList = dlg->addToMatchList();
      delete dlg; // and kill the dialog
      if (payee_id.isEmpty())
        return; // the user aborted the dialog, so let's abort as well

      newPayee = file->payee(payee_id);

      // TODO : check if we have a report that explicitively uses one of our payees
      //        and issue an appropriate warning
      try {
        QList<MyMoneySplit>::iterator s_it;
        // now loop over all transactions and reassign payee
        for (QList<MyMoneyTransaction>::iterator it = translist.begin(); it != translist.end(); ++it) {
          // create a copy of the splits list in the transaction
          QList<MyMoneySplit> splits = (*it).splits();
          // loop over all splits
          for (s_it = splits.begin(); s_it != splits.end(); ++s_it) {
            // if the split is assigned to one of the selected payees, we need to modify it
            if(payeeInList(d->m_selectedPayees, (*s_it).payeeId())) {
              (*s_it).setPayeeId(payee_id); // first modify payee in current split
              // then modify the split in our local copy of the transaction list
              (*it).modifySplit(*s_it); // this does not modify the list object 'splits'!
            }
          } // for - Splits
          file->modifyTransaction(*it);  // modify the transaction in the MyMoney object
        } // for - Transactions

        // now loop over all schedules and reassign payees
        for (QList<MyMoneySchedule>::iterator it = used_schedules.begin();
             it != used_schedules.end(); ++it)
        {
          // create copy of transaction in current schedule
          MyMoneyTransaction trans = (*it).transaction();
          // create copy of lists of splits
          QList<MyMoneySplit> splits = trans.splits();
          for (s_it = splits.begin(); s_it != splits.end(); ++s_it) {
            if(payeeInList(d->m_selectedPayees, (*s_it).payeeId())) {
              (*s_it).setPayeeId(payee_id);
              trans.modifySplit(*s_it); // does not modify the list object 'splits'!
            }
          } // for - Splits
          // store transaction in current schedule
          (*it).setTransaction(trans);
          file->modifySchedule(*it);  // modify the schedule in the MyMoney engine
        } // for - Schedules
      } catch(MyMoneyException *e) {
        KMessageBox::detailedSorry(0, i18n("Unable to reassign payee of transaction/split"),
          (e->what() + ' ' + i18n("thrown in") + ' ' + e->file()+ ":%1").arg(e->line()));
        delete e;
      }
    } // if !translist.isEmpty()

    bool ignorecase;
    QStringList payeeNames;
    MyMoneyPayee::payeeMatchType matchType = newPayee.matchData(ignorecase, payeeNames);
    QStringList deletedPayeeNames;

    // now loop over all selected payees and remove them
    for (QList<MyMoneyPayee>::iterator it = d->m_selectedPayees.begin();
      it != d->m_selectedPayees.end(); ++it) {
      if(addToMatchList) {
        deletedPayeeNames << (*it).name();
      }
      file->removePayee(*it);
    }

    // if we initially have no matching turned on, we just ignore the case (default)
    if(matchType == MyMoneyPayee::matchDisabled)
      ignorecase = true;

    // update the destination payee if this was requested by the user
    if(addToMatchList && deletedPayeeNames.count() > 0) {
      // add new names to the list
      // TODO: it would be cool to somehow shrink the list to make better use
      //       of regular expressions at this point. For now, we leave this task
      //       to the user himeself.
      QStringList::const_iterator it_n;
      for(it_n = deletedPayeeNames.constBegin(); it_n != deletedPayeeNames.constEnd(); ++it_n) {
        if(matchType == MyMoneyPayee::matchKey) {
          // make sure we really need it and it is not caught by an existing regexp
          QStringList::const_iterator it_k;
          for(it_k = payeeNames.constBegin(); it_k != payeeNames.constEnd(); ++it_k) {
            QRegExp exp(*it_k, ignorecase ? Qt::CaseInsensitive : Qt::CaseSensitive);
            if(exp.indexIn(*it_n) != -1)
              break;
          }
          if(it_k == payeeNames.constEnd())
            payeeNames << QRegExp::escape(*it_n);
        } else if(payeeNames.contains(*it_n) == 0)
          payeeNames << QRegExp::escape(*it_n);
      }

      // and update the payee in the engine context
      // make sure to turn on matching for this payee in the right mode
      newPayee.setMatchData(MyMoneyPayee::matchKey, ignorecase, payeeNames);
      file->modifyPayee(newPayee);
    }
    ft.commit();

    // If we just deleted the payees, they sure don't exist anymore
    slotSelectPayees(QList<MyMoneyPayee>());

  } catch(MyMoneyException *e) {
    KMessageBox::detailedSorry(0, i18n("Unable to remove payee(s)"),
      (e->what() + ' ' + i18n("thrown in") + ' ' + e->file()+ ":%1").arg(e->line()));
    delete e;
  }
}

void KMyMoney2App::slotCurrencyNew(void)
{
  QString sid = KInputDialog::getText(i18n("New currency"), i18n("Enter ISO 4217 code for the new currency"), QString::null, 0, 0, 0, 0, ">AAA");
  if(!sid.isEmpty()) {
    QString id(sid);
    MyMoneySecurity currency(id, i18n("New currency"));
    MyMoneyFileTransaction ft;
    try {
      MyMoneyFile::instance()->addCurrency(currency);
      ft.commit();
    } catch(MyMoneyException* e) {
      KMessageBox::sorry(this, i18n("Cannot create new currency. %1",e->what()), i18n("New currency"));
      delete e;
    }
    emit currencyCreated(id);
  }
}

void KMyMoney2App::slotCurrencyRename(Q3ListViewItem* item, int, const QString& txt)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  KMyMoneyListViewItem* p = static_cast<KMyMoneyListViewItem *>(item);

  try {
    if(txt != d->m_selectedCurrency.name()) {
      MyMoneySecurity currency = file->currency(p->id());
      currency.setName(txt);
      MyMoneyFileTransaction ft;
      try {
        file->modifyCurrency(currency);
        d->m_selectedCurrency = currency;
        ft.commit();
      } catch(MyMoneyException* e) {
        KMessageBox::sorry(this, i18n("Cannot rename currency. %1",e->what()), i18n("Rename currency"));
        delete e;
      }
    }
  } catch(MyMoneyException *e) {
    KMessageBox::sorry(this, i18n("Cannot rename currency. %1",e->what()), i18n("Rename currency"));
    delete e;
  }
}

void KMyMoney2App::slotCurrencyDelete(void)
{
  if(!d->m_selectedCurrency.id().isEmpty()) {
    MyMoneyFileTransaction ft;
    try {
      MyMoneyFile::instance()->removeCurrency(d->m_selectedCurrency);
      ft.commit();
    } catch(MyMoneyException* e) {
      KMessageBox::sorry(this, i18n("Cannot delete currency %1. %2",d->m_selectedCurrency.name(),e->what()), i18n("Delete currency"));
      delete e;
    }
  }
}

void KMyMoney2App::slotCurrencySetBase(void)
{
  if(!d->m_selectedCurrency.id().isEmpty()) {
    if(d->m_selectedCurrency.id() != MyMoneyFile::instance()->baseCurrency().id()) {
      MyMoneyFileTransaction ft;
      try {
        MyMoneyFile::instance()->setBaseCurrency(d->m_selectedCurrency);
        ft.commit();
      } catch(MyMoneyException *e) {
        KMessageBox::sorry(this, i18n("Cannot set %1 as base currency: %2",d->m_selectedCurrency.name(),e->what()), i18n("Set base currency"));
        delete e;
      }
    }
  }
}

void KMyMoney2App::slotBudgetNew(void)
{
  QDate date = QDate::currentDate();
  date.setYMD(date.year(), 1, 1);
  QString newname = i18n("Budget <numid>%1</numid>",QString::number(date.year()));

  MyMoneyBudget budget;

  // make sure we have a unique name
  try {
    int i=1;
    // Exception thrown when the name is not found
    while (1) {
      MyMoneyFile::instance()->budgetByName(newname);
      newname = i18n("Budget <numid>%1</numid> (<numid>%2</numid>)",QString::number(date.year()), QString::number(i++));
    }
  } catch(MyMoneyException *e) {
    // all ok, the name is unique
    delete e;
  }

  MyMoneyFileTransaction ft;
  try {
    budget.setName(newname);
    budget.setBudgetStart(date);

    MyMoneyFile::instance()->addBudget(budget);
    ft.commit();
  } catch(MyMoneyException *e) {
    KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to add budget: %1, thrown in %2:%3",e->what(),e->file(),e->line()));
    delete e;
  }
}

void KMyMoney2App::slotBudgetDelete(void)
{
  if(d->m_selectedBudgets.isEmpty())
    return; // shouldn't happen

  MyMoneyFile * file = MyMoneyFile::instance();

  // get confirmation from user
  QString prompt;
  if (d->m_selectedBudgets.size() == 1)
    prompt = QString("<p>")+i18n("Do you really want to remove the budget <b>%1</b>?",d->m_selectedBudgets.front().name());
  else
    prompt = i18n("Do you really want to remove all selected budgets?");

  if (KMessageBox::questionYesNo(this, prompt, i18n("Remove Budget"))==KMessageBox::No)
    return;

  MyMoneyFileTransaction ft;
  try {
    // now loop over all selected budgets and remove them
    for (QList<MyMoneyBudget>::iterator it = d->m_selectedBudgets.begin();
      it != d->m_selectedBudgets.end(); ++it) {
      file->removeBudget(*it);
    }
    ft.commit();

  } catch(MyMoneyException *e) {
    KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to remove budget: %1, thrown in %2:%3", e->what(),e->file(),e->line()));
    delete e;
  }
}

void KMyMoney2App::slotBudgetCopy(void)
{
  if(d->m_selectedBudgets.size() == 1) {
    MyMoneyFileTransaction ft;
    try {
      MyMoneyBudget budget = d->m_selectedBudgets.first();
      budget.clearId();
      budget.setName(i18n("Copy of %1",budget.name()));

      MyMoneyFile::instance()->addBudget(budget);
      ft.commit();
    } catch(MyMoneyException *e) {
      KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to add budget: %1, thrown in %2:%3",e->what(),e->file(),e->line()));
      delete e;
    }
  }
}

void KMyMoney2App::slotBudgetChangeYear(void)
{
  if(d->m_selectedBudgets.size() == 1) {
    QStringList years;
    int current = 0;
    bool haveCurrent = false;
    MyMoneyBudget budget = *(d->m_selectedBudgets.begin());
    for(int i = (QDate::currentDate().year()-3); i < (QDate::currentDate().year()+5); ++i) {
      years << QString("%1").arg(i);
      if(i == budget.budgetStart().year()) {
        haveCurrent = true;
      }
      if(!haveCurrent)
        ++current;
    }
    if(!haveCurrent)
      current = 0;
    bool ok = false;

    QString yearString = KInputDialog::getItem(i18n("Select year"), i18n("Budget year"), years, current, false, &ok, this);

    if(ok) {
      int year = yearString.toInt(0, 0);
      QDate newYear = QDate(year, 1, 1);
      if(newYear != budget.budgetStart()) {
        MyMoneyFileTransaction ft;
        try {
          budget.setBudgetStart(newYear);
          MyMoneyFile::instance()->modifyBudget(budget);
          ft.commit();
        } catch(MyMoneyException *e) {
          KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to modify budget: %1, thrown in %2:%3", e->what(), e->file(), e->line()));
          delete e;
        }
      }
    }
  }
}

void KMyMoney2App::slotBudgetForecast(void)
{
  if(d->m_selectedBudgets.size() == 1) {
    MyMoneyFileTransaction ft;
    try {
      MyMoneyBudget budget = d->m_selectedBudgets.first();
      bool calcBudget = budget.getaccounts().count() == 0;
      if(!calcBudget) {
        if(KMessageBox::warningContinueCancel(0, i18n("The current budget already contains data. Continuing will replace all current values of this budget."), i18nc("Warning message box", "Warning")) == KMessageBox::Continue)
          calcBudget = true;
      }

      if(calcBudget) {
        QDate historyStart;
        QDate historyEnd;
        QDate budgetStart;
        QDate budgetEnd;

        budgetStart = budget.budgetStart();
        budgetEnd = budgetStart.addYears(1).addDays(-1);
        historyStart = budgetStart.addYears(-1);
        historyEnd = budgetEnd.addYears(-1);

        MyMoneyForecast forecast;
        forecast.createBudget(budget, historyStart, historyEnd, budgetStart, budgetEnd, true);

        MyMoneyFile::instance()->modifyBudget(budget);
        ft.commit();
      }
    } catch(MyMoneyException *e) {
      KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to modify budget: %1, thrown in %2:%3",e->what(),e->file(),e->line()));
      delete e;
    }
  }
}

void KMyMoney2App::slotKDELanguageSettings(void)
{
  KMessageBox::information(this, i18n("Please be aware that changes made in the following dialog affect all KDE applications not only KMyMoney."), i18nc("Warning message box", "Warning"), "LanguageSettingsWarning");

  QStringList args;
  args << "language";
  QString error;
  int pid;

  KToolInvocation::kdeinitExec("kcmshell4", args, &error, &pid);
}

void KMyMoney2App::slotNewFeature(void)
{
}

void KMyMoney2App::slotTransactionsDelete(void)
{
  // since we may jump here via code, we have to make sure to react only
  // if the action is enabled
  if(!kmymoney2->action("transaction_delete")->isEnabled())
    return;
  if(d->m_selectedTransactions.count() == 0)
    return;
  if(d->m_selectedTransactions.warnLevel() == 1) {
    if(KMessageBox::warningContinueCancel(0,
        i18n("At least one split of the selected transactions has been reconciled. "
            "Do you wish to delete the transactions anyway?"),
            i18n("Transaction already reconciled"), KStandardGuiItem::cont(),
                KGuiItem("DeleteReconciledTransaction")) == KMessageBox::Cancel)
                 return;
  }
  QString msg;
  if(d->m_selectedTransactions.count() == 1) {
    msg = i18n("Do you really want to delete the selected transaction?");
  } else {
    msg = i18n("Do you really want to delete all %1 selected transactions?",d->m_selectedTransactions.count());
  }
  if(KMessageBox::questionYesNo(this, msg, i18n("Delete transaction")) == KMessageBox::Yes) {
    KMSTATUS(i18n("Deleting transactions"));
    doDeleteTransactions();
  }
}

void KMyMoney2App::slotTransactionDuplicate(void)
{
  // since we may jump here via code, we have to make sure to react only
  // if the action is enabled
  if(kmymoney2->action("transaction_duplicate")->isEnabled()) {
    KMyMoneyRegister::SelectedTransactions list = d->m_selectedTransactions;
    KMyMoneyRegister::SelectedTransactions::iterator it_t;

    int i = 0;
    int cnt = d->m_selectedTransactions.count();
    KMSTATUS(i18n("Duplicating transactions"));
    slotStatusProgressBar(0, cnt);
    MyMoneyFileTransaction ft;
    MyMoneyTransaction lt;
    try {
      for(it_t = list.begin(); it_t != list.end(); ++it_t) {
        MyMoneyTransaction t = (*it_t).transaction();
        QList<MyMoneySplit>::iterator it_s;
        // wipe out any reconciliation information
        for(it_s = t.splits().begin(); it_s != t.splits().end(); ++it_s) {
          (*it_s).setReconcileFlag(MyMoneySplit::NotReconciled);
          (*it_s).setReconcileDate(QDate());
          (*it_s).setBankID(QString());
        }
        // clear invalid data
        t.setEntryDate(QDate());
        t.clearId();
        // and set the post date to today
        t.setPostDate(QDate::currentDate());

        MyMoneyFile::instance()->addTransaction(t);
        lt = t;
        slotStatusProgressBar(i++, 0);
      }
      ft.commit();

      // select the new transaction in the ledger
      if(!d->m_selectedAccount.id().isEmpty())
        d->m_myMoneyView->slotLedgerSelected(d->m_selectedAccount.id(), lt.id());

    } catch(MyMoneyException* e) {
      KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to duplicate transaction(s): %1, thrown in %2:%3",e->what(),e->file(),e->line()));
      delete e;
    }
    // switch off the progress bar
    slotStatusProgressBar(-1, -1);
  }
}

void KMyMoney2App::doDeleteTransactions(void)
{
  KMyMoneyRegister::SelectedTransactions list = d->m_selectedTransactions;
  KMyMoneyRegister::SelectedTransactions::const_iterator it_t;
  int cnt = list.count();
  int i = 0;
  slotStatusProgressBar(0, cnt);
  MyMoneyFileTransaction ft;
  MyMoneyFile* file = MyMoneyFile::instance();
  try {
    for(it_t = list.constBegin(); it_t != list.constEnd(); ++it_t) {
      // only remove those transactions that do not reference a closed account
      if(!file->referencesClosedAccount((*it_t).transaction()))
        file->removeTransaction((*it_t).transaction());
      slotStatusProgressBar(i++, 0);
    }
    ft.commit();
  } catch(MyMoneyException* e) {
      KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to delete transaction(s): %1, thrown in %2:%3",e->what(),e->file(),e->line()));
    delete e;
  }
  slotStatusProgressBar(-1, -1);
}

void KMyMoney2App::slotTransactionsNew(void)
{
  // since we jump here via code, we have to make sure to react only
  // if the action is enabled
  if(kmymoney2->action("transaction_new")->isEnabled()) {
    if(d->m_myMoneyView->createNewTransaction()) {
      d->m_transactionEditor = d->m_myMoneyView->startEdit(d->m_selectedTransactions);
       if(d->m_transactionEditor){
      KMyMoneyPayeeCombo* payeeEdit = dynamic_cast<KMyMoneyPayeeCombo*>(d->m_transactionEditor->haveWidget("payee"));
      if(payeeEdit && !d->m_lastPayeeEntered.isEmpty()) {
        // in case we entered a new transaction before and used a payee,
        // we reuse it here. Save the text to the edit widget, select it
        // so that hitting any character will start entering another payee.
        // close the completion list
        payeeEdit->setCurrentText(d->m_lastPayeeEntered);
        payeeEdit->completion()->slotMakeCompletion(d->m_lastPayeeEntered);
        QStringList payeeId;
        payeeEdit->selector()->selectedItems(payeeId);
        if(payeeId.count() == 1) {
          payeeEdit->setSelectedItem(payeeId[0]);
        }
        payeeEdit->lineEdit()->selectAll();
        payeeEdit->completion()->hide();
      }
      if(d->m_transactionEditor) {
        connect(d->m_transactionEditor, SIGNAL(statusProgress(int, int)), this, SLOT(slotStatusProgressBar(int, int)));
        connect(d->m_transactionEditor, SIGNAL(statusMsg(const QString&)), this, SLOT(slotStatusMsg(const QString&)));
        connect(d->m_transactionEditor, SIGNAL(scheduleTransaction(const MyMoneyTransaction&, MyMoneySchedule::occurrenceE)), this, SLOT(slotScheduleNew(const MyMoneyTransaction&, MyMoneySchedule::occurrenceE)));
      }
      slotUpdateActions();
    }
   }
  }
}

void KMyMoney2App::slotTransactionsEdit(void)
{
  // qDebug("KMyMoney2App::slotTransactionsEdit()");
  // since we jump here via code, we have to make sure to react only
  // if the action is enabled
  if(kmymoney2->action("transaction_edit")->isEnabled()) {
    // as soon as we edit a transaction, we don't remember the last payee entered
    d->m_lastPayeeEntered = QString();
    d->m_transactionEditor = d->m_myMoneyView->startEdit(d->m_selectedTransactions);
    slotUpdateActions();
  }
}

void KMyMoney2App::deleteTransactionEditor(void)
{
  // make sure, we don't use the transaction editor pointer
  // anymore from now on
  TransactionEditor* p = d->m_transactionEditor;
  d->m_transactionEditor = 0;
  delete p;
}

void KMyMoney2App::slotTransactionsEditSplits(void)
{
  // since we jump here via code, we have to make sure to react only
  // if the action is enabled
  if(kmymoney2->action("transaction_editsplits")->isEnabled()) {
    // as soon as we edit a transaction, we don't remember the last payee entered
    d->m_lastPayeeEntered = QString();
    d->m_transactionEditor = d->m_myMoneyView->startEdit(d->m_selectedTransactions);
    slotUpdateActions();

    if(d->m_transactionEditor) {
      if(d->m_transactionEditor->slotEditSplits() == QDialog::Accepted) {
        MyMoneyFileTransaction ft;
        try {
          QString id;
          connect(d->m_transactionEditor, SIGNAL(balanceWarning(QWidget*, const MyMoneyAccount&, const QString&)), d->m_balanceWarning, SLOT(slotShowMessage(QWidget*, const MyMoneyAccount&, const QString&)));
          d->m_transactionEditor->enterTransactions(id);
          ft.commit();
        } catch(MyMoneyException* e) {
          KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to modify transaction: %1, thrown in %2:%3",e->what(),e->file(),e->line()));
          delete e;
        }
      }
    }
    deleteTransactionEditor();
    slotUpdateActions();
  }
}

void KMyMoney2App::slotTransactionsCancel(void)
{
  // since we jump here via code, we have to make sure to react only
  // if the action is enabled
  if(kmymoney2->action("transaction_cancel")->isEnabled()) {
    // make sure, we block the enter function
    action("transaction_enter")->setEnabled(false);
    // qDebug("KMyMoney2App::slotTransactionsCancel");
    deleteTransactionEditor();
    slotUpdateActions();
  }
}

void KMyMoney2App::slotTransactionsEnter(void)
{
  // since we jump here via code, we have to make sure to react only
  // if the action is enabled
  if(kmymoney2->action("transaction_enter")->isEnabled()) {
    if(d->m_transactionEditor) {
      QString accountId = d->m_selectedAccount.id();
      QString newId;
      connect(d->m_transactionEditor, SIGNAL(balanceWarning(QWidget*, const MyMoneyAccount&, const QString&)), d->m_balanceWarning, SLOT(slotShowMessage(QWidget*, const MyMoneyAccount&, const QString&)));
      if(d->m_transactionEditor->enterTransactions(newId)) {
        KMyMoneyPayeeCombo* payeeEdit = dynamic_cast<KMyMoneyPayeeCombo*>(d->m_transactionEditor->haveWidget("payee"));
        if(payeeEdit && !newId.isEmpty()) {
          d->m_lastPayeeEntered = payeeEdit->currentText();
        }
        deleteTransactionEditor();
      }
      if(!newId.isEmpty()) {
        d->m_myMoneyView->slotLedgerSelected(accountId, newId);
      }
    }
    slotUpdateActions();
  }
}

void KMyMoney2App::slotTransactionsCancelOrEnter(bool& okToSelect)
{
  static bool oneTime = false;
  if(!oneTime) {
    oneTime = true;
    QString dontShowAgain = "CancelOrEditTransaction";
    // qDebug("KMyMoney2App::slotCancelOrEndEdit");
    if(d->m_transactionEditor) {
      if(KMyMoneyGlobalSettings::focusChangeIsEnter() && kmymoney2->action("transaction_enter")->isEnabled()) {
        slotTransactionsEnter();
      } else {
        // okToSelect is preset to true if a cancel of the dialog is useful and false if it is not
        int rc;
        if(okToSelect == true) {
          rc = KMessageBox::warningYesNoCancel(0, QString("<p>")+i18n("Do you really want to cancel editing this transaction without saving it?</p>- <b>Yes</b> cancels editing the transaction<br/>- <b>No</b> saves the transaction prior to cancelling and<br/>- <b>Cancel</b> returns to the transaction editor.<p>You can also select an option to save the transaction automatically when e.g. selecting another transaction.</p>"), i18n("Cancel transaction edit"), KStandardGuiItem::yes(), KStandardGuiItem::no(), KStandardGuiItem::cancel(), dontShowAgain);

        } else {
          rc = KMessageBox::warningYesNo(0, QString("<p>")+i18n("Do you really want to cancel editing this transaction without saving it?</p>- <b>Yes</b> cancels editing the transaction<br/>- <b>No</b> saves the transaction prior to cancelling.<p>You can also select an option to save the transaction automatically when e.g. selecting another transaction.</p>"), i18n("Cancel transaction edit"), KStandardGuiItem::yes(), KStandardGuiItem::no(), dontShowAgain);
        }

        switch(rc) {
          case KMessageBox::Yes:
            slotTransactionsCancel();
            break;
          case KMessageBox::No:
            slotTransactionsEnter();
            // make sure that we'll see this message the next time no matter
            // if the user has chosen the 'Don't show again' checkbox
            KMessageBox::enableMessage(dontShowAgain);
            break;
          case KMessageBox::Cancel:
            // make sure that we'll see this message the next time no matter
            // if the user has chosen the 'Don't show again' checkbox
            KMessageBox::enableMessage(dontShowAgain);
            okToSelect = false;
            break;
        }
      }
    }
    oneTime = false;
  }
}

void KMyMoney2App::slotToggleReconciliationFlag(void)
{
  markTransaction(MyMoneySplit::Unknown);
}

void KMyMoney2App::slotMarkTransactionCleared(void)
{
  markTransaction(MyMoneySplit::Cleared);
}

void KMyMoney2App::slotMarkTransactionReconciled(void)
{
  markTransaction(MyMoneySplit::Reconciled);
}

void KMyMoney2App::slotMarkTransactionNotReconciled(void)
{
  markTransaction(MyMoneySplit::NotReconciled);
}

void KMyMoney2App::markTransaction(MyMoneySplit::reconcileFlagE flag)
{
  KMyMoneyRegister::SelectedTransactions list = d->m_selectedTransactions;
  KMyMoneyRegister::SelectedTransactions::const_iterator it_t;
  int cnt = list.count();
  int i = 0;
  slotStatusProgressBar(0, cnt);
  MyMoneyFileTransaction ft;
  try {
    for(it_t = list.constBegin(); it_t != list.constEnd(); ++it_t) {
      // turn on signals before we modify the last entry in the list
      cnt--;
      MyMoneyFile::instance()->blockSignals(cnt != 0);

      // get a fresh copy
      MyMoneyTransaction t = MyMoneyFile::instance()->transaction((*it_t).transaction().id());
      MyMoneySplit sp = t.splitById((*it_t).split().id());
      if(sp.reconcileFlag() != flag) {
        if(flag == MyMoneySplit::Unknown) {
          if(d->m_reconciliationAccount.id().isEmpty()) {
            // in normal mode we cycle through all states
            switch(sp.reconcileFlag()) {
              case MyMoneySplit::NotReconciled:
                sp.setReconcileFlag(MyMoneySplit::Cleared);
                break;
              case MyMoneySplit::Cleared:
                sp.setReconcileFlag(MyMoneySplit::Reconciled);
                break;
              case MyMoneySplit::Reconciled:
                sp.setReconcileFlag(MyMoneySplit::NotReconciled);
                break;
              default:
                break;
            }
          } else {
            // in reconciliation mode we skip the reconciled state
            switch(sp.reconcileFlag()) {
              case MyMoneySplit::NotReconciled:
                sp.setReconcileFlag(MyMoneySplit::Cleared);
                break;
              case MyMoneySplit::Cleared:
                sp.setReconcileFlag(MyMoneySplit::NotReconciled);
                break;
              default:
                break;
            }
          }
        } else {
          sp.setReconcileFlag(flag);
        }

        t.modifySplit(sp);
        MyMoneyFile::instance()->modifyTransaction(t);
      }
      slotStatusProgressBar(i++, 0);
    }
    slotStatusProgressBar(-1, -1);
    ft.commit();
  } catch(MyMoneyException* e) {
    KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to modify transaction: %1, thrown in %2:%3",e->what(),e->file(),e->line()));
    delete e;
  }
}

void KMyMoney2App::slotTransactionsAccept(void)
{
  KMyMoneyRegister::SelectedTransactions list = d->m_selectedTransactions;
  KMyMoneyRegister::SelectedTransactions::const_iterator it_t;
  int cnt = list.count();
  int i = 0;
  slotStatusProgressBar(0, cnt);
  MyMoneyFileTransaction ft;
  try {
    for(it_t = list.constBegin(); it_t != list.constEnd(); ++it_t) {
      // reload transaction in case it got changed during the course of this loop
      MyMoneyTransaction t = MyMoneyFile::instance()->transaction((*it_t).transaction().id());
      if(t.isImported()) {
        t.setImported(false);
        if(!d->m_selectedAccount.id().isEmpty()) {
          QList<MyMoneySplit> list = t.splits();
          QList<MyMoneySplit>::const_iterator it_s;
          for(it_s = list.constBegin(); it_s != list.constEnd(); ++it_s) {
            if((*it_s).accountId() == d->m_selectedAccount.id()) {
              if((*it_s).reconcileFlag() == MyMoneySplit::NotReconciled) {
                MyMoneySplit s = (*it_s);
                s.setReconcileFlag(MyMoneySplit::Cleared);
                t.modifySplit(s);
              }
            }
          }
        }
        MyMoneyFile::instance()->modifyTransaction(t);
      }
      if((*it_t).split().isMatched()) {
        // reload split in case it got changed during the course of this loop
        MyMoneySplit s = t.splitById((*it_t).split().id());
        TransactionMatcher matcher(d->m_selectedAccount);
        matcher.accept(t, s);
      }
      slotStatusProgressBar(i++, 0);
    }
    slotStatusProgressBar(-1, -1);
    ft.commit();
  } catch(MyMoneyException* e) {
    KMessageBox::detailedSorry(0, i18n("Error"), i18n("Unable to accept transaction: %1, thrown in %2:%3",e->what(),e->file(),e->line()));
    delete e;
  }
}

void KMyMoney2App::slotTransactionGotoAccount(void)
{
  if(!d->m_accountGoto.isEmpty()) {
    try {
      QString transactionId;
      if(d->m_selectedTransactions.count() == 1) {
        transactionId = d->m_selectedTransactions[0].transaction().id();
      }
      // make sure to pass a copy, as d->myMoneyView->slotLedgerSelected() overrides
      // d->m_accountGoto while calling slotUpdateActions()
      QString accountId = d->m_accountGoto;
      d->m_myMoneyView->slotLedgerSelected(accountId, transactionId);
    } catch(MyMoneyException* e) {
      delete e;
    }
  }
}

void KMyMoney2App::slotTransactionGotoPayee(void)
{
  if(!d->m_payeeGoto.isEmpty()) {
    try {
      QString transactionId;
      if(d->m_selectedTransactions.count() == 1) {
        transactionId = d->m_selectedTransactions[0].transaction().id();
      }
      // make sure to pass copies, as d->myMoneyView->slotPayeeSelected() overrides
      // d->m_payeeGoto and d->m_selectedAccount while calling slotUpdateActions()
      QString payeeId = d->m_payeeGoto;
      QString accountId = d->m_selectedAccount.id();
      d->m_myMoneyView->slotPayeeSelected(payeeId, accountId, transactionId);
    } catch(MyMoneyException* e) {
      delete e;
    }
  }
}

void KMyMoney2App::slotTransactionCreateSchedule(void)
{
  if(d->m_selectedTransactions.count() == 1) {
    // make sure to have the current selected split as first split in the schedule
    MyMoneyTransaction t = d->m_selectedTransactions[0].transaction();
    MyMoneySplit s = d->m_selectedTransactions[0].split();
    QString splitId = s.id();
    s.clearId();
    s.setReconcileFlag(MyMoneySplit::NotReconciled);
    s.setReconcileDate(QDate());
    t.removeSplits();
    t.addSplit(s);
    const QList<MyMoneySplit>& splits = d->m_selectedTransactions[0].transaction().splits();
    QList<MyMoneySplit>::const_iterator it_s;
    for(it_s = splits.begin(); it_s != splits.end(); ++it_s) {
      if((*it_s).id() != splitId) {
        MyMoneySplit s0 = (*it_s);
        s0.clearId();
        s0.setReconcileFlag(MyMoneySplit::NotReconciled);
        s0.setReconcileDate(QDate());
        t.addSplit(s0);
      }
    }
    slotScheduleNew(t);
  }
}

void KMyMoney2App::slotTransactionAssignNumber(void)
{
  if(d->m_transactionEditor)
    d->m_transactionEditor->assignNextNumber();
}

void KMyMoney2App::slotTransactionCombine(void)
{
  qDebug("slotTransactionCombine() not implemented yet");
}

void KMyMoney2App::slotMoveToAccount(const QString& id)
{
  // close the menu, if it is still open
  QWidget* w = factory()->container("transaction_move_menu", this);
  if(w) {
    if(w->isVisible()) {
      w->close();
    }
  }

  if(d->m_selectedTransactions.count() > 0) {
    MyMoneyFileTransaction ft;
    try {
      KMyMoneyRegister::SelectedTransactions::const_iterator it_t;
      for(it_t = d->m_selectedTransactions.constBegin(); it_t != d->m_selectedTransactions.constEnd(); ++it_t) {
        if (d->m_selectedAccount.accountType() == MyMoneyAccount::Investment) {
          d->moveInvestmentTransaction(d->m_selectedAccount.id(), id, (*it_t).transaction());
        } else {
          QList<MyMoneySplit>::const_iterator it_s;
          bool changed = false;
          MyMoneyTransaction t = (*it_t).transaction();
          for(it_s = (*it_t).transaction().splits().constBegin(); it_s != (*it_t).transaction().splits().constEnd(); ++it_s) {
            if((*it_s).accountId() == d->m_selectedAccount.id()) {
              MyMoneySplit s = (*it_s);
              s.setAccountId(id);
              t.modifySplit(s);
              changed = true;
            }
          }
          if(changed) {
            MyMoneyFile::instance()->modifyTransaction(t);
          }
        }
      }
      ft.commit();
    } catch(MyMoneyException *e) {
      delete e;
    }
  }
}

// move a stock transaction from one investment account to another
void KMyMoney2App::Private::moveInvestmentTransaction(const QString& /*fromId*/,
                                              const QString& toId,
                                              const MyMoneyTransaction& tx)
{
  MyMoneyAccount toInvAcc = MyMoneyFile::instance()->account(toId);
  MyMoneyTransaction t(tx);
  // first determine which stock we are dealing with.
  // fortunately, investment transactions have only one stock involved
  QString stockAccountId;
  QString stockSecurityId;
  QList<MyMoneySplit>::const_iterator it_s;
  for(it_s = t.splits().constBegin(); it_s != t.splits().constEnd(); ++it_s) {
     stockAccountId = (*it_s).accountId();
     stockSecurityId =
        MyMoneyFile::instance()->account(stockAccountId).currencyId();
     if (!MyMoneyFile::instance()->security(stockSecurityId).isCurrency())
       break;
  }
  // Now check the target investment account to see if it
  // contains a stock with this id
  QString newStockAccountId = QString();
  QStringList accountList = toInvAcc.accountList();
  QStringList::const_iterator it_a;
  for (it_a = accountList.constBegin(); it_a != accountList.constEnd(); ++it_a) {
    if (MyMoneyFile::instance()->account((*it_a)).currencyId() ==
        stockSecurityId) {
      newStockAccountId = (*it_a);
      break;
    }
  }
  // if it doesn't exist, we need to add it as a copy of the old one
  // no 'copyAccount()' function??
  if (newStockAccountId.isEmpty()) {
    MyMoneyAccount stockAccount =
        MyMoneyFile::instance()->account(stockAccountId);
    MyMoneyAccount newStock;
    newStock.setName(stockAccount.name());
    newStock.setNumber(stockAccount.number());
    newStock.setDescription(stockAccount.description());
    newStock.setInstitutionId(stockAccount.institutionId());
    newStock.setOpeningDate(stockAccount.openingDate());
    newStock.setAccountType(stockAccount.accountType());
    newStock.setCurrencyId(stockAccount.currencyId());
    newStock.setClosed(stockAccount.isClosed());
    MyMoneyFile::instance()->addAccount(newStock, toInvAcc);
    newStockAccountId = newStock.id();
  }
  // now update the split and the transaction
  MyMoneySplit s = (*it_s);
  s.setAccountId(newStockAccountId);
  t.modifySplit(s);
  MyMoneyFile::instance()->modifyTransaction(t);
}

void KMyMoney2App::slotUpdateMoveToAccountMenu(void)
{
  if(!d->m_selectedAccount.id().isEmpty()) {
    AccountSet accountSet;
    if(d->m_selectedAccount.accountType() == MyMoneyAccount::Investment) {
      accountSet.addAccountType(MyMoneyAccount::Investment);
    } else if(d->m_selectedAccount.isAssetLiability()) {

      accountSet.addAccountType(MyMoneyAccount::Checkings);
      accountSet.addAccountType(MyMoneyAccount::Savings);
      accountSet.addAccountType(MyMoneyAccount::Cash);
      accountSet.addAccountType(MyMoneyAccount::AssetLoan);
      accountSet.addAccountType(MyMoneyAccount::CertificateDep);
      accountSet.addAccountType(MyMoneyAccount::MoneyMarket);
      accountSet.addAccountType(MyMoneyAccount::Asset);
      accountSet.addAccountType(MyMoneyAccount::Currency);
      accountSet.addAccountType(MyMoneyAccount::CreditCard);
      accountSet.addAccountType(MyMoneyAccount::Loan);
      accountSet.addAccountType(MyMoneyAccount::Liability);
    } else if(d->m_selectedAccount.isIncomeExpense()) {
      accountSet.addAccountType(MyMoneyAccount::Income);
      accountSet.addAccountType(MyMoneyAccount::Expense);
    }

    accountSet.load(d->m_moveToAccountSelector);
    // remove those accounts that we currently reference
    KMyMoneyRegister::SelectedTransactions::const_iterator it_t;
    for(it_t = d->m_selectedTransactions.constBegin(); it_t != d->m_selectedTransactions.constEnd(); ++it_t) {
      QList<MyMoneySplit>::const_iterator it_s;
      for(it_s = (*it_t).transaction().splits().constBegin(); it_s != (*it_t).transaction().splits().constEnd(); ++it_s) {
        d->m_moveToAccountSelector->removeItem((*it_s).accountId());
      }
    }
    // remove those accounts from the list that are denominated
    // in a different currency
    QStringList list = d->m_moveToAccountSelector->accountList();
    QList<QString>::const_iterator it_a;
    for(it_a = list.constBegin(); it_a != list.constEnd(); ++it_a) {
      MyMoneyAccount acc = MyMoneyFile::instance()->account(*it_a);
      if(acc.currencyId() != d->m_selectedAccount.currencyId())
        d->m_moveToAccountSelector->removeItem((*it_a));
    }
    // Now update the width of the list
    d->m_moveToAccountSelector->setOptimizedWidth();
  }
}

void KMyMoney2App::slotTransactionMatch(void)
{
  if(action("transaction_match")->text() == i18nc("Button text for match transaction", "Match"))
    transactionMatch();
  else
    transactionUnmatch();
}

void KMyMoney2App::transactionUnmatch(void)
{
  KMyMoneyRegister::SelectedTransactions::const_iterator it;
  MyMoneyFileTransaction ft;
  try {
    for(it = d->m_selectedTransactions.constBegin(); it != d->m_selectedTransactions.constEnd(); ++it) {
      if((*it).split().isMatched()) {
        TransactionMatcher matcher(d->m_selectedAccount);
        matcher.unmatch((*it).transaction(), (*it).split());
      }
    }
    ft.commit();

  } catch(MyMoneyException *e) {
    KMessageBox::detailedSorry(0, i18n("Unable to unmatch the selected transactions"), e->what() );
    delete e;
  }
}

void KMyMoney2App::transactionMatch(void)
{
  if(d->m_selectedTransactions.count() != 2)
    return;

  MyMoneyTransaction startMatchTransaction;
  MyMoneyTransaction endMatchTransaction;
  MyMoneySplit startSplit;
  MyMoneySplit endSplit;

  KMyMoneyRegister::SelectedTransactions::const_iterator it;
  KMyMoneyRegister::SelectedTransactions toBeDeleted;
  for(it = d->m_selectedTransactions.constBegin(); it != d->m_selectedTransactions.constEnd(); ++it) {
    if((*it).transaction().isImported()) {
      endMatchTransaction = (*it).transaction();
      endSplit = (*it).split();
      toBeDeleted << *it;
    } else if(!(*it).split().isMatched()) {
      startMatchTransaction = (*it).transaction();
      startSplit = (*it).split();
    }
  }

#if 0
  KMergeTransactionsDlg dlg(d->m_selectedAccount);
  dlg.addTransaction(startMatchTransaction);
  dlg.addTransaction(endMatchTransaction);
  if (dlg.exec() == QDialog::Accepted)
#endif
  {
    MyMoneyFileTransaction ft;
    try
    {
      if(startMatchTransaction.id().isEmpty())
        throw new MYMONEYEXCEPTION(i18n("No manually entered transaction selected for matching"));
      if(endMatchTransaction.id().isEmpty())
        throw new MYMONEYEXCEPTION(i18n("No imported transaction selected for matching"));

      TransactionMatcher matcher(d->m_selectedAccount);
      matcher.match(startMatchTransaction, startSplit, endMatchTransaction, endSplit);
      ft.commit();
    }
    catch(MyMoneyException *e)
    {
      KMessageBox::detailedSorry(0, i18n("Unable to match the selected transactions"), e->what() );
      delete e;
    }
  }
}


void KMyMoney2App::showContextMenu(const QString& containerName)
{
  QWidget* w = factory()->container(containerName, this);
  KMenu *menu = dynamic_cast<KMenu*>(w);
  if(menu)
    menu->exec(QCursor::pos());
}

void KMyMoney2App::slotShowTransactionContextMenu(void)
{
  if(d->m_selectedTransactions.count() == 0 && d->m_selectedSchedule != MyMoneySchedule()) {
    showContextMenu("schedule_context_menu");
  } else {
    showContextMenu("transaction_context_menu");
  }
}

void KMyMoney2App::slotShowInvestmentContextMenu(void)
{
  showContextMenu("investment_context_menu");
}

void KMyMoney2App::slotShowScheduleContextMenu(void)
{
  showContextMenu("schedule_context_menu");
}

void KMyMoney2App::slotShowAccountContextMenu(const MyMoneyObject& obj)
{
//  qDebug("KMyMoney2App::slotShowAccountContextMenu");
  if(typeid(obj) != typeid(MyMoneyAccount))
    return;

  const MyMoneyAccount& acc = dynamic_cast<const MyMoneyAccount&>(obj);

  // if the selected account is actually a stock account, we
  // call the right slot instead
  if(acc.isInvest()) {
    showContextMenu("investment_context_menu");
  } else if(acc.isIncomeExpense()){
    showContextMenu("category_context_menu");
  } else {
    showContextMenu("account_context_menu");
  }
}

void KMyMoney2App::slotShowInstitutionContextMenu(const MyMoneyObject& obj)
{
  if(typeid(obj) != typeid(MyMoneyInstitution))
    return;

  showContextMenu("institution_context_menu");
}

void KMyMoney2App::slotShowPayeeContextMenu(void)
{
  showContextMenu("payee_context_menu");
}

void KMyMoney2App::slotShowBudgetContextMenu(void)
{
  showContextMenu("budget_context_menu");
}

void KMyMoney2App::slotShowCurrencyContextMenu(void)
{
  showContextMenu("currency_context_menu");
}

void KMyMoney2App::slotPrintView(void)
{
  d->m_myMoneyView->slotPrintView();
}

void KMyMoney2App::updateCaption(bool skipActions)
{
  QString caption;

  caption = d->m_fileName.fileName();

  if(caption.isEmpty() && d->m_myMoneyView && d->m_myMoneyView->fileOpen())
    caption = i18n("Untitled");

  // MyMoneyFile::instance()->dirty() throws an exception, if
  // there's no storage object available. In this case, we
  // assume that the storage object is not changed. Actually,
  // this can only happen if we are newly created early on.
  bool modified;
  try {
    modified = MyMoneyFile::instance()->dirty();
  } catch(MyMoneyException *e) {
    delete e;
    modified = false;
    skipActions = true;
  }

#if KMM_DEBUG
  caption += QString(" (%1 x %2)").arg(width()).arg(height());
#endif
  if(modified) {
    caption = KDialog::makeStandardCaption(caption, false, KDialog::ModifiedCaption);
  } else {
    caption = KDialog::makeStandardCaption(caption, false, KDialog::NoCaptionFlags);
  }

  if(caption.length() > 0)
    caption += " - ";
  caption += "KMyMoney";
  setPlainCaption(caption);

  if(!skipActions) {
    d->m_myMoneyView->enableViews();
    slotUpdateActions();
  }
}

void KMyMoney2App::slotUpdateActions(void)
{
  MyMoneyFile* file = MyMoneyFile::instance();
  bool fileOpen = d->m_myMoneyView->fileOpen();
  bool modified = file->dirty();
  QWidget* w;

  action("open_database")->setEnabled(true);
  action("saveas_database")->setEnabled(fileOpen);
  action("file_save")->setEnabled(modified && !d->m_myMoneyView->isDatabase());
  action("file_save_as")->setEnabled(fileOpen);
  action("file_close")->setEnabled(fileOpen);
  action("view_personal_data")->setEnabled(fileOpen);
  action("file_backup")->setEnabled(fileOpen && !d->m_myMoneyView->isDatabase());
  action("file_print")->setEnabled(fileOpen && d->m_myMoneyView->canPrint());
#if KMM_DEBUG
  action("view_file_info")->setEnabled(fileOpen);
  action("file_dump")->setEnabled(fileOpen);
#endif

  action("edit_find_transaction")->setEnabled(fileOpen);

  bool importRunning = (d->m_qifReader != 0) || (d->m_smtReader != 0);
  action("file_export_qif")->setEnabled(fileOpen && !importRunning);
  action("file_import_qif")->setEnabled(fileOpen && !importRunning);
  action("file_import_gnc")->setEnabled(!importRunning);
  action("file_import_template")->setEnabled(fileOpen && !importRunning);
  action("file_export_template")->setEnabled(fileOpen && !importRunning);


  action("tools_security_editor")->setEnabled(fileOpen);
  action("tools_currency_editor")->setEnabled(fileOpen);
  action("tools_price_editor")->setEnabled(fileOpen);
  action("tools_update_prices")->setEnabled(fileOpen);
  action("tools_consistency_check")->setEnabled(fileOpen);

  action("account_new")->setEnabled(fileOpen);
  action("account_reconcile")->setEnabled(false);
  action("account_reconcile_finish")->setEnabled(false);
  action("account_reconcile_postpone")->setEnabled(false);
  action("account_edit")->setEnabled(false);
  action("account_delete")->setEnabled(false);
  action("account_open")->setEnabled(false);
  action("account_close")->setEnabled(false);
  action("account_reopen")->setEnabled(false);
  action("account_transaction_report")->setEnabled(false);
  action("account_online_map")->setEnabled(false);
  action("account_online_update")->setEnabled(false);
  action("account_online_update_all")->setEnabled(false);
  //action("account_online_update_menu")->setEnabled(false);
  action("account_online_unmap")->setEnabled(false);
  action("account_chart")->setEnabled(false);

  action("category_new")->setEnabled(fileOpen);
  action("category_edit")->setEnabled(false);
  action("category_delete")->setEnabled(false);

  action("institution_new")->setEnabled(fileOpen);
  action("institution_edit")->setEnabled(false);
  action("institution_delete")->setEnabled(false);
  action("investment_new")->setEnabled(false);
  action("investment_edit")->setEnabled(false);
  action("investment_delete")->setEnabled(false);
  action("investment_online_price_update")->setEnabled(false);
  action("investment_manual_price_update")->setEnabled(false);

  action("schedule_edit")->setEnabled(false);
  action("schedule_delete")->setEnabled(false);
  action("schedule_enter")->setEnabled(false);
  action("schedule_skip")->setEnabled(false);

  action("payee_delete")->setEnabled(false);
  action("payee_rename")->setEnabled(false);

  action("budget_delete")->setEnabled(false);
  action("budget_rename")->setEnabled(false);
  action("budget_change_year")->setEnabled(false);
  action("budget_new")->setEnabled(true);
  action("budget_copy")->setEnabled(false);
  action("budget_forecast")->setEnabled(false);

  QString tooltip = i18n("Create a new transaction");
  action("transaction_new")->setEnabled(fileOpen && d->m_myMoneyView->canCreateTransactions(KMyMoneyRegister::SelectedTransactions(), tooltip));
  action("transaction_new")->setToolTip(tooltip);

  action("transaction_edit")->setEnabled(false);
  action("transaction_editsplits")->setEnabled(false);
  action("transaction_enter")->setEnabled(false);
  action("transaction_cancel")->setEnabled(false);
  action("transaction_delete")->setEnabled(false);
  action("transaction_match")->setEnabled(false);
  action("transaction_match")->setText(i18nc("Button text for match transaction", "Match"));
  action("transaction_match")->setIcon(QIcon("connect_creating"));

  action("transaction_accept")->setEnabled(false);
  action("transaction_duplicate")->setEnabled(false);
  action("transaction_mark_toggle")->setEnabled(false);
  action("transaction_mark_cleared")->setEnabled(false);
  action("transaction_mark_reconciled")->setEnabled(false);
  action("transaction_mark_notreconciled")->setEnabled(false);
  action("transaction_goto_account")->setEnabled(false);
  action("transaction_goto_payee")->setEnabled(false);
  action("transaction_assign_number")->setEnabled(false);
  action("transaction_create_schedule")->setEnabled(false);
  action("transaction_combine")->setEnabled(false);
  action("transaction_select_all")->setEnabled(false);

  action("schedule_new")->setEnabled(fileOpen);
  action("schedule_edit")->setEnabled(false);
  action("schedule_delete")->setEnabled(false);
  action("schedule_duplicate")->setEnabled(false);
  action("schedule_enter")->setEnabled(false);
  action("schedule_skip")->setEnabled(false);

  action("currency_new")->setEnabled(fileOpen);
  action("currency_rename")->setEnabled(false);
  action("currency_delete")->setEnabled(false);
  action("currency_setbase")->setEnabled(false);

  w = factory()->container("transaction_move_menu", this);
  if(w)
    w->setEnabled(false);

  w = factory()->container("transaction_mark_menu", this);
  if(w)
    w->setEnabled(false);

  w = factory()->container("transaction_context_mark_menu", this);
  if(w)
    w->setEnabled(false);

  // FIXME for now it's always on, but we should only allow it, if we
  //       can select at least a single transaction
  action("transaction_select_all")->setEnabled(true);
  if(!d->m_selectedTransactions.isEmpty()) {
    if(d->m_selectedTransactions.count() != 0) {
      // enable 'delete transaction' only if at least one of the
      // selected transactions does not reference a closed account
      bool enable = false;
      KMyMoneyRegister::SelectedTransactions::const_iterator it_t;
      for(it_t = d->m_selectedTransactions.constBegin(); (enable == false) && (it_t != d->m_selectedTransactions.constEnd()); ++it_t) {
        enable = !file->referencesClosedAccount((*it_t).transaction());
      }
      action("transaction_delete")->setEnabled(enable);
    }

    if(!d->m_transactionEditor) {
      tooltip = i18n("Duplicate the current selected transactions");
      action("transaction_duplicate")->setEnabled(d->m_myMoneyView->canDuplicateTransactions(d->m_selectedTransactions, tooltip) && !d->m_selectedTransactions[0].transaction().id().isEmpty());
      action("transaction_duplicate")->setToolTip(tooltip);
      if(d->m_myMoneyView->canEditTransactions(d->m_selectedTransactions, tooltip)) {
        action("transaction_edit")->setEnabled(true);
        // editing splits is allowed only if we have one transaction selected
        if(d->m_selectedTransactions.count() == 1) {
          action("transaction_editsplits")->setEnabled(true);
        }
        if(d->m_selectedAccount.isAssetLiability() && d->m_selectedAccount.accountType() != MyMoneyAccount::Investment) {
          action("transaction_create_schedule")->setEnabled(d->m_selectedTransactions.count() == 1);
        }
      }
      action("transaction_edit")->setToolTip(tooltip);

      if(!d->m_selectedAccount.isClosed()) {
        w = factory()->container("transaction_move_menu", this);
        if(w)
          w->setEnabled(true);
      }

      w = factory()->container("transaction_mark_menu", this);
      if(w)
        w->setEnabled(true);

      w = factory()->container("transaction_context_mark_menu", this);
      if(w)
        w->setEnabled(true);

      // Allow marking the transaction if at least one is selected
      action("transaction_mark_cleared")->setEnabled(true);
      action("transaction_mark_reconciled")->setEnabled(true);
      action("transaction_mark_notreconciled")->setEnabled(true);
      action("transaction_mark_toggle")->setEnabled(true);

      if(!d->m_accountGoto.isEmpty())
        action("transaction_goto_account")->setEnabled(true);
      if(!d->m_payeeGoto.isEmpty())
        action("transaction_goto_payee")->setEnabled(true);

      // Matching is enabled as soon as one regular and one imported transaction is selected
      int matchedCount = 0;
      int importedCount = 0;
      KMyMoneyRegister::SelectedTransactions::const_iterator it;
      for(it = d->m_selectedTransactions.constBegin(); it != d->m_selectedTransactions.constEnd(); ++it) {
        if((*it).transaction().isImported())
          ++importedCount;
        if((*it).split().isMatched())
          ++matchedCount;
      }

      if(d->m_selectedTransactions.count() == 2 /* && action("transaction_edit")->isEnabled() */) {
        if(importedCount == 1 && matchedCount == 0) {
          action("transaction_match")->setEnabled(true);
        }
      }
      if(importedCount != 0 || matchedCount != 0)
        action("transaction_accept")->setEnabled(true);
      if(matchedCount != 0) {
        action("transaction_match")->setEnabled(true);
        action("transaction_match")->setText(i18nc("Button text for unmatch transaction", "Unmatch"));
        action("transaction_match")->setIcon(QIcon("process-stop"));
      }

      if(d->m_selectedTransactions.count() > 1) {
        action("transaction_combine")->setEnabled(true);
      }
    } else {
      action("transaction_assign_number")->setEnabled(d->m_transactionEditor->canAssignNumber());
      action("transaction_new")->setEnabled(false);
      action("transaction_delete")->setEnabled(false);
      action("transaction_enter")->setEnabled(d->m_transactionEditor->isComplete());
      action("transaction_cancel")->setEnabled(true);
    }
  }

  QList<MyMoneyAccount> accList;
  file->accountList(accList);
  QList<MyMoneyAccount>::const_iterator it_a;
  QMap<QString, KMyMoneyPlugin::OnlinePlugin*>::const_iterator it_p = d->m_onlinePlugins.constEnd();
  for(it_a = accList.constBegin(); (it_p == d->m_onlinePlugins.constEnd()) && (it_a != accList.constEnd()); ++it_a) {
    if ( !(*it_a).onlineBankingSettings().value("provider").isEmpty() ) {
      // check if provider is available
      it_p = d->m_onlinePlugins.constFind((*it_a).onlineBankingSettings().value("provider"));
      if(it_p != d->m_onlinePlugins.constEnd()) {
        QStringList protocols;
        (*it_p)->protocols(protocols);
        if(protocols.count() > 0) {
          action("account_online_update_all")->setEnabled(true);
          action("account_online_update_menu")->setEnabled(true);
        }
      }
    }
  }
  MyMoneyFileBitArray skip(IMyMoneyStorage::MaxRefCheckBits);
  if(!d->m_selectedAccount.id().isEmpty()) {
    if(!file->isStandardAccount(d->m_selectedAccount.id())) {
      switch(d->m_selectedAccount.accountGroup()) {
        case MyMoneyAccount::Asset:
        case MyMoneyAccount::Liability:
        case MyMoneyAccount::Equity:
          action("account_transaction_report")->setEnabled(true);
          action("account_edit")->setEnabled(true);
          action("account_delete")->setEnabled(!file->isReferenced(d->m_selectedAccount));
          action("account_open")->setEnabled(true);
          if(d->m_selectedAccount.accountGroup() != MyMoneyAccount::Equity) {
            if(d->m_reconciliationAccount.id().isEmpty()) {
              action("account_reconcile")->setEnabled(true);
            } else {
              if(!d->m_transactionEditor) {
                action("account_reconcile_finish")->setEnabled(d->m_selectedAccount.id() == d->m_reconciliationAccount.id());
                action("account_reconcile_postpone")->setEnabled(d->m_selectedAccount.id() == d->m_reconciliationAccount.id());
              }
            }
          }

          if(d->m_selectedAccount.accountType() == MyMoneyAccount::Investment)
            action("investment_new")->setEnabled(true);

          if(d->m_selectedAccount.isClosed())
            action("account_reopen")->setEnabled(true);
          else if(canCloseAccount(d->m_selectedAccount))
            action("account_close")->setEnabled(true);

          if ( !d->m_selectedAccount.onlineBankingSettings().value("provider").isEmpty() ) {
            action("account_online_unmap")->setEnabled(true);
            // check if provider is available
            QMap<QString, KMyMoneyPlugin::OnlinePlugin*>::const_iterator it_p;
            it_p = d->m_onlinePlugins.constFind(d->m_selectedAccount.onlineBankingSettings().value("provider"));
            if(it_p != d->m_onlinePlugins.constEnd()) {
              QStringList protocols;
              (*it_p)->protocols(protocols);
              if(protocols.count() > 0) {
                action("account_online_update")->setEnabled(true);
                action("account_online_update_menu")->setEnabled(true);
              }
            }
          } else {
            action("account_online_map")->setEnabled(d->m_onlinePlugins.count() > 0);
          }

          action("account_chart")->setEnabled(true);
          break;

        case MyMoneyAccount::Income :
        case MyMoneyAccount::Expense :
          action("category_edit")->setEnabled(true);
          // enable delete action, if category/account itself is not referenced
          // by any object except accounts, because we want to allow
          // deleting of sub-categories. Also, we allow transactions, schedules and budgets
          // to be present because we can re-assign them during the delete process
          skip.fill(false);
          skip.setBit(IMyMoneyStorage::RefCheckTransaction);
          skip.setBit(IMyMoneyStorage::RefCheckAccount);
          skip.setBit(IMyMoneyStorage::RefCheckSchedule);
          skip.setBit(IMyMoneyStorage::RefCheckBudget);
          action("category_delete")->setEnabled(!file->isReferenced(d->m_selectedAccount, skip));
          action("account_open")->setEnabled(true);
        break;

        default:
          break;
      }
    }
  }

  if(!d->m_selectedInstitution.id().isEmpty()) {
    action("institution_edit")->setEnabled(true);
    action("institution_delete")->setEnabled(!file->isReferenced(d->m_selectedInstitution));
  }

  if(!d->m_selectedInvestment.id().isEmpty()) {
    action("investment_edit")->setEnabled(true);
    action("investment_delete")->setEnabled(!file->isReferenced(d->m_selectedInvestment));
    action("investment_manual_price_update")->setEnabled(true);
    try {
      MyMoneySecurity security = MyMoneyFile::instance()->security(d->m_selectedInvestment.currencyId());
      if(!security.value("kmm-online-source").isEmpty())
        action("investment_online_price_update")->setEnabled(true);

    } catch(MyMoneyException *e) {
      qDebug("Error retrieving security for investment %s: %s", qPrintable(d->m_selectedInvestment.name()), qPrintable(e->what()));
      delete e;
    }
    if(d->m_selectedInvestment.isClosed())
      action("account_reopen")->setEnabled(true);
    else if(canCloseAccount(d->m_selectedInvestment))
      action("account_close")->setEnabled(true);
  }

  if(!d->m_selectedSchedule.id().isEmpty()) {
    action("schedule_edit")->setEnabled(true);
    action("schedule_duplicate")->setEnabled(true);
    action("schedule_delete")->setEnabled(!file->isReferenced(d->m_selectedSchedule));
    if(!d->m_selectedSchedule.isFinished()) {
      action("schedule_enter")->setEnabled(true);
      // a schedule with a single occurrence cannot be skipped
      if(d->m_selectedSchedule.occurrence() != MyMoneySchedule::OCCUR_ONCE) {
        action("schedule_skip")->setEnabled(true);
      }
    }
  }

  if(d->m_selectedPayees.count() >= 1) {
    action("payee_rename")->setEnabled(d->m_selectedPayees.count() == 1);
    action("payee_delete")->setEnabled(true);
  }

  if(d->m_selectedBudgets.count() >= 1) {
    action("budget_delete")->setEnabled(true);
    if(d->m_selectedBudgets.count() == 1) {
      action("budget_change_year")->setEnabled(true);
      action("budget_copy")->setEnabled(true);
      action("budget_rename")->setEnabled(true);
      action("budget_forecast")->setEnabled(true);
    }
  }

  if(!d->m_selectedCurrency.id().isEmpty()) {
    action("currency_rename")->setEnabled(true);
    // no need to check each transaction. accounts are enough in this case
    skip.fill(false);
    skip.setBit(IMyMoneyStorage::RefCheckTransaction);
    action("currency_delete")->setEnabled(!file->isReferenced(d->m_selectedCurrency, skip));
    if(d->m_selectedCurrency.id() != file->baseCurrency().id())
      action("currency_setbase")->setEnabled(true);
  }
}

void KMyMoney2App::slotResetSelections(void)
{
  slotSelectAccount();
  slotSelectInstitution();
  slotSelectInvestment();
  slotSelectSchedule();
  slotSelectCurrency();
  slotSelectPayees(QList<MyMoneyPayee>());
  slotSelectBudget(QList<MyMoneyBudget>());
  slotSelectTransactions(KMyMoneyRegister::SelectedTransactions());
  slotUpdateActions();
}

void KMyMoney2App::slotSelectCurrency(const MyMoneySecurity& currency)
{
  d->m_selectedCurrency = currency;
  slotUpdateActions();
  emit currencySelected(d->m_selectedCurrency);
}

void KMyMoney2App::slotSelectBudget(const QList<MyMoneyBudget>& list)
{
  d->m_selectedBudgets = list;
  slotUpdateActions();
  emit budgetSelected(d->m_selectedBudgets);
}

void KMyMoney2App::slotSelectPayees(const QList<MyMoneyPayee>& list)
{
  d->m_selectedPayees = list;
  slotUpdateActions();
  emit payeesSelected(d->m_selectedPayees);
}

void KMyMoney2App::slotSelectTransactions(const KMyMoneyRegister::SelectedTransactions& list)
{
  // list can either contain a list of transactions or a single selected scheduled transaction
  // in the latter case, the transaction id is actually the one of the schedule. In order
  // to differentiate between the two, we just ask for the schedule. If we don't find one - because
  // we passed the id of a real transaction - then we know that fact.  We use the schedule here,
  // because the list of schedules is kept in a cache by MyMoneyFile. This way, we save some trips
  // to the backend which we would have to do if we check for the transaction.
  d->m_selectedTransactions.clear();
  d->m_selectedSchedule = MyMoneySchedule();

  d->m_accountGoto = QString();
  d->m_payeeGoto = QString();
  if(!list.isEmpty() && !list.first().isScheduled()) {
    d->m_selectedTransactions = list;
    if(list.count() == 1) {
      const MyMoneySplit& sp = d->m_selectedTransactions[0].split();
      if(!sp.payeeId().isEmpty()) {
        try {
          MyMoneyPayee payee = MyMoneyFile::instance()->payee(sp.payeeId());
          if(!payee.name().isEmpty()) {
            d->m_payeeGoto = payee.id();
            QString name = payee.name();
            name.replace(QRegExp("&(?!&)"), "&&");
            action("transaction_goto_payee")->setText(i18n("Goto '%1'", name));
          }
        } catch(MyMoneyException *e) {
          delete e;
        }
      }
      try {
        QList<MyMoneySplit>::const_iterator it_s;
        const MyMoneyTransaction& t = d->m_selectedTransactions[0].transaction();
        // search the first non-income/non-expense accunt and use it for the 'goto account'
        const MyMoneySplit& sp = d->m_selectedTransactions[0].split();
        for(it_s = t.splits().begin(); it_s != t.splits().end(); ++it_s) {
          if((*it_s).id() != sp.id()) {
            MyMoneyAccount acc = MyMoneyFile::instance()->account((*it_s).accountId());
            if(!acc.isIncomeExpense()) {
              // for stock accounts we show the portfolio account
              if(acc.isInvest()) {
                acc = MyMoneyFile::instance()->account(acc.parentAccountId());
              }
              d->m_accountGoto = acc.id();
              QString name = acc.name();
              name.replace(QRegExp("&(?!&)"), "&&");
              action("transaction_goto_account")->setText(i18n("Goto '%1'", name));
              break;
            }
          }
        }
      } catch(MyMoneyException* e) {
        delete e;
      }
    }

    slotUpdateActions();
    emit transactionsSelected(d->m_selectedTransactions);

  } else if(!list.isEmpty()) {
    slotSelectSchedule(MyMoneyFile::instance()->schedule(list.first().scheduleId()));

  } else {
    slotUpdateActions();
  }

  // make sure, we show some neutral menu entry if we don't have an object
  if(d->m_payeeGoto.isEmpty())
    action("transaction_goto_payee")->setText(i18n("Goto payee"));
  if(d->m_accountGoto.isEmpty())
    action("transaction_goto_account")->setText(i18n("Goto account"));
}

void KMyMoney2App::slotSelectInstitution(const MyMoneyObject& institution)
{
  if(typeid(institution) != typeid(MyMoneyInstitution))
    return;

  d->m_selectedInstitution = dynamic_cast<const MyMoneyInstitution&>(institution);
  // qDebug("slotSelectInstitution('%s')", d->m_selectedInstitution.name().data());
  slotUpdateActions();
  emit institutionSelected(d->m_selectedInstitution);
}

void KMyMoney2App::slotSelectAccount(const MyMoneyObject& obj)
{
  if(typeid(obj) != typeid(MyMoneyAccount))
    return;

  d->m_selectedAccount = MyMoneyAccount();
  const MyMoneyAccount& acc = dynamic_cast<const MyMoneyAccount&>(obj);
  if(!acc.isInvest())
    d->m_selectedAccount = acc;

  // qDebug("slotSelectAccount('%s')", d->m_selectedAccount.name().data());
  slotUpdateActions();
  emit accountSelected(d->m_selectedAccount);
}

void KMyMoney2App::slotSelectInvestment(const MyMoneyObject& obj)
{
  if(typeid(obj) != typeid(MyMoneyAccount))
    return;

  // qDebug("slotSelectInvestment('%s')", account.name().data());
  d->m_selectedInvestment = MyMoneyAccount();
  const MyMoneyAccount& acc = dynamic_cast<const MyMoneyAccount&>(obj);
  if(acc.isInvest())
    d->m_selectedInvestment = acc;

  slotUpdateActions();
  emit investmentSelected(d->m_selectedInvestment);
}

void KMyMoney2App::slotSelectSchedule(const MyMoneySchedule& schedule)
{
  // qDebug("slotSelectSchedule('%s')", schedule.name().data());
  d->m_selectedSchedule = schedule;
  slotUpdateActions();
  emit scheduleSelected(d->m_selectedSchedule);
}

void KMyMoney2App::slotDataChanged(void)
{
  // As this method is called every time the MyMoneyFile instance
  // notifies a modification, it's the perfect place to start the timer if needed
  if (d->m_autoSaveEnabled && !d->m_autoSaveTimer->isActive()) {
    d->m_autoSaveTimer->setSingleShot(true);
    d->m_autoSaveTimer->start(d->m_autoSavePeriod * 60 * 1000);  //miliseconds
  }
  updateCaption();
}

void KMyMoney2App::slotCurrencyDialog(void)
{
  QPointer<KCurrencyEditDlg> dlg = new KCurrencyEditDlg(this);
  connect(dlg, SIGNAL(selectObject(const MyMoneySecurity&)), this, SLOT(slotSelectCurrency(const MyMoneySecurity&)));
  connect(dlg, SIGNAL(openContextMenu(const MyMoneySecurity&)), this, SLOT(slotShowCurrencyContextMenu()));
  connect(this, SIGNAL(currencyRename()), dlg, SLOT(slotStartRename()));
  connect(dlg, SIGNAL(renameCurrency(Q3ListViewItem*, int, const QString&)), this, SLOT(slotCurrencyRename(Q3ListViewItem*,int,const QString&)));
  connect(this, SIGNAL(currencyCreated(const QString&)), dlg, SLOT(slotSelectCurrency(const QString&)));
  connect(dlg, SIGNAL(selectBaseCurrency(const MyMoneySecurity&)), this, SLOT(slotCurrencySetBase()));

  dlg->exec();
  delete dlg;

  slotSelectCurrency(MyMoneySecurity());
}

void KMyMoney2App::slotPriceDialog(void)
{
  QPointer<KMyMoneyPriceDlg> dlg = new KMyMoneyPriceDlg(this);
  dlg->exec();
}

void KMyMoney2App::slotFileConsitencyCheck(void)
{
  KMSTATUS(i18n("Running consistency check..."));

  QStringList msg;
  MyMoneyFileTransaction ft;
  try {
    msg = MyMoneyFile::instance()->consistencyCheck();
    ft.commit();
  } catch(MyMoneyException *e) {
    msg.append(i18n("Consistency check failed: %1",e->what()));
    delete e;
  }

  KMessageBox::warningContinueCancelList(0, "Result", msg, i18n("Consistency check result"));

  updateCaption();
}

void KMyMoney2App::slotCheckSchedules(void)
{
  if(KMyMoneyGlobalSettings::checkSchedule() == true) {

    KMSTATUS(i18n("Checking for overdue scheduled transactions..."));
    MyMoneyFile *file = MyMoneyFile::instance();
    QDate checkDate = QDate::currentDate().addDays(KMyMoneyGlobalSettings::checkSchedulePreview());

    QList<MyMoneySchedule> scheduleList =  file->scheduleList();
    QList<MyMoneySchedule>::Iterator it;

    KMyMoneyUtils::EnterScheduleResultCodeE rc = KMyMoneyUtils::Enter;
    for (it=scheduleList.begin(); (it != scheduleList.end()) && (rc != KMyMoneyUtils::Cancel); ++it) {
      // Get the copy in the file because it might be modified by commitTransaction
      MyMoneySchedule schedule = file->schedule((*it).id());

      if(schedule.autoEnter()) {
        try {
          while(!schedule.isFinished() && (schedule.nextDueDate() <= checkDate)
                 && rc != KMyMoneyUtils::Ignore
                 && rc != KMyMoneyUtils::Cancel) {
            rc = enterSchedule(schedule, true, true);
            schedule = file->schedule((*it).id()); // get a copy of the modified schedule
          }
        } catch(MyMoneyException* e) {
          delete e;
        }
      }
    }
    updateCaption();
  }
}

void KMyMoney2App::writeLastUsedDir(const QString& directory)
{
  //get global config object for our app.
  KSharedConfigPtr kconfig = KGlobal::config();
  if(kconfig)
  {
    KConfigGroup grp = kconfig->group("General Options");

    //write path entry, no error handling since its void.
    grp.writeEntry("LastUsedDirectory", directory);
  }
}

void KMyMoney2App::writeLastUsedFile(const QString& fileName)
{
  //get global config object for our app.
  KSharedConfigPtr kconfig = KGlobal::config();
  if(kconfig)
  {
    KConfigGroup grp = d->m_config->group("General Options");

    // write path entry, no error handling since its void.
    // use a standard string, as fileName could contain a protocol
    // e.g. file:/home/thb/....
    grp.writeEntry("LastUsedFile", fileName);
  }
}

QString KMyMoney2App::readLastUsedDir(void) const
{
  QString str;

  //get global config object for our app.
  KSharedConfigPtr kconfig = KGlobal::config();
  if(kconfig)
  {
    KConfigGroup grp = d->m_config->group("General Options");

    //read path entry.  Second parameter is the default if the setting is not found, which will be the default document path.
    str = grp.readEntry("LastUsedDirectory", KGlobalSettings::documentPath());
    // if the path stored is empty, we use the default nevertheless
    if(str.isEmpty())
      str = KGlobalSettings::documentPath();
  }

  return str;
}

QString KMyMoney2App::readLastUsedFile(void) const
{
  QString str;

  // get global config object for our app.
  KSharedConfigPtr kconfig = KGlobal::config();
  if(kconfig)
  {
    KConfigGroup grp = d->m_config->group("General Options");

    // read filename entry.
    str = grp.readEntry("LastUsedFile", "");
  }

  return str;
}

const QString KMyMoney2App::filename(void) const
{
  return d->m_fileName.url();
}

const QList<QString> KMyMoney2App::instanceList(void) const
{
  QList<QString> list;
  QDBusReply<QStringList> reply = QDBusConnection::sessionBus().interface()->registeredServiceNames();

  if (reply.isValid()) {
    QStringList apps = reply.value();
    QStringList::ConstIterator it;

    for(it = apps.constBegin(); it != apps.constEnd(); ++it) {
      // skip over myself
      QDBusReply<uint> pid = QDBusConnection::sessionBus().interface()->servicePid(*it);
      if (pid.isValid() && pid.value() == getpid())
        continue;
      if((*it).indexOf("org.kde.kmymoney2") == 0) {
        list += (*it);
      }
    }
  } else {
    reply.error();
  }
  return list;
}

void KMyMoney2App::slotEquityPriceUpdate(void)
{
  QPointer<KEquityPriceUpdateDlg> dlg = new KEquityPriceUpdateDlg(this);
  if(dlg->exec() == QDialog::Accepted)
    dlg->storePrices();
  delete dlg;
}

void KMyMoney2App::webConnect(const QString& url, const QByteArray& asn_id)
{
  //
  // Web connect attempts to go through the known importers and see if the file
  // can be importing using that method.  If so, it will import it using that
  // plugin
  //

  // Bring this window to the forefront.  This method was suggested by
  // Lubos Lunak <l.lunak@suse.cz> of the KDE core development team.
  KStartupInfo::setNewStartupId(this, asn_id);

  // Make sure we have an open file
  if ( ! d->m_myMoneyView->fileOpen() &&
    KMessageBox::warningContinueCancel(kmymoney2, i18n("You must first select a KMyMoney file before you can import a statement.")) == KMessageBox::Continue )
      kmymoney2->slotFileOpen();

  // only continue if the user really did open a file.
  if ( d->m_myMoneyView->fileOpen() )
  {
    KMSTATUS(i18n("Importing a statement via Web Connect"));

    // remove the statement files
    d->unlinkStatementXML();

    QMap<QString,KMyMoneyPlugin::ImporterPlugin*>::const_iterator it_plugin = d->m_importerPlugins.constBegin();
    while ( it_plugin != d->m_importerPlugins.constEnd() )
    {
      if ( (*it_plugin)->isMyFormat(url) )
      {
        Q3ValueList<MyMoneyStatement> statements;
        if (!(*it_plugin)->import(url) )
        {
          KMessageBox::error( this, i18n("Unable to import %1 using %2 plugin.  The plugin returned the following error: %3", url,(*it_plugin)->formatName(),(*it_plugin)->lastError()), i18n("Importing error"));
        }

        break;
      }
      ++it_plugin;
    }

    // If we did not find a match, try importing it as a KMM statement file,
    // which is really just for testing.  the statement file is not exposed
    // to users.
    if ( it_plugin == d->m_importerPlugins.constEnd() )
      if ( MyMoneyStatement::isStatementFile( url ) )
        slotStatementImport(url);

  }
}

void KMyMoney2App::slotEnableMessages(void)
{
  KMessageBox::enableAllMessages();
  KMessageBox::information(this, i18n("All messages have been enabled."), i18n("All messages"));
}

void KMyMoney2App::slotSecurityEditor(void)
{
  KSecurityListEditor dlg(this);
  dlg.exec();
}

void KMyMoney2App::createInterfaces(void)
{
  // Sets up the plugin interface, and load the plugins
  d->m_pluginInterface = new QObject( this, "_pluginInterface" );

  new KMyMoneyPlugin::KMMViewInterface(this, d->m_myMoneyView, d->m_pluginInterface, "view interface");
  new KMyMoneyPlugin::KMMStatementInterface(this, d->m_pluginInterface, "statement interface");
  new KMyMoneyPlugin::KMMImportInterface(this, d->m_pluginInterface, "import interface");
}

void KMyMoney2App::loadPlugins(void)
{
  d->m_pluginLoader = new KMyMoneyPlugin::PluginLoader(this);

  connect( d->m_pluginLoader, SIGNAL( plug(KPluginInfo*) ), this, SLOT( slotPluginPlug(KPluginInfo*) ) );
  connect( d->m_pluginLoader, SIGNAL( unplug(KPluginInfo*) ), this, SLOT( slotPluginUnplug(KPluginInfo*) ) );

  d->m_pluginLoader->loadPlugins();
}

void KMyMoney2App::slotPluginPlug(KPluginInfo* info)
{
  KMyMoneyPlugin::Plugin* plugin = d->m_pluginLoader->getPluginFromInfo(info);

  // check for online plugin
  KMyMoneyPlugin::OnlinePlugin* op = dynamic_cast<KMyMoneyPlugin::OnlinePlugin *>(plugin);
  // check for importer plugin
  KMyMoneyPlugin::ImporterPlugin* ip = dynamic_cast<KMyMoneyPlugin::ImporterPlugin *>(plugin);

  // plug the plugin
  guiFactory()->addClient(plugin);

  if(op)
    d->m_onlinePlugins[plugin->objectName()] = op;

  if(ip)
    d->m_importerPlugins[plugin->objectName()] = ip;

  slotUpdateActions();
}

void KMyMoney2App::slotPluginUnplug(KPluginInfo* info)
{
  KMyMoneyPlugin::Plugin* plugin = d->m_pluginLoader->getPluginFromInfo(info);

  // check for online plugin
  KMyMoneyPlugin::OnlinePlugin* op = dynamic_cast<KMyMoneyPlugin::OnlinePlugin *>(plugin);
  // check for importer plugin
  KMyMoneyPlugin::ImporterPlugin* ip = dynamic_cast<KMyMoneyPlugin::ImporterPlugin *>(plugin);

  // unplug the plugin
  guiFactory()->removeClient(plugin);

  if(op)
    d->m_onlinePlugins.remove(plugin->objectName());

  if(ip)
    d->m_importerPlugins.remove(plugin->objectName());

  slotUpdateActions();
}

void KMyMoney2App::slotAutoSave(void)
{
  if(!d->m_inAutoSaving) {
    d->m_inAutoSaving = true;
    KMSTATUS(i18n("Auto saving..."));

    //calls slotFileSave if needed, and restart the timer
    //it the file is not saved, reinitializes the countdown.
    if (d->m_myMoneyView->dirty() && d->m_autoSaveEnabled) {
      if (!slotFileSave() && d->m_autoSavePeriod > 0) {
        d->m_autoSaveTimer->setSingleShot(true);
        d->m_autoSaveTimer->start(d->m_autoSavePeriod * 60 * 1000);
      }
    }

    d->m_inAutoSaving = false;
  }
}

void KMyMoney2App::slotDateChanged(void)
{
  QDateTime dt = QDateTime::currentDateTime();
  QDateTime nextDay( QDate(dt.date().addDays(1)), QTime(0, 0, 0) );

  QTimer::singleShot(dt.secsTo(nextDay)*1000, this, SLOT(slotDateChanged()));
  d->m_myMoneyView->slotRefreshViews();
}

const MyMoneyAccount& KMyMoney2App::account(const QString& key, const QString& value) const
{
  QList<MyMoneyAccount> list;
  QList<MyMoneyAccount>::const_iterator it_a;
  MyMoneyFile::instance()->accountList(list);
  QList<MyMoneyAccount> accList;
  for(it_a = list.constBegin(); it_a != list.constEnd(); ++it_a) {
    const QString& id = (*it_a).onlineBankingSettings().value(key);
    if(id.contains(value)) {
      accList << MyMoneyFile::instance()->account((*it_a).id());
    }
    if(id == value) {
      return MyMoneyFile::instance()->account((*it_a).id());
    }
  }
  // if we did not find an exact match of the value, we take the one that partially
  // matched, but only if not more than one matched partially.
  if(accList.count() == 1) {
    return accList.first();
  }

  // return reference to empty element
  return MyMoneyFile::instance()->account(QString());
}

void KMyMoney2App::setAccountOnlineParameters(const MyMoneyAccount& _acc, const MyMoneyKeyValueContainer& kvps)
{
  MyMoneyFileTransaction ft;
  try {
    MyMoneyAccount acc = MyMoneyFile::instance()->account(_acc.id());
    acc.setOnlineBankingSettings(kvps);
    MyMoneyFile::instance()->modifyAccount(acc);
    ft.commit();

  } catch(MyMoneyException *e) {
    KMessageBox::detailedSorry(0, i18n("Unable to setup online parameters for account ''%1'",_acc.name()), e->what() );
    delete e;
  }
}

void KMyMoney2App::slotAccountUnmapOnline(void)
{
  // no account selected
  if(d->m_selectedAccount.id().isEmpty())
    return;

  // not a mapped account
  if(d->m_selectedAccount.onlineBankingSettings().value("provider").isEmpty())
    return;

  if(KMessageBox::warningYesNo(this, QString("<qt>%1</qt>").arg(i18n("Do you really want to remove the mapping of account <b>%1</b> to an online account? Depending on the details of the online banking method used, this action cannot be reverted.",d->m_selectedAccount.name())), i18n("Remove mapping to online account")) == KMessageBox::Yes) {
    MyMoneyFileTransaction ft;
    try {
      d->m_selectedAccount.setOnlineBankingSettings(MyMoneyKeyValueContainer());
      // delete the kvp that is used in MyMoneyStatementReader too
      // we should really get rid of it, but since I don't know what it
      // is good for, I'll keep it around. (ipwizard)
      d->m_selectedAccount.deletePair("StatementKey");
      MyMoneyFile::instance()->modifyAccount(d->m_selectedAccount);
      ft.commit();
    } catch(MyMoneyException* e) {
      KMessageBox::error(this, i18n("Unable to unmap account from online account: %1",e->what()));
      delete e;
    }
  }
}

void KMyMoney2App::slotAccountMapOnline(void)
{
  // no account selected
  if(d->m_selectedAccount.id().isEmpty())
    return;

  // already an account mapped
  if(!d->m_selectedAccount.onlineBankingSettings().value("provider").isEmpty())
    return;

  // check if user tries to map a brokerageAccount
  if(d->m_selectedAccount.name().contains(i18n(" (Brokerage)"))) {
    if(KMessageBox::warningContinueCancel(this, i18n("You try to map a brokerage account to an online account. This is usually not advisable. In general, the investment account should be mapped to the online account. Please cancel if you intended to map the investment account, continue otherwise"), i18n("Mapping brokerage account")) == KMessageBox::Cancel) {
      return;
    }
  }

  // if we have more than one provider let the user select the current provider
  QString provider;
  QMap<QString, KMyMoneyPlugin::OnlinePlugin*>::const_iterator it_p;
  switch(d->m_onlinePlugins.count()) {
    case 0:
      break;
    case 1:
      provider = d->m_onlinePlugins.begin().key();
      break;
    default:
      {
        QMenu popup(this);
        popup.setTitle(i18n("Select online banking plugin"));

        // Populate the pick list with all the provider
        for (it_p = d->m_onlinePlugins.constBegin(); it_p != d->m_onlinePlugins.constEnd(); ++it_p) {
          popup.addAction(it_p.key())->setData(it_p.key());
        }

        QAction *item = popup.actions()[0];
        if (item) {
          popup.setActiveAction(item);
        }

        // cancelled
        if ((item = popup.exec(QCursor::pos(), item)) == 0) {
          return;
        }

        // We need to create a valid date in the month selected so we can find out how many days are
        // in the month.
        provider = item->data().toString();
      }
      break;
  }

  if(provider.isEmpty())
    return;

  // find the provider
  it_p = d->m_onlinePlugins.constFind(provider);
  if(it_p != d->m_onlinePlugins.constEnd()) {
    // plugin found, call it
    MyMoneyKeyValueContainer settings;
    if((*it_p)->mapAccount(d->m_selectedAccount, settings)) {
      settings["provider"] = provider;
      MyMoneyAccount acc(d->m_selectedAccount);
      acc.setOnlineBankingSettings(settings);
      MyMoneyFileTransaction ft;
      try {
        MyMoneyFile::instance()->modifyAccount(acc);
        ft.commit();
      } catch(MyMoneyException* e) {
        KMessageBox::error(this, i18n("Unable to map account to online account: %1",e->what()));
        delete e;
      }
    }
  }
}

void KMyMoney2App::slotAccountUpdateOnlineAll(void)
{
  QList<MyMoneyAccount> accList;
  MyMoneyFile::instance()->accountList(accList);
  QList<MyMoneyAccount>::iterator it_a;
  QMap<QString, KMyMoneyPlugin::OnlinePlugin*>::const_iterator it_p;
  d->m_statementResults.clear();
  d->m_collectingStatements = true;

  // remove all those from the list, that don't have a 'provider' or the
  // provider is not currently present
  for(it_a = accList.begin(); it_a != accList.end();) {
    if ((*it_a).onlineBankingSettings().value("provider").isEmpty()
    || d->m_onlinePlugins.find((*it_a).onlineBankingSettings().value("provider")) == d->m_onlinePlugins.end() ) {
      it_a = accList.erase(it_a);
    } else
      ++it_a;
  }

  // now work on the remaining list of accounts
  int cnt = accList.count() - 1;
  for(it_a = accList.begin(); it_a != accList.end(); ++it_a) {
    it_p = d->m_onlinePlugins.constFind((*it_a).onlineBankingSettings().value("provider"));
    (*it_p)->updateAccount(*it_a, cnt != 0);
    --cnt;
  }

  d->m_collectingStatements = false;
  KMessageBox::informationList(this, i18n("The statements have been processed with the following results:"), d->m_statementResults, i18n("Statement stats"));
}

void KMyMoney2App::slotAccountUpdateOnline(void)
{
  // no account selected
  if(d->m_selectedAccount.id().isEmpty())
    return;

  // no online account mapped
  if(d->m_selectedAccount.onlineBankingSettings().value("provider").isEmpty())
    return;

  // find the provider
  QMap<QString, KMyMoneyPlugin::OnlinePlugin*>::const_iterator it_p;
  it_p = d->m_onlinePlugins.constFind(d->m_selectedAccount.onlineBankingSettings().value("provider"));
  if(it_p != d->m_onlinePlugins.constEnd()) {
    // plugin found, call it
    d->m_collectingStatements = true;
    d->m_statementResults.clear();
    (*it_p)->updateAccount(d->m_selectedAccount);
    d->m_collectingStatements = false;
    KMessageBox::informationList(this, i18n("The statements have been processed with the following results:"), d->m_statementResults, i18n("Statement stats"));
  }
}

KMStatus::KMStatus (const QString &text)
{
  m_prevText = kmymoney2->slotStatusMsg(text);
}

KMStatus::~KMStatus()
{
  kmymoney2->slotStatusMsg(m_prevText);
}

void KMyMoney2App::Private::unlinkStatementXML(void)
{
  QDir d("/home/thb", "kmm-statement*");
  for(uint i = 0; i < d.count(); ++i) {
    qDebug("Remove %s", qPrintable(d[i]));
//FIXME: FIX on windows
    d.remove(QString("/home/thb/%1").arg(d[i]));
  }
  m_statementXMLindex = 0;
}

#include "kmymoney2.moc"
// vim:cin:si:ai:et:ts=2:sw=2:

