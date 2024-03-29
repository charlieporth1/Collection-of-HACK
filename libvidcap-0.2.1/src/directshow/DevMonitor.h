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

#ifndef _DEVMONITOR_H_
#define _DEVMONITOR_H_

#include <vidcap/vidcap.h>

class DevMonitor
{

public:
	DevMonitor();
	~DevMonitor();
	int registerCallback(void *);

private:
	static LRESULT __stdcall processWindowsMsgs(HWND, UINT, WPARAM, LPARAM);
	static unsigned int STDCALL monitorDevices(void * lpParam);

private:
	HANDLE initDoneEvent_;
	enum threadStatusEnum { initializing=0, initFailed, initDone };
	threadStatusEnum threadStatus_;

	HWND windowHandle_;
	vc_thread devMonitorThread_;
	unsigned int devMonitorThreadID_;
	TCHAR * szTitle_;
	TCHAR * szWindowClass_;

	void * sapiCtx_;

	vc_mutex registrationMutex_;
};

#endif
