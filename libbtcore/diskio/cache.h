/***************************************************************************
 *   Copyright (C) 2005 by Joris Guisson                                   *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#ifndef BTCACHE_H
#define BTCACHE_H

#include <btcore_export.h>

class QStringList;

namespace bt
{
	class Torrent;
	class TorrentFile;
	class Chunk;
	class PreallocationThread;
	class TorrentFileInterface;


	/**
	 * @author Joris Guisson
	 * @brief Manages the temporary data
	 *
	 * Interface for a class which manages downloaded data.
	 * Subclasses should implement the load and save methods.
	 */
	class BTCORE_EXPORT Cache
	{
	protected:
		Torrent & tor;
		QString tmpdir;
		QString datadir;
		bool preexisting_files;
		Uint32 mmap_failures;
	public:
		Cache(Torrent & tor,const QString & tmpdir,const QString & datadir);
		virtual ~Cache();

		/**
		 * Load the file map of a torrent.
		 * If it doesn't exist, it needs to be created.
		 */
		virtual void loadFileMap() = 0;
		
		/**
		 * Save the file map of a torrent
		 */
		virtual void saveFileMap() = 0;
		
		/// Get the datadir
		QString getDataDir() const {return datadir;}
		
		/**
		 * Get the actual output path.
		 * @return The output path
		 */
		virtual QString getOutputPath() const = 0;
		
		/**
		 * Changes the tmp dir. All data files should already been moved.
		 * This just modifies the tmpdir variable.
		 * @param ndir The new tmpdir
		 */
		virtual void changeTmpDir(const QString & ndir);
		
		/**
		 * Changes output path. All data files should already been moved.
		 * This just modifies the datadir variable.
		 * @param outputpath New output path
		 */
		virtual void changeOutputPath(const QString & outputpath) = 0;
		
		/**
		 * Move the data files to a new directory.
		 * @param ndir The directory
		 */
		virtual void moveDataFiles(const QString & ndir) = 0;
		
		/**
		 * Load a chunk into memory. If something goes wrong,
		 * an Error should be thrown.
		 * @param c The Chunk
		 */
		virtual void load(Chunk* c) = 0;

		/**
		 * Save a chunk to disk. If something goes wrong,
		 * an Error should be thrown.
		 * @param c The Chunk
		 */
		virtual void save(Chunk* c) = 0;
		
		/**
		 * Prepare a chunk for downloading.
		 * @param c The Chunk
		 * @return true if ok, false otherwise
		 */
		virtual bool prep(Chunk* c) = 0;
		
		/**
		 * Create all the data files to store the data.
		 */
		virtual void create() = 0;
		
		/**
		 * Close the cache file(s).
		 */
		virtual void close() = 0;
		
		/**
		 * Open the cache file(s)
		 */
		virtual void open() = 0;
		
		/// Does nothing, can be overridden to be alerted of download status changes of a TorrentFile
		virtual void downloadStatusChanged(TorrentFile*, bool) {};
		
		/**
		 * Preallocate diskspace for all files
		 * @param prealloc The thread doing the preallocation
		 */
		virtual void preallocateDiskSpace(PreallocationThread* prealloc) = 0;
		
		/// See if the download has existing files
		bool hasExistingFiles() const {return preexisting_files;}
		
		
		/**
		 * Test all files and see if they are not missing.
		 * If so put them in a list
		 */
		virtual bool hasMissingFiles(QStringList & sl) = 0;

		/**
		 * Delete all data files, in case of multi file torrents
		 * empty directories should also be deleted.
		 */
		virtual void deleteDataFiles() = 0;
		
		/**
		 * Move some files to a new location
		 * @param files Map of files to move and their new location
		 */
		virtual void moveDataFiles(const QMap<TorrentFileInterface*,QString> & files);
		
		/** 
		 * See if we are allowed to use mmap, when loading chunks.
		 * This will return false if we are close to system limits.
		 */
		static bool mappedModeAllowed();
		
		/**
		 * Get the number of bytes all the files of this torrent are currently using on disk.
		 * */
		virtual Uint64 diskUsage() = 0;
		
		/**
		 * Enable or disable diskspace preallocation
		 * @param on 
		 */
		static void setPreallocationEnabled(bool on) {preallocate_files = on;}
		
		/**
		 * Check if diskspace preallocation is enabled
		 * @return true if it is
		 */
		static bool preallocationEnabled() {return preallocate_files;}
		
		/**
		 * Enable or disable full diskspace preallocation 
		 * @param on 
		 */
		static void setPreallocateFully(bool on) {preallocate_fully = on;}
		
		/**
		 * Check if full diskspace preallocation is enabled.
		 * @return true if it is
		 */
		static bool preallocateFully() {return preallocate_fully;}
		
		/**
		 * Enable or disable FS specific preallocation methods.
		 * @param on 
		 */
		static void setUseFSSpecificPreallocMethod(bool on) {preallocate_fs_specific = on;}
		
		/**
		 * Check if FS specific preallocation is enabled
		 * @return true if it is
		 */
		static bool useFSSpecificPreallocMethod() {return preallocate_fs_specific;}
	private:
		static bool preallocate_files;
		static bool preallocate_fully;
		static bool preallocate_fs_specific;
	};

}

#endif