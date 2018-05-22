//
// libvidcap - a cross-platform video capture library
//
// Copyright 2007 Wimba, Inc.
//
// Contributors:
// Peter Grayson <jpgrayson@gmail.com>
// Bill Cholewka <bcholew@gmail.com>
//
// libvidcap is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of
// the License, or (at your option) any later version.
//
// libvidcap is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this program.  If not, see
// <http://www.gnu.org/licenses/>.
//

#include "os_funcs.h"
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <atlbase.h>
#include <comutil.h>

#include <vidcap/converters.h>
#include "hotlist.h"
#include "logging.h"
#include "sapi.h"
#include "DirectShowSource.h"

DirectShowSource::DirectShowSource(struct sapi_src_context *src,
		bufferCallbackFunc cbFunc, cancelCaptureFunc cancelCaptureCB, void * parent)
	: sourceContext_(src),
	  bufferCB_(cbFunc),
	  cancelCaptureCB_(cancelCaptureCB),
	  parent_(parent),
	  graphMon_(0),
	  pSource_(0),
	  pCapGraphBuilder_(0),
	  pStreamConfig_(0),
	  pFilterGraph_(0),
	  pMediaEventIF_(0),
	  pSampleGrabber_(0),
	  pSampleGrabberIF_(0),
	  pNullRenderer_(0),
	  pMediaControlIF_(0),
	  nativeMediaType_(0),
	  graphIsSetup_(false),
	  graphHandle_(0)
{
	IBindCtx * pBindCtx = 0;
	IMoniker * pMoniker = 0;

	// Get the capture device - identified by it's long display name
	if ( !getCaptureDevice(getID(), &pBindCtx, &pMoniker) ){
		log_warn("Failed to get device '%s'.\n", getID());
		throw std::runtime_error("failed to get device");
	}

	HRESULT hr = pMoniker->BindToObject(pBindCtx, 0, IID_IBaseFilter,
				(void **)&pSource_);

	if ( FAILED(hr) )
	{
		log_error("Failed BindToObject (%d)\n", hr);
		goto constructionFailure;
	}

	if ( createCapGraphFoo() )
		goto constructionFailure;

	try
	{
		graphMon_ = new GraphMonitor(graphHandle_,
				(graphEventCBFunc)&DirectShowSource::processGraphEvent,
				this);
	}
	catch (std::runtime_error &)
	{
		log_error("failed to create graph monitor for source\n");
		goto constructionFailure;
	}

	return;

constructionFailure:

	destroyCapGraphFoo();

	if ( pSource_ )
		pSource_->Release();

	pMoniker->Release();
	pBindCtx->Release();

	throw std::runtime_error("failed source construction");
}

DirectShowSource::~DirectShowSource()
{
	delete graphMon_;

	stop();

	if ( nativeMediaType_ )
		freeMediaType(*nativeMediaType_);

	destroyCapGraphFoo();

	pSource_->Release();
}

