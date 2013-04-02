/***************************************************************************
*   Copyright (C) 2009 by Joris Guisson                                   *
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

#ifndef PLAYLISTWIDGET_H
#define PLAYLISTWIDGET_H

#include <QLabel>
#include <QTreeView>
#include <QToolBar>
#include <QComboBox>
#include <QCheckBox>
#include <KSharedConfig>
#include <KMenu>
#include "mediafile.h"

class QSortFilterProxyModel;

namespace kt
{

	class PlayList;
	class MediaPlayer;
	class MediaFileCollection;
	
	/**
	 * The widget shows the list of files in playlist
	 * Consists of:
	 *  - 
	 */
	class PlayListWidget : public QWidget
	{
		Q_OBJECT
	public:
		PlayListWidget(MediaFileCollection* collection,MediaPlayer* player,QWidget* parent);
		virtual ~PlayListWidget();
		
		/// Get the play list
		PlayList* playList() {return play_list;}
		
		/// Get the current selected item
		QModelIndex selectedItem() const;
		
		void saveState(KSharedConfigPtr cfg);
		void loadState(KSharedConfigPtr cfg);
		
		/// Get the next item to play, if idx is invalid return the first playable item
		QModelIndex getNext(const QModelIndex & idx,bool random) const;
		
		/// Get the file of a given index
		QString getFileByIndex(const QModelIndex& index) const;
		
		/// Get the index of a file
		QModelIndex getIndexForFile(const QString & file) const;
		
		/// Is random mode activated ?
		bool isRandomModeActivated() const {return random_mode->isChecked();}
		
	public slots:
		QModelIndex play();
		void addMedia();
		void clearPlayList();
		
	private slots:
		void onSelectionChanged(const QItemSelection & s, const QItemSelection & d);
		void doubleClicked(const QModelIndex & index);
		void showContextMenu(QPoint pos);
		void removeFiles();
		void onItemsDropped();
		
	signals:
		void fileSelected(const MediaFileRef & file);
		void doubleClicked(const MediaFileRef & file);
		void randomModeActivated(bool random);
		void enableNext(bool on);
		
	private:
		QModelIndex next(const QModelIndex & idx) const;
		QModelIndex randomNext(const QModelIndex & idx) const;
		
	private:
		MediaPlayer* player;
		PlayList* play_list;
		QToolBar* tool_bar;
		/// The list of files in playlist
		QTreeView* view;
		QCheckBox* random_mode;

		KMenu* right_click_menu;
		QSortFilterProxyModel* proxy_model;
		MediaFileCollection* collection;
	};
}

#endif // PLAYLISTWIDGET_H
