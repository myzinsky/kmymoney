/*******************************************************************************
*                               redefinedlg.cpp
*                             ------------------
* begin                       : Sat Jan 01 2010
* copyright                   : (C) 2010 by Allan Anderson
* email                       : aganderson@ukonline.co.uk
********************************************************************************/

/*******************************************************************************
*                                                                              *
*   This program is free software; you can redistribute it and/or modify       *
*   it under the terms of the GNU General Public License as published by       *
*   the Free Software Foundation; either version 2 of the License, or          *
*   (at your option) any later version.                                        *
*                                                                              *
********************************************************************************/

#include "redefinedlg.h"

// ----------------------------------------------------------------------------
// QT Headers

#include <QtCore/QDebug>
#include <QtCore/QPointer>
#include <QtGui/QResizeEvent>
#include <QtGui/QScrollBar>

// ----------------------------------------------------------------------------
// KDE Headers

#include <KInputDialog>
#include <KLocale>
#include <KMessageBox>
#include <KIconLoader>
#include <KGlobal>

RedefineDlg::RedefineDlg()
{
  m_iconYes = QPixmap(KIconLoader::global()->loadIcon("dialog-ok", KIconLoader::Small, KIconLoader::DefaultState));
  m_iconNo = QPixmap(KIconLoader::global()->loadIcon("dialog-cancel", KIconLoader::Small, KIconLoader::DefaultState));

  m_widget = new RedefineDlgDecl();
  setMainWidget(m_widget);
  m_widget->tableWidget->setColumnCount(defMAXCOL);
  m_widget->tableWidget->setToolTip("Results table");
  m_widget->tableWidget->setRowCount(2);
  m_mainWidth = m_widget->tableWidget->size().width();
  m_mainHeight = m_widget->tableWidget->size().height();
  this->enableButtonOk(false);

  connect(m_widget->kcombobox_Actions, SIGNAL(activated(QString)), this, SLOT(slotNewActionSelected(QString)));
  connect(this, SIGNAL(okClicked()), this, SLOT(slotAccepted()));
  connect(this, SIGNAL(cancelClicked()), this, SLOT(slotRejected()));
}

RedefineDlg::~RedefineDlg()
{
  delete m_widget;
}

void RedefineDlg::displayLine(const QString& info)
{
  this->enableButtonOk(false);
  QString txt;
  txt.setNum(m_typeColumn + 1);
  m_widget->label_actionCol->setText("Column " + txt);
  m_widget->label_info->setText(info);
  int cCount = m_columnList.count();
  m_widget->tableWidget->setColumnCount(cCount);

  QBrush brush;
  QColor colr;
  colr.setRgb(255, 0, 127, 100);
  brush.setColor(colr);
  brush.setStyle(Qt::SolidPattern);
  int row;
  m_columnTotalWidth = 0;
  m_widget->tableWidget->setRowCount(2);

  for (int col = 0; col < cCount; col++) {
    row = 1;
    txt = m_columnList[col];
    txt = txt.remove('"');

    QTableWidgetItem *item = new QTableWidgetItem;//        add items to UI
    item->setText(txt);
    m_widget->tableWidget->setItem(row, col, item);//     add items to UI here
    if (m_typeColumn == col) {
      item->setBackground(brush);
    }
    m_columnTotalWidth += m_widget->tableWidget->columnWidth(col);

    row = 0;
    if (col == m_quantityColumn) {
      QTableWidgetItem *item = new QTableWidgetItem;//        add items to UI
      item->setText("Quantity");
      m_widget->tableWidget->setItem(row, col, item);
    } else if (col == m_priceColumn) {
      QTableWidgetItem *item = new QTableWidgetItem;//        add items to UI
      item->setText("Price");
      m_widget->tableWidget->setItem(row, col, item);
    } else if (col == m_amountColumn) {
      QTableWidgetItem *item = new QTableWidgetItem;//        add items to UI
      item->setText("Amount");
      m_widget->tableWidget->setItem(row, col, item);
    } else if (col == m_typeColumn) {
      QTableWidgetItem *item = new QTableWidgetItem;//        add items to UI
      item->setText("Type");
      m_widget->tableWidget->setItem(row, col, item);
      m_widget->tableWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    }
  }
  updateWindow();
}

void RedefineDlg::slotAccepted()
{
  m_ret = KMessageBox::Ok;
  m_columnList[m_typeColumn] = m_newType;
  m_inBuffer = m_columnList.join(",");
  emit changedType(m_newType);
  m_widget->kcombobox_Actions->setCurrentIndex(-1);
  accept();
}

void RedefineDlg::slotNewActionSelected(QString type)
{
  m_newType = type.section('-', 1, 1);
  if (m_okTypeList.contains(m_newType)) {
    QTableWidgetItem *item = new QTableWidgetItem;//        add new type to UI
    item->setText(type);
    m_widget->tableWidget->setItem(1, m_typeColumn, item);
    this->enableButtonOk(true);
  }
}

void RedefineDlg::slotRejected()
{
  m_ret = KMessageBox::Cancel;
  reject();
}


int RedefineDlg::checkValid(const QString& type, QString info)
{
  this->enableButtonOk(false);
  getValues();
  if ((m_priceColumn < 1) || (m_priceColumn >= defMAXCOL) ||
      (m_quantityColumn < 1) || (m_quantityColumn >= defMAXCOL) ||
      (m_amountColumn < 1) || (m_amountColumn >= defMAXCOL)) {
    info = "There is a problem with the columns selected\n for 'Price', 'Quantity and 'Amount'.\n \
            You will need to reselect those columns.";
    int ret = suspectType(info);
    return ret;
  }

  if ((type == "reinvdiv") || (type == "buy") || (type == "sell")) {
    m_widget->label_info->setText("OK");
    if ((m_quantity > 0) && (m_price > 0) && (m_amount != 0)) {
      m_okTypeList << "reinvdiv" << "buy" << "sell";
      if (m_accountName.isEmpty())
        m_accountName =  getParameter("   Brokerage or Chk. Account name:");
      if (m_accountName.isEmpty())
        return KMessageBox::Cancel;
      m_newType = type;
      this->enableButtonOk(true);
      return KMessageBox::Ok;
    }
    int ret = suspectType(info);
    return ret;
  } else if (type.toLower() == "divx") {
    m_widget->label_info->setText("OK");
    if ((m_quantity == 0) && (m_price == 0) && (m_amount != 0)) {
      m_okTypeList << "divx";
      if (m_accountName.isEmpty())
        m_accountName =  getParameter("   Brokerage or Chk. Account name:");
      if (m_accountName.isEmpty())
        return KMessageBox::Cancel;
      m_newType = type;
      this->enableButtonOk(true);
      return KMessageBox::Ok;
    }
    //                    validity suspect
    int ret = suspectType(info);
    return ret;
  } else if ((type == "shrsin") || (type == "shrsout")) {
    m_widget->label_info->setText("OK");
    if ((m_quantity > 0) && (m_price == 0) && (m_amount == 0)) {
      m_okTypeList << "shrsin" << "shrsout";
      m_newType = type;
      this->enableButtonOk(true);
      return KMessageBox::Ok;
    }
    m_okTypeList.clear();
    int ret = suspectType(info);
    return ret;
  }
  return KMessageBox::Cancel;
}

int RedefineDlg::suspectType(QString info)
{
  displayLine(info);
  buildOkTypeList();
  for (int i = 0; i < 6 ; i++) {
    if (m_okTypeList.contains(m_widget->kcombobox_Actions->itemText(i).section('-', 1, 1)))
      m_widget->kcombobox_Actions->setItemIcon(i, m_iconYes);
    else
      m_widget->kcombobox_Actions->setItemIcon(i, m_iconNo);
  }

  int ret = exec();
  if (ret == QDialog::Rejected)
    ret = KMessageBox::Cancel;
  return ret;
}

void RedefineDlg::buildOkTypeList()
{
  getValues();

  m_okTypeList.clear();
  if ((m_quantity > 0) && (m_price > 0) && (m_amount != 0))
    m_okTypeList << "reinvdiv" << "buy" << "sell";
  else if ((m_quantity == 0) && (m_price == 0) && (m_amount != 0))
    m_okTypeList << "divx";
  else if ((m_quantity > 0) && (m_price == 0) && (m_amount == 0))
    m_okTypeList << "shrsin" << "shrsout";
  else {
    m_okTypeList.clear();
    KMessageBox::sorry(this, i18n(" The values in the columns you have selected\
                                      \n do not match any expected investment type.\
                                      \n Please check the fields in the current transaction,\
                                      \n                   and also your selections."), i18n("CSV import"));
  }
}

QString RedefineDlg::getParameter(QString aName)
{
  bool ok;
  static QString accntName;
  accntName = KInputDialog::getText(i18n("Enter Account Name"), aName, QString(), &ok, 0, 0, 0);

  if (ok && !accntName.isEmpty())
    return accntName;
  else return "";
}

void RedefineDlg::resizeEvent(QResizeEvent * event)
{
  event->accept();

  m_mainWidth = m_widget->verticalLayout->geometry().size().width() - 10;
  m_mainHeight = m_widget->tableWidget->geometry().size().height();
  updateWindow();
}

void RedefineDlg::updateWindow()
{
  int w = m_widget->tableWidget->width();
  int hght = 4 + (m_widget->tableWidget->rowHeight(0) * 2);
  hght += m_widget->tableWidget->horizontalHeader()->height() + 2;//  frig factor for vert. headers?

  if (m_columnTotalWidth > (m_mainWidth - 12))
    hght += m_widget->tableWidget->horizontalScrollBar()->height() + 1;//  ....and for hor. scroll bar

  m_widget->tableWidget->setFixedHeight(hght);
  w = m_widget->tableWidget->width();
}

void RedefineDlg::getValues()
{
  m_price = m_columnList[m_priceColumn].remove('"').toFloat();
  m_quantity = m_columnList[m_quantityColumn].remove('"').toFloat();
  QString txt = m_columnList[m_amountColumn].remove('"');
  if (txt.contains(')')) { //          replace negative ( ) with '-'
    txt = '-' + txt.remove(QRegExp("[(),]"));
  }
  m_amount = txt.toFloat();
}
