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

#ifndef _DIRECTSHOWSOURCE_H_
#define _DIRECTSHOWSOURCE_H_

#include <stdexcept>
#include <qedit.h>
#include <DShow.h>

#include "DShowSrcManager.h"
#include "sapi_context.h"
#include "GraphMonitor.h"

class DirectShowSource : public ISampleGrabberCB, public DirectShowObject
{

public:
	typedef int (*bufferCallbackFunc)(double, BYTE *, long, void *);
	typedef void (*cancelCaptureFunc)(void *);

	DirectShowSource(struct sapi_src_context *,
			bufferCallbackFunc, cancelCaptureFunc, void *);
	~DirectShowSource();

	int start();
	void stop();
	int bindFormat(const vidcap_fmt_info * fmtInfo);
	int validateFormat(const vidcap_fmt_info * fmtNominal,
			vidcap_fmt_info * fmtNative) const;

	typedef void (*graphEventCBFunc)(void *);
	static void processGraphEvent(void *);

	const char * getID() const
	{
		return sourceContext_->src_info.identifier;
	}

private:
	int createCapGraphFoo();
	void destroyCapGraphFoo();
	int setupCapGraphFoo();
	int resetCapGraphFoo();
	bool checkFormat(const vidcap_fmt_info * fmtNominal,
			vidcap_fmt_info * fmtNative,
			int formatNum,
			bool *needsFramerateEnforcing, bool *needsFormatConversion,
			AM_MEDIA_TYPE **candidateMediaFormat) const;
	bool findBestFormat(const vidcap_fmt_info * fmtNominal,
			vidcap_fmt_info * fmtNative, AM_MEDIA_TYPE **mediaFormat) const;
	void freeMediaType(AM_MEDIA_TYPE &) const;
	bool getCaptureDevice(const char *devLongName,
			IBindCtx **ppBindCtx,
			IMoniker **ppMoniker) const;

	// Fake out COM
	STDMETHODIMP_(ULONG) AddRef() { return 2; }
	STDMETHODIMP_(ULONG) Release() { return 1; }
	STDMETHODIMP QueryInterface(REFIID riid, void ** ppv);

	// Not used
	STDMETHODIMP SampleCB( double, IMediaSample *)
	{
		return S_OK;
	}

	// The sample grabber calls us back from its deliver thread
	STDMETHODIMP BufferCB( double, BYTE *, long );

	static int mapDirectShowMediaTypeToVidcapFourcc(DWORD data, int & fourcc);

private:
	struct sapi_src_context * sourceContext_;
	bufferCallbackFunc bufferCB_;
	cancelCaptureFunc cancelCaptureCB_;
	void * parent_;
	GraphMonitor *graphMon_;

	IBaseFilter * pSource_;
	ICaptureGraphBuilder2 *pCapGraphBuilder_;
	IAMStreamConfig * pStreamConfig_;
	IGraphBuilder *pFilterGraph_;
	IMediaEventEx * pMediaEventIF_;
	IBaseFilter * pSampleGrabber_;
	ISampleGrabber * pSampleGrabberIF_;
	IBaseFilter * pNullRenderer_;
	IMediaControl * pMediaControlIF_;
	AM_MEDIA_TYPE *nativeMediaType_;
	HANDLE *graphHandle_;
	bool graphIsSetup_;
};

#endif
