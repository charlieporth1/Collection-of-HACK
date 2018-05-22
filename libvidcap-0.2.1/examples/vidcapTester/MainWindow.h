#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include <QWidget>
#include <QVBoxLayout>
#include "VideoWidget.h"

class MainWindow : public QWidget
{
	Q_OBJECT

public:
	MainWindow(const QString & title, int width, int height);
	~MainWindow();

public slots:

	void displayVideo(int width, int height, QByteArray data);

private:

	VideoWidget * videoWidget;
	QVBoxLayout * mainLayout;
};

#endif
