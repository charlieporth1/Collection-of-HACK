#include <stdexcept>
#include <vidcap/converters.h>

#include <QDebug>

#include "Grabber.h"

Grabber::Grabber(QObject * parent,
		vidcap_sapi * sapi,
		vidcap_src_info * src_info,
		int width,
		int height,
		int fourcc,
		int framerate)
	: QObject(parent),
	  width_(width),
	  height_(height),
	  fourcc_(fourcc),
	  src_(vidcap_src_acquire(sapi, src_info)),
	  srcId_(src_info->identifier),
	  srcDesc_(src_info->description)
{
	if ( !src_ )
		throw std::runtime_error("failed vidcap_src_acquire()");

	struct vidcap_fmt_info fmt_info;

	fmt_info.width  = width_;
	fmt_info.height = height_;
	fmt_info.fourcc = fourcc_;
	fmt_info.fps_numerator = framerate;
	fmt_info.fps_denominator = 1;

	// if no fourcc passed in, take anything that will work
	if ( !fmt_info.fourcc )
	{
		const int fourccs[] =
		{
			VIDCAP_FOURCC_RGB32,
			VIDCAP_FOURCC_I420,
			VIDCAP_FOURCC_YUY2,
			0
		};

		bool bound = false;
		for ( int i = 0; fourccs[i]; i++ )
		{
			fmt_info.fourcc = fourccs[i];
			if ( !vidcap_format_bind(src_, &fmt_info) )
			{
				bound = true;
				break;
			}
		}

		if ( !bound )
			throw std::runtime_error("failed this vidcap_format_bind()");

		fourcc_ = fmt_info.fourcc;
	}
	else if ( vidcap_format_bind(src_, &fmt_info) )
		throw std::runtime_error("failed to bind");

	if ( vidcap_format_info_get(src_, &fmt_info) )
		throw std::runtime_error("failed vidcap_format_info_get()");

	qDebug() << " bind fmt:"
		<< QString("%2x%3 %4 %5 %6/%7")
		.arg(fmt_info.width, 3)
		.arg(fmt_info.height, 3)
		.arg(vidcap_fourcc_string_get(fmt_info.fourcc))
		.arg(static_cast<double>(fmt_info.fps_numerator) /
		     static_cast<double>(fmt_info.fps_denominator),
		     0, 'f', 2)
		.arg(fmt_info.fps_numerator)
		.arg(fmt_info.fps_denominator);
}

Grabber::~Grabber()
{
	vidcap_src_capture_stop(src_);

	if ( vidcap_src_release(src_) )
		qCritical() << "failed vidcap_src_release()";

	qDebug() << "destroyed grabber for source" << srcDesc_ << "-" << srcId_;
}

void
Grabber::capture()
{
	if ( vidcap_src_capture_start(src_, Grabber::captureCallbackStatic,
				this) )
		throw std::runtime_error("failed vidcap_src_capture_start()");
}

int
Grabber::captureCallbackStatic(vidcap_src * src,
		void * user_context,
		vidcap_capture_info * cap_info)
{
	return reinterpret_cast<Grabber *>(user_context)->captureCallback(src,
			cap_info);
}

int
Grabber::captureCallback(vidcap_src *, vidcap_capture_info * cap_info)
{
	if ( cap_info->error_status )
	{
		qDebug() << "Received final capture callback (emitting"
			<< "captureAborted())";

		emit captureAborted(this);
		return 0;
	}

	if ( !cap_info->video_data_size )
	{
		qWarning() << "capture callback gave no data to present";
		return 0;
	}

	QByteArray rgb_buf;

	switch ( fourcc_ )
	{
	case VIDCAP_FOURCC_RGB32:
		rgb_buf = QByteArray(cap_info->video_data,
				cap_info->video_data_size);
		break;

	case VIDCAP_FOURCC_I420:
		rgb_buf.resize(width_ * height_ * 4);

		vidcap_i420_to_rgb32(width_, height_, cap_info->video_data,
				rgb_buf.data());
		break;

	case VIDCAP_FOURCC_YUY2:
		rgb_buf.resize(width_ * height_ * 4);
		vidcap_yuy2_to_rgb32(width_, height_, cap_info->video_data,
				rgb_buf.data());

		break;

	default:
		qCritical() << "invalid fourcc"
			<< QString("0x%1").arg(fourcc_, 0, 16);
		return -1;
	}

	emit videoFrame(width_, height_, rgb_buf);

	return 0;
}
