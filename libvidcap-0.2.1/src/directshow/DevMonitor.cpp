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
#include <dbt.h>
#include <process.h>
#include <string>
#include "sapi_context.h"
#include "DevMonitor.h"
#include "logging.h"

DevMonitor::DevMonitor()
	: threadStatus_(initializing),
	  windowHandle_(0),
	  devMonitorThread_(0),
	  devMonitorThreadID_(0),
	  szTitle_(0),
	  szWindowClass_(0),
	  sapiCtx_(0)
{
	vc_mutex_init(&registrationMutex_);

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
		throw std::runtime_error("DevMonitor: failed creating event");
	}

	const int max_loadstring = 100;
	szTitle_ = new TCHAR [max_loadstring];
	szWindowClass_ = new TCHAR [max_loadstring];
	lstrcpy(szTitle_, L"DevMonitor");
	lstrcpy(szWindowClass_, L"DEVMONITOR");

	// Register window class
	// Specify window-processing function
	// Specify context (where is this used?)
	WNDCLASSEX wcex;
	wcex.cbSize         = sizeof(WNDCLASSEX);
	wcex.style          = CS_HREDRAW;
	wcex.lpfnWndProc    = &DevMonitor::processWindowsMsgs;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = (HINSTANCE)this;
	wcex.hIcon          = NULL;
	wcex.hCursor        = NULL;
	wcex.hbrBackground  = NULL;
	wcex.lpszMenuName   = NULL;
	wcex.lpszClassName  = szWindowClass_;
	wcex.hIconSm        = NULL;

	if ( !RegisterClassEx(&wcex) )
	{
		log_error("DevMonitor: RegisterClassEx() failed (%d)\n",
				GetLastError());
		throw std::runtime_error("DevMonitor: RegisterClassEx() failed");
	}

	// pass instance to thread
	if ( vc_create_thread(&devMonitorThread_,
				&DevMonitor::monitorDevices,
				this, &devMonitorThreadID_) )
	{
		log_error("DevMonitor: failed spinning DevMonitor thread (%d)\n",
				GetLastError());

		goto bail;
	}

	// wait for signal from thread that it is ready
	DWORD rc = WaitForSingleObject(initDoneEvent_, INFINITE);
	//FIXME: consider a timeout

	if ( threadStatus_ == initDone )
		return;

	log_error("DevMonitor: failed initializing DevMonitor thread\n");

bail:
	if( !UnregisterClass(szWindowClass_, 0) )
	{
		log_error("DevMonitor: failed to unregister class (%d)\n",
				GetLastError());
	}

	throw std::runtime_error("DevMonitor: construction failure");
}

DevMonitor::~DevMonitor()
{
	// signal to the monitor thread that it's time to die
	if ( !PostMessage(windowHandle_, WM_CLOSE, 0, 0) )
	{
		log_error("DevMonitor: failed posting message to thread (%d)\n",
				GetLastError());
		return;
	}

	DWORD rc = WaitForSingleObject((HANDLE)devMonitorThread_, INFINITE);

	if ( rc == WAIT_FAILED )
	{
		log_error("DevMonitor: failed waiting for thread to return (%d)\n",
				GetLastError());
	}

	if( !UnregisterClass(szWindowClass_, 0) )
	{
		log_error("DevMonitor: failed unregistering class (%d)\n",
				GetLastError());
	}

	vc_mutex_destroy(&registrationMutex_);
}

LRESULT CALLBACK
DevMonitor::processWindowsMsgs(HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		// the main thread wants the window to die
		if ( !DestroyWindow(hWnd) )
		{
			log_error("DevMonitor: failed destroying window (%d)\n",
					GetLastError());

			// take more drastic action
			PostQuitMessage(0);
		}
		break;

	case WM_DESTROY:
		// DestroyWindow() complete.
		// Cause thread to exit it's message loop.
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

unsigned int
DevMonitor::monitorDevices(void * param)
{
	//FIXME: move window creation to constructor

	// extract instance
	DevMonitor * pDevMon = (DevMonitor *)param;

	// Create a window, so main thread can communicate with us
	pDevMon->windowHandle_ = CreateWindow(
			pDevMon->szWindowClass_,
			pDevMon->szTitle_,
			WS_DISABLED,
			CW_USEDEFAULT,
			0,
			CW_USEDEFAULT,
			0,
			NULL,
			NULL,
			(HINSTANCE)0,
			NULL);

	if ( !pDevMon->windowHandle_ )
	{
		log_error("DevMonitor: CreateWindow failed (%d)\n", GetLastError());
		pDevMon->threadStatus_ = initFailed;

		if ( !SetEvent(pDevMon->initDoneEvent_) )
		{
			log_error("failed to signal that dev monitor thread failed (%d)\n",
					GetLastError());
		}

		return -1;
	}

	log_info("[[ monitoring system for device events... ]]\n");

	// signal to main thread that we are ready to monitor device events
	pDevMon->threadStatus_ = initDone;
	if ( !SetEvent(pDevMon->initDoneEvent_) )
	{
		log_error("failed to signal that dev monitor thread is ready (%d)\n",
				GetLastError());
		return -1;
	}

	// Main message loop
	// Breaks out when PostQuitMessage() is called
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		std::string str;
	
		sapi_context * sapiCtx = 
					static_cast<sapi_context *>(pDevMon->sapiCtx_);

		switch (msg.message)
		{
		case WM_DEVICECHANGE:
			switch(msg.wParam)
			{
			case DBT_CONFIGCHANGECANCELED:
				str.assign("DBT_CONFIGCHANGECANCELED");
				break;
			case DBT_CONFIGCHANGED:
				str.assign("DBT_CONFIGCHANGED");
				break;
			case DBT_CUSTOMEVENT:
				str.assign("DBT_CUSTOMEVENT");
				break;
			case DBT_DEVICEARRIVAL:
				str.assign("DBT_DEVICEARRIVAL");
				break;
			case DBT_DEVICEQUERYREMOVE:
				str.assign("DBT_DEVICEQUERYREMOVE");
				break;
			case DBT_DEVICEQUERYREMOVEFAILED:
				str.assign("DBT_DEVICEQUERYREMOVEFAILED");
				break;
			case DBT_DEVICEREMOVECOMPLETE:
				str.assign("DBT_DEVICEREMOVECOMPLETE");
				break;
			case DBT_DEVICEREMOVEPENDING:
				str.assign("DBT_DEVICEREMOVEPENDING");
				break;
			case DBT_DEVICETYPESPECIFIC:
				str.assign("DBT_DEVICETYPESPECIFIC");
				break;
			case DBT_DEVNODES_CHANGED:
				str.assign("DBT_DEVNODES_CHANGED");
				break;
			case DBT_QUERYCHANGECONFIG:
				str.assign("DBT_QUERYCHANGECONFIG");
				break;
			case DBT_USERDEFINED:
				str.assign("DBT_USERDEFINED");
				break;
			default:
				str.assign("default");
				break;
			}
			log_info("[[ Device event: %s ]]\n", str.c_str());

			vc_mutex_lock(&pDevMon->registrationMutex_);

			if ( sapiCtx &&
					sapiCtx->notify_callback )
			{
				// TODO: If the user returns non-zero from
				// their callback, maybe that should have
				// some semantic meaning. For example, maybe
				// that should be a way that the user can
				// cancel further notification callbacks.
				sapiCtx->notify_callback(sapiCtx, sapiCtx->notify_data);
			}
			else
			{
				log_info("[[ no user-defined handler registered ]]\n");
			}

			vc_mutex_unlock(&pDevMon->registrationMutex_);

		default:
			str.assign("");
			DispatchMessage(&msg);
			break;
		}
	}

	log_info("[[ system device monitor halted ]]\n");
	return (int) msg.wParam;
}

int
DevMonitor::registerCallback(void * sapiCtx)
{
	if ( !sapiCtx )
		return -1;

	vc_mutex_lock(&registrationMutex_);

	sapiCtx_ = sapiCtx;

	vc_mutex_unlock(&registrationMutex_);

	return 0;
}