int
DirectShowSource::createCapGraphFoo()
{
	HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_ICaptureGraphBuilder2,
			(void **)&pCapGraphBuilder_);
	if ( FAILED(hr) )
	{
		log_error("failed creating capture graph builder (%d)\n", hr);
		return -1;
	}

	hr = pCapGraphBuilder_->FindInterface(&PIN_CATEGORY_CAPTURE,
			&MEDIATYPE_Video,
			pSource_,
			IID_IAMStreamConfig,
			(void **)&pStreamConfig_);
	if( FAILED(hr) )
	{
		log_error("failed getting stream config (%d)\n", hr);
		return -1;
	}

	hr = CoCreateInstance(CLSID_FilterGraph,
			0,
			CLSCTX_INPROC_SERVER,
			IID_IGraphBuilder,
			(void **)&pFilterGraph_);
	if ( FAILED(hr) )
	{
		log_error("failed creating the filter graph (%d)\n", hr);
		return -1;
	}

	hr = pCapGraphBuilder_->SetFiltergraph(pFilterGraph_);
	if ( FAILED(hr) )
	{
		log_error("failed setting the filter graph (%d)\n", hr);
		return -1;
	}

	hr = pFilterGraph_->QueryInterface(IID_IMediaEventEx,
			(void **)&pMediaEventIF_);
	if ( FAILED(hr) )
	{
		log_error("failed getting IMediaEventEx interface (%d)\n", hr);
		return -1;
	}

	hr = pFilterGraph_->QueryInterface(IID_IMediaControl,
			(void **)&pMediaControlIF_);
	if ( FAILED(hr) )
	{
		log_error("failed getting Media Control interface (%d)\n", hr);
		return -1;
	}

	hr = CoCreateInstance(CLSID_NullRenderer,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_IBaseFilter,
			(void **)&pNullRenderer_);
	if ( FAILED(hr) )
	{
		log_error("failed creating a NULL renderer (%d)\n", hr);
		return -1;
	}

	hr = CoCreateInstance(CLSID_SampleGrabber,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_IBaseFilter,
			(void **)&pSampleGrabber_);
	if ( FAILED(hr) )
	{
		log_error("failed creating Sample Grabber (%d)\n", hr);
		return -1;
	}

	hr = pSampleGrabber_->QueryInterface(IID_ISampleGrabber,
			(void**)&pSampleGrabberIF_);
	if ( FAILED(hr) )
	{
		log_error("failed getting ISampleGrabber interface (%d)\n", hr);
		return -1;
	}

	// Capture more than once
	hr = pSampleGrabberIF_->SetOneShot(FALSE);
	if ( FAILED(hr) )
	{
		log_error("failed SetOneShot (%d)\n", hr);
		return -1;
	}

	hr = pSampleGrabberIF_->SetBufferSamples(FALSE);
	if ( FAILED(hr) )
	{
		log_error("failed SetBufferSamples (%d)\n", hr);
		return -1;
	}

	// set which callback type and function to use
	hr = pSampleGrabberIF_->SetCallback( this, 1 );
	if ( FAILED(hr) )
	{
		log_error("failed to set callback (%d)\n", hr);
		return -1;
	}

	hr = pFilterGraph_->AddFilter(pSource_, L"Source Device");
	if ( FAILED(hr) )
	{
		log_error("failed to add source (%d)\n", hr);
		return -1;
	}

	hr = pFilterGraph_->AddFilter(pNullRenderer_, L"NullRenderer");
	if ( FAILED(hr) )
	{
		log_error("failed to add null renderer (%d)\n", hr);
		return -1;
	}

	hr = pMediaEventIF_->GetEventHandle((OAEVENT *)&graphHandle_);
	if ( FAILED(hr) )
	{
		log_error("failed to get handle for filter graph\n");
		return -1;
	}

	return 0;
}

void
DirectShowSource::destroyCapGraphFoo()
{
	if ( pNullRenderer_ )
		pNullRenderer_->Release();

	if ( pSampleGrabberIF_ )
		pSampleGrabberIF_->Release();

	if ( pSampleGrabber_ )
		pSampleGrabber_->Release();

	if ( pMediaControlIF_ )
		pMediaControlIF_->Release();

	if ( pMediaEventIF_ )
		pMediaEventIF_->Release();

	if ( pFilterGraph_ )
		pFilterGraph_->Release();

	if ( pStreamConfig_ )
		pStreamConfig_->Release();

	if ( pCapGraphBuilder_ )
		pCapGraphBuilder_->Release();
}

int
DirectShowSource::resetCapGraphFoo()
{
	if ( graphIsSetup_ )
	{
		// necessary to allow subsequent calls to RenderStream()
		// (like after rebinding) to succeed
		HRESULT hr = pFilterGraph_->RemoveFilter(pSampleGrabber_);
		if ( FAILED(hr) )
		{
			log_error("failed to remove Sample Grabber (%d)\n", hr);

			//FIXME: is this still necessary? Does it still work?
			if ( hr == VFW_E_NOT_STOPPED )
			{
				log_error("Capture wasn't stopped. Repeating STOP...\n");

				HRESULT hr = pMediaControlIF_->Stop();
				if ( FAILED(hr) )
				{
					log_error("failed in 2nd STOP attempt (0x%0x)\n", hr);
				}
				else
				{
					HRESULT hr = pFilterGraph_->RemoveFilter(pSampleGrabber_);
					if ( FAILED(hr) )
					{
						// this is silly
						log_error("failed twice to remove sample grabber (%d)",
								hr);
					}
					else
					{
						graphIsSetup_ = false;
						return 0;
					}
				}
			}
			else
			{
				log_error("not processing removal failure\n");
			}
			return 1;
		}

		graphIsSetup_ = false;
	}

	return 0;
}

