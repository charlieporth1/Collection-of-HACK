#include <stdexcept>

#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QMutexLocker>

#include "GrabberManager.h"

GrabberManager::GrabberManager(QObject * parent, const QString & title,
		int width, int height, int framerate)
	: QObject(parent),
	  title_(title),
	  width_(width),
	  height_(height),
	  framerate_(framerate),
	  vc_(vidcap_initialize()),
	  sapi_(0),
	  ctxList_(0),
	  srcList_(0),
	  srcListLen_(0)
{
	qDebug() << "Starting grabber for each capture device...";

	if ( !vc_ )
		throw std::runtime_error("failed vidcap_initialize()");

	if ( !(sapi_ = vidcap_sapi_acquire(vc_, 0)) )
		throw std::runtime_error("failed to acquire default sapi");

	if ( vidcap_sapi_info_get(sapi_, &sapiInfo_) )
		throw std::runtime_error("failed to get default sapi info");

	qDebug() << "sapi:" << sapiInfo_.identifier
		<< "|" << sapiInfo_.description;

	if ( vidcap_srcs_notify(sapi_, &GrabberManager::userNotificationCallback, this) )
		throw std::runtime_error("failed vidcap_srcs_notify()");

	srcListLen_ = vidcap_src_list_update(sapi_);

	if ( srcListLen_ < 0 )
		throw std::runtime_error("failed vidcap_src_list_update()");

	srcList_ = new vidcap_src_info[srcListLen_];

	if ( vidcap_src_list_get(sapi_, srcListLen_, srcList_) )
		throw std::runtime_error("failed vidcap_srcList_get()");

	ctxList_ = new mySourceContext[srcListLen_];

	// Rescan on notification
	// NOTE: connecting before mgr construction is complete
	connect(this, SIGNAL(deviceNotification()),
			this, SLOT(rescanDevices()));

	// create a grabber for each video capture device
	for ( int i = 0; i < srcListLen_; ++i )
	{
		qDebug() << "src:" << srcList_[i].identifier
			<< "|" << srcList_[i].description;

		createContext(&ctxList_[i], &srcList_[i]);
	}
}

GrabberManager::~GrabberManager()
{
	qDebug() << "Destroying all grabbers...";

	{
		QMutexLocker locker(&sourceMutex_);

		for ( int i = srcListLen_ - 1; i >= 0; --i )
		{
			if ( ctxList_[i].grabber )
				delete ctxList_[i].grabber;

			if ( ctxList_[i].window )
				delete ctxList_[i].window;
		}

		delete [] ctxList_;
		delete [] srcList_;
	}

	if ( vidcap_sapi_release(sapi_) )
		qWarning() << "failed vidcap_sapi_release()";

	vidcap_destroy(vc_);
}

void
GrabberManager::killGrabber(Grabber *grabber)
{
	//  protect context list and srcListLen_ from concurrent access
	//       (from notification callback)
	QMutexLocker locker(&sourceMutex_);

	for ( int i = 0; i < srcListLen_; ++i )
	{
		if ( ctxList_[i].grabber == grabber )
		{
			if ( ctxList_[i].grabber )
				delete ctxList_[i].grabber;
			ctxList_[i].grabber = 0;

			if ( ctxList_[i].window )
				delete ctxList_[i].window;
			ctxList_[i].window = 0;
			break;
		}
	}
}

void
GrabberManager::shutdown()
{
	qDebug() << "grabber manager got signal to shutdown";

	// cause message loop to exit
	QApplication::quit();
}

int
GrabberManager::userNotificationCallback(vidcap_sapi *, void * user_context)
{
	GrabberManager *grabMgr = (GrabberManager *)user_context;

	// trigger main thread to handle this - rescan the devices
	emit grabMgr->deviceNotification();

	return 0;
}

