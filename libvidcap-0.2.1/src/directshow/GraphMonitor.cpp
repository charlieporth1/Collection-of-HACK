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

// This thread will monitor a DirectShow filter graph for a capture device.
// If the device is removed or encounters an error, the capture callback will
// be called with a special status code, indicating it's the last callback

#include <stdexcept>
#include "GraphMonitor.h"
#include "logging.h"

GraphMonitor::GraphMonitor(HANDLE *graphHandle,
		graphEventCBFunc cb, void * parentCtx)
		: graphHandle_(graphHandle),
		  processGraphEvent_(cb),
		  parentContext_(parentCtx),
		  graphMonitorThread_(0),
		  graphMonitorThreadID_(0)
{
	// create an event used to wake up thread
	// when thread must terminate
	terminateEvent_ = CreateEvent( 
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		); 

	if ( terminateEvent_ == NULL)
	{ 
		log_error("CreateEvent failed (%d)\n", GetLastError());
		throw std::runtime_error("GraphMonitor: failed creating event");
	}

	// create an event used to signal that the thread has been created
	initDoneEvent_ = CreateEvent( 
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		); 

	if ( initDoneEvent_ == NULL) 
	{ 
		log_error("CreateEvent failed (%d)\n", GetLastError());
		throw std::runtime_error("GraphMonitor: failed creating init event");
	}

	// create thread, pass instance
	if ( vc_create_thread(&graphMonitorThread_,
				&GraphMonitor::monitorGraph,
				this, &graphMonitorThreadID_) )
	{
		log_error("GraphMonitor: failed spinning GraphMonitor thread (%d)\n",
				GetLastError());

		throw std::runtime_error("GraphMonitor: failed spinning thread");
	}

	DWORD rc = WaitForSingleObject(initDoneEvent_, INFINITE);

}

GraphMonitor::~GraphMonitor()
{
	// signal thread to shutdown
	if ( !SetEvent(terminateEvent_) )
	{
		log_error("failed to signal graph monitor thread to terminate (%d)\n",
				GetLastError());
		return;
	}

	// wait for thread to shutdown
	DWORD rc = WaitForSingleObject((HANDLE)graphMonitorThread_, INFINITE);

	if ( rc == WAIT_FAILED )
	{
		log_error("GraphMonitor: failed waiting for thread to return (%d)\n",
				GetLastError());
	}

	log_info("graph monitor thread destroyed\n");
}

unsigned int
GraphMonitor::monitorGraph(void * param)
{
	// extract instance
	GraphMonitor * pGraphMon = (GraphMonitor *)param;

	// signal that thread is ready
	if ( !SetEvent(pGraphMon->initDoneEvent_) )
	{
		log_error("failed to signal that graph monitor thread is ready (%d)\n",
				GetLastError());
		return -1;
	}

	// Setup array of handles to wait on...
	enum { terminateIndex = 0, graphEventIndex };
	const size_t numHandles = graphEventIndex + 1;
	HANDLE * waitHandles = new HANDLE [numHandles];

	waitHandles[terminateIndex] = pGraphMon->terminateEvent_;

	// complete the array of handles
	waitHandles[graphEventIndex] = pGraphMon->graphHandle_;

	while ( true )
	{
		// wait until the graph signals an event OR
		// the thread must die
		DWORD rc = WaitForMultipleObjects(static_cast<DWORD>(numHandles),
				waitHandles, false, INFINITE);

		// get index of object that signaled
		unsigned int index = rc - WAIT_OBJECT_0;

		if ( rc == WAIT_FAILED )
		{
			log_warn("graph monitor wait failed. (0x%x)\n", GetLastError());
		}
		else if ( index == terminateIndex )
		{
			// terminate
			break;
		}
		// check if it was the filter graph handle that signaled
		else if ( index == graphEventIndex )
		{
			// have parent handle the graph event
			pGraphMon->processGraphEvent_(pGraphMon->parentContext_);
		}
		else
		{
			log_warn("graph monitor: unknown wait rc = 0x%x\n", rc);
		}
	}

	delete [] waitHandles;

	return 0;
}
