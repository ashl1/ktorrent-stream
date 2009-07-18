/***************************************************************************
 *   Copyright (C) 2009 by                                                 *
 *   Joris Guisson <joris.guisson@gmail.com>                               *
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

#ifndef BT_JOBQUEUE_H
#define BT_JOBQUEUE_H

#include <QObject>
#include <btcore_export.h>
#include <torrent/job.h>

namespace bt 
{
	class Job;
	
	/**
		A job queue handles all jobs running on a torrent in a sequential order
	*/
	class BTCORE_EXPORT JobQueue : public QObject
	{
		Q_OBJECT
	public:
		JobQueue(QObject* parent = 0);
		virtual ~JobQueue();
		
		/// Are there running jobs
		bool runningJobs() const;
		
		/// Start the next job
		void startNextJob();
		
		/// Enqueue a job
		void enqueue(Job* job);
		
	private slots:
		void jobDone(KJob* job);
		
	private:
		QList<Job*> queue;
	};

}

#endif // BT_JOBQUEUE_H