#include "MainWindow.h"

MainWindow::MainWindow(const QString & title, int width, int height)
	: QWidget(0, 0)
{
	setWindowTitle(title);

	mainLayout = new QVBoxLayout(this);
	mainLayout->setMargin(0);

	videoWidget = new VideoWidget(this, width, height);
	mainLayout->addWidget(videoWidget);
}

MainWindow::~MainWindow()
{
	delete videoWidget;
	delete mainLayout;
}

void MainWindow::displayVideo(int width, int height, QByteArray data)
{
	videoWidget->displayRGB32Video(width, height, data);
}

