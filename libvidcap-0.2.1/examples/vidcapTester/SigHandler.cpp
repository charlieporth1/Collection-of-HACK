#include "SigHandler.h"

bool SigHandler::signalWasSent = false;

SigHandler::SigHandler()
{
	signal(SIGINT, this->mySigHandler);

	// Check for signal periodically
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(checkForSignal()));
	timer->start(500);
}

SigHandler::~SigHandler()
{
}

void
SigHandler::checkForSignal()
{
	if ( signalWasSent )
	{
#ifdef WIN32
		signal(SIGINT, this->mySigHandler);
#endif
		emit ctrlCPressed();
	}
}

