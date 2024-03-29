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
#ifndef KTMEDIAVIEW_H
#define KTMEDIAVIEW_H


#include <QListView>
#include <QCheckBox>
#include <QSortFilterProxyModel>
#include <KSharedConfigPtr>
#include "mediafile.h"


class KLineEdit;
class KToolBar;

namespace kt
{
	class MediaModel;
	
	/**
	 * QSortFilterProxyModel to filter out incomplete files
	 */
	class MediaViewFilter : public QSortFilterProxyModel
	{
		Q_OBJECT
	public:
		MediaViewFilter(QObject* parent = 0);
		virtual ~MediaViewFilter();
		
		virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const;
		
		/// Enable or disable showing of incomplete files
		void setShowIncomplete(bool on);
		
	public slots:
		void refresh();
		
	private:
		bool show_incomplete;
	};

	/**
	 * The widget shows files from all torrents. The user can enqueue file or start playing
	 *  immediately by double clicking on it.
	 * Contains:
	 *  - refresh button
	 *  - "show incomplete files" button
	 *  - search textbox
	 *  - the list of files
	 * Uses MediaModel as files representation.
		@author
	*/
	class MediaView : public QWidget
	{
		Q_OBJECT
	public:
		MediaView(MediaModel* model,QWidget* parent);
		virtual ~MediaView();
		
		void saveState(KSharedConfigPtr cfg);
		void loadState(KSharedConfigPtr cfg);
		
	signals:
		void doubleClicked(const MediaFileRef & mf);
		
	private slots:
		void onDoubleClicked(const QModelIndex & index);
		void showIncompleteChanged(bool on);

	private:
		MediaModel* model;
		QListView* media_tree;
		KLineEdit* search_box;
		MediaViewFilter* filter;
		KToolBar* tool_bar;
		QAction* show_incomplete;
		QAction* refresh;
	};

}

#endif
