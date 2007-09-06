/***************************************************************************
 *   Copyright (C) 2006 by Diego R. Brogna and Joris Guisson               *
 *   dierbro@gmail.com                                               	   *
 *   joris.guisson@gmail.com                                               *
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
#include <kio/global.h>
#include <settings.h>
#include <peer/peermanager.h>
#include <torrent/queuemanager.h>
#include <interfaces/coreinterface.h>
#include <interfaces/functions.h>
#include <interfaces/torrentinterface.h>
#include "phpcodegenerator.h"


namespace kt
{

	PhpCodeGenerator::PhpCodeGenerator(CoreInterface *c)
	{
		core=c;
	}
	
	PhpCodeGenerator::~PhpCodeGenerator()
	{}
	
	QString PhpCodeGenerator::downloadStatus()
	{
		QString ret;
		ret.append("function downloadStatus()\n{\nreturn ");
		ret.append("array(");
	
		QList<TorrentInterface*>::iterator i= core->getQueueManager()->begin();
		for(int k=0; i != core->getQueueManager()->end(); i++, k++)
		{
			const TorrentStats & stats = (*i)->getStats();
			ret.append(QString("%1 => array(").arg(k));
			
			ret.append(QString("\"imported_bytes\" => %1,").arg(stats.imported_bytes));
			ret.append(QString("\"bytes_downloaded\" => \"%1\",").arg(KIO::convertSize(stats.bytes_downloaded)));
			ret.append(QString("\"bytes_uploaded\" => \"%1\",").arg(KIO::convertSize(stats.bytes_uploaded)));
			ret.append(QString("\"bytes_left\" => %1,").arg(stats.bytes_left));
			ret.append(QString("\"bytes_left_to_download\" => %1,").arg(stats.bytes_left_to_download));
			ret.append(QString("\"total_bytes\" => \"%1\",").arg(KIO::convertSize(stats.total_bytes)));
			ret.append(QString("\"total_bytes_to_download\" => %1,").arg(stats.total_bytes_to_download));
			ret.append(QString("\"download_rate\" => \"%1/s\",").arg(KIO::convertSize(stats.download_rate)));
			ret.append(QString("\"upload_rate\" => \"%1/s\",").arg(KIO::convertSize(stats.upload_rate)));
			ret.append(QString("\"num_peers\" => %1,").arg(stats.num_peers));
			ret.append(QString("\"num_chunks_downloading\" => %1,").arg(stats.num_chunks_downloading));
			ret.append(QString("\"total_chunks\" => %1,").arg(stats.total_chunks));
			ret.append(QString("\"num_chunks_downloaded\" => %1,").arg(stats.num_chunks_downloaded));
			ret.append(QString("\"num_chunks_excluded\" => %1,").arg(stats.num_chunks_excluded));
			ret.append(QString("\"chunk_size\" => %1,").arg(stats.chunk_size));
			ret.append(QString("\"seeders_total\" => %1,").arg(stats.seeders_total));
			ret.append(QString("\"seeders_connected_to\" => %1,").arg(stats.seeders_connected_to));
			ret.append(QString("\"leechers_total\" => %1,").arg(stats.leechers_total));
			ret.append(QString("\"leechers_connected_to\" => %1,").arg(stats.leechers_connected_to));
			ret.append(QString("\"status\" => %1,").arg(stats.status));
			QString tmp = stats.trackerstatus;
			ret.append(QString("\"trackerstatus\" => \"%1\",").arg(tmp.replace("\\", "\\\\").replace("\"", "\\\"").replace("$", "\\$")));
			ret.append(QString("\"session_bytes_downloaded\" => %1,").arg(stats.session_bytes_downloaded));
			ret.append(QString("\"session_bytes_uploaded\" => %1,").arg(stats.session_bytes_uploaded));
			ret.append(QString("\"trk_bytes_downloaded\" => %1,").arg(stats.trk_bytes_downloaded));
			ret.append(QString("\"trk_bytes_uploaded\" => %1,").arg(stats.trk_bytes_uploaded));
			
			tmp = stats.torrent_name;
			ret.append(QString("\"torrent_name\" => \"%1\",").arg(tmp.replace("\\", "\\\\").replace("\"", "\\\"").replace("$", "\\$")));
			tmp = stats.output_path;
			ret.append(QString("\"output_path\" => \"%1\",").arg(tmp.replace("\\", "\\\\").replace("\"", "\\\"").replace("$", "\\$")));
			ret.append(QString("\"stopped_by_error\" => \"%1\",").arg(stats.stopped_by_error));
			ret.append(QString("\"completed\" => \"%1\",").arg(stats.completed));
			ret.append(QString("\"user_controlled\" => \"%1\",").arg(stats.user_controlled));
			ret.append(QString("\"max_share_ratio\" => %1,").arg(stats.max_share_ratio));
			ret.append(QString("\"priv_torrent\" => \"%1\"").arg(stats.priv_torrent));
	
	
			ret.append("),");
		}
		if(ret.endsWith(","))
			ret.truncate(ret.length()-1);
		ret.append(");\n}\n");
		return ret;
		
	}
	
	QString PhpCodeGenerator::globalInfo()
	{
		QString ret;
		ret.append("function globalInfo()\n{\nreturn ");
		ret.append("array(");
		CurrentStats stats=core->getStats();
	
		ret.append(QString("\"download_speed\" => \"%1/s\",").arg(KIO::convertSize(stats.download_speed)));
		ret.append(QString("\"upload_speed\" => \"%1/s\",").arg(KIO::convertSize(stats.upload_speed)));
		ret.append(QString("\"bytes_downloaded\" => \"%1\",").arg(stats.bytes_downloaded));
		ret.append(QString("\"bytes_uploaded\" => \"%1\",").arg(stats.bytes_uploaded));
		ret.append(QString("\"max_download_speed\" => \"%1\",").arg(Settings::maxDownloadRate()));
		ret.append(QString("\"max_upload_speed\" => \"%1\",").arg(Settings::maxUploadRate()));
		ret.append(QString("\"max_downloads\" => \"%1\",").arg(Settings::maxDownloads()));
		ret.append(QString("\"max_seeds\"=> \"%1\",").arg(Settings::maxSeeds()));
		ret.append(QString("\"dht_support\" => \"%1\",").arg(Settings::dhtSupport()));
		ret.append(QString("\"use_encryption\" => \"%1\"").arg(Settings::useEncryption()));
		ret.append(");\n}\n");
	
		return ret;
	}
	
}