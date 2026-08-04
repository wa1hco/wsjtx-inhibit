#include "about.h"

#include <QCoreApplication>
#include <QString>

#include "revision_utils.hpp"

#include "ui_about.h"

CAboutDlg::CAboutDlg(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::CAboutDlg)
{
  ui->setupUi(this);

  // Product identity for operators: this binary is wsjtx-inhibit (mainline + TX Inhibit).
  // Keep upstream author credits and version base (WSJT-X mainline v3.0.x).
  auto const ver = QCoreApplication::applicationVersion ();
  auto const rev = display_revision ();
  ui->labelTxt->setText (
    "<h2>" + QString {"wsjtx-inhibit  v%1  %2"}.arg (ver).arg (rev).simplified () + "</h2>"
    "<p><b>WSJT-X mainline v" + ver + " with low-latency TX Inhibit</b></p>"
    "<p>Project: "
    "<a href=\"https://github.com/wa1hco/wsjtx-inhibit\">"
    "https://github.com/wa1hco/wsjtx-inhibit</a></p>"
    "<p>WSJT-X implements digital modes for weak-signal Amateur Radio "
    "communication. This branch adds TX Inhibit for multi-op and same-band "
    "stations (PTT key-line hold while sequencing continues).</p>"
    "<hr />"
    "<p>&copy; 2001-2026 by Joe Taylor, K1JT, Bill Somerville, G4WJS, <br />"
    "Steve Franke, K9AN, Nico Palermo, IV3NWV, <br />"
    "Uwe Risse, DG2YCB, Brian Moran, N9ADG, <br />"
    "Roger Rehr, W3SZ, John Nelson, G4KLA, <br />"
    "Charlie Suckling, DL3WDG, and Terrell Deppe, KJ5HST</p>"
    "<p>We gratefully acknowledge contributions from AC6SL, AE4JY,<br />"
    "DF2ET, DJ0OT, DL3WDG, EA4AC, G4KLA, IW3RAB, JA7UDE,<br />"
    "K3WYC, KA1GT, KA6MAL, KA9Q, KB1ZMX, KD6EKQ, KG4IYS, KI7MT,<br />"
    "KK1D, ND0B, PY1ZRJ, PY2SDR, VE1SKY, VK3ACF, VK4BDJ,<br />"
    "VK7MO, VR2UPU, W3DJS, W4TI, W4TV, and W9MDB.</p>"
    "<p>TX Inhibit additions: wa1hco / wsjtx-inhibit (GPL-3).</p>"
    "<p>Licensed under Version 3 of the GNU General Public License (GPL).</p>"
    "<p>"
    "<a href=\"https://github.com/wa1hco/wsjtx-inhibit\">"
    "<img src=\":/icon_128x128.png\" /></a>"
    "&nbsp;"
    "<a href=\"https://www.gnu.org/licenses/gpl-3.0.txt\">"
    "<img src=\":/gpl-v3-logo.svg\" height=\"80\" /><br />"
    "https://www.gnu.org/licenses/gpl-3.0.txt</a>"
    "</p>");
}

CAboutDlg::~CAboutDlg()
{
}
