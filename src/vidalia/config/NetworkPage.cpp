/*
**  This file is part of Vidalia, and is subject to the license terms in the
**  LICENSE file, found in the top level directory of this distribution. If you
**  did not receive the LICENSE file with this file, you may obtain it from the
**  Vidalia source package distributed by the Vidalia Project at
**  http://www.vidalia-project.net/. No part of Vidalia, including this file,
**  may be copied, modified, propagated, or distributed except according to the
**  terms described in the LICENSE file.
*/

/*
** \file NetworkPage.cpp
** \version $Id$
** \brief Network and firewall configuration options
*/

#include "NetworkPage.h"
#include "NetworkSettings.h"
#include "VMessageBox.h"
#include "Vidalia.h"
#include "BridgeDownloaderProgressDialog.h"
#include "DomainValidator.h"

#include "stringutil.h"

#include <QMenu>
#include <QIntValidator>
#include <QClipboard>
#include <QHostAddress>
#include <QRegExp>
#include <QMessageBox>

#define IMG_COPY  ":/images/22x22/edit-copy.png"


/** Constructor */
NetworkPage::NetworkPage(QWidget *parent)
: ConfigPage(parent, "Network")
{
  /* Invoke the Qt Designer generated object setup routine */
  ui.setupUi(this);

  connect(ui.btnAddBridge, SIGNAL(clicked()), this, SLOT(addBridge()));
  connect(ui.btnRemoveBridge, SIGNAL(clicked()), this, SLOT(removeBridge()));
  connect(ui.btnCopyBridge, SIGNAL(clicked()), 
          this, SLOT(copySelectedBridgesToClipboard()));
  connect(ui.listBridges, SIGNAL(customContextMenuRequested(QPoint)),
          this, SLOT(bridgeContextMenuRequested(QPoint)));
  connect(ui.listBridges, SIGNAL(itemSelectionChanged()),
          this, SLOT(bridgeSelectionChanged()));
  connect(ui.lineBridge, SIGNAL(returnPressed()), this, SLOT(addBridge()));
  connect(ui.lblHelpFindBridges, SIGNAL(linkActivated(QString)),
          this, SLOT(onLinkActivated(QString)));
  connect(ui.btnFindBridges, SIGNAL(clicked()), this, SLOT(findBridges()));

  ui.lineHttpProxyAddress->setValidator(new DomainValidator(this));
  ui.lineHttpProxyPort->setValidator(new QIntValidator(1, 65535, this));

  vApp->createShortcut(QKeySequence(QKeySequence::Copy),
                       ui.listBridges, this,
                       SLOT(copySelectedBridgesToClipboard()));

  if (! BridgeDownloader::isMethodSupported(BridgeDownloader::DownloadMethodHttps)) {
    ui.btnFindBridges->setVisible(false);
    ui.lblHelpFindBridges->setText(
      tr("<a href=\"bridges.finding\">How can I find bridges?</a>"));
    _bridgeDownloader = 0;
  } else {
    _bridgeDownloader = new BridgeDownloader(this);
    connect(_bridgeDownloader, SIGNAL(bridgeRequestFinished(QStringList)),
            this, SLOT(bridgeRequestFinished(QStringList)));
  }

#if defined(Q_WS_MAC)
  /* On OS X, the network page looks better without frame titles. Everywhere
   * else needs titles or else there's a break in the frame border. */
  ui.grpProxySettings->setTitle("");
  ui.grpFirewallSettings->setTitle("");
  ui.grpBridgeSettings->setTitle("");
#endif
}

/** Called when the user changes the UI translation. */
void
NetworkPage::retranslateUi()
{
  ui.retranslateUi(this);
}

/** Applies the network configuration settings to Tor. Returns true if the   *
 * settings were applied successfully. Otherwise, <b>errmsg</b> is set and   *
 * false is returned. */
bool
NetworkPage::apply(QString &errmsg)
{
  return NetworkSettings(Vidalia::torControl()).apply(&errmsg);
}

