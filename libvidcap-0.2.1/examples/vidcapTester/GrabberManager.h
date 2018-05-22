#ifndef __GRABBER_MANAGER_H__
#define __GRABBER_MANAGER_H__

#include <QObject>
#include <QMutex>
#include <vidcap/vidcap.h>
#include "Grabber.h"
#include "MainWindow.h"

class GrabberManager : public QObject
{
	Q_OBJECT

public:
	GrabberManager(QObject * parent, const QString & title,
			int width, int height, int framerate);
	~GrabberManager();

signals:
	void deviceNotification();

public slots:
	void shutdown();

private slots:
	void killGrabber(Grabber *);
	void rescanDevices();

private:
	typedef struct mySourceContext
	{
		Grabber * grabber;
		MainWindow * window;
	};

	static int userNotificationCallback(vidcap_sapi *, void * user_data);
	void getFreshSrcList(vidcap_src_info * &pNewSrcList, int & newSrcListLen);
	void createContext(mySourceContext *, vidcap_src_info *);

private:
	const QString title_;
	const int width_;
	const int height_;
	const int framerate_;
	vidcap_state * vc_;
	vidcap_sapi * sapi_;

	struct vidcap_sapi_info sapiInfo_;

	// must protect these from concurrent access
	mySourceContext * ctxList_;
	vidcap_src_info * srcList_;
	int srcListLen_;

	QMutex sourceMutex_;
};

#endif
