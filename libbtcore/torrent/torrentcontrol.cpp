/***************************************************************************
 *   Copyright (C) 2005 by                                                 *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#include <qdir.h>
#include <qfile.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <qtextstream.h>
#include <util/log.h>
#include <util/error.h>
#include <util/bitset.h>
#include <util/functions.h>
#include <util/fileops.h>
#include <util/waitjob.h>
#include <interfaces/trackerslist.h>
#include <interfaces/monitorinterface.h>
#include <datachecker/singledatachecker.h>
#include <datachecker/multidatachecker.h>
#include <datachecker/datacheckerlistener.h>
#include <datachecker/datacheckerthread.h>
#include <migrate/ccmigrate.h>
#include <migrate/cachemigrate.h>
#include <dht/dhtbase.h>

#include <download/downloader.h>
#include "uploader.h"
#include "peersourcemanager.h"
#include <diskio/cache.h>
#include <diskio/chunkmanager.h>
#include "torrent.h"
#include <peer/peermanager.h>

#include "torrentfile.h"
#include "torrentcontrol.h"

#include <peer/peer.h>
#include "choker.h"

#include "globals.h"
#include "server.h"
#include <peer/packetwriter.h>
#include <interfaces/queuemanagerinterface.h>
#include "statsfile.h"
#include <diskio/preallocationthread.h>
#include "timeestimator.h"
#include <net/socketmonitor.h>

namespace bt
{

	KUrl TorrentControl::completed_dir;
	bool TorrentControl::completed_datacheck = false;
	Uint32 TorrentControl::min_diskspace = 100;
	bool TorrentControl::auto_recheck = true;
	Uint32 TorrentControl::num_corrupted_for_recheck = 3;

	TorrentControl::TorrentControl()
	: tor(0),psman(0),cman(0),pman(0),down(0),up(0),choke(0),tmon(0),prealloc(false)
	{
		istats.last_announce = 0;
		stats.imported_bytes = 0;
		stats.trk_bytes_downloaded = 0;
		stats.trk_bytes_uploaded = 0;
		stats.running = false;
		stats.started = false;
		stats.stopped_by_error = false;
		stats.session_bytes_downloaded = 0;
		stats.session_bytes_uploaded = 0;
		istats.session_bytes_uploaded = 0;
		old_tordir = QString::null;
		stats.status = NOT_STARTED;
		stats.autostart = true;
		stats.user_controlled = false;
		stats.priv_torrent = false;
		stats.seeders_connected_to = stats.seeders_total = 0;
		stats.leechers_connected_to = stats.leechers_total = 0;
		stats.max_share_ratio = 0.00f;
		stats.max_seed_time = 0;
		istats.running_time_dl = istats.running_time_ul = 0;
		istats.prev_bytes_dl = 0;
		istats.prev_bytes_ul = 0;
		istats.trk_prev_bytes_dl = istats.trk_prev_bytes_ul = 0;
		istats.io_error = false;
		istats.priority = 0;
		istats.custom_output_name = false;
		istats.diskspace_warning_emitted = false;
		updateStats();
		prealloc_thread = 0;
		dcheck_thread = 0;
		istats.dht_on = false;
		stats.num_corrupted_chunks = 0;
		
		m_eta = new TimeEstimator(this);
		// by default no torrent limits
		upload_gid = download_gid = 0;
		upload_limit = download_limit = 0;
		moving_files = false;
	}




	TorrentControl::~TorrentControl()
	{
		if (stats.running)
			stop(false);
		
		if (tmon)
			tmon->destroyed();
		delete choke;
		delete down;
		delete up;
		delete cman;
		delete pman;
		delete psman;
		delete tor;
		delete m_eta;
	}
	
	bool TorrentControl::updateNeeded() const
	{
		return stats.running || moving_files || prealloc_thread || dcheck_thread;
	}

	void TorrentControl::update()
	{
		UpdateCurrentTime();
		if (moving_files)
			return;

		if (dcheck_thread)
		{
			if (!dcheck_thread->isRunning())
			{
				dcheck_thread->wait();
				afterDataCheck();
			}
			else
				return;
		}
		
		if (istats.io_error)
		{
			stop(false);
			emit stoppedByError(this, error_msg);
			return;
		}
		
		if (prealloc_thread)
		{
			if (prealloc_thread->isDone())
			{
				prealloc_thread->wait();
				preallocThreadDone();
			}
			else
				return; // preallocation still going on, so just return
		}
		

		try
		{
			// first update peermanager
			pman->update();
			bool comp = stats.completed;

			// then the downloader and uploader
			up->update(choke->getOptimisticlyUnchokedPeerID());			
			down->update();

			//helper var, check if needed to move completed files somewhere
			bool moveCompleted = false;
			bool checkOnCompletion = false;

			stats.completed = cman->completed();
			if (stats.completed && !comp)
			{
				pman->killSeeders();
				QDateTime now = QDateTime::currentDateTime();
				istats.running_time_dl += istats.time_started_dl.secsTo(now);
				updateStatusMsg();
				updateStats();
				
				// download has just been completed
				// only sent completed to tracker when we have all chunks (so no excluded chunks)
				if (cman->haveAllChunks())
					psman->completed();
				
				finished(this);

				//Move completed download to specified directory if needed
				if (!completed_dir.path().isNull())
					moveCompleted = true;
				
				// See if we need to do a data check
				if (completed_datacheck)
					checkOnCompletion = true;
			}
			else if (!stats.completed && comp)
			{
				// restart download if necesarry
				// when user selects that files which were previously excluded,
				// should now be downloaded
				if (!psman->isStarted())
					psman->start();
				else
					psman->manualUpdate();
				istats.last_announce = bt::GetCurrentTime();
				istats.time_started_dl = QDateTime::currentDateTime();
			}
			updateStatusMsg();
			
			// get rid of dead Peers
			Uint32 num_cleared = pman->clearDeadPeers();
			
			// we may need to update the choker
			if (choker_update_timer.getElapsedSinceUpdate() >= 10000 || num_cleared > 0)
			{
				// also get rid of seeders & uninterested when download is finished
				// no need to keep them around, but also no need to do this
				// every update, so once every 10 seconds is fine
				if (stats.completed)
				{
					pman->killSeeders();
				}
				
				doChoking();
				choker_update_timer.update();
				// a good opportunity to make sure we are not keeping to much in memory
				cman->checkMemoryUsage();
			}

			// to satisfy people obsessed with their share ratio
			if (stats_save_timer.getElapsedSinceUpdate() >= 5*60*1000)
			{
				saveStats();
				stats_save_timer.update();
			}

			// Update DownloadCap
			updateStats();

			if (stats.download_rate > 0)
				stalled_timer.update();
			
			// do a manual update if we are stalled for more then 2 minutes
			// we do not do this for private torrents
			if (stalled_timer.getElapsedSinceUpdate() > 120000 && !stats.completed &&
				!stats.priv_torrent)
			{
				Out(SYS_TRK|LOG_NOTICE) << "Stalled for too long, time to get some fresh blood" << endl;
				psman->manualUpdate();
				stalled_timer.update();
			}
			
			if(overMaxRatio() || overMaxSeedTime()) 
			{ 
				if(istats.priority!=0) //if it's queued make sure to dequeue it 
				{
					setPriority(0);
					stats.user_controlled = true;
				}
                 
				stop(true); 
				emit seedingAutoStopped(this, overMaxRatio() ? MAX_RATIO_REACHED : MAX_SEED_TIME_REACHED);
			} 			

			//Move completed files if needed:
			if(moveCompleted)
			{
				QString outdir = completed_dir.path();
				if(!outdir.endsWith(bt::DirSeparator()))
					outdir += bt::DirSeparator();
				
				changeOutputDir(outdir,bt::TorrentInterface::MOVE_FILES);
			}
			
			//Update diskspace if needed (every 1 min)			
			if(!stats.completed && stats.running && bt::GetCurrentTime() - last_diskspace_check >= 60 * 1000)
			{
				checkDiskSpace(true);
			}

			// Emit the needDataCheck signal if needed
			if (checkOnCompletion || (auto_recheck && stats.num_corrupted_chunks >= num_corrupted_for_recheck))
				needDataCheck(this);
		}
		catch (Error & e)
		{
			onIOError(e.toString());
		}
	}

	void TorrentControl::onIOError(const QString & msg)
	{
		Out(SYS_DIO|LOG_IMPORTANT) << "Error : " << msg << endl;
		stats.stopped_by_error = true;
		stats.status = ERROR;
		error_msg = msg;
		istats.io_error = true;
		statusChanged(this);
	}

	void TorrentControl::start()
	{	
		// do not start running torrents
		if (stats.running || stats.status == ALLOCATING_DISKSPACE || moving_files)
			return;

		stats.stopped_by_error = false;
		istats.io_error = false;
		istats.diskspace_warning_emitted = false;
		try
		{	
			bool ret = true;
			aboutToBeStarted(this,ret);
			if (!ret)
				return;
		}
		catch (Error & err)
		{
			// something went wrong when files were recreated, set error and rethrow
			onIOError(err.toString());
			return;
		}
		
		try
		{
			cman->start();
		}
		catch (Error & e)
		{
			onIOError(e.toString());
			throw;
		}
		
		istats.time_started_ul = istats.time_started_dl = QDateTime::currentDateTime();
		resetTrackerStats();
		
		if (prealloc)
		{
			// only start preallocation if we are allowed by the settings
			if (Cache::preallocationEnabled() && !cman->haveAllChunks())
			{
				Out(SYS_GEN|LOG_NOTICE) << "Pre-allocating diskspace" << endl;
				prealloc_thread = new PreallocationThread(cman);
				stats.running = true;
				stats.status = ALLOCATING_DISKSPACE;
				prealloc_thread->start();
				statusChanged(this);
				return;
			}
			else
			{
				prealloc = false;
			}
		}
		
		continueStart();
	}
	
	void TorrentControl::continueStart()
	{
		// continues start after the prealloc_thread has finished preallocation	
		pman->start();
		pman->loadPeerList(tordir + "peer_list");
		try
		{
			down->loadDownloads(tordir + "current_chunks");
		}
		catch (Error & e)
		{
			// print out warning in case of failure
			// we can still continue the download
			Out(SYS_GEN|LOG_NOTICE) << "Warning : " << e.toString() << endl;
		}
		
		loadStats();
		stats.running = true;
		stats.started = true;
		stats.autostart = true;
		choker_update_timer.update();
		stats_save_timer.update();
		
		
		stalled_timer.update();
		psman->start();
		istats.last_announce = bt::GetCurrentTime();
		stalled_timer.update();
	}
		

	void TorrentControl::stop(bool user,WaitJob* wjob)
	{
		QDateTime now = QDateTime::currentDateTime();
		if(!stats.completed)
			istats.running_time_dl += istats.time_started_dl.secsTo(now);
		istats.running_time_ul += istats.time_started_ul.secsTo(now);
		istats.time_started_ul = istats.time_started_dl = now;
		
		// stop preallocation thread if necesarry
		if (prealloc_thread)
		{
			prealloc_thread->stop();
			prealloc_thread->wait();
			
			if (prealloc_thread->errorHappened() || prealloc_thread->isNotFinished())
			{
				delete prealloc_thread;
				prealloc_thread = 0;
				prealloc = true;
				saveStats(); // save stats, so that we will start preallocating the next time
			}
			else
			{
				delete prealloc_thread;
				prealloc_thread = 0;
				prealloc = false;
			}
		}
	
		if (stats.running)
		{
			psman->stop(wjob);

			if (tmon)
				tmon->stopped();

			try
			{
				down->saveDownloads(tordir + "current_chunks");
			}
			catch (Error & e)
			{
				// print out warning in case of failure
				// it doesn't corrupt the data, so just a couple of lost chunks
				Out(SYS_GEN|LOG_NOTICE) << "Warning : " << e.toString() << endl;
			}
			
			down->clearDownloads();
			if (user)
			{
				//make this torrent user controlled
				setPriority(0);
				stats.autostart = false;
			}
		}
		pman->savePeerList(tordir + "peer_list");
		pman->stop();
		pman->closeAllConnections();
		pman->clearDeadPeers();
		cman->stop();
		
		stats.running = false;
		saveStats();
		updateStatusMsg();
		updateStats();
		stats.trk_bytes_downloaded = 0;
		stats.trk_bytes_uploaded = 0;

		emit torrentStopped(this);
	}

	void TorrentControl::setMonitor(MonitorInterface* tmo)
	{
		tmon = tmo;
		down->setMonitor(tmon);
		if (tmon)
		{
			for (Uint32 i = 0;i < pman->getNumConnectedPeers();i++)
				tmon->peerAdded(pman->getPeer(i));
		}
	}
	
	
	void TorrentControl::init(QueueManagerInterface* qman,
							  const QString & torrent,
							  const QString & tmpdir,
							  const QString & ddir,
							  const QString & default_save_dir)
	{
		// first load the torrent file
		tor = new Torrent();
		try
		{
			tor->load(torrent,false);
		}
		catch (...)
		{
			delete tor;
			tor = 0;
			throw Error(i18n("An error occurred while loading the torrent."
					" The torrent is probably corrupt or is not a torrent file.\n%1",torrent));
		}
		
		initInternal(qman,tmpdir,ddir,default_save_dir,torrent.startsWith(tmpdir));
		
		// copy torrent in tor dir
		QString tor_copy = tordir + "torrent";
		if (tor_copy != torrent)
		{
			bt::CopyFile(torrent,tor_copy);
		}
		
	}
	
	
	void TorrentControl::init(QueueManagerInterface* qman, const QByteArray & data,const QString & tmpdir,
							  const QString & ddir,const QString & default_save_dir)
	{
		// first load the torrent file
		tor = new Torrent();
		try
		{
			tor->load(data,false);
		}
		catch (...)
		{
			delete tor;
			tor = 0;
			throw Error(i18n("An error occurred while loading the torrent."
					" The torrent is probably corrupt or is not a torrent file."));
		}
		
		initInternal(qman,tmpdir,ddir,default_save_dir,true);
		// copy data into torrent file
		QString tor_copy = tordir + "torrent";
		QFile fptr(tor_copy);
		if (!fptr.open(QIODevice::WriteOnly))
			throw Error(i18n("Unable to create %1 : %2",tor_copy,fptr.errorString()));
	
		fptr.write(data.data(),data.size());
	}
	
	void TorrentControl::checkExisting(QueueManagerInterface* qman)
	{
		// check if we haven't already loaded the torrent
		// only do this when qman isn't 0
		if (qman && qman->allreadyLoaded(tor->getInfoHash()))
		{
			if (!stats.priv_torrent)
			{
				qman->mergeAnnounceList(tor->getInfoHash(),tor->getTrackerList());

				throw Error(i18n("You are already downloading this torrent %1, the list of trackers of both torrents has been merged.",tor->getNameSuggestion()));
			}
			else
			{
				throw Error(i18n("You are already downloading the torrent %1",tor->getNameSuggestion()));
			}
		}
	}
	
	void TorrentControl::setupDirs(const QString & tmpdir,const QString & ddir)
	{
		tordir = tmpdir;
		
		if (!tordir.endsWith(DirSeparator()))
			tordir += DirSeparator();

		outputdir = ddir.trimmed();
		if (outputdir.length() > 0 && !outputdir.endsWith(DirSeparator()))
			outputdir += DirSeparator();
		
		if (!bt::Exists(tordir))
		{
			bt::MakeDir(tordir);
		}
	}
	
	void TorrentControl::setupStats()
	{
		stats.completed = false;
		stats.running = false;
		stats.torrent_name = tor->getNameSuggestion();
		stats.multi_file_torrent = tor->isMultiFile();
		stats.total_bytes = tor->getFileLength();
		stats.priv_torrent = tor->isPrivate();
		
		// check the stats file for the custom_output_name variable
		StatsFile st(tordir + "stats");
		if (st.hasKey("CUSTOM_OUTPUT_NAME") && st.readULong("CUSTOM_OUTPUT_NAME") == 1)
		{
			istats.custom_output_name = true;
		}
		
		// load outputdir if outputdir is null
		if (outputdir.isNull() || outputdir.length() == 0)
			loadOutputDir();
	}
	
	void TorrentControl::setupData(const QString & ddir)
	{
		// create PeerManager and Tracker
		pman = new PeerManager(*tor);
		//Out() << "Tracker url " << url << " " << url.protocol() << " " << url.prettyURL() << endl;
		psman = new PeerSourceManager(this,pman);
		connect(psman,SIGNAL(statusChanged( const QString& )),
				this,SLOT(trackerStatusChanged( const QString& )));


		// Create chunkmanager, load the index file if it exists
		// else create all the necesarry files
		cman = new ChunkManager(*tor,tordir,outputdir,istats.custom_output_name);
		
		connect(cman,SIGNAL(updateStats()),this,SLOT(updateStats()));
		if (bt::Exists(tordir + "index"))
			cman->loadIndexFile();

		stats.completed = cman->completed();

		// create downloader,uploader and choker
		down = new Downloader(*tor,*pman,*cman);
		connect(down,SIGNAL(ioError(const QString& )),
				this,SLOT(onIOError(const QString& )));
		up = new Uploader(*cman,*pman);
		choke = new Choker(*pman,*cman);


		connect(pman,SIGNAL(newPeer(Peer* )),this,SLOT(onNewPeer(Peer* )));
		connect(pman,SIGNAL(peerKilled(Peer* )),this,SLOT(onPeerRemoved(Peer* )));
		connect(cman,SIGNAL(excluded(Uint32, Uint32 )),down,SLOT(onExcluded(Uint32, Uint32 )));
		connect(cman,SIGNAL(included( Uint32, Uint32 )),down,SLOT(onIncluded( Uint32, Uint32 )));
		connect(cman,SIGNAL(corrupted( Uint32 )),this,SLOT(corrupted( Uint32 )));
	}
	
	void TorrentControl::initInternal(QueueManagerInterface* qman,
									  const QString & tmpdir,
									  const QString & ddir,
									  const QString & default_save_dir,
									  bool first_time)
	{
		checkExisting(qman);
		setupDirs(tmpdir,ddir);
		setupStats();

		if (!first_time)
		{
			// if we do not need to copy the torrent, it is an existing download and we need to see
			// if it is not an old download
			try
			{
				migrateTorrent(default_save_dir);
			}
			catch (Error & err)
			{
				
				throw Error(i18n("Cannot migrate %1 : %2",tor->getNameSuggestion(),err.toString()));
			}
		}
		setupData(ddir);

		updateStatusMsg();

		// to get rid of phantom bytes we need to take into account
		// the data from downloads already in progress
		try
		{
			Uint64 db = down->bytesDownloaded();
			Uint64 cb = down->getDownloadedBytesOfCurrentChunksFile(tordir + "current_chunks");
			istats.prev_bytes_dl = db + cb;
				
		//	Out() << "Downloaded : " << BytesToString(db) << endl;
		//	Out() << "current_chunks : " << BytesToString(cb) << endl;
		}
		catch (Error & e)
		{
			// print out warning in case of failure
			Out() << "Warning : " << e.toString() << endl;
			istats.prev_bytes_dl = down->bytesDownloaded();
		}
		
		loadStats();
		updateStats();
		saveStats();
		stats.output_path = cman->getOutputPath();
	/*	if (stats.output_path.isNull())
		{
			cman->createFiles();
			stats.output_path = cman->getOutputPath();
		}*/
		Out() << "OutputPath = " << stats.output_path << endl;
	}
	


	bool TorrentControl::announceAllowed()
	{
		if(istats.last_announce == 0)
			return true;
		
		if (psman && psman->getNumFailures() == 0)
			return bt::GetCurrentTime() - istats.last_announce >= 60 * 1000;
		else
			return true;
	}
	
	void TorrentControl::updateTracker()
	{
		if (stats.running && announceAllowed())
		{
			psman->manualUpdate();
			istats.last_announce = bt::GetCurrentTime();
		}
	}
	
	void TorrentControl::scrapeTracker()
	{
		psman->scrape();
	}

	void TorrentControl::onNewPeer(Peer* p)
	{
		connect(p,SIGNAL(gotPortPacket( const QString&, Uint16 )),
				this,SLOT(onPortPacket( const QString&, Uint16 )));
		
		if (p->getStats().fast_extensions)
		{
			const BitSet & bs = cman->getBitSet();
			if (bs.allOn())
				p->getPacketWriter().sendHaveAll();
			else if (bs.numOnBits() == 0)
				p->getPacketWriter().sendHaveNone();
			else
				p->getPacketWriter().sendBitSet(bs);
		}
		else
		{
			p->getPacketWriter().sendBitSet(cman->getBitSet());
		}
		
		if (!stats.completed)
			p->getPacketWriter().sendInterested();
		
		if (!stats.priv_torrent)
		{
#ifdef ENABLE_DHT_SUPPORT
			if (p->isDHTSupported())
				p->getPacketWriter().sendPort(Globals::instance().getDHT().getPort());
			else
#endif
				// WORKAROUND so we can contact µTorrent's DHT
				// They do not properly support the standard and do not turn on
				// the DHT bit in the handshake, so we just ping each peer by default.
				p->emitPortPacket();
		}
		
		// set group ID's for traffic shaping
		p->setGroupIDs(upload_gid,download_gid);
		
		if (tmon)
			tmon->peerAdded(p);
	}

	void TorrentControl::onPeerRemoved(Peer* p)
	{
		disconnect(p,SIGNAL(gotPortPacket( const QString&, Uint16 )),
				this,SLOT(onPortPacket( const QString&, Uint16 )));
		if (tmon)
			tmon->peerRemoved(p);
	}

	void TorrentControl::doChoking()
	{
		choke->update(stats.completed,stats);
	}

	bool TorrentControl::changeTorDir(const QString & new_dir)
	{
		int pos = tordir.lastIndexOf(bt::DirSeparator(),-2);
		if (pos == -1)
		{
			Out(SYS_GEN|LOG_DEBUG) << "Could not find torX part in " << tordir << endl;
			return false;
		}
		
		QString ntordir = new_dir + tordir.mid(pos + 1);
		
		Out(SYS_GEN|LOG_DEBUG) << tordir << " -> " << ntordir << endl;
		try
		{
			bt::Move(tordir,ntordir);
			old_tordir = tordir;
			tordir = ntordir;
		}
		catch (Error & err)
		{
			Out(SYS_GEN|LOG_IMPORTANT) << "Could not move " << tordir << " to " << ntordir << endl;
			return false;
		}
		
		cman->changeDataDir(tordir);
		return true;
	}
	
	bool TorrentControl::changeOutputDir(const QString & ndir,int flags)
	{
		bool start = false;
		int old_prio = getPriority();
		
		//check if torrent is running and stop it before moving data
		if(stats.running)
		{
			start = true;
			this->stop(false);
		}
		QString new_dir = ndir;
		if (!new_dir.endsWith(bt::DirSeparator()))
			new_dir += bt::DirSeparator();
		
		moving_files = true;
		try
		{
			QString nd;
			if (! (flags & bt::TorrentInterface::FULL_PATH))
			{
				if (istats.custom_output_name)
				{
					int slash_pos = stats.output_path.lastIndexOf(bt::DirSeparator(),-2);
					nd = new_dir + stats.output_path.mid(slash_pos + 1);
				}
				else
				{
					nd = new_dir + tor->getNameSuggestion();
				}
			}
			else
			{
				nd = new_dir;
			}
			
			if (stats.output_path != nd)
			{
				if (flags & bt::TorrentInterface::MOVE_FILES)
				{
					if (stats.multi_file_torrent)
						cman->moveDataFiles(nd);
					else
						cman->moveDataFiles(new_dir);
					// bt::Move(stats.output_path, new_dir);
				}
				
				cman->changeOutputPath(nd);
				outputdir = stats.output_path = nd;
				istats.custom_output_name = true;
				
				saveStats();
				Out(SYS_GEN|LOG_NOTICE) << "Data directory changed for torrent " << "'" << stats.torrent_name << "' to: " << new_dir << endl;
			}
			else
			{
				Out(SYS_GEN|LOG_NOTICE) << "Source is the same as destination, so doing nothing" << endl;
			}
		}
		catch (Error& err)
		{			
			Out(SYS_GEN|LOG_IMPORTANT) << "Could not move " << stats.output_path << " to " << new_dir << ". Exception: " << endl;
			moving_files = false;
			return false;
		}
		
		moving_files = false;
		if(start)
			this->start();
		
		return true;
	}

	bool TorrentControl::moveTorrentFiles(const QMap<TorrentFileInterface*,QString> & files)
	{
		bool start = false;
		
		// check if torrent is running and stop it before moving data
		if(stats.running)
		{
			start = true;
			this->stop(false);
		}
	
		moving_files = true;
		try
		{
			cman->moveDataFiles(files);
			Out(SYS_GEN|LOG_NOTICE) << "Move of data files completed " << endl;
		}
		catch (Error& err)
		{			
			moving_files = false;
			return false;
		}
	
		moving_files = false;
		if(start)
			this->start();
		
		return true;
	}

	void TorrentControl::rollback()
	{
		try
		{
			bt::Move(tordir,old_tordir);
			tordir = old_tordir;
			cman->changeDataDir(tordir);
		}
		catch (Error & err)
		{
			Out(SYS_GEN|LOG_IMPORTANT) << "Could not move " << tordir << " to " << old_tordir << endl;
		}
	}

	void TorrentControl::updateStatusMsg()
	{
		TorrentStatus old = stats.status;
		if (stats.stopped_by_error)
			stats.status = ERROR;
		else if (!stats.started)
			stats.status = NOT_STARTED;
		else if(!stats.running && !stats.user_controlled)
			stats.status = QUEUED;
		else if (!stats.running && stats.completed && (overMaxRatio() || overMaxSeedTime()))
			stats.status = SEEDING_COMPLETE;
		else if (!stats.running && stats.completed)
			stats.status = DOWNLOAD_COMPLETE;
		else if (!stats.running)
			stats.status = STOPPED;
		else if (stats.running && stats.completed)
			stats.status = SEEDING;
		else if (stats.running) 
			// protocol messages are also included in speed calculation, so lets not compare with 0
			stats.status = down->downloadRate() > 100 ?
					DOWNLOADING : STALLED;
		
		if (old != stats.status)
			statusChanged(this);
	}

	const BitSet & TorrentControl::downloadedChunksBitSet() const
	{
		if (cman)
			return cman->getBitSet();
		else
			return BitSet::null;
	}

	const BitSet & TorrentControl::availableChunksBitSet() const
	{
		if (!pman)
			return BitSet::null;
		else
			return pman->getAvailableChunksBitSet();
	}

	const BitSet & TorrentControl::excludedChunksBitSet() const
	{
		if (!cman)
			return BitSet::null;
		else
			return cman->getExcludedBitSet();
	}
	
	const BitSet & TorrentControl::onlySeedChunksBitSet() const
	{
		if (!cman)
			return BitSet::null;
		else
			return cman->getOnlySeedBitSet();
	}

	void TorrentControl::saveStats()
	{
		StatsFile st(tordir + "stats");

		st.write("OUTPUTDIR", cman->getDataDir());			
		
		if (cman->getDataDir() != outputdir)
			outputdir = cman->getDataDir();
		
		st.write("UPLOADED", QString::number(up->bytesUploaded()));
		
		if (stats.running)
		{
			QDateTime now = QDateTime::currentDateTime();
			st.write("RUNNING_TIME_DL",QString("%1").arg(istats.running_time_dl + istats.time_started_dl.secsTo(now)));
			st.write("RUNNING_TIME_UL",QString("%1").arg(istats.running_time_ul + istats.time_started_ul.secsTo(now)));
		}
		else
		{
			st.write("RUNNING_TIME_DL", QString("%1").arg(istats.running_time_dl));
			st.write("RUNNING_TIME_UL", QString("%1").arg(istats.running_time_ul));
		}
		
		st.write("PRIORITY", QString("%1").arg(istats.priority));
		st.write("AUTOSTART", QString("%1").arg(stats.autostart));
		st.write("IMPORTED", QString("%1").arg(stats.imported_bytes));
		st.write("CUSTOM_OUTPUT_NAME",istats.custom_output_name ? "1" : "0");
		st.write("MAX_RATIO", QString("%1").arg(stats.max_share_ratio,0,'f',2));
		st.write("MAX_SEED_TIME",QString::number(stats.max_seed_time));
		st.write("RESTART_DISK_PREALLOCATION",prealloc ? "1" : "0");
		
		if(!stats.priv_torrent)
		{
			//save dht and pex 
			st.write("DHT", isFeatureEnabled(DHT_FEATURE) ? "1" : "0");
			st.write("UT_PEX", isFeatureEnabled(UT_PEX_FEATURE) ? "1" : "0");
		}
		
		st.write("UPLOAD_LIMIT",QString::number(upload_limit));
		st.write("DOWNLOAD_LIMIT",QString::number(download_limit));
		
		st.writeSync();
	}

	void TorrentControl::loadStats()
	{
		StatsFile st(tordir + "stats");
		
		Uint64 val = st.readUint64("UPLOADED");
		// stats.session_bytes_uploaded will be calculated based upon prev_bytes_ul
		// seeing that this will change here, we need to save it 
		istats.session_bytes_uploaded = stats.session_bytes_uploaded; 
		istats.prev_bytes_ul = val;
		up->setBytesUploaded(val);
		
		this->istats.running_time_dl = st.readULong("RUNNING_TIME_DL");
		this->istats.running_time_ul = st.readULong("RUNNING_TIME_UL");
		outputdir = st.readString("OUTPUTDIR").trimmed();
		if (st.hasKey("CUSTOM_OUTPUT_NAME") && st.readULong("CUSTOM_OUTPUT_NAME") == 1)
		{
			istats.custom_output_name = true;
		}
		
		setPriority(st.readInt("PRIORITY"));
		stats.user_controlled = istats.priority == 0 ? true : false;
		stats.autostart = st.readBoolean("AUTOSTART");
		
		stats.imported_bytes = st.readUint64("IMPORTED");
		stats.max_share_ratio = st.readFloat("MAX_RATIO");
		stats.max_seed_time = st.readFloat("MAX_SEED_TIME");


		if (st.hasKey("RESTART_DISK_PREALLOCATION"))
			prealloc = st.readString("RESTART_DISK_PREALLOCATION") == "1";
		
		if (!stats.priv_torrent)
		{
			if(st.hasKey("DHT"))
				istats.dht_on = st.readBoolean("DHT");
			else
				istats.dht_on = true;
			
			setFeatureEnabled(DHT_FEATURE,istats.dht_on);
			if (st.hasKey("UT_PEX"))
				setFeatureEnabled(UT_PEX_FEATURE,st.readBoolean("UT_PEX"));
		}
		
		net::SocketMonitor & smon = net::SocketMonitor::instance();
		
		Uint32 nl = st.readInt("UPLOAD_LIMIT");
		if (nl != upload_limit)
		{
			if (nl > 0)
			{
				if (upload_gid)
					smon.setGroupLimit(net::SocketMonitor::UPLOAD_GROUP,upload_gid,nl);
				else
					upload_gid = smon.newGroup(net::SocketMonitor::UPLOAD_GROUP,nl);
			}
			else
			{
				smon.removeGroup(net::SocketMonitor::UPLOAD_GROUP,upload_gid);
				upload_gid = 0;
			}
		}
		upload_limit = nl;
		
		nl = st.readInt("DOWNLOAD_LIMIT");
		if (nl != download_limit)
		{
			if (nl > 0)
			{
				if (download_gid)
					smon.setGroupLimit(net::SocketMonitor::DOWNLOAD_GROUP,download_gid,nl);
				else
					download_gid = smon.newGroup(net::SocketMonitor::DOWNLOAD_GROUP,nl);
			}
			else
			{
				smon.removeGroup(net::SocketMonitor::DOWNLOAD_GROUP,download_gid);
				download_gid = 0;
			}
		}
		download_limit = nl;
	}

	void TorrentControl::loadOutputDir()
	{
		StatsFile st(tordir + "stats");
		if (!st.hasKey("OUTPUTDIR"))
			return;
		
		outputdir = st.readString("OUTPUTDIR").trimmed();
		if (st.hasKey("CUSTOM_OUTPUT_NAME") && st.readULong("CUSTOM_OUTPUT_NAME") == 1)
		{
			istats.custom_output_name = true;
		}
	}

	bool TorrentControl::readyForPreview(int start_chunk, int end_chunk)
	{
		if ( !tor->isMultimedia() && !tor->isMultiFile()) return false;

		const BitSet & bs = downloadedChunksBitSet();
		for(int i = start_chunk; i<end_chunk; ++i)
		{
			if ( !bs.get(i) ) return false;
		}
		return true;
	}

	Uint32 TorrentControl::getTimeToNextTrackerUpdate() const
	{
		if (psman)
			return psman->getTimeToNextUpdate();
		else
			return 0;
	}

	void TorrentControl::updateStats()
	{
		stats.num_chunks_downloading = down ? down->numActiveDownloads() : 0;
		stats.num_peers = pman ? pman->getNumConnectedPeers() : 0;
		stats.upload_rate = up && stats.running ? up->uploadRate() : 0;
		stats.download_rate = down && stats.running ? down->downloadRate() : 0;
		stats.bytes_left = cman ? cman->bytesLeft() : 0;
		stats.bytes_left_to_download = cman ? cman->bytesLeftToDownload() : 0;
		stats.bytes_uploaded = up ? up->bytesUploaded() : 0;
		stats.bytes_downloaded = down ? down->bytesDownloaded() : 0;
		stats.total_chunks = tor ? tor->getNumChunks() : 0;
		stats.num_chunks_downloaded = cman ? cman->chunksDownloaded() : 0;
		stats.num_chunks_excluded = cman ? cman->chunksExcluded() : 0;
		stats.chunk_size = tor ? tor->getChunkSize() : 0;
		stats.num_chunks_left = cman ? cman->chunksLeft() : 0;
		stats.total_bytes_to_download = (tor && cman) ?	tor->getFileLength() - cman->bytesExcluded() : 0;
		
		
		
		if (stats.bytes_downloaded >= istats.prev_bytes_dl)
			stats.session_bytes_downloaded = stats.bytes_downloaded - istats.prev_bytes_dl;
		else
			stats.session_bytes_downloaded = 0;
					
		if (stats.bytes_uploaded >= istats.prev_bytes_ul)
			stats.session_bytes_uploaded = (stats.bytes_uploaded - istats.prev_bytes_ul) + istats.session_bytes_uploaded;
		else
			stats.session_bytes_uploaded = istats.session_bytes_uploaded;
		/*
			Safety check, it is possible that stats.bytes_downloaded gets subtracted in Downloader.
			Which can cause stats.bytes_downloaded to be smaller the istats.trk_prev_bytes_dl.
			This can screw up your download ratio.
		*/
		if (stats.bytes_downloaded >= istats.trk_prev_bytes_dl)
			stats.trk_bytes_downloaded = stats.bytes_downloaded - istats.trk_prev_bytes_dl;
		else
			stats.trk_bytes_downloaded = 0;
		
		if (stats.bytes_uploaded >= istats.trk_prev_bytes_ul)
			stats.trk_bytes_uploaded = stats.bytes_uploaded - istats.trk_prev_bytes_ul;
		else
			stats.trk_bytes_uploaded = 0;
		
		getSeederInfo(stats.seeders_total,stats.seeders_connected_to);
		getLeecherInfo(stats.leechers_total,stats.leechers_connected_to);
	}

	void TorrentControl::trackerScrapeDone()
	{
		stats.seeders_total = psman->getNumSeeders();
		stats.leechers_total = psman->getNumLeechers();
	}

	void TorrentControl::getSeederInfo(Uint32 & total,Uint32 & connected_to) const
	{
		total = 0;
		connected_to = 0;
		if (!pman || !psman)
			return;

		for (Uint32 i = 0;i < pman->getNumConnectedPeers();i++)
		{
			if (pman->getPeer(i)->isSeeder())
				connected_to++;
		}
		total = psman->getNumSeeders();
		if (total == 0)
			total = connected_to;
	}

	void TorrentControl::getLeecherInfo(Uint32 & total,Uint32 & connected_to) const
	{
		total = 0;
		connected_to = 0;
		if (!pman || !psman)
			return;

		for (Uint32 i = 0;i < pman->getNumConnectedPeers();i++)
		{
			if (!pman->getPeer(i)->isSeeder())
				connected_to++;
		}
		total = psman->getNumLeechers();
		if (total == 0)
			total = connected_to;
	}

	Uint32 TorrentControl::getRunningTimeDL() const
	{
		if (!stats.running || stats.completed)
			return istats.running_time_dl;
		else
			return istats.running_time_dl + istats.time_started_dl.secsTo(QDateTime::currentDateTime());
	}

	Uint32 TorrentControl::getRunningTimeUL() const
	{
		if (!stats.running)
			return istats.running_time_ul;
		else
			return istats.running_time_ul + istats.time_started_ul.secsTo(QDateTime::currentDateTime());
	}

	Uint32 TorrentControl::getNumFiles() const
	{
		if (tor && tor->isMultiFile())
			return tor->getNumFiles();
		else
			return 0;
	}
	
	TorrentFileInterface & TorrentControl::getTorrentFile(Uint32 index)
	{
		if (tor)
			return tor->getFile(index);
		else
			return TorrentFile::null;
	}
	
	const TorrentFileInterface & TorrentControl::getTorrentFile(Uint32 index) const
	{
		if (tor)
			return tor->getFile(index);
		else
			return TorrentFile::null;
	}

	void TorrentControl::migrateTorrent(const QString & default_save_dir)
	{
		if (bt::Exists(tordir + "current_chunks") && bt::IsPreMMap(tordir + "current_chunks"))
		{
			// in case of error copy torX dir to migrate-failed-tor
			QString dd = tordir;
			int pos = dd.lastIndexOf("tor");
			if (pos != - 1)
			{
				dd = dd.replace(pos,3,"migrate-failed-tor");
				Out() << "Copying " << tordir << " to " << dd << endl;
				bt::CopyDir(tordir,dd,true);
			}
				
			bt::MigrateCurrentChunks(*tor,tordir + "current_chunks");
			if (outputdir.isNull() && bt::IsCacheMigrateNeeded(*tor,tordir + "cache"))
			{
				// if the output dir is NULL
				if (default_save_dir.isNull())
				{
					KMessageBox::information(0,
						i18n("The torrent %1 was started with a previous version of KTorrent."
							" To make sure this torrent still works with this version of KTorrent, "
							"we will migrate this torrent. You will be asked for a location to save "
							"the torrent to. If you press cancel, we will select your home directory.",
							tor->getNameSuggestion()));
					outputdir = KFileDialog::getExistingDirectory(KUrl("kfiledialog:///openTorrent"), 0,i18n("Select Folder to Save To"));
					if (outputdir.isNull())
						outputdir = QDir::homePath();
				}
				else
				{
					outputdir = default_save_dir;
				}
				
				if (!outputdir.endsWith(bt::DirSeparator()))
					outputdir += bt::DirSeparator();
				
				bt::MigrateCache(*tor,tordir + "cache",outputdir);
			}
			
			// delete backup
			if (pos != - 1)
				bt::Delete(dd);
		}
	}
	
	void TorrentControl::setPriority(int p)
	{
		istats.priority = p;
		stats.user_controlled = p == 0 ? true : false;
		if(p)
		{
			stats.status = QUEUED;
			statusChanged(this);
		}
		else
			updateStatusMsg();
		
		saveStats();
	}
	
	void TorrentControl::setMaxShareRatio(float ratio)
	{
		if(ratio == 1.00f)
		{
			if(stats.max_share_ratio != ratio)
				stats.max_share_ratio = ratio;
		}
		else
			stats.max_share_ratio = ratio;
		
		if(stats.completed && !stats.running && !stats.user_controlled && (ShareRatio(stats) >= stats.max_share_ratio))
			setPriority(0); //dequeue it
		
		saveStats();
		emit maxRatioChanged(this);
	}
	
	void TorrentControl::setMaxSeedTime(float hours)
	{
		stats.max_seed_time = hours;
		saveStats();
	}

	bool TorrentControl::overMaxRatio()
	{
		if(stats.completed && stats.bytes_uploaded != 0 && stats.bytes_downloaded != 0 && stats.max_share_ratio > 0)
		{
			if(ShareRatio(stats) >= stats.max_share_ratio)
				return true;
		}
		
		return false;
	}

	bool TorrentControl::overMaxSeedTime()
	{
		if(stats.completed && stats.bytes_uploaded != 0 && stats.bytes_downloaded != 0 && stats.max_seed_time > 0)
		{
			Uint32 dl = getRunningTimeDL();
			Uint32 ul = getRunningTimeUL();
			if ((ul - dl) / 3600.0f > stats.max_seed_time)
				return true;
		}
		
		return false;
	}
	
	QString TorrentControl::statusToString() const
	{
		switch (stats.status)
		{
			case NOT_STARTED :
				return i18n("Not started");
			case DOWNLOAD_COMPLETE :
				return i18n("Download completed");
			case SEEDING_COMPLETE :
				return i18n("Seeding completed");
			case SEEDING :
				return i18n("Seeding");
			case DOWNLOADING:
				return i18n("Downloading");
			case STALLED:
				return i18n("Stalled");
			case STOPPED:
				return i18n("Stopped");
			case ERROR :
				return i18n("Error: ") + getShortErrorMessage(); 
			case ALLOCATING_DISKSPACE:
				return i18n("Allocating diskspace");
			case QUEUED:
				return i18n("Queued");
			case CHECKING_DATA:
				return i18n("Checking data");
			case NO_SPACE_LEFT:
				return i18n("Stopped. No space left on device.");
		}
		return QString::null;
	}

	TrackersList* TorrentControl::getTrackersList()
	{
		return psman;
	}
	
	const TrackersList* TorrentControl::getTrackersList() const 
	{
		return psman;
	}

	void TorrentControl::onPortPacket(const QString & ip,Uint16 port)
	{
		if (Globals::instance().getDHT().isRunning() && !stats.priv_torrent)
			Globals::instance().getDHT().portRecieved(ip,port);
	}
	
	void TorrentControl::startDataCheck(bt::DataCheckerListener* lst)
	{
		if (stats.status == ALLOCATING_DISKSPACE)
			return;
		
		
		DataChecker* dc = 0;
		stats.status = CHECKING_DATA;
		stats.num_corrupted_chunks = 0; // reset the number of corrupted chunks found
		if (stats.multi_file_torrent)
			dc = new MultiDataChecker();
		else
			dc = new SingleDataChecker();
	
		dc->setListener(lst);
		
		dcheck_thread = new DataCheckerThread(dc,stats.output_path,*tor,tordir + "dnd" + bt::DirSeparator());
		
		// dc->check(stats.output_path,*tor,tordir + "dnd" + bt::DirSeparator());
		dcheck_thread->start();
		statusChanged(this);
	}
	
	void TorrentControl::afterDataCheck()
	{
		DataChecker* dc = dcheck_thread->getDataChecker();
		DataCheckerListener* lst = dc->getListener();
		
		bool err = !dcheck_thread->getError().isNull();
		if (err)
		{
			// show a queued error message when an error has occurred
			KMessageBox::queuedMessageBox(0,KMessageBox::Error,dcheck_thread->getError());
			lst->stop();
		}
		
		if (lst && !lst->isStopped())
		{
			down->dataChecked(dc->getDownloaded());
				// update chunk manager
			cman->dataChecked(dc->getDownloaded());
			if (lst->isAutoImport())
			{
				down->recalcDownloaded();
				stats.imported_bytes = down->bytesDownloaded();
				if (cman->haveAllChunks())
					stats.completed = true;
			}
			else
			{
				Uint64 downloaded = stats.bytes_downloaded;
				down->recalcDownloaded();
				updateStats();
				if (stats.bytes_downloaded > downloaded)
					stats.imported_bytes = stats.bytes_downloaded - downloaded;
				 
				if (cman->haveAllChunks())
					stats.completed = true;
			}
		}
			
		stats.status = NOT_STARTED;
		// update the status
		updateStatusMsg();
		updateStats();
		if (lst)
			lst->finished();
		delete dcheck_thread;
		dcheck_thread = 0;
	}
	
	bool TorrentControl::isCheckingData(bool & finished) const
	{
		if (dcheck_thread)
		{
			finished = !dcheck_thread->isRunning();
			return true;
		}
		return false;
	}
	
	bool TorrentControl::hasExistingFiles() const
	{
		return cman->hasExistingFiles();
	}
	
	bool TorrentControl::hasMissingFiles(QStringList & sl)
	{
		return cman->hasMissingFiles(sl);
	}
	
	void TorrentControl::recreateMissingFiles()
	{
		try
		{
			cman->recreateMissingFiles();
			prealloc = true; // set prealloc to true so files will be truncated again
			down->dataChecked(cman->getBitSet()); // update chunk selector
		}
		catch (Error & err)
		{
			onIOError(err.toString());
			throw;
		}
	}
	
	void TorrentControl::dndMissingFiles()
	{
		try
		{
			cman->dndMissingFiles();
			prealloc = true; // set prealloc to true so files will be truncated again
			missingFilesMarkedDND(this);
			down->dataChecked(cman->getBitSet()); // update chunk selector
		}
		catch (Error & err)
		{
			onIOError(err.toString());
			throw;
		}
	}
	
	void TorrentControl::handleError(const QString & err)
	{
		onIOError(err);
	}
	
	Uint32 TorrentControl::getNumDHTNodes() const
	{
		return tor->getNumDHTNodes();
	}
	
	const DHTNode & TorrentControl::getDHTNode(Uint32 i) const 
	{
		return tor->getDHTNode(i);
	}

	void TorrentControl::deleteDataFiles()
	{
		cman->deleteDataFiles();
	}
	
	const bt::SHA1Hash & TorrentControl::getInfoHash() const
	{
		return tor->getInfoHash();
	}
	
	void TorrentControl::resetTrackerStats()
	{
		istats.trk_prev_bytes_dl = stats.bytes_downloaded,
		istats.trk_prev_bytes_ul = stats.bytes_uploaded,
		stats.trk_bytes_downloaded = 0;
		stats.trk_bytes_uploaded = 0;
	}
	
	void TorrentControl::trackerStatusChanged(const QString & ns)
	{
		stats.trackerstatus = ns;
	}
	
	void TorrentControl::addPeerSource(PeerSource* ps)
	{
		if (psman)
			psman->addPeerSource(ps);
	}
	
	void TorrentControl::removePeerSource(PeerSource* ps)
	{
		if (psman)
			psman->removePeerSource(ps);
	}
	
	void TorrentControl::corrupted(Uint32 chunk)
	{
		// make sure we will redownload the chunk
		down->corrupted(chunk);
		if (stats.completed)
			stats.completed = false;
		
		// emit signal to show a systray message
		stats.num_corrupted_chunks++;
		corruptedDataFound(this);
	}
	
	int TorrentControl::getETA()
	{
		return m_eta->estimate();
	}
	
	
	
	const bt::PeerID & TorrentControl::getOwnPeerID() const
	{
		return tor->getPeerID();
	}
	
	
	bool TorrentControl::isFeatureEnabled(TorrentFeature tf)
	{
		switch (tf)
		{
		case DHT_FEATURE:
			return psman->dhtStarted();
		case UT_PEX_FEATURE:
			return pman->isPexEnabled();
		default:
			return false;
		}
	}
		
	void TorrentControl::setFeatureEnabled(TorrentFeature tf,bool on)
	{
		switch (tf)
		{
		case DHT_FEATURE:
			if (on)
			{
				if(!stats.priv_torrent)
				{
					psman->addDHT();
					istats.dht_on = psman->dhtStarted();
					saveStats();
				}
			}
			else
			{
				psman->removeDHT();
				istats.dht_on = false;
				saveStats();
			}
			break;
		case UT_PEX_FEATURE:
			if (on)
			{
				if (!stats.priv_torrent && !pman->isPexEnabled())
				{
					pman->setPexEnabled(true);
				}
			}
			else
			{
				pman->setPexEnabled(false);
			}
			break;
		}	
	}
	
	void TorrentControl::createFiles()
	{
		cman->createFiles(true);
		stats.output_path = cman->getOutputPath();
	}
	
	bool TorrentControl::checkDiskSpace(bool emit_sig)
	{	
		last_diskspace_check = bt::GetCurrentTime();
		
		//calculate free disk space
		Uint64 bytes_free = 0;
		if (FreeDiskSpace(getDataDir(),bytes_free))
		{
			Uint64 bytes_to_download = stats.total_bytes_to_download;
			Uint64 downloaded = 0;
			try
			{
				downloaded = cman->diskUsage();
			}
			catch (bt::Error & err)
			{
				Out(SYS_GEN|LOG_DEBUG) << "Error : " << err.toString() << endl;
			}
			Uint64 remaining = 0;
			if (downloaded <= bytes_to_download)
				remaining = bytes_to_download - downloaded;

			if (remaining > bytes_free)
			{
				bool toStop = bytes_free < (Uint64) min_diskspace * 1024 * 1024;						
				
				// if we don't need to stop the torrent, only emit the signal once
				// so that we do bother the user continously
				if (emit_sig && (toStop || !istats.diskspace_warning_emitted))
				{
 					emit diskSpaceLow(this, toStop);
					istats.diskspace_warning_emitted = true; 
				}

				if (!stats.running)
				{
					stats.status = NO_SPACE_LEFT;
					statusChanged(this);
				}
				
				return false;
			}
		}
		
		return true;
	}
	
	void TorrentControl::setTrafficLimits(Uint32 up,Uint32 down)
	{
		net::SocketMonitor & smon = net::SocketMonitor::instance();
		if (up && !upload_gid)
		{
			// create upload group
			upload_gid = smon.newGroup(net::SocketMonitor::UPLOAD_GROUP,up);
			upload_limit = up;
		}
		else if (up && upload_gid)
		{
			// change existing group limit
			smon.setGroupLimit(net::SocketMonitor::UPLOAD_GROUP,upload_gid,up);
			upload_limit = up;
		}
		else if (!up && !upload_gid)
		{
			upload_limit = up;
		}
		else // !up && upload_gid
		{
			// remove existing group
			smon.removeGroup(net::SocketMonitor::UPLOAD_GROUP,upload_gid);
			upload_gid = upload_limit = 0;
		}
		
		if (down && !download_gid)
		{
			// create download grodown
			download_gid = smon.newGroup(net::SocketMonitor::DOWNLOAD_GROUP,down);
			download_limit = down;
		}
		else if (down && download_gid)
		{
			// change existing grodown limit
			smon.setGroupLimit(net::SocketMonitor::DOWNLOAD_GROUP,download_gid,down);
			download_limit = down;
		}
		else if (!down && !download_gid)
		{
			download_limit = down;
		}
		else // !down && download_gid
		{
			// remove existing grodown
			smon.removeGroup(net::SocketMonitor::DOWNLOAD_GROUP,download_gid);
			download_gid = download_limit = 0;
		}
		
		saveStats();
		pman->setGroupIDs(upload_gid,download_gid);
	}
	
	void TorrentControl::getTrafficLimits(Uint32 & up,Uint32 & down)
	{
		up = upload_limit;
		down = download_limit;
	}
	
	const PeerManager * TorrentControl::getPeerMgr() const
	{
		return pman;
	}
	
	void TorrentControl::preallocThreadDone()
	{
		// thread done
		if (prealloc_thread->errorHappened())
		{
			// upon error just call onIOError and return
			onIOError(prealloc_thread->errorMessage());
			delete prealloc_thread;
			prealloc_thread = 0;
			prealloc = true; // still need to do preallocation
		}
		else
		{
			// continue the startup of the torrent
			delete prealloc_thread;
			prealloc_thread = 0;
			prealloc = false;
			stats.status = NOT_STARTED;
			saveStats();
			continueStart();
			statusChanged(this);
		}
	}


	
}

#include "torrentcontrol.moc"