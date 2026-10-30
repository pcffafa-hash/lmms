/*
 * AudioOboe.h - device-class that performs low-latency PCM output on Android
 *               via Google's Oboe library (AAudio / OpenSL ES)
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef AUDIO_OBOE_H
#define AUDIO_OBOE_H

#include "lmmsconfig.h"

#ifdef LMMS_HAVE_OBOE

#include <atomic>
#include <cstring>	// needed by oboe/FullDuplexStream.h
#include <memory>
#include <mutex>

#include <oboe/Oboe.h>

#include "AudioDevice.h"
#include "AudioDeviceSetupWidget.h"


class AudioOboe : public AudioDevice, public oboe::AudioStreamCallback
{
public:
	AudioOboe( bool & _success_ful, Mixer * _mixer );
	virtual ~AudioOboe();

	inline static QString name()
	{
		return QT_TRANSLATE_NOOP( "setupWidget",
					"Oboe (Android low-latency audio)" );
	}


	class setupWidget : public AudioDeviceSetupWidget
	{
	public:
		setupWidget( QWidget * _parent );
		virtual ~setupWidget();

		virtual void saveSettings();
	} ;


	// oboe::AudioStreamCallback
	oboe::DataCallbackResult onAudioReady( oboe::AudioStream * _stream,
						void * _audioData,
						int32_t _numFrames ) override;
	bool onError( oboe::AudioStream *, oboe::Result ) override;
	void onErrorAfterClose( oboe::AudioStream *, oboe::Result ) override;


private:
	virtual void startProcessing();
	virtual void stopProcessing();

	bool openStream();
	void closeStream();

	std::mutex m_streamMutex;	// guards m_stream (error callbacks
					// run on a separate Oboe thread)
	std::shared_ptr<oboe::AudioStream> m_stream;

	// one LMMS period, as delivered by the mixer
	int m_outBufCapacity;
	surroundSampleFrame * m_outBuf;
	fpp_t m_outBufFrames;	// valid frames in m_outBuf
	fpp_t m_outBufPos;	// frames of m_outBuf already handed to Oboe

	std::atomic<bool> m_stopped;
	std::atomic<bool> m_wantRunning;

} ;

#endif

#endif
