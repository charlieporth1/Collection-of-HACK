#ifndef __GRABBER_H__
#define __GRABBER_H__

#include <QObject>
#include <vidcap/vidcap.h>

class Grabber : public QObject
{
	Q_OBJECT

public:
	Grabber(QObject * parent, vidcap_sapi *, vidcap_src_info *,
			int width, int height, int fourcc, int framerate);
	~Grabber();
	void capture();

private:
	static int captureCallbackStatic(vidcap_src *, void *,
			vidcap_capture_info *);

	int captureCallback(vidcap_src *, vidcap_capture_info *);

signals:
	void videoFrame(int width, int height, QByteArray rgb32Data);
	void captureAborted(Grabber *);

private:
	const int width_;
	const int height_;
	int fourcc_;

	vidcap_src * src_;
	const QString srcId_;
	const QString srcDesc_;

};

#endif

