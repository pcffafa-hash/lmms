/*
 * AudioOboe.cpp - device-class that performs low-latency PCM output on
 *                 Android via Google's Oboe library (AAudio / OpenSL ES)
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

#include "AudioOboe.h"

#ifdef LMMS_HAVE_OBOE

#include <algorithm>
#include <cstring>

#include <QLabel>

#include "Mixer.h"


AudioOboe::AudioOboe( bool & _success_ful, Mixer * _mixer ) :
	AudioDevice( DEFAULT_CHANNELS, _mixer ),
	// the mixer may hand back more frames than one period when the device
	// sample rate is higher than the processing rate (resampling)
	m_outBufCapacity( std::max( 4096, mixer()->framesPerPeriod() * 4 ) ),
	m_outBuf( new surroundSampleFrame[m_outBufCapacity] ),
	m_outBufFrames( 0 ),
	m_outBufPos( 0 ),
	m_stopped( true ),
	m_wantRunning( false )
{
	std::lock_guard<std::mutex> lock( m_streamMutex );
	_success_ful = openStream();
}




AudioOboe::~AudioOboe()
{
	stopProcessing();

	std::lock_guard<std::mutex> lock( m_streamMutex );
	closeStream();
	delete[] m_outBuf;
}




// caller must hold m_streamMutex
bool AudioOboe::openStream()
{
	oboe::AudioStreamBuilder builder;
	builder.setDirection( oboe::Direction::Output )
		->setPerformanceMode( oboe::PerformanceMode::LowLatency )
		->setSharingMode( oboe::SharingMode::Shared )
		->setUsage( oboe::Usage::Media )
		->setFormat( oboe::AudioFormat::Float )
		->setChannelCount( oboe::ChannelCount::Stereo )
		->setSampleRate( sampleRate() )
		->setSampleRateConversionQuality(
					oboe::SampleRateConversionQuality::Medium )
		->setFormatConversionAllowed( true )
		->setChannelConversionAllowed( true )
		->setDataCallback( this )
		->setErrorCallback( this );

	oboe::Result result = builder.openStream( m_stream );
	if( result != oboe::Result::OK || !m_stream )
	{
		qCritical( "AudioOboe: couldn't open stream: %s",
						oboe::convertToText( result ) );
		m_stream.reset();
		return false;
	}

	// If the system refused our rate, let AudioDevice resample to it.
	if( m_stream->getSampleRate() > 0 &&
		(sample_rate_t) m_stream->getSampleRate() != sampleRate() )
	{
		setSampleRate( m_stream->getSampleRate() );
	}

	// Two bursts is the usual compromise between latency and underruns.
	// LMMS renders a whole period inside the callback, so never go below
	// one period.
	const int32_t burst = m_stream->getFramesPerBurst();
	m_stream->setBufferSizeInFrames(
		std::max<int32_t>( burst * 2, mixer()->framesPerPeriod() ) );

	m_outBufFrames = 0;
	m_outBufPos = 0;

	return true;
}




// caller must hold m_streamMutex
void AudioOboe::closeStream()
{
	if( m_stream )
	{
		m_stream->stop();
		m_stream->close();
		m_stream.reset();
	}
}




void AudioOboe::startProcessing()
{
	std::lock_guard<std::mutex> lock( m_streamMutex );

	m_wantRunning = true;
	m_stopped = false;

	if( !m_stream && !openStream() )
	{
		return;
	}

	oboe::Result result = m_stream->requestStart();
	if( result != oboe::Result::OK )
	{
		qCritical( "AudioOboe: couldn't start stream: %s",
						oboe::convertToText( result ) );
	}
}




void AudioOboe::stopProcessing()
{
	m_wantRunning = false;
	m_stopped = true;

	std::lock_guard<std::mutex> lock( m_streamMutex );
	if( m_stream )
	{
		m_stream->requestStop();
	}
}




oboe::DataCallbackResult AudioOboe::onAudioReady( oboe::AudioStream *,
							void * _audioData,
							int32_t _numFrames )
{
	float * out = static_cast<float *>( _audioData );

	if( m_stopped )
	{
		memset( out, 0, _numFrames * DEFAULT_CHANNELS * sizeof( float ) );
		return oboe::DataCallbackResult::Continue;
	}

	const float gain = mixer()->masterGain();
	int32_t done = 0;

	while( done < _numFrames )
	{
		if( m_outBufPos >= m_outBufFrames )
		{
			m_outBufFrames = getNextBuffer( m_outBuf );
			m_outBufPos = 0;
			if( m_outBufFrames <= 0 )
			{
				// mixer has nothing more for us
				m_outBufFrames = 0;
				m_stopped = true;
				memset( out + done * DEFAULT_CHANNELS, 0,
					( _numFrames - done ) * DEFAULT_CHANNELS
							* sizeof( float ) );
				return oboe::DataCallbackResult::Continue;
			}
		}

		const int32_t n = std::min<int32_t>( _numFrames - done,
						m_outBufFrames - m_outBufPos );
		for( int32_t i = 0; i < n; ++i )
		{
			for( ch_cnt_t c = 0; c < DEFAULT_CHANNELS; ++c )
			{
				const float s =
					m_outBuf[m_outBufPos + i][c] * gain;
				out[( done + i ) * DEFAULT_CHANNELS + c] =
					std::max( -1.0f, std::min( 1.0f, s ) );
			}
		}
		done += n;
		m_outBufPos += n;
	}

	return oboe::DataCallbackResult::Continue;
}




bool AudioOboe::onError( oboe::AudioStream *, oboe::Result )
{
	// false: let Oboe close the stream, then onErrorAfterClose() reopens
	return false;
}




void AudioOboe::onErrorAfterClose( oboe::AudioStream *, oboe::Result _error )
{
	// Typically oboe::Result::ErrorDisconnected: headphones unplugged,
	// Bluetooth device switched, audio route changed.
	qWarning( "AudioOboe: stream closed (%s), reopening",
						oboe::convertToText( _error ) );

	std::lock_guard<std::mutex> lock( m_streamMutex );
	m_stream.reset();

	if( m_wantRunning && openStream() )
	{
		m_stream->requestStart();
	}
}




AudioOboe::setupWidget::setupWidget( QWidget * _parent ) :
	AudioDeviceSetupWidget( AudioOboe::name(), _parent )
{
	QLabel * info = new QLabel( tr( "Low-latency audio output via "
					"AAudio / OpenSL ES. No settings "
					"needed." ), this );
	info->setWordWrap( true );
	info->setGeometry( 10, 20, 160, 40 );
}




AudioOboe::setupWidget::~setupWidget()
{
}




void AudioOboe::setupWidget::saveSettings()
{
}


#endif