int
DirectShowSource::setupCapGraphFoo()
{
	if ( !graphIsSetup_ )
	{
		// set the stream's media type
		HRESULT hr = pStreamConfig_->SetFormat(nativeMediaType_);
		if ( FAILED(hr) )
		{
			log_error("failed setting stream format (%d)\n", hr);

			return -1;
		}

		// Set sample grabber's media type
		hr = pSampleGrabberIF_->SetMediaType(nativeMediaType_);
		if ( FAILED(hr) )
		{
			log_error("failed to set grabber media type (%d)\n", hr);
			return -1;
		}

		hr = pFilterGraph_->AddFilter(pSampleGrabber_, L"Sample Grabber");
		if ( FAILED(hr) )
		{
			log_error("failed to add Sample Grabber (%d)\n", hr);
			return -1;
		}

		hr = pCapGraphBuilder_->RenderStream(&PIN_CATEGORY_CAPTURE,
				&MEDIATYPE_Video,
				pSource_,
				pSampleGrabber_,
				pNullRenderer_);
		if ( FAILED(hr) )
		{
			log_error("failed to connect source, grabber "
					"and null renderer (%d)\n", hr);
			return -1;
		}

		graphIsSetup_ = true;
	}

	return 0;
}

int
DirectShowSource::start()
{
	if ( !setupCapGraphFoo() )
	{
		HRESULT hr = pMediaControlIF_->Run();
		if ( SUCCEEDED(hr) )
			return 0;

		log_error("failed to run filter graph for source '%s' (%ul 0x%x)\n",
				sourceContext_->src_info.description, hr, hr);
	}

	return -1;
}

// NOTE: This function will block until capture is 
//       completely stopped.
//       Even when returning failure, it guarantees
//       no more callbacks will occur
void
DirectShowSource::stop()
{
	if ( graphIsSetup_ )
	{
		HRESULT hr = pMediaControlIF_->Stop();
		if ( FAILED(hr) )
		{
			log_error("failed to STOP the filter graph (0x%0x)\n", hr);
		}

		// If source was removed while not capturing (following a stop),
		// graph's Run() could block forever. The RenderStream(), however,
		// would fail. Therefore, call resetCapGraphFoo() whenever
		// stopping, to force RenderStream() to validate setup before
		// any start (and reduce likelihood of Run() blocking forever).
		//
		// And if source is effectively removed after
		// RenderStream(), but before graph's Run()?
		resetCapGraphFoo();
	}
}

int
DirectShowSource::validateFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative) const
{
	AM_MEDIA_TYPE *mediaFormat;

	if ( !findBestFormat(fmtNominal, fmtNative, &mediaFormat) )
		return 0;

	freeMediaType(*mediaFormat);
	return 1;
}

int
DirectShowSource::bindFormat(const vidcap_fmt_info * fmtNominal)
{
	vidcap_fmt_info fmtNative;

	// If we've already got one, free it
	if ( nativeMediaType_ )
	{
		freeMediaType(*nativeMediaType_);
		nativeMediaType_ = 0;
	}

	if ( !findBestFormat(fmtNominal, &fmtNative, &nativeMediaType_) )
		return 1;

	// set the framerate
	VIDEOINFOHEADER * vih = (VIDEOINFOHEADER *) nativeMediaType_->pbFormat;
	vih->AvgTimePerFrame = 10000000 *
		fmtNative.fps_denominator /
		fmtNative.fps_numerator;

	// set the dimensions
	vih->bmiHeader.biWidth = fmtNative.width;
	vih->bmiHeader.biHeight = fmtNative.height;

	return 0;
}

