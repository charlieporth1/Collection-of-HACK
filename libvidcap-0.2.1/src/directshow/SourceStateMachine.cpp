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

#include "sapi.h"
#include "hotlist.h"
#include "logging.h"
#include "SourceStateMachine.h"

SourceStateMachine::SourceStateMachine(struct sapi_src_context *src,
		DShowSrcManager *mgr)
	: sourceContext_(src),
	  dshowMgr_(mgr),
	  src_(0),
	  eventInitDone_(0),
	  eventStart_(0),
	  eventStop_(0),
	  eventTerminate_(0),
	  eventCancel_(0),
	  sourceThread_(0),
	  sourceThreadID_(0),
	  okToSendStart_(true),
	  okToSendStop_(true),
	  allowCallbacks_(false),
	  callbackInProgress_(false),
	  callbackCancellationInProgress_(false),
	  captureStopped_(true)
{
	if ( !dshowMgr_ )
	{
		log_error("NULL manager passed to SourceStateMachine constructor.");
		throw std::runtime_error("NULL manager passed to SourceStateMachine");
	}

	if ( !sourceContext_ )
	{
		log_error("NULL source context passed to SourceStateMachine ctor.");
		throw std::runtime_error("NULL source context passed to constructor");
	}

	if ( !dshowMgr_->okayToBuildSource( getID() ) )
	{
		log_warn("Mgr won't permit construction of SourceStateMachine w/id '%s'."
				" It's already been acquired.\n", getID());
		throw std::runtime_error("source already acquired");
	}

	try
	{
		src_ = new DirectShowSource(src,
				(bufferCallbackFunc)&SourceStateMachine::bufferCB,
				(cancelCaptureFunc)&SourceStateMachine::cancelCaptureCB,
				this);
	}
	catch (std::runtime_error &) //check
	{
		log_error("failed creating dshow source\n");
		goto constructionFailure;
	}

	if ( createEvents() )
	{
		log_error("failed creating events for source thread");
		goto constructionFailure;
	}

	// pass instance to thread
	if ( !vc_create_thread(&sourceThread_,
				&SourceStateMachine::waitForCmd,
				this, &sourceThreadID_) )
	{
		// wait for signal from thread that it is ready
		WaitForSingleObject(eventInitDone_, INFINITE);

		return;
	}

	log_error("failed spinning source thread (%d)\n",
			GetLastError());

constructionFailure:

	throw std::runtime_error("failed source construction");
}

SourceStateMachine::~SourceStateMachine()
{
	log_info("Signaling source '%s' to terminate...\n", 
			sourceContext_->src_info.description);

	// signal thread to shutdown
	if ( !SetEvent(eventTerminate_) )
	{
		log_error("failed to signal graph monitor thread to terminate (%d)\n",
				GetLastError());
		return;
	}

	// wait for thread to shutdown
	DWORD rc = WaitForSingleObject((HANDLE)sourceThread_, INFINITE);

	if ( rc == WAIT_FAILED )
	{
		log_error("SourceStateMachine: failed waiting for thread to return (%d)\n",
				GetLastError());
	}

	log_info("source '%s' has terminated\n", 
			sourceContext_->src_info.description);

	CloseHandle(eventInitDone_);
	CloseHandle(eventStart_);
	CloseHandle(eventStop_);
	CloseHandle(eventTerminate_);
}

