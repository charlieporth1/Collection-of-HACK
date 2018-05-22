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

#ifndef _GRAPHMONITOR_H_
#define _GRAPHMONITOR_H_

#include "os_funcs.h"

class GraphMonitor
{

public:
	typedef void (*graphEventCBFunc)(void *);

	GraphMonitor(HANDLE *, graphEventCBFunc, void *);
	~GraphMonitor();

private:
	static unsigned int STDCALL monitorGraph(void * param);

private:
	HANDLE *graphHandle_;
	void * parentContext_;
	graphEventCBFunc processGraphEvent_;

	HANDLE initDoneEvent_;
	HANDLE terminateEvent_;

	vc_thread graphMonitorThread_;
	unsigned int graphMonitorThreadID_;
};

#endif