bool
DirectShowSource::findBestFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative, AM_MEDIA_TYPE **mediaFormat) const

{
	bool needsFpsEnforcement = false;
	bool needsFmtConv = false;

	int iCount = 0, iSize = 0;
	HRESULT hr = pStreamConfig_->GetNumberOfCapabilities(&iCount, &iSize);

	// Check the size to make sure we pass in the correct structure.
	if (iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS) )
	{
		log_error("capabilities struct is wrong size (%d not %d)\n",
				iSize, sizeof(VIDEO_STREAM_CONFIG_CAPS));
		return false;
	}

	struct formatProperties
	{
		bool isSufficient;
		bool needsFmtConversion;
		bool needsFpsEnforcement;
		AM_MEDIA_TYPE *mediaFormat;
	};

	// these will be filled-in by checkFormat()
	struct formatProperties * candidateFmtProps =
				new struct formatProperties [iCount];
	vidcap_fmt_info * fmtsNative = new vidcap_fmt_info [iCount];

	// enumerate each NATIVE source format
	bool itCanWork = false;
	for (int iFormat = 0; iFormat < iCount; ++iFormat )
	{
		candidateFmtProps[iFormat].isSufficient = false;
		candidateFmtProps[iFormat].needsFmtConversion = false;
		candidateFmtProps[iFormat].needsFpsEnforcement = false;
		candidateFmtProps[iFormat].mediaFormat = 0;

		// evaluate each native source format
		if ( checkFormat(fmtNominal, &fmtsNative[iFormat], iFormat,
					&(candidateFmtProps[iFormat].needsFmtConversion),
					&(candidateFmtProps[iFormat].needsFpsEnforcement),
					&(candidateFmtProps[iFormat].mediaFormat)) )
		{
			candidateFmtProps[iFormat].isSufficient = true;
			itCanWork = true;
		}
	}

	// Can ANY of this source's native formats
	// satisfy the requested format (to be bound)?
	if ( !itCanWork )
	{
		goto freeThenReturn;
	}

	// evaluate the possibilities

	int bestFmtNum = -1;

	// any that work without mods?
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient &&
			!candidateFmtProps[iFmt].needsFmtConversion &&
			!candidateFmtProps[iFmt].needsFpsEnforcement )
		{
			// found a perfect match
			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	// any that work without format conversion? but with framerate conversion?
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient &&
			!candidateFmtProps[iFmt].needsFmtConversion &&
			 candidateFmtProps[iFmt].needsFpsEnforcement )
		{
			// found an okay match
			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	// any that work with format conversion? but without framerate conversion?
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient &&
			 candidateFmtProps[iFmt].needsFmtConversion &&
			!candidateFmtProps[iFmt].needsFpsEnforcement )
		{
			// found a so-so match
			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	// any that work at all (with both conversions)?
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient )
		{
			// found a poor match
			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	log_error("findBestFormat: coding ERROR\n");
	itCanWork = false;

freeThenReturn:

	// Free all sufficient candidates (unless it's the the CHOSEN one)
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient )
		{
			// NOT the chosen one?
			if ( !itCanWork || iFmt != bestFmtNum )
				freeMediaType(*candidateFmtProps[iFmt].mediaFormat);
		}
	}

	// Can bind succeed?
	if ( itCanWork )
	{
		// take note of native media type, fps, dimensions, fourcc
		*mediaFormat = candidateFmtProps[bestFmtNum].mediaFormat;

		fmtNative->fps_numerator =
						fmtsNative[bestFmtNum].fps_numerator;
		fmtNative->fps_denominator =
						fmtsNative[bestFmtNum].fps_denominator;
		fmtNative->width = fmtsNative[bestFmtNum].width;
		fmtNative->height = fmtsNative[bestFmtNum].height;
		fmtNative->fourcc = fmtsNative[bestFmtNum].fourcc;
	}

	delete [] candidateFmtProps;
	delete [] fmtsNative;

	return itCanWork;
}

// Evaluate one of perhaps several native formats for
// suitability for providing the nominal format.
// Fill-in output parameter 'mediaFormat'.
bool
DirectShowSource::checkFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative,
		int formatNum,
		bool *needsFramerateEnforcing,
		bool *needsFormatConversion,
		AM_MEDIA_TYPE **mediaFormat) const
{
	// get video stream capabilities structure #(formatNum)
	VIDEO_STREAM_CONFIG_CAPS scc;
	AM_MEDIA_TYPE *pMediaType;
	HRESULT hr = pStreamConfig_->GetStreamCaps(formatNum, &pMediaType,
			(BYTE*)&scc);

	if (FAILED(hr))
	{
		log_warn("checkFormat: failed getting stream capabilities [%d] (%d)\n",
				formatNum + 1, hr);
		return false;
	}

	// check resolution
	if ( fmtNominal->width < scc.MinOutputSize.cx ||
	     fmtNominal->height < scc.MinOutputSize.cy ||
	     fmtNominal->width > scc.MaxOutputSize.cx ||
	     fmtNominal->height > scc.MaxOutputSize.cy )
	{
		freeMediaType(*pMediaType);
		return false;
	}

	bool matchesWidth = false;
	for (int width = scc.MinOutputSize.cx; width <= scc.MaxOutputSize.cx;
				width += scc.OutputGranularityX)
	{
		if ( width == fmtNominal->width )
		{
			matchesWidth = true;
			break;
		}
	}

	bool matchesHeight = false;
	for (int height = scc.MinOutputSize.cy; height <= scc.MaxOutputSize.cy;
				height += scc.OutputGranularityY)
	{
		if ( height == fmtNominal->height )
		{
			matchesHeight = true;
			break;
		}
	}

	if ( !matchesWidth || !matchesHeight )
	{
		freeMediaType(*pMediaType);
		return false;
	}

	// calculate range of supported frame rates
	double fpsMin = static_cast<double>( 1000000000 / scc.MaxFrameInterval)
			/ 100.0;
	double fpsMax = static_cast<double>( 1000000000 / scc.MinFrameInterval)
			/ 100.0;

	double fps = static_cast<double>(fmtNominal->fps_numerator) /
		static_cast<double>(fmtNominal->fps_denominator);

	// check framerate
	// TODO: a timer thread could enforce this framerate,
	//       but would need to pass src_ctx->use_timer_thread here
	//       to know that
	if ( fps > fpsMax )
	{
		freeMediaType(*pMediaType);
		return false;
	}

	if ( fps < fpsMin )
		*needsFramerateEnforcing = true;

	// check media type

	int nativeFourcc = 0;
	if ( mapDirectShowMediaTypeToVidcapFourcc(
				pMediaType->subtype.Data1, nativeFourcc) )
	{
		freeMediaType(*pMediaType);
		return false;
	}

	*needsFormatConversion = ( nativeFourcc != fmtNominal->fourcc );

	if ( *needsFormatConversion )
	{
		if ( conv_conversion_func_get(nativeFourcc, fmtNominal->fourcc) == 0 )
		{
			freeMediaType(*pMediaType);
			return false;
		}
	}

	fmtNative->fourcc = nativeFourcc;

	fmtNative->width = fmtNominal->width;
	fmtNative->height = fmtNominal->height;

	if ( *needsFramerateEnforcing )
	{
		//FIXME: Use float. Drop numerator/denominator business.
		fmtNative->fps_numerator = (int)fpsMax;
		fmtNative->fps_denominator = 1;
	}
	else
	{
		fmtNative->fps_numerator = fmtNominal->fps_numerator;
		fmtNative->fps_denominator = fmtNominal->fps_denominator;
	}

	// return this suitable media type
	*mediaFormat = pMediaType;

	return true;
}

void
DirectShowSource::freeMediaType(AM_MEDIA_TYPE &mediaType) const
{
	if ( mediaType.cbFormat != 0 )
		CoTaskMemFree((PVOID)mediaType.pbFormat);

	if ( mediaType.pUnk != NULL )
		mediaType.pUnk->Release();
}

