  /***************************************************************************
 *   Copyright (C) 2007 by Dagur Valberg Johannsson                        *
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

function update_interval(time) {
	update_all();
	if (!time) {
		return;
	}
	var seconds = time * 1000;
	window.setInterval(update_all, seconds);
}

function update_all() {
	fetch_xml("rest.php?global_status", new Array("update_status_bar", "update_title"));
	fetch_xml("rest.php?download_status", new Array("update_torrent_table"));

};

function fetch_xml(url, callback_functions) {
		var request = false;
		
		if (window.XMLHttpRequest) { // most browsers
			request = new XMLHttpRequest();
//			if (request.overrideMimeType) {
//				request.overrideMimeType('text/xml');
//			}
		}
		
		else if (window.ActiveXObject) { //ie
			try {
				request = new ActiveXObject("Msxml2.XMLHTTP");
			}
			catch(e) {
				try { request = new ActiveXObject("Microsoft.XMLHTTP"); }
				catch(e) { }
			}
		}
		
		if (!request) { 
			// Browser doesn't support XMLHttpRequest
			return false;
		}
		request.onreadystatechange = function() {
			if (request.readyState == 4) {
				if (request.status == 200) {
					//overrideMimeType didn't work in Konqueror,
					//so we'll have to parse the response into XML
					//object ourselfs. responseXML won't work.
					var xmlstring = request.responseText;
					var xmldoc;
					if (window.DOMParser) {
						xmldoc = (new DOMParser())
								.parseFromString(xmlstring, "text/xml");
					}
					else if (window.ActiveXObject) { //ie
						xmldoc = new ActiveXObject("Microsoft.XMLDOM");
						xmldoc.async = false;
						xmldoc.loadXML(xmlstring);
					}

					for (var i in callback_functions) {
						eval(callback_functions[i] + "(xmldoc)");
					}

				}
				else {
					// could not fetch
				}
			}
		}
		
		request.open('GET', url, true);
		request.send(null);
};

function update_title(xmldoc) {
	var down = _get_text(xmldoc, 'download_speed').data;
	var up   = _get_text(xmldoc, 'upload_speed').data;
	var new_title = "(D: " + down + ") (U: " + up + ") - ktorrent web interface";
	document.title = new_title;
}

function update_status_bar(xmldoc) {
	var newtable = document.createElement('table');
	newtable.setAttribute('id', 'status_bar_table');

	
	//dth and encryption
	{
		var row = newtable.insertRow(0);
		var cell = row.insertCell(0);
		var dht = _get_text_from_attribute(xmldoc, 'dht', 'status').data;
		var encryption = _get_text_from_attribute(xmldoc, 'encryption', 'status').data;
		cell.appendChild(
			document.createTextNode("DHT : " +dht));
		cell = row.insertCell(1);
		cell.appendChild(
			document.createTextNode("Encryption : " + encryption));
	}	
	//speed up/down 
	{
		var row = newtable.insertRow(1);
		var cell = row.insertCell(0);
		cell.appendChild(
			document.createTextNode("Speed"));
		
		cell = row.insertCell(1);
		var down = _get_text(xmldoc, 'download_speed').data;
		var up   = _get_text(xmldoc, 'upload_speed').data;
		cell.appendChild(
			document.createTextNode("down: " + down + " / up: " + up));
	}
	//transferred
	{
		var row = newtable.insertRow(2);
		var cell = row.insertCell(0);
		cell.appendChild(
			document.createTextNode("Transferred"));
		
		cell = row.insertCell(1);
		var down = _get_text(xmldoc, 'downloaded_total').data;
		var up   = _get_text(xmldoc, 'uploaded_total').data;
		cell.appendChild(
			document.createTextNode("down: " + down + " / up: " + up));
	}
	var oldtable = document.getElementById('status_bar_table');
	oldtable.parentNode.replaceChild(newtable, oldtable);
}


function update_torrent_table(xmldoc) {
	
	var newtable = document.createElement('table');
	newtable.setAttribute('id', 'torrent_list_table');

	var torrents = xmldoc.getElementsByTagName('torrent');
	var i = 0;
	while (torrents[i]) {
		_torrent_table_row(torrents[i], newtable, i);
		i++;
	}
	_torrent_table_header(newtable.insertRow(0));

	var oldtable = document.getElementById('torrent_list_table');
	oldtable.parentNode.replaceChild(newtable, oldtable);
	
};

function _torrent_table_row(torrent, table, i) {
	var row = table.insertRow(i);
	var row_color = (i % 2) ?
		"#ffffff" : "#dce4f9";
	row.setAttribute("style", "background-color : " + row_color);

	//actions
	{
		var cell = row.insertCell(0);
		var start_button  = _create_action_button('Start', 'start.png', 'start='+i);
		var stop_button   = _create_action_button('Stop', 'stop.png', 'stop='+i);
		var remove_button = _create_action_button('Remove', 'remove.png', 'remove='+i);
		remove_button.setAttribute("onClick", "return validate('remove_torrent')");
		
		cell.appendChild(start_button);
		cell.appendChild(stop_button);
		cell.appendChild(remove_button)
	}
		
		
	//file
	{
		var cell = row.insertCell(1);
		cell.appendChild(
			_get_text(torrent, 'name'));
	}
	
	//status
	{
		var cell = row.insertCell(2);
		cell.appendChild(
			_get_text(torrent, 'status'));
	}
	
	//speed
	{
		var cell = row.insertCell(3);
		
		cell.appendChild(
			_get_text(torrent, 'download_rate'));
		cell.appendChild(document.createElement('br'));
		cell.appendChild(
			_get_text(torrent, 'upload_rate'));
	}
	//size
	{
		var cell = row.insertCell(4);
		cell.appendChild(
			_get_text(torrent, 'size'));
	}
	//peers
	{
		var cell = row.insertCell(5);
		cell.appendChild(
			_get_text(torrent, 'peers'));
	}
	
	//transferred
	{
		var cell = row.insertCell(6);
		
		cell.appendChild(
			_get_text(torrent, 'downloaded'));
		cell.appendChild(document.createElement('br'));
		cell.appendChild(
			_get_text(torrent, 'uploaded'));	
		
	}
	//done
	{
		var cell = row.insertCell(7);
		cell.setAttribute("style", "padding-right : 2px;");

		var percent_done
			= _get_text_from_attribute(torrent, 'downloaded', 'percent').data;	

		var bar = document.createElement('div');
		bar.setAttribute("class", "percent_bar");
		bar.setAttribute("style", "width : " + percent_done + "%;");
		cell.appendChild(bar);

		var bar_text = document.createElement('div');
		bar_text.appendChild(
			document.createTextNode(percent_done + "%"));

		bar.appendChild(bar_text);
		
		
	}
	
};

function _create_action_button(button_name, image_src, command) {
	var a = document.createElement("a");
	a.setAttribute("href", "interface.php?" + command);
	var image = document.createElement("img");
	image.setAttribute("src", image_src);
	image.setAttribute("alt", button_name);
	image.setAttribute("title", button_name);
	a.appendChild(image);
	return a;
};

// gets element with given tag and crates text node from it
function _get_text(element, tag) {
	var text_node;
	try {
		text_node = document.createTextNode(
			element.getElementsByTagName(tag)[0].firstChild.data);
	}
	catch (e) {
		text_node = document.createTextNode('');
	}
		
	return text_node;
}

function _get_text_from_attribute(element, tag, attribute) {
	var text_node;
	try {
		text_node = document.createTextNode(
			element.getElementsByTagName(tag)[0].getAttribute(attribute));
	}
	catch (e) { 
		text_node = document.createTextNode('');
	 }
	return text_node;
}

function _torrent_table_header(row) {
	headers = new Array(
		"Actions", "File", "Status", 
		"Speed", "Size", "Peers", 
		"Transferred", "% done"
	);

	for (var i in headers) {
		var header =  document.createElement("th");
		header.appendChild(
			document.createTextNode(headers[i]));
		row.appendChild(header);
	}
	return row;
}