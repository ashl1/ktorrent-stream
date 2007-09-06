 /***************************************************************************
 *   Copyright (C) 2006 by Diego R. Brogna                                 *
 *   dierbro@gmail.com                                               	   *
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

#ifndef WEBINTERFACEPREFWIDGET_H
#define WEBINTERFACEPREFWIDGET_H

#include <interfaces/prefpageinterface.h>
#include "ui_webinterfaceprefwidget.h"


namespace kt
{

	class WebInterfacePrefWidget :public PrefPageInterface,public Ui_WebInterfacePrefWidget
	{
		Q_OBJECT
	public:
		WebInterfacePrefWidget(QWidget *parent = 0);
		virtual ~WebInterfacePrefWidget();
		
		
	public slots:
		void textChanged(const QString & path);
	};

}
#endif