/***************************************************************************
 *   Copyright (C) 2005-2007 by Joris Guisson                              *
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
#ifndef KTSEARCHENGINELIST_H
#define KTSEARCHENGINELIST_H

#include <kurl.h>
#include <QList>
#include <util/constants.h>

namespace kt
{
	

	/**
		@author Joris Guisson <joris.guisson@gmail.com>
	*/
	class SearchEngineList
	{
		struct SearchEngine
		{
			QString name;
			KUrl url;
		};
		
		QList<SearchEngine> m_search_engines;
	public:
		SearchEngineList();
		virtual ~SearchEngineList();

		void save(const QString& file);
		void load(const QString& file);
		void makeDefaultFile(const QString& file);
		
		KUrl getSearchURL(bt::Uint32 engine) const;
		QString getEngineName(bt::Uint32 engine) const;
		
		/// Get the number of engines
		bt::Uint32 getNumEngines() const {return m_search_engines.count();} 
	};

}

#endif