STDMETHODIMP
DirectShowSource::QueryInterface(REFIID riid, void ** ppv)
{
	if ( ppv == NULL )
		return E_POINTER;

	if( riid == IID_ISampleGrabberCB || riid == IID_IUnknown )
	{
		*ppv = (void *) static_cast<ISampleGrabberCB*> ( this );
		return NOERROR;
	}

	return E_NOINTERFACE;
}

// The sample grabber calls this from its deliver thread
// NOTE: This function must not block, else it will cause a
//       graph's Stop() to block - which could result in a deadlock
STDMETHODIMP
DirectShowSource::BufferCB( double dblSampleTime, BYTE * pBuff, long buffSize )
{
	return bufferCB_(dblSampleTime, pBuff, buffSize, parent_);
}

int
DirectShowSource::mapDirectShowMediaTypeToVidcapFourcc(DWORD data, int & fourcc)
{
	switch ( data )
	{
	case 0xe436eb7e: 
		fourcc = VIDCAP_FOURCC_RGB32;
		break;
	case 0x30323449: // I420
	case 0x56555949: // IYUV
		fourcc = VIDCAP_FOURCC_I420;
		break;
	case 0x32595559:
		fourcc = VIDCAP_FOURCC_YUY2;
		break;
	case 0xe436eb7d:
		fourcc = VIDCAP_FOURCC_BOTTOM_UP_RGB24;
		break;
	case 0xe436eb7c:
		fourcc = VIDCAP_FOURCC_RGB555;
		break;
	case 0x39555659:
		fourcc = VIDCAP_FOURCC_YVU9;
		break;
	default:
		log_warn("failed to map 0x%08x to vidcap fourcc\n", data);
		return -1;
	}

	return 0;
}

