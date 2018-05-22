#include <QImage>
#include <QPainter>
#include <QSizePolicy>
#include "VideoWidget.h"

VideoWidget::VideoWidget(QWidget * parent, int width, int height)
	: QWidget(parent),
	  image(320, 240, QImage::Format_RGB32),
	  pref_width(width),
	  pref_height(height),
	  start_time(),
	  frame_count(0)
{
	setAttribute(Qt::WA_OpaquePaintEvent);
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

	setMinimumSize(32, 24);
	setMaximumSize(1280, 1024);

	bg_color = QColor( 193, 196, 205 );
	image.fill(bg_color.rgb());

	update();
}

VideoWidget::~VideoWidget()
{
}

int VideoWidget::displayRGB32Video(int width, int height, QByteArray data)
{
	if ( !(frame_count++) )
		start_time.start();

	// Create a local copy of the video data
	rgb32buffer = data;

	image = QImage(reinterpret_cast<const uchar *>(rgb32buffer.constData()),
			width, height, QImage::Format_RGB32);

	update();

	return 0;
}

void VideoWidget::paintEvent(QPaintEvent *)
{
	int rx, ry;
	QRect r;

	QPainter painter(this);
	QRect big_r = rect();

	// Calculate the maximum 4:3 aspect ratio rectangle inside big_r
	QSize s = image.size();
	s.scale(big_r.width(), big_r.height(), Qt::KeepAspectRatio);
	rx = (big_r.width() - s.width()) / 2;
	ry = (big_r.height() - s.height()) / 2;
	r = QRect(rx, ry, s.width(), s.height());

	// Draw two background colored bars to fill the extra space
	if ( big_r.width() > r.width() )
	{
		// Drawing two vertical bars to the left
		// and to the right of the main view area
		painter.fillRect(0, 0, rx, big_r.height(), QBrush(bg_color));
		painter.fillRect(r.right(),
				0,
				big_r.width() - r.width() - rx + 1,
				big_r.height(),
				QBrush(bg_color));
	}
	if ( big_r.height() > r.height() )
	{
		// Drawing two horizontal bars on top
		// and on the bottom of the main view area
		painter.fillRect(0, 0, big_r.width(), ry, QBrush(bg_color));
		painter.fillRect(0,
				r.bottom(),
				big_r.width(),
				big_r.height() - r.height() - ry,
				QBrush(bg_color));
	}

	painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

	painter.drawImage( r, image );

	int msec = start_time.elapsed();

	if ( msec > 3000 )
	{
		QString rate = QString("%1")
			.arg(static_cast<double>(frame_count * 1000)/
					static_cast<double>(msec),
					0, 'f', 2);
		painter.setPen(bg_color.dark());
		QRect text_rect(5, 5, 100, 20);
		painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignTop, rate, &big_r);
	}
}

QSize VideoWidget::sizeHint() const
{
	return QSize(pref_width, pref_height);
}

