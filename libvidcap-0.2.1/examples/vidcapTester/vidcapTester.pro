TEMPLATE = app
CONFIG += qt console thread
QT = core gui network

unix:!macx:{
	platform = linux
	LIBS += -lrt
}

macx:{
	platform = macosx
	DEFINES += MACOSX
	CONFIG -= app_bundle
	CONFIG += x86 ppc
	CONFIG += release
}

unix:{
	INCLUDEPATH += ../../include
	CONFIG(special_config): {
		local_lib_path = ../../../local-$$platform/lib
	}
	else {
		local_lib_path = ../../src/.libs
	}
	LIBS += $$local_lib_path/libvidcap.a
}

win32: {
	platform = win32
	CONFIG += embed_manifest_exe

	DEFINES += WIN32
	DEFINES += NTDDI_VERSION=NTDDI_WIN2KSP4
	DEFINES += _WIN32_WINNT=0x0500
	DEFINES += WINVER=0x0500

	CONFIG(release, debug|release): {
		config_name = "Release"
	}
	else {
		config_name = "Debug"
	}

	INCLUDEPATH += ../../include

	LIBS += -L\"$(PSDK_DIR)/Lib\"
	CONFIG(special_config): {
		LIBS += -L../../../build-win32/libvidcap/$$config_name
	}
	else {
		LIBS += -L../../contrib/win/vs2005/$$config_name/libvidcap
	}

	LIBS += strmiids.lib
	LIBS += comsuppw.lib
	LIBS += libvidcap.lib
}

macx: {
	LIBS += -framework Carbon
	LIBS += -framework QuartzCore
	LIBS += -framework QuickTime
}

RESOURCES = resources.qrc

HEADERS +=  \
	   Grabber.h \
	   GrabberManager.h \
	   MainWindow.h \
	   SigHandler.h \
	   VideoWidget.h \

SOURCES += \
	   Grabber.cpp \
	   GrabberManager.cpp \
	   MainWindow.cpp \
	   SigHandler.cpp \
	   VideoWidget.cpp \
	   vidcapTester.cpp \

unix:{
	CONFIG(special_config): {
		build_dir = ../../../build-$$platform/$$TARGET
	}
	else {
		build_dir = ../../build/$$TARGET
	}

	DESTDIR = $$build_dir
	MOC_DIR = $$build_dir
	OBJECTS_DIR = $$build_dir
	RCC_DIR = $$build_dir
	UI_DIR = $$build_dir
}