// Direct Show filter graph has signaled an event
// Device may have been removed
// Error may have occurred
void
DirectShowSource::processGraphEvent(void *context)
{
	DirectShowSource * pSrc = static_cast<DirectShowSource *>(context);

	HRESULT hr;
	long evCode, param1, param2;
	hr = pSrc->pMediaEventIF_->GetEvent(&evCode, &param1, &param2, 0);

	if ( SUCCEEDED(hr) )
	{
		// Event codes taken from:
		// http://msdn2.microsoft.com/en-us/library/ms783649.aspx
		// Embedded reference not used:
		// http://msdn2.microsoft.com/en-us/library/aa921722.aspx

		std::string str;

	    switch(evCode) 
	    { 
		case EC_DEVICE_LOST:
			if ( (int)param2 == 0 )
			{
				str.assign("EC_DEVICE_LOST\n");
				log_info("device removal detected\n");

				// shutdown capture
				pSrc->cancelCaptureCB_(pSrc->parent_);
			}
			else
			{
				str.assign("EC_DEVICE_LOST - (device re-inserted)\n");
			}
			break;

		case EC_ACTIVATE:
			str.assign("EC_ACTIVATE\n");
			break;

/*		case EC_BANDWIDTHCHANGE:
			str.assign("EC_BANDWIDTHCHANGE\n");
			break;
*/
		case EC_BUFFERING_DATA:
			str.assign("EC_BUFFERING_DATA\n");
			break;

		case EC_BUILT:
			str.assign("EC_BUILT\n");
			break;

		case EC_CLOCK_CHANGED:
			str.assign("EC_CLOCK_CHANGED\n");
			break;

		case EC_CLOCK_UNSET:
			str.assign("EC_CLOCK_UNSET\n");
			break;

		case EC_CODECAPI_EVENT:
			str.assign("EC_CODECAPI_EVENT\n");
			break;

		case EC_COMPLETE:
			str.assign("EC_COMPLETE\n");
			break;

/*		case EC_CONTENTPROPERTY_CHANGED:
			str.assign("EC_CONTENTPROPERTY_CHANGED\n");
			break;
*/
		case EC_DISPLAY_CHANGED:
			str.assign("EC_DISPLAY_CHANGED\n");
			break;

		case EC_END_OF_SEGMENT:
			str.assign("EC_END_OF_SEGMENT\n");
			break;

/*		case EC_EOS_SOON:
			str.assign("EC_EOS_SOON\n");
			break;
*/
		case EC_ERROR_STILLPLAYING:
			str.assign("EC_ERROR_STILLPLAYING\n");
			break;

		case EC_ERRORABORT:
			str.assign("EC_ERRORABORT\n");

			log_info("graph monitor stopping capture...\n");

			// shutdown capture
			pSrc->cancelCaptureCB_(pSrc->parent_);

			break;

/*		case EC_ERRORABORTEX:
			str.assign("EC_ERRORABORTEX\n");
			break;
*/
		case EC_EXTDEVICE_MODE_CHANGE:
			str.assign("EC_EXTDEVICE_MODE_CHANGE\n");
			break;

/*		case EC_FILE_CLOSED:
			str.assign("EC_FILE_CLOSED\n");
			break;
*/
		case EC_FULLSCREEN_LOST:
			str.assign("EC_FULLSCREEN_LOST\n");
			break;

		case EC_GRAPH_CHANGED:
			str.assign("EC_GRAPH_CHANGED\n");
			break;

		case EC_LENGTH_CHANGED:
			str.assign("EC_LENGTH_CHANGED\n");
			break;

/*		case EC_LOADSTATUS:
			str.assign("EC_LOADSTATUS\n");
			break;
*/
/*		case EC_MARKER_HIT:
			str.assign("EC_MARKER_HIT\n");
			break;
*/
		case EC_NEED_RESTART:
			str.assign("EC_NEED_RESTART\n");
			break;

/*		case EC_NEW_PIN:
			str.assign("EC_NEW_PIN\n");
			break;
*/
		case EC_NOTIFY_WINDOW:
			str.assign("EC_NOTIFY_WINDOW\n");
			break;

		case EC_OLE_EVENT:
			str.assign("EC_OLE_EVENT\n");
			break;

		case EC_OPENING_FILE:
			str.assign("EC_OPENING_FILE\n");
			break;

		case EC_PALETTE_CHANGED:
			str.assign("EC_PALETTE_CHANGED\n");
			break;

		case EC_PAUSED:
			str.assign("EC_PAUSED\n");
			break;

/*		case EC_PLEASE_REOPEN:
			str.assign("EC_PLEASE_REOPEN\n");
			break;
*/
		case EC_PREPROCESS_COMPLETE:
			str.assign("EC_PREPROCESS_COMPLETE\n");
			break;

/*		case EC_PROCESSING_LATENCY:
			str.assign("EC_PROCESSING_LATENCY\n");
			break;
*/
		case EC_QUALITY_CHANGE:
			str.assign("EC_QUALITY_CHANGE\n");
			break;

/*		case EC_RENDER_FINISHED:
			str.assign("EC_RENDER_FINISHED\n");
			break;
*/
		case EC_REPAINT:
			str.assign("EC_REPAINT\n");
			break;

/*		case EC_SAMPLE_LATENCY:
			str.assign("EC_SAMPLE_LATENCY\n");
			break;

		case EC_SAMPLE_NEEDED:
			str.assign("EC_SAMPLE_NEEDED\n");
			break;

		case EC_SCRUB_TIME:
			str.assign("EC_SCRUB_TIME\n");
			break;
*/
		case EC_SEGMENT_STARTED:
			str.assign("EC_SEGMENT_STARTED\n");
			break;

		case EC_SHUTTING_DOWN:
			str.assign("EC_SHUTTING_DOWN\n");
			break;

		case EC_SNDDEV_IN_ERROR:
			str.assign("EC_SNDDEV_IN_ERROR\n");
			break;

		case EC_SNDDEV_OUT_ERROR:
			str.assign("EC_SNDDEV_OUT_ERROR\n");
			break;

		case EC_STARVATION:
			str.assign("EC_STARVATION\n");
			break;

		case EC_STATE_CHANGE:
			str.assign("EC_STATE_CHANGE\n");
			break;

/*		case EC_STATUS:
			str.assign("EC_STATUS\n");
			break;
*/
		case EC_STEP_COMPLETE:
			str.assign("EC_STEP_COMPLETE\n");
			break;

		case EC_STREAM_CONTROL_STARTED:
			str.assign("EC_STREAM_CONTROL_STARTED\n");
			break;

		case EC_STREAM_CONTROL_STOPPED:
			str.assign("EC_STREAM_CONTROL_STOPPED\n");
			break;

		case EC_STREAM_ERROR_STILLPLAYING:
			str.assign("EC_STREAM_ERROR_STILLPLAYING\n");
			break;

		case EC_STREAM_ERROR_STOPPED:
			str.assign("EC_STREAM_ERROR_STOPPED\n");
			break;

		case EC_TIMECODE_AVAILABLE:
			str.assign("EC_TIMECODE_AVAILABLE\n");
			break;

		case EC_UNBUILT:
			str.assign("EC_UNBUILT\n");
			break;

		case EC_USERABORT:
			str.assign("EC_USERABORT\n");
			break;

		case EC_VIDEO_SIZE_CHANGED:
			str.assign("EC_VIDEO_SIZE_CHANGED\n");
			break;

/*		case EC_VIDEOFRAMEREADY:
			str.assign("EC_VIDEOFRAMEREADY\n");
			break;
*/
		case EC_VMR_RECONNECTION_FAILED:
			str.assign("EC_VMR_RECONNECTION_FAILED\n");
			break;

		case EC_VMR_RENDERDEVICE_SET:
			str.assign("EC_VMR_RENDERDEVICE_SET\n");
			break;

		case EC_VMR_SURFACE_FLIPPED:
			str.assign("EC_VMR_SURFACE_FLIPPED\n");
			break;

		case EC_WINDOW_DESTROYED:
			str.assign("EC_WINDOW_DESTROYED\n");
			break;

		case EC_WMT_EVENT:
			str.assign("EC_WMT_EVENT\n");
			break;

		case EC_WMT_INDEX_EVENT:
			str.assign("EC_WMT_INDEX_EVENT\n");
			break;

		default:
			str.assign("unknown graph event code (%ld)\n", evCode);
			break;
	    } 

		log_info("graph processed event: %s", str.c_str());

	    hr = pSrc->pMediaEventIF_->FreeEventParams(evCode, param1, param2);
	}
	else
	{
		log_error("failed getting event for a graph\n");
	}
}

