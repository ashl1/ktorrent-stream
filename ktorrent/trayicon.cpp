/***************************************************************************
 *   Copyright (C) 2005-2007 by                                            *
 *   Joris Guisson <joris.guisson@gmail.com>                               *
 *   Ivan Vasic <ivasic@gmail.com>                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/
#include <QPainter>
#include <QtAlgorithms>
#include <KIcon>
#include <klocale.h>
#include <kapplication.h>
#include <qtooltip.h>
#include <kpassivepopup.h>
#include <knotification.h>
#include <interfaces/torrentinterface.h>
#include <util/functions.h>
#include <net/socketmonitor.h>
#include <util/log.h>
#include <settings.h>
#include <interfaces/torrentinterface.h>
#include <interfaces/torrentactivityinterface.h>
#include <torrent/queuemanager.h>
#include "core.h"
#include "trayicon.h"
#include "gui.h"
#include <kactioncollection.h>


using namespace bt;

namespace kt
{
	
	
	TrayIcon::TrayIcon(Core* core,GUI* parent) : QObject(parent),core(core),mwnd(parent)
	{
		status_notifier_item = 0;
		max_upload_rate = 0;
		max_download_rate = 0;
		previousDownloadHeight = 0;
		previousUploadHeight = 0;
		queue_suspended = false;
		menu = 0;


		connect(core, SIGNAL(openedSilently(bt::TorrentInterface*)),
				this, SLOT(torrentSilentlyOpened(bt::TorrentInterface* )));
		connect(core, SIGNAL(finished(bt::TorrentInterface* )),
				this, SLOT(finished(bt::TorrentInterface* )));
		connect(core,SIGNAL(torrentStoppedByError(bt::TorrentInterface*, QString )),
				this,SLOT(torrentStoppedByError(bt::TorrentInterface*, QString )));
		connect(core,SIGNAL(maxShareRatioReached( bt::TorrentInterface* )),
				this,SLOT(maxShareRatioReached( bt::TorrentInterface* )));
		connect(core,SIGNAL(maxSeedTimeReached(bt::TorrentInterface*)),
				this, SLOT(maxSeedTimeReached(bt::TorrentInterface*)));
		connect(core,SIGNAL(corruptedData( bt::TorrentInterface* )),
				this,SLOT(corruptedData( bt::TorrentInterface* )));
		connect(core, SIGNAL(queuingNotPossible( bt::TorrentInterface* )),
				this, SLOT(queuingNotPossible( bt::TorrentInterface* )));
		connect(core,SIGNAL(canNotStart(bt::TorrentInterface*, bt::TorrentStartResponse)),
				this,SLOT(canNotStart(bt::TorrentInterface*, bt::TorrentStartResponse)));
		connect(core, SIGNAL(lowDiskSpace(bt::TorrentInterface*, bool)),
				this, SLOT(lowDiskSpace(bt::TorrentInterface*, bool)));
		connect(core,SIGNAL(canNotLoadSilently(const QString&)),
				this,SLOT(cannotLoadTorrentSilently(const QString&)));
		connect(core,SIGNAL(dhtNotEnabled(QString)),
				this,SLOT(dhtNotEnabled(QString)));
		connect(core->getQueueManager(),SIGNAL(suspendStateChanged(bool)),
				this,SLOT(suspendStateChanged(bool))); 
		
		suspendStateChanged(core->getQueueManager()->getSuspendedState());
	}

	TrayIcon::~TrayIcon()
	{}
	
	void TrayIcon::hide()
	{
		if (!status_notifier_item)
			return;
		
		delete status_notifier_item;
		status_notifier_item = 0;
		menu = 0;
		max_download_rate = max_upload_rate = 0;
	}
	
	void TrayIcon::show()
	{
		if (status_notifier_item)
		{
			suspendStateChanged(core->getQueueManager()->getSuspendedState());
			return;
		}
		
		status_notifier_item = new KStatusNotifierItem(mwnd);
		connect(status_notifier_item, SIGNAL(secondaryActivateRequested(QPoint)), this, SLOT(secondaryActivate(QPoint)));
		
		menu = status_notifier_item->contextMenu();
	
		max_upload_rate = new SetMaxRate(core,SetMaxRate::UPLOAD,menu);
		max_upload_rate->setTitle(i18n("Set max upload speed"));
		max_download_rate = new SetMaxRate(core,SetMaxRate::DOWNLOAD,menu);
		max_download_rate->setTitle(i18n("Set max download speed"));
		menu->addMenu(max_download_rate);
		menu->addMenu(max_upload_rate);
		menu->addSeparator();
		
		KActionCollection* ac = mwnd->getTorrentActivity()->part()->actionCollection();
		menu->addAction(ac->action("start_all"));
		menu->addAction(ac->action("stop_all"));
		menu->addAction(ac->action("queue_suspend"));
		menu->addSeparator();
		
		ac = mwnd->actionCollection();
		menu->addAction(ac->action("paste_url"));
		menu->addAction(ac->action(KStandardAction::name(KStandardAction::Open)));
		menu->addSeparator();
		menu->addAction(ac->action(KStandardAction::name(KStandardAction::Preferences)));
		menu->addSeparator();
		
		
		status_notifier_item->setIconByName("ktorrent");
		status_notifier_item->setCategory(KStatusNotifierItem::ApplicationStatus);
		status_notifier_item->setStatus(KStatusNotifierItem::Passive);
		status_notifier_item->setStandardActionsEnabled(true);
		status_notifier_item->setContextMenu(menu);
		
		queue_suspended = core->getQueueManager()->getSuspendedState();
		if (queue_suspended)
			status_notifier_item->setOverlayIconByName("kt-pause");
	}


	void TrayIcon::updateStats(const CurrentStats & stats)
	{
		if (!status_notifier_item)
			return;
		
		status_notifier_item->setStatus(core->getQueueManager()->getNumRunning(QueueManager::DOWNLOADS) > 0 ?
						KStatusNotifierItem::Active : KStatusNotifierItem::Passive);
		QString tip = i18n("<table>"
				"<tr><td>Download&nbsp;speed:</td><td><b>%1</b></td></tr>"
				"<tr><td>Upload&nbsp;speed:</td><td><b>%2</b></td></tr>"
				"<tr><td>Received:</td><td><b>%3</b></td></tr>"
				"<tr><td>Transmitted:</td><td><b>%4</b></td></tr>"
				"</table>",
				BytesPerSecToString((double)stats.download_speed),
				BytesPerSecToString((double)stats.upload_speed),
				BytesToString(stats.bytes_downloaded),
				BytesToString(stats.bytes_uploaded));
		
		status_notifier_item->setToolTip("ktorrent",i18n("Status"),tip);
	}

	void TrayIcon::showPassivePopup(const QString & msg,const QString & title)
	{
		if (status_notifier_item)
			status_notifier_item->showMessage(title,msg,"ktorrent");
	}
	
	void TrayIcon::cannotLoadTorrentSilently(const QString & msg)
	{
		if (!Settings::showPopups())
			return;
		
		KNotification::event("CannotLoadSilently",msg,QPixmap(),mwnd);
	}

	void TrayIcon::dhtNotEnabled(const QString& msg)
	{
		if (!Settings::showPopups())
			return;

		KNotification::event("DHTNotEnabled",msg,QPixmap(),mwnd);
	}

	void TrayIcon::torrentSilentlyOpened(bt::TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;

		QString msg = i18n("<b>%1</b> was silently opened.",
					tc->getDisplayName());
		KNotification::event("TorrentSilentlyOpened",msg,QPixmap(),mwnd);
	}

	void TrayIcon::finished(bt::TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
		double speed_up = (double)s.bytes_uploaded;
		double speed_down = (double)(s.bytes_downloaded - s.imported_bytes);

		QString msg = i18n("<b>%1</b> has completed downloading."
					"<br>Average speed: %2 DL / %3 UL.",
					tc->getDisplayName(),
					BytesPerSecToString(speed_down / tc->getRunningTimeDL()),
					BytesPerSecToString(speed_up / tc->getRunningTimeUL()));

		KNotification::event("TorrentFinished",msg,QPixmap(),mwnd);
	}

	void TrayIcon::maxShareRatioReached(bt::TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
		KLocale* loc = KGlobal::locale();
		double speed_up = (double)s.bytes_uploaded;
		
		QString msg = i18n("<b>%1</b> has reached its maximum share ratio of %2 and has been stopped."
				"<br>Uploaded %3 at an average speed of %4.",
				tc->getDisplayName(),
				loc->formatNumber(s.max_share_ratio,2),
				BytesToString(s.bytes_uploaded),
				BytesPerSecToString(speed_up / tc->getRunningTimeUL()));

		KNotification::event("MaxShareRatioReached",msg,QPixmap(),mwnd);
	}

	void TrayIcon::maxSeedTimeReached(bt::TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
		KLocale* loc = KGlobal::locale();
		double speed_up = (double)s.bytes_uploaded;
		
		QString msg = i18n("<b>%1</b> has reached its maximum seed time of %2 hours and has been stopped."
				"<br>Uploaded %3 at an average speed of %4.",
				tc->getDisplayName(),
				loc->formatNumber(s.max_seed_time,2),
				BytesToString(s.bytes_uploaded),
				BytesPerSecToString(speed_up / tc->getRunningTimeUL()));

		KNotification::event("MaxSeedTimeReached",msg,QPixmap(),mwnd);
	}

	void TrayIcon::torrentStoppedByError(bt::TorrentInterface* tc, QString msg)
	{
		if (!Settings::showPopups())
			return;
		
		QString err_msg = i18n("<b>%1</b> has been stopped with the following error: <br>%2",
					tc->getDisplayName(),msg);

		KNotification::event("TorrentStoppedByError",err_msg,QPixmap(),mwnd);
	}

	void TrayIcon::corruptedData(bt::TorrentInterface* tc)
	{	
		if (!Settings::showPopups())
			return;
		
		QString err_msg = i18n("Corrupted data has been found in the torrent <b>%1</b>"
				"<br>It would be a good idea to do a data integrity check on the torrent.",tc->getDisplayName());
				
		KNotification::event("CorruptedData",err_msg,QPixmap(),mwnd);
	}

	void TrayIcon::queuingNotPossible(bt::TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
			
		QString msg;
		KLocale* loc = KGlobal::locale();
		 
		if (tc->overMaxRatio())
			msg = i18n("<b>%1</b> has reached its maximum share ratio of %2 and cannot be enqueued. "
					"<br>Remove the limit manually if you want to continue seeding.",
				tc->getDisplayName(),loc->formatNumber(s.max_share_ratio,2));
		else
			msg = i18n("<b>%1</b> has reached its maximum seed time of %2 hours and cannot be enqueued. "
					"<br>Remove the limit manually if you want to continue seeding.",
				tc->getDisplayName(),loc->formatNumber(s.max_seed_time,2));
		
		KNotification::event("QueueNotPossible",msg,QPixmap(),mwnd);
	}

	void TrayIcon::canNotStart(bt::TorrentInterface* tc,bt::TorrentStartResponse reason)
	{
		if (!Settings::showPopups())
			return;
		
		QString msg = i18n("Cannot start <b>%1</b>: <br>",tc->getDisplayName());
		switch (reason)
		{
		case bt::QM_LIMITS_REACHED:
			if (tc->getStats().bytes_left_to_download == 0)
			{
				// is a seeder
				msg += i18np("Cannot seed more than 1 torrent. <br>",
							"Cannot seed more than %1 torrents. <br>",Settings::maxSeeds());
			}
			else
			{
				msg += i18np("Cannot download more than 1 torrent. <br>",
							"Cannot download more than %1 torrents. <br>",Settings::maxDownloads());
			}
			msg += i18n("Go to Settings -> Configure KTorrent, if you want to change the limits.");
			KNotification::event("CannotStart",msg,QPixmap(),mwnd);
			break;
		case bt::NOT_ENOUGH_DISKSPACE:
			msg += i18n("There is not enough diskspace available.");
			KNotification::event("CannotStart",msg,QPixmap(),mwnd);
			break;
		default:
			break;
		}
	}

	void TrayIcon::lowDiskSpace(bt::TorrentInterface * tc, bool stopped)
	{
		if (!Settings::showPopups())
			return;
		
		QString msg = i18n("Your disk is running out of space.<br /><b>%1</b> is being downloaded to '%2'.",tc->getDisplayName(),tc->getDataDir());
		
		if(stopped)
			msg.prepend(i18n("Torrent has been stopped.<br />"));

		KNotification::event("LowDiskSpace",msg);
	}
	
	void TrayIcon::updateMaxRateMenus()
	{
		if (max_download_rate && max_upload_rate)
		{
			max_upload_rate->update();
			max_download_rate->update();
		}
	}



	SetMaxRate::SetMaxRate(Core* core, Type t, QWidget *parent) : KMenu(parent)
	{
		setIcon(t == UPLOAD ? KIcon("kt-set-max-upload-speed") : KIcon("kt-set-max-download-speed"));
		m_core = core;
		type=t;
		makeMenu();
		connect(this,SIGNAL(triggered(QAction*)),this,SLOT(onTriggered(QAction* )));
		connect(this,SIGNAL(aboutToShow()),this,SLOT(update()));
	}
	
	SetMaxRate::~SetMaxRate()
	{}

	void SetMaxRate::makeMenu()
	{
		int rate = (type==UPLOAD) ? net::SocketMonitor::getUploadCap() / 1024 : net::SocketMonitor::getDownloadCap() / 1024;
		int maxBandwidth=(rate > 0) ? rate : (type==UPLOAD) ? 0 : 20 ;
		int delta = 0;
		int maxBandwidthRounded;

		addTitle(i18n("Speed limit in KiB/s"));

		unlimited = addAction(i18n("Unlimited"));
		unlimited->setCheckable(true);
		unlimited->setChecked(rate == 0);

		if((maxBandwidth%5)>=3)
			maxBandwidthRounded=maxBandwidth + 5 - (maxBandwidth%5);
		else
			maxBandwidthRounded=maxBandwidth - (maxBandwidth%5);

		QList<int> values;
		for (int i = 0; i < 15; i++)
		{
			if (delta == 0)
				values.append(maxBandwidth);
			else
			{
				if ((maxBandwidth%5)!=0)
				{
					values.append(maxBandwidthRounded - delta);
					values.append(maxBandwidthRounded + delta);
				}
				else
				{
					values.append(maxBandwidth - delta);
					values.append(maxBandwidth + delta);
				}
			}

			delta += (delta >= 50) ? 50 : (delta >= 20) ? 10 : 5;
		}
		
		qSort(values);
		foreach(int v,values)
		{
			if (v >= 1)
			{
				QAction* act = addAction(QString("%1").arg(v));
				act->setCheckable(true);
				act->setChecked(rate == v);
			}
		}
	}

	void SetMaxRate::update()
	{
		clear();
		makeMenu();
	}

	void SetMaxRate::onTriggered(QAction* act)
	{
		int rate;
		if(act == unlimited)
			rate=0;
		else
			rate=act->text().remove("&").toInt(); // remove ampersands

		if(type == UPLOAD)
		{
			Settings::setMaxUploadRate(rate);
			net::SocketMonitor::setUploadCap( Settings::maxUploadRate() * 1024);
		}
		else
		{
			Settings::setMaxDownloadRate(rate);
			net::SocketMonitor::setDownloadCap(Settings::maxDownloadRate()*1024);
		}
		Settings::self()->writeConfig();
	}
		
	void TrayIcon::suspendStateChanged(bool suspended)
	{
		queue_suspended = queue_suspended;
		if (!suspended)
		{
			if (status_notifier_item)
				status_notifier_item->setOverlayIconByName(QString());
		}
		else
		{
			if (status_notifier_item)
				status_notifier_item->setOverlayIconByName("kt-pause");
		}
	}

	void TrayIcon::secondaryActivate(const QPoint& pos)
	{
		Q_UNUSED(pos);
		core->setSuspendedState(!core->getSuspendedState());
	}

}

#include "trayicon.moc"
