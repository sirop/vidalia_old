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
** \file generalpage.cpp
** \version $Id$
** \brief General Tor and Vidalia configuration options
*/

#include <stringutil.h>
#include "generalpage.h"


/** Constructor */
GeneralPage::GeneralPage(QWidget *parent)
: ConfigPage(parent, tr("General"))
{
  /* Invoke the Qt Designer generated object setup routine */
  ui.setupUi(this);

  /* Create settings objects */
  _vidaliaSettings = new VidaliaSettings;
  _torSettings = new TorSettings;
  
  /* Bind event to actions */
  connect(ui.btnBrowseTorExecutable, SIGNAL(clicked()), 
          this, SLOT(browseTorExecutable()));
  connect(ui.btnBrowseProxyExecutable, SIGNAL(clicked()), 
          this, SLOT(browseProxyExecutable()));

  /* Hide platform specific features */
#ifndef Q_WS_WIN
  ui.chkRunVidaliaAtSystemStartup->setVisible(false);
  ui.lineHorizontalSeparator->setVisible(false);
  ui.grpSoftwareUpdates->setVisible(false);
#endif
}

/** Destructor */
GeneralPage::~GeneralPage()
{
  delete _vidaliaSettings;
  delete _torSettings;
}

/** Displays a file dialog allowing the user to browse for an executable
 * file. <b>caption</b> will be displayed in the dialog's title bar and
 * <b>file</b>, if specified, is the default file selected in the dialog.
 */
QString
GeneralPage::browseExecutable(const QString &caption, const QString &file)
{
#if defined(Q_OS_WIN32)
  QString filter = tr("Executables (*.exe)");
#else
  QString filter = "";
#endif
 
  QString filePath = QFileDialog::getOpenFileName(this, caption, file, filter);
  return QDir::convertSeparators(filePath);
}

/** Open a QFileDialog to browse for a Tor executable file. */
void
GeneralPage::browseTorExecutable()
{
  QString filePath = browseExecutable(tr("Select Path to Tor"),
                                      ui.lineTorExecutable->text());
  if (! filePath.isEmpty())
    ui.lineTorExecutable->setText(filePath);
}

/** Open a QFileDialog to browse for a proxy executable file. */
void
GeneralPage::browseProxyExecutable()
{
  QString filePath = browseExecutable(tr("Select Proxy Executable"),
                                      ui.lineProxyExecutable->text());

  if (! filePath.isEmpty())
    ui.lineProxyExecutable->setText(filePath);
}

/** Saves all settings for this page */
bool
GeneralPage::save(QString &errmsg)
{
  QString torExecutable = ui.lineTorExecutable->text();
  if (torExecutable.isEmpty()) {
    errmsg = tr("You must specify the name of your Tor executable.");
    return false;
  }
  if (ui.chkRunProxyAtTorStartup->isChecked()) {
    bool ok;
    QStringList proxyArgs = string_parse_arguments(
                              ui.lineProxyExecutableArguments->text(), &ok);
    if (! ok) {
      errmsg = tr("The proxy arguments specified are not properly formatted.");
      return false;
    }
    _vidaliaSettings->setProxyExecutable(ui.lineProxyExecutable->text());
    _vidaliaSettings->setProxyExecutableArguments(proxyArgs);
  }
  
  _torSettings->setExecutable(torExecutable);
  _vidaliaSettings->setRunTorAtStart(ui.chkRunTorAtVidaliaStartup->isChecked());
  _vidaliaSettings->setRunVidaliaOnBoot(
    ui.chkRunVidaliaAtSystemStartup->isChecked());
  _vidaliaSettings->setRunProxyAtStart(
    ui.chkRunProxyAtTorStartup->isChecked());

  return true;
}

/** Loads previously saved settings */
void
GeneralPage::load()
{
  ui.chkRunVidaliaAtSystemStartup->setChecked(
    _vidaliaSettings->runVidaliaOnBoot());
  
  ui.lineTorExecutable->setText(_torSettings->getExecutable());
  ui.chkRunTorAtVidaliaStartup->setChecked(_vidaliaSettings->runTorAtStart());

  ui.lineProxyExecutable->setText(_vidaliaSettings->getProxyExecutable());
  ui.lineProxyExecutableArguments->setText(
    string_format_arguments(_vidaliaSettings->getProxyExecutableArguments()));
  ui.chkRunProxyAtTorStartup->setChecked(_vidaliaSettings->runProxyAtStart());
}