bool
DirectShowSource::getCaptureDevice(const char *devLongName,
		IBindCtx **ppBindCtx,
		IMoniker **ppMoniker) const
{
	HRESULT hr;

	// Create an enumerator
	CComPtr<ICreateDevEnum> pCreateDevEnum;

	pCreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);

	if ( !pCreateDevEnum )
	{
		log_error("failed creating device enumerator - to get a source\n");
		return false;
	}

	// Enumerate video capture devices
	CComPtr<IEnumMoniker> pEm;

	pCreateDevEnum->CreateClassEnumerator(
			CLSID_VideoInputDeviceCategory, &pEm, 0);

	if ( !pEm )
	{
		log_error("failed creating enumerator moniker\n");
		return false;
	}

	pEm->Reset();

	ULONG ulFetched;
	IMoniker * pM;

	// Iterate over all video capture devices
	int i=0;
	while ( pEm->Next(1, &pM, &ulFetched) == S_OK )
	{
		IBindCtx *pbc;

		hr = CreateBindCtx(0, &pbc);

		if ( FAILED(hr) )
		{
			log_error("failed CreateBindCtx\n");
			pM->Release();
			return false;
		}

		// Get the device names
		char *shortName;
		char *longName;
		if ( getDeviceInfo(pM, pbc, &shortName, &longName) )
		{
			log_warn("failed to get device info.\n");
			pbc->Release();
			continue;
		}

		// Compare with the desired dev name
		if ( !strcmp(longName, devLongName) )
		{
			// Got the correct device
			*ppMoniker = pM;
			*ppBindCtx = pbc;

			free(shortName);
			free(longName);
			return true;
		}

		// Wrong device. Cleanup and try again
		free(shortName);
		free(longName);

		pbc->Release();
	}

	pM->Release();

	return false;
}
