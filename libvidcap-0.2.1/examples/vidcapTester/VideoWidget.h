#ifndef __VIDEOWIDGET_H__
#define __VIDEOWIDGET_H__

#include <QWidget>
#include <QImage>
#include <QTime>

class VideoWidget : public QWidget
{
	Q_OBJECT

public:
	VideoWidget(QWidget * parent, int width, int height);
	virtual ~VideoWidget();

	virtual QSize sizeHint() const;

public slots:
	int displayRGB32Video(int width, int height, QByteArray data);

protected:
	virtual void paintEvent(QPaintEvent *event);

private:
	QColor bg_color;

	QImage image;
	QByteArray rgb32buffer;

	int pref_width;
	int pref_height;

	QTime start_time;
	int frame_count;
};

#endif
