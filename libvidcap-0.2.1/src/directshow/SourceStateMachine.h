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

#ifndef _SOURCESTATEMACHINE_H_
#define _SOURCESTATEMACHINE_H_

#include "DirectShowSource.h"
#include "sapi_context.h"

class SourceStateMachine
{

public:
	SourceStateMachine(struct sapi_src_context *src, DShowSrcManager *);
	~SourceStateMachine();

	int start();
	int stop();
	int bindFormat(const vidcap_fmt_info * fmtInfo);
	int validateFormat(const vidcap_fmt_info * fmtNominal,
			vidcap_fmt_info * fmtNative) const;

	const char * getID() const
	{
		return sourceContext_->src_info.identifier;
	}

private:
	static unsigned int STDCALL waitForCmd(void *);
	void terminate();
	void doStart();
	void doStop();
	void doCancelCapture();
	int createEvents();
	void cancelCapture();

	typedef int (*bufferCallbackFunc)(double, BYTE *, long, void *);
	static int bufferCB( double dblSampleTime, BYTE * pBuff, long buffSize, void *parent);

	typedef void (*cancelCaptureFunc)(void *);
	static void cancelCaptureCB(void *parent);

private:
	struct sapi_src_context * sourceContext_;
	DShowSrcManager * dshowMgr_;
	DirectShowSource * src_;
	HANDLE eventInitDone_;
	HANDLE eventStart_;
	HANDLE eventStop_;
	HANDLE eventTerminate_;
	HANDLE eventCancel_;
	vc_thread sourceThread_;
	unsigned int sourceThreadID_;

	bool okToSendStart_;
	bool okToSendStop_;
	bool allowCallbacks_;
	bool callbackInProgress_;
	bool callbackCancellationInProgress_;
	bool captureStopped_;
};

#endif
