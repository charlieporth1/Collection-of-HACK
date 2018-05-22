#include <stdexcept>

#include <QApplication>
#include <QDebug>
#include <QIcon>

#include "GrabberManager.h"
#include "SigHandler.h"

static void message_handler(QtMsgType msg_type, const char * msg)
{
	QString level_string;

	switch ( msg_type )
	{
	case QtDebugMsg:
		level_string = "debug";
		break;
	case QtWarningMsg:
		level_string = "warning";
		break;
	case QtCriticalMsg:
		level_string = "error";
		break;
	case QtFatalMsg:
		level_string = "fatal";
		std::abort();
	default:
		level_string = "unknown";
		break;
	}

	fprintf(stderr, "vidcapTester: %s: %s\n",
			level_string.toAscii().constData(), msg);
}

static void usage()
{
	qDebug() << "usage: vidcapTester " <<
		"[-f FRAMESPERSECOND] " <<
		"[-w WIDTH] [-h HEIGHT]";
}

static void parse_args(int argc, char * argv[], int & width, int & height,
		int & framerate)
{
	for ( int i = 1 ; i < argc ; i++ )
	{
		bool okay;

		QString option(argv[i]);
		QString option_arg;

		if ( i + 1 < argc )
			option_arg = argv[i+1];

		if ( option == "-w" )
		{
			if ( ++i >= argc )
				throw std::runtime_error("argument expected");

			width = option_arg.toInt(&okay, 0);

			if ( !okay )
				throw std::runtime_error("bogus argument");
		}
		else if ( option == "-h" )
		{
			if ( ++i >= argc )
				throw std::runtime_error("argument expected");

			height = option_arg.toInt(&okay, 0);

			if ( !okay )
				throw std::runtime_error("bogus argument");
		}
		else if ( option == "-f" )
		{
			if ( ++i >= argc )
				throw std::runtime_error("argument expected");

			framerate = option_arg.toInt(&okay, 0);

			if ( !okay )
				throw std::runtime_error("bogus argument");
		}
		else
		{
			throw std::runtime_error(QString(
					"Unknown command line parameter: %1")
					.arg(argv[i]).toAscii().constData());
		}
	}
}

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	qInstallMsgHandler(message_handler);

	int width = 320;
	int height = 240;
	int framerate = 30;

	try
	{
		parse_args(argc, argv, width, height, framerate);
	}
	catch ( const std::runtime_error & e )
	{
		qCritical(e.what());
		usage();
		return 1;
	}

	app.setWindowIcon(QIcon(":/app_icon.png"));

	app.setQuitOnLastWindowClosed(false);

	try
	{
		GrabberManager grabMgr(&app, "vidcap", width, height, framerate);
		SigHandler sigH;

		QObject::connect(&sigH, SIGNAL(ctrlCPressed()),
				&grabMgr, SLOT(shutdown()),
				Qt::AutoConnection);

		app.exec();
	}
	catch ( const std::runtime_error & e)
	{
		qDebug("failed constructing GrabberManager '%s'", e.what());
		return 1;
	}

	return 0;
}
