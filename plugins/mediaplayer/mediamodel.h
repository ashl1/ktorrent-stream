/***************************************************************************
 *   Copyright (C) 2008 by Joris Guisson and Ivan Vasic                    *
 *   joris.guisson@gmail.com                                               *
 *   ivasic@gmail.com                                                      *
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
#ifndef KTMEDIAMODEL_H
#define KTMEDIAMODEL_H

#include <QAbstractListModel>
#include <util/constants.h>
#include "mediafile.h"


namespace kt
{
	class CoreInterface;
	
	/**
		Interface class to find MediaFileRef objects in the collection
	*/
	class MediaFileCollection
	{
	public:
		virtual ~MediaFileCollection() {}
		
		/** 
			Find a MediaFileRef given a path, if the path is not in the collection 
			a simple file MediaFileRef will be constructed
		*/
		virtual MediaFileRef find(const QString & path) = 0;
	};

	/**
	 * Represents the list of media files. Used in MediaView and PlayListWidget.
		@author
	*/
	class MediaModel : public QAbstractListModel,public MediaFileCollection
	{
		Q_OBJECT
	public:
		MediaModel(CoreInterface* core,QObject* parent);
		virtual ~MediaModel();
		
		virtual int rowCount(const QModelIndex & parent) const;
		virtual int columnCount(const QModelIndex & parent) const;
		virtual QVariant headerData(int section, Qt::Orientation orientation,int role) const;
		virtual QVariant data(const QModelIndex & index, int role) const;
		virtual bool removeRows(int row,int count,const QModelIndex & parent);
		virtual bool insertRows(int row,int count,const QModelIndex & parent);
		virtual Qt::ItemFlags flags(const QModelIndex &index) const;
		virtual QStringList mimeTypes() const;
		virtual QMimeData* mimeData(const QModelIndexList &indexes) const;
		virtual QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const;
		
		/// Get the file given a model index
		MediaFileRef fileForIndex(const QModelIndex & idx) const;
		
		/// Get the index of a full path
		QModelIndex indexForPath(const QString & path) const;
		
		virtual MediaFileRef find(const QString & path);
		
	public slots:
		void onTorrentAdded(bt::TorrentInterface* t);
		void onTorrentRemoved(bt::TorrentInterface* t);
		
	private:
		CoreInterface* core;
		QList<MediaFile::Ptr> items;
	};

}

#endif