/** Returns true if the user has changed their server settings since the   *
 * last time they were applied to Tor. */
bool
NetworkPage::changedSinceLastApply()
{
  return NetworkSettings(Vidalia::torControl()).changedSinceLastApply();
}

/** Reverts the server configuration settings to their values at the last   *
 * time they were successfully applied to Tor. */
void
NetworkPage::revert()
{
  NetworkSettings settings(Vidalia::torControl());
  settings.revert();
}

/** Called when a link in a label is clicked. <b>url</b> is the target of
 * the clicked link. */
void
NetworkPage::onLinkActivated(const QString &url)
{
  emit helpRequested(url);
}

/** Verifies that <b>bridge</b> is a valid bridge identifier and places a 
 * normalized identifier in <b>out</b>. The normalized identifier will have
 * all spaces removed from the fingerprint portion (if any) and all
 * hexadecimal characters converted to uppercase. Returns true if
 * <b>bridge</b> is a valid bridge identifier, false otherwise. */
bool
NetworkPage::validateBridge(const QString &bridge, QString *out)
{
  QString temp = bridge;
  if (temp.startsWith("bridge ", Qt::CaseInsensitive))
    temp = temp.remove(0, 7); /* remove "bridge " */

  QStringList parts = temp.split(" ", QString::SkipEmptyParts);
  if (parts.isEmpty())
    return false;

  QString s = parts.at(0);
  QRegExp re("(\\d{1,3}\\.){3}\\d{1,3}(:\\d{1,5})?");
  if (re.exactMatch(s)) {
    if (s.endsWith(":"))
      return false;

    int index = s.indexOf(":");
    QString host = s.mid(0, index);
    if (QHostAddress(host).isNull()
          || QHostAddress(host).protocol() != QAbstractSocket::IPv4Protocol) {
      return false;
    }
    if (index > 0) {
      QString port = s.mid(index + 1);
      if (port.toUInt() < 1 || port.toUInt() > 65535)
        return false;
    }

    temp = s;
    if (parts.size() > 1) {
      QString fp = static_cast<QStringList>(parts.mid(1)).join("");
      if (fp.length() != 40 || !string_is_hex(fp))
        return false;
      temp += " " + fp.toUpper();
    }
  } else {
    return false;
  }
  *out = temp;
  return true;
}

/** Adds a bridge to the bridge list box. */
void
NetworkPage::addBridge()
{
  QString bridge;
  QString input = ui.lineBridge->text().trimmed();

  if (input.isEmpty())
    return;
  if (!validateBridge(input, &bridge)) {
    VMessageBox::warning(this,
                  tr("Invalid Bridge"),
                  tr("The specified bridge identifier is not valid."),
                  VMessageBox::Ok|VMessageBox::Default);
    return;
  }
  if (!ui.listBridges->findItems(bridge, Qt::MatchFixedString).isEmpty())
    return; /* duplicate bridge */

  ui.listBridges->addItem(bridge);
  ui.lineBridge->clear();
}

/** Removes one or more selected bridges from the bridge list box. */
void
NetworkPage::removeBridge()
{
  qDeleteAll(ui.listBridges->selectedItems());
}

/** Copies all selected bridges to the clipboard. */
void
NetworkPage::copySelectedBridgesToClipboard()
{
  QString contents;

  foreach (QListWidgetItem *item, ui.listBridges->selectedItems()) {
#if defined(Q_WS_WIN)
    contents += item->text() + "\r\n";
#else
    contents += item->text() + "\n";
#endif
  }
  if (!contents.isEmpty())
    vApp->clipboard()->setText(contents.trimmed());
}

/** Called when the user right-clicks on a bridge and displays a context
 * menu. */
void
NetworkPage::bridgeContextMenuRequested(const QPoint &pos)
{
  QMenu menu(this);
  
  QListWidgetItem *item = ui.listBridges->itemAt(pos);
  if (!item)
    return;
  
  QAction *copyAction =
    new QAction(QIcon(IMG_COPY), tr("Copy (Ctrl+C)"), &menu);
  connect(copyAction, SIGNAL(triggered()),
          this, SLOT(copySelectedBridgesToClipboard()));

  menu.addAction(copyAction);
  menu.exec(ui.listBridges->mapToGlobal(pos));
}

/** Called when the user changes which bridges they have selected. */
void
NetworkPage::bridgeSelectionChanged()
{
  bool enabled = !ui.listBridges->selectedItems().isEmpty();
  ui.btnCopyBridge->setEnabled(enabled);
  ui.btnRemoveBridge->setEnabled(enabled);
}

/** Saves changes made to settings on the Firewall settings page. */
bool
NetworkPage::save(QString &errmsg)
{
  NetworkSettings settings(Vidalia::torControl());
  QStringList bridgeList;
  QList<quint16> reachablePorts;
  bool ok;
  
  if (ui.chkUseProxy->isChecked()
        && (ui.lineHttpProxyAddress->text().isEmpty()
            || ui.lineHttpProxyPort->text().isEmpty())) {
    errmsg = tr("You must specify both an IP address or hostname and a "
                "port number to configure Tor to use a proxy to access "
                "the Internet.");
    return false;
  }
  if (ui.chkFascistFirewall->isChecked()
        && ui.lineReachablePorts->text().isEmpty()) {
    errmsg = tr("You must specify one or more ports to which your "
                "firewall allows you to connect.");
    return false;
  }

  /* Save the HTTP/HTTPS proxy settings */
  settings.setUseHttpProxy(ui.chkUseProxy->isChecked());
  settings.setUseHttpsProxy(ui.chkProxyUseHttps->isChecked());
  if (!ui.lineHttpProxyAddress->text().isEmpty()) {
    QString proxy = ui.lineHttpProxyAddress->text();
    if (!ui.lineHttpProxyPort->text().isEmpty())
      proxy += ":" + ui.lineHttpProxyPort->text();

    settings.setHttpProxy(proxy);
    settings.setHttpsProxy(proxy);
  } else {
    settings.setHttpProxy("");
    settings.setHttpsProxy("");
  }

  if (!ui.lineHttpProxyUsername->text().isEmpty() ||
      !ui.lineHttpProxyPassword->text().isEmpty()) {
    QString auth = ui.lineHttpProxyUsername->text() + ":" +
                   ui.lineHttpProxyPassword->text();
    settings.setHttpProxyAuthenticator(auth);
    settings.setHttpsProxyAuthenticator(auth);
  } else {
    settings.setHttpProxyAuthenticator("");
    settings.setHttpsProxyAuthenticator("");
  }
  
  /* Save the reachable port settings */
  settings.setFascistFirewall(ui.chkFascistFirewall->isChecked());
  foreach (QString portString,
           ui.lineReachablePorts->text().split(",", QString::SkipEmptyParts)) {
    quint32 port = portString.toUInt(&ok);
    if (!ok || port < 1 || port > 65535) {
      errmsg = tr("'%1' is not a valid port number.").arg(portString);
      return false;
    }
    reachablePorts << (quint16)port;
  }
  settings.setReachablePorts(reachablePorts);

  /* Save the bridge settings */
  settings.setUseBridges(ui.chkUseBridges->isChecked());
  for (int i = 0; i < ui.listBridges->count(); i++)
    bridgeList << ui.listBridges->item(i)->text();
  settings.setBridgeList(bridgeList);

  return true;
}

