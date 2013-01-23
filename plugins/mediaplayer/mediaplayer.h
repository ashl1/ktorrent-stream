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
#ifndef KTAUDIOPLAYER_H
#define KTAUDIOPLAYER_H

#include <QTimer>
#include <QStringList>
#include <Phonon/MediaObject>
#include "mediafile.h"
#include "mediafilestream.h"


namespace Phonon
{
	class AudioOutput;
	class MediaController;
}


namespace kt
{
	enum ActionFlags
	{
		MEDIA_PLAY = 1,MEDIA_PAUSE = 2,MEDIA_STOP = 4,MEDIA_PREV = 8,MEDIA_NEXT = 16
	};

	/**
		@author
	*/
	class MediaPlayer : public QObject
	{
		Q_OBJECT
	public:
		MediaPlayer(QObject* parent);
		virtual ~MediaPlayer();
		
		QList<Phonon::AudioChannelDescription> getAudioChannels() const;
		
		/// Get the current file we are playing
		MediaFileRef getCurrentSource() const;
		
		Phonon::MediaObject* media0bject() {return media;}
		
		Phonon::AudioOutput* output() {return audio;}

		/// Pause playing
		void pause();
		
		/// Are we paused
		bool paused() const;
		
		/// Play a file
		void play(MediaFileRef file);
		
		/// Play the previous song
		MediaFileRef prev();
		
		/// Queue a file
		void queue(MediaFileRef file);
		
		/// Resume paused stuff
		void resume(); 
		
		void setAudioChannel(Phonon::AudioChannelDescription audio_track);
		
		/// Stop playing
		void stop();
		
	private slots:
		void onStateChanged(Phonon::State cur,Phonon::State old);
		void hasVideoChanged(bool hasVideo);
		void streamStateChanged(int state);
		void availableAudioChannelsChanged();
		
	signals:
		/**
		 * Emitted when availabilitu of audio channels of the video has been changed.
		 * @param audio_channel_index The index of audio stream (valid or invalid - should be checked by isValid())
		 */
		void availableAudioChannelsChanged(quint8 audio_channel_index);
		
		/**
		 * Emitted to enable or disable the play buttons.
		 * @param flags Flags indicating which buttons to enable
		 */
		void enableActions(unsigned int flags);
		
		/**
		 * A video has been detected, create the video player window. 
		 */
		void openVideo();
		
		/**
		 * Emitted when the video widget needs to be closed.
		 */
		void closeVideo();
		
		/**
		 * Emitted when we have finished playing something
		 */
		void stopped();
		
		/**
		 * Emitted when the player is about to finish
		 */
		void aboutToFinish();
		
		/**
		 * Emitted when the player starts playing
		 */
		void playing(const MediaFileRef & file);
		
		/**
		 * Emitted when the video is being loaded
		 */
		void loading();

	private:
		Phonon::MediaObject* media;
		Phonon::AudioOutput* audio;
		QList<Phonon::AudioChannelDescription> audio_channels_in_video;
		Phonon::MediaController* media_controller;
		QList<MediaFileRef> history;
		MediaFileRef current;
		bool buffering;
		bool manually_paused;
	};

}

#endif