int
SourceStateMachine::createEvents()
{
	// create an event used to signal that the thread has been created
	eventInitDone_ = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		);

	if ( eventInitDone_ == NULL)
	{
		log_error("SourceStateMachine: failed creating initDone event (%d)\n",
				GetLastError());
		return -1;
	}

	// create an event used to signal thread to start capture
	eventStart_ = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		);

	if ( eventStart_ == NULL)
	{
		log_error("SourceStateMachine: failed creating start event (%d)\n",
				GetLastError());
		CloseHandle(eventInitDone_);
		return -1;
	}

	// create an event used to signal thread to stop capture
	eventStop_ = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		);

	if ( eventStop_ == NULL)
	{
		log_error("SourceStateMachine: failed creating stop event (%d)\n",
				GetLastError());
		CloseHandle(eventInitDone_);
		CloseHandle(eventStart_);
		return -1;
	}

	// create an event used to signal thread to terminate
	eventTerminate_ = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		);

	if ( eventTerminate_ == NULL)
	{
		log_error("SourceStateMachine: failed creating terminate event (%d)\n",
				GetLastError());
		CloseHandle(eventInitDone_);
		CloseHandle(eventStart_);
		CloseHandle(eventStop_);
		return -1;
	}

	// create an event used to signal thread to cancel capture
	eventCancel_ = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		);

	if ( eventCancel_ == NULL)
	{
		log_error("SourceStateMachine: failed creating cancel event (%d)\n",
				GetLastError());
		CloseHandle(eventInitDone_);
		CloseHandle(eventStart_);
		CloseHandle(eventStop_);
		CloseHandle(eventTerminate_);
		return -1;
	}

	return 0;
}

unsigned int
SourceStateMachine::waitForCmd(void *param)
{
	// extract instance
	SourceStateMachine * pSrc = (SourceStateMachine *)param;

	// signal to main thread that we are ready for commands
	if ( !SetEvent(pSrc->eventInitDone_) )
	{
		log_error("failed to signal that source thread is ready (%d)\n",
				GetLastError());
		return -1;
	}

	enum { cancelEventIndex = 0, startEventIndex, stopEventIndex,
			terminateEventIndex };

	size_t numHandles = terminateEventIndex + 1;
	HANDLE * waitHandles = new HANDLE [numHandles];

	waitHandles[cancelEventIndex]    = pSrc->eventCancel_;
	waitHandles[startEventIndex]     = pSrc->eventStart_;
	waitHandles[stopEventIndex]      = pSrc->eventStop_;
	waitHandles[terminateEventIndex] = pSrc->eventTerminate_;

	while ( true )
	{
		// wait until signaled to start or stop capture
		// OR to terminate
		DWORD rc = WaitForMultipleObjects(static_cast<DWORD>(numHandles),
				waitHandles, false, INFINITE);

		// get index of object that signaled
		unsigned int index = rc - WAIT_OBJECT_0;

		if ( rc == WAIT_FAILED )
		{
			log_warn("source wait failed. (0x%x)\n", GetLastError());
		}
		else if ( index == terminateEventIndex )
		{
			// give terminate the highest priority

			pSrc->terminate();
			break;
		}
		else if ( index == cancelEventIndex )
		{
			// give cancel a higher priority than start/stop

			if ( !ResetEvent(pSrc->eventCancel_) )
			{
				log_error("failed to reset source start event flag."
						"Terminating.\n");
				// terminate
				break;
			}

			pSrc->okToSendStart_ = true;

			pSrc->doCancelCapture();
		}
		else if ( index == startEventIndex )
		{
			if ( !ResetEvent(pSrc->eventStart_) )
			{
				log_error("failed to reset source start event flag."
						"Terminating.\n");
				// terminate
				break;
			}

			pSrc->okToSendStop_ = true;

			pSrc->doStart();
		}
		else if ( index == stopEventIndex )
		{
			if ( !ResetEvent(pSrc->eventStop_) )
			{
				log_error("failed to reset source stop event flag."
						"Terminating.\n");
				// terminate
				break;
			}

			pSrc->okToSendStart_ = true;

			pSrc->doStop();
		}
	}

	delete [] waitHandles;

	return 0;
}

void
SourceStateMachine::terminate()
{
	delete src_;

	dshowMgr_->sourceReleased( getID() );
}

int
SourceStateMachine::start()
{
	if ( !okToSendStart_ )
	{
		log_warn("ERROR: Source not ready for a start command\n");
		return -1;
	}

	okToSendStart_ = false;

	// signal source thread to start capturing
	if ( !SetEvent(eventStart_) )
	{
		log_error("failed to signal source to start (%d)\n",
				GetLastError());
		return -1;
	}

	return 0;
}