/** Loads previously saved settings */
void
NetworkPage::load()
{
  NetworkSettings settings(Vidalia::torControl());
  QStringList reachablePortStrings;

  /* Load HTTP/HTTPS proxy settings */
  ui.chkUseProxy->setChecked(settings.getUseHttpProxy());
  ui.chkProxyUseHttps->setChecked(settings.getUseHttpsProxy());
  QStringList proxy = settings.getHttpProxy().split(":");
  QStringList proxyAuth = settings.getHttpProxyAuthenticator().split(":");
  if (proxy.size() >= 1)
    ui.lineHttpProxyAddress->setText(proxy.at(0));
  if (proxy.size() >= 2)
    ui.lineHttpProxyPort->setText(proxy.at(1));
  if (proxyAuth.size() >= 1)  
    ui.lineHttpProxyUsername->setText(proxyAuth.at(0));
  if (proxyAuth.size() >= 2)
    ui.lineHttpProxyPassword->setText(proxyAuth.at(1));

  /* Load firewall settings */
  ui.chkFascistFirewall->setChecked(settings.getFascistFirewall());
  QList<quint16> reachablePorts = settings.getReachablePorts();
  foreach (quint16 port, reachablePorts) {
    reachablePortStrings << QString::number(port);
  }
  ui.lineReachablePorts->setText(reachablePortStrings.join(","));

  /* Load bridge settings */
  ui.chkUseBridges->setChecked(settings.getUseBridges()); 
  ui.listBridges->clear();
  ui.listBridges->addItems(settings.getBridgeList());
}

/** Called when the user clicks the "Find Bridges Now" button.
 * Attempts to establish an HTTPS connection to bridges.torproject.org
 * and download one or more bridge addresses. */
void
NetworkPage::findBridges()
{
  BridgeDownloaderProgressDialog *dlg = new BridgeDownloaderProgressDialog(this);

  connect(_bridgeDownloader, SIGNAL(statusChanged(QString)),
          dlg, SLOT(setStatus(QString)));
  connect(_bridgeDownloader, SIGNAL(downloadProgress(int, int)),
          dlg, SLOT(setDownloadProgress(int, int)));
  connect(_bridgeDownloader, SIGNAL(bridgeRequestFailed(QString)),
          dlg, SLOT(bridgeRequestFailed(QString)));
  connect(_bridgeDownloader, SIGNAL(bridgeRequestFinished(QStringList)),
          dlg, SLOT(bridgeRequestFinished(QStringList)));
  connect(dlg, SIGNAL(retry()), this, SLOT(startBridgeRequest()));

  startBridgeRequest();
  switch (dlg->exec()) {
    case QDialogButtonBox::Cancel:
      _bridgeDownloader->cancelBridgeRequest();
      break;

    case QDialogButtonBox::Help:
      emit helpRequested("bridges.finding");
      break;
  }

  delete dlg;
}

/** Starts a new request for additional bridge addresses. */
void
NetworkPage::startBridgeRequest()
{ 
  if (ui.chkUseProxy->isChecked() && ui.chkProxyUseHttps->isChecked()) {
    _bridgeDownloader->setProxy(ui.lineHttpProxyAddress->text(),
                                ui.lineHttpProxyPort->text().toUInt(),
                                ui.lineHttpProxyUsername->text(),
                                ui.lineHttpProxyPassword->text());
  }

  _bridgeDownloader->downloadBridges(BridgeDownloader::DownloadMethodHttps);
}

/** Called when a previous bridge request initiated by the findBridges()
 * method has completed. <b>bridges</b> contains a list of all bridges
 * received. */
void
NetworkPage::bridgeRequestFinished(const QStringList &bridges)
{
  bool foundNewBridges = false;
  QString normalized;

  foreach (QString bridge, bridges) {
    if (! validateBridge(bridge, &normalized))
      continue;

    QString address = normalized.split(" ").at(0);
    if (ui.listBridges->findItems(address, Qt::MatchContains).isEmpty()) {
      ui.listBridges->addItem(normalized);
      foundNewBridges = true;
    }
  }

  if (! foundNewBridges) {
    QMessageBox dlg(this);
    dlg.setIcon(QMessageBox::Information);
    dlg.setText(tr("No new bridges are currently available. You can either "
                   "wait a while and try again, or try another method of "
                   "finding new bridges."));
    dlg.setInformativeText(tr("Click Help to see other methods of finding "
                              "new bridges."));
    dlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Help);
 
    if (dlg.exec() == QMessageBox::Help)
      emit helpRequested("bridges.finding");      
  }
}
