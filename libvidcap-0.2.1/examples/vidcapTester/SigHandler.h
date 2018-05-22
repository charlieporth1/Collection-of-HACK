#ifndef __SIGHANDLER_H__
#define __SIGHANDLER_H__

#include <QObject>
#include <QTimer>
#include <signal.h>

class SigHandler : public QObject
{
	Q_OBJECT

public:
	SigHandler();
	~SigHandler();

static void
mySigHandler(int)
{
	signalWasSent = true;
}

private:
	static bool signalWasSent;
	QTimer *timer;

private slots:
	void checkForSignal();

signals:
	void ctrlCPressed();
};


#endif //__SIGHANDLER_H__