void
SourceStateMachine::doStart()
{
	if ( src_->start() )
	{
		log_error("failed to run filter graph for source '%s'\n",
				sourceContext_->src_info.description);

		allowCallbacks_ = false;
		okToSendStart_ = true;

		// final capture callback - with error status
		sapi_src_capture_notify(sourceContext_, 0, 0, 0, -1);
	}
	else
	{
		allowCallbacks_ = true;
	}

}

// NOTE: This function will block until capture is 
//       completely stopped.
//       Even when returning failure, it guarantees
//       no more callbacks will occur
int
SourceStateMachine::stop()
{
	while ( !okToSendStop_ )
		Sleep(10);

	okToSendStop_ = false;

	captureStopped_ = false;

	// signal source thread to stop capturing 
	if ( !SetEvent(eventStop_) )
	{
		log_error("failed to signal source to stop (%d)\n",
				GetLastError());

		// Need to ensure no more callbacks arrive
		// Do this the hard way
		allowCallbacks_ = false;
		while ( callbackInProgress_ || callbackCancellationInProgress_ )
			Sleep(10);

		return -1;
	}

	// wait for source thread to indicate that capture has stopped
	// guaranteeing no more callbacks will occur after this returns
	while ( !captureStopped_ )
		Sleep(10);

	return 0;
}

void
SourceStateMachine::doStop()
{
	allowCallbacks_ = false;

	src_->stop();

	// make sure we're not in a callback
	while ( callbackInProgress_ )
		Sleep(10);

	// signal back to main thread that capture has stopped
	captureStopped_ = true;
}

int
SourceStateMachine::validateFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative) const
{
	return src_->validateFormat(fmtNominal, fmtNative);
}

int
SourceStateMachine::bindFormat(const vidcap_fmt_info * fmtNominal)
{
	return src_->bindFormat(fmtNominal);
}

void
SourceStateMachine::cancelCaptureCB(void *parent)
{
	SourceStateMachine *instance = static_cast<SourceStateMachine *>(parent);

	instance->cancelCapture();
}

void
SourceStateMachine::cancelCapture()
{
	// signal source thread to cancel capturing
	if ( !SetEvent(eventCancel_) )
	{
		log_error("failed to signal source to cancel (%d)\n",
				GetLastError());

		// Need to ensure app is notified
		// Do it the hard way

		// prevent future callbacks
		allowCallbacks_ = false;

		// wait for current callback to finish
		while ( callbackInProgress_ )
			Sleep(10);

		// final capture callback - with error status
		sapi_src_capture_notify(sourceContext_, 0, 0, 0, -1);
	}
}

void
SourceStateMachine::doCancelCapture()
{
	callbackCancellationInProgress_ = true;

	// has capture been stopped already?
	if ( !allowCallbacks_ )
	{
		callbackCancellationInProgress_ = false;
		return;
	}

	// prevent future callbacks
	allowCallbacks_ = false;

	// block until current callback is not in progress
	while ( callbackInProgress_ )
		Sleep(10);

	// stop callbacks before sending final callback
	src_->stop();

	// final capture callback - with error status
	sapi_src_capture_notify(sourceContext_, 0, 0, 0, -1);

	callbackCancellationInProgress_ = false;
}

// The sample grabber calls this from its deliver thread
// NOTE: This function must not block, else it will cause a
//       graph's Stop() to block - which could result in a deadlock
int
SourceStateMachine::bufferCB( double dblSampleTime, BYTE * pBuff, long buffSize, void *context )
{
	SourceStateMachine *instance = static_cast<SourceStateMachine *>(context);

	instance->callbackInProgress_ = true;

	if ( !instance->allowCallbacks_ )
	{
		instance->callbackInProgress_ = false;
		return 0;
	}

	int ret = sapi_src_capture_notify(instance->sourceContext_,
			reinterpret_cast<char *>(pBuff),
			static_cast<int>(buffSize), 0, 0);

	instance->callbackInProgress_ = false;

	return ret;
}