void
GrabberManager::rescanDevices()
{
	vidcap_src_info * newSrcList;
	int newSrcListLen;

	// query for device list
	try
	{
		getFreshSrcList(newSrcList, newSrcListLen);
	}
	catch (std::runtime_error &e)
	{
		qCritical() << "exception while rescanning devices:" << e.what();
		return;
	}

	bool removal = false;
	bool addition = false;
	// compare each existing device to new list
	//    If it's not there, clean it up
	for ( int oldIdx = 0; oldIdx < srcListLen_; ++oldIdx )
	{
		bool found = false;
		for ( int newIdx = 0; newIdx < newSrcListLen; ++newIdx )
		{
			if ( !strcmp(srcList_[oldIdx].identifier,
				newSrcList[newIdx].identifier) )
			{
				found = true;
				break;
			}
		}

		if ( !found )
		{
			removal = true;
			qDebug() << "source removed:"
				<< srcList_[oldIdx].description;

			// NOTE: Source might not be capturing when
			// removed, so don't rely on capture callback
			// to clean up after a device removal.
			//
			// cleanup after this device removal
			killGrabber(ctxList_[oldIdx].grabber);
		}
	}

	mySourceContext * newCtxList = new mySourceContext[newSrcListLen];

	// Compare each device in new list to old list
	// If it's new, add it to the new context list
	for ( int newIdx = 0; newIdx < newSrcListLen; ++newIdx )
	{
		bool found = false;
		for ( int oldIdx = 0; oldIdx < srcListLen_; ++oldIdx )
		{
			if ( !strcmp(srcList_[oldIdx].identifier,
				newSrcList[newIdx].identifier) )
			{
				// Add unchanged device to new context list
				// Take from existing context list to new
				newCtxList[newIdx].grabber = ctxList_[oldIdx].grabber;
				newCtxList[newIdx].window = ctxList_[oldIdx].window;

				found = true;
				break;
			}
		}

		if ( !found )
		{
			addition = true;
			qDebug() << "source added:"
				<< newSrcList[newIdx].description;

			// create grabber and window (& record in ctxList)
			createContext(&newCtxList[newIdx], &newSrcList[newIdx]);
		}
	}

	// Don't grab mutex if not necessary
	if ( !removal && !addition )
	{
		delete [] newSrcList;
		delete [] newCtxList;
		return;
	}

	qDebug() << "New list of sources: there were" << srcListLen_
		<< "now there are" << newSrcListLen;

	QMutexLocker locker(&sourceMutex_);

	// replace old lists with new
	delete [] srcList_;
	delete [] ctxList_;
	srcList_ = newSrcList;
	ctxList_ = newCtxList;
	srcListLen_ = newSrcListLen;
}

// get a fresh list of devices
void
GrabberManager::getFreshSrcList(vidcap_src_info * &pNewSrcList, int & newSrcListLen)
{
	newSrcListLen = vidcap_src_list_update(sapi_);

	if ( newSrcListLen < 0 )
		throw std::runtime_error("failed vidcap_src_list_update()");
	else if ( newSrcListLen == 0 )
		qDebug() << "no sources available";

	pNewSrcList = new vidcap_src_info[newSrcListLen];

	if ( vidcap_src_list_get(sapi_, newSrcListLen, pNewSrcList) )
		throw std::runtime_error("failed vidcap_srcList_get()");
}

// used by constructor and notification callback
void
GrabberManager::createContext(mySourceContext *mySrcCtxt,
		vidcap_src_info *srcList)
{
	QMutexLocker locker(&sourceMutex_);

	mySrcCtxt->grabber = 0;
	mySrcCtxt->window = 0;

	try
	{
		mySrcCtxt->grabber = new Grabber(this, sapi_, srcList,
				width_, height_, 0, framerate_);
	}
	catch (std::runtime_error &)
	{
		qWarning() << "Could not find a suitable format for device "
			<< srcList->description << "-" << srcList->identifier;
		return;
	}

	// Cleanup if capture stopped due to error or device removal
	// NOTE: may be connecting before mgr construction is complete.
	connect(mySrcCtxt->grabber, SIGNAL(captureAborted(Grabber *)),
			this, SLOT(killGrabber(Grabber *)));

	try
	{
		mySrcCtxt->window = new MainWindow(QString("%1 - %2")
				.arg(title_).arg(srcList->description),
				width_, height_);
		mySrcCtxt->window->show();
	}
	catch (std::exception &)
	{
		qDebug() << "Could not create window for device '"
			<< srcList->description << "' ("
			<< srcList->identifier << ")";

		delete mySrcCtxt->grabber;
		mySrcCtxt->grabber = 0;

		return;
	}

	// Start capture AFTER window is created.
	// If capture fails, both grabber and window will be deleted.
	try
	{
		mySrcCtxt->grabber->capture();
	}
	catch ( std::runtime_error & e )
	{
		qCritical() << "failed to start capture for grabber ("
			<< e.what() << ")";

		delete mySrcCtxt->window;
		mySrcCtxt->window = 0;

		delete mySrcCtxt->grabber;
		mySrcCtxt->grabber = 0;

		return;
	}

	mySrcCtxt->window->show();

	// connect video directly to window, so that a blocked
	// main thread won't result in a pile-up of frames
	QObject::connect(mySrcCtxt->grabber,
				SIGNAL(videoFrame(int, int, QByteArray)),
				mySrcCtxt->window,
				SLOT(displayVideo(int, int, QByteArray)),
				Qt::DirectConnection);

	// Set the Application icon
	QIcon app_icon(":/app_icon.png");

	// Windows and Linux need to have the icon set for the
	// main window so it will appear in the top left corner
	// as well as in taskbar
	// Mac OSX windows do not usually have an icon in the title
	// bar, so we disable that
#ifdef MACOSX
	mySrcCtxt->window->setWindowIcon(QPixmap());
#else
	mySrcCtxt->window->setWindowIcon(app_icon);
#endif
}

