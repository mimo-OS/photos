/*
 * Copyright (C) 2026 CutefishOS Team.
 *
 * Cutefish Photos Application Entry Point
 */

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTranslator>
#include <QLocale>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDir>
#include <QCommandLineParser>
#include <QPixmapCache>
#include <QTimer>
#ifdef __GLIBC__
#include <malloc.h>
#endif

#include "photosapp.h"
#include "photomodel.h"
#include "foldermodel.h"
#include "imageprovider.h"

int main(int argc, char *argv[])
{
#ifdef __GLIBC__
    mallopt(M_ARENA_MAX, 2);
    mallopt(M_TRIM_THRESHOLD, 128 * 1024);
    mallopt(M_MMAP_THRESHOLD, 64 * 1024);
#endif

    // Dynamic EGL vendor selection to prevent NVIDIA driver bloat on Intel/AMD GPUs
    if (!QFile::exists(QStringLiteral("/dev/nvidia0")) &&
        QFile::exists(QStringLiteral("/usr/share/glvnd/egl_vendor.d/50_mesa.json"))) {
        qputenv("__EGL_VENDOR_LIBRARY_FILENAMES", "/usr/share/glvnd/egl_vendor.d/50_mesa.json");
    }

    qputenv("QSG_RENDER_LOOP", "basic");

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

    QApplication app(argc, argv);
    QPixmapCache::setCacheLimit(4096);

    // Trim freed memory after startup and periodically
    QTimer::singleShot(2000, []() {
#ifdef __GLIBC__
        malloc_trim(0);
#endif
    });

    QTimer *trimTimer = new QTimer(&app);
    trimTimer->setInterval(45000);
    QObject::connect(trimTimer, &QTimer::timeout, []() {
#ifdef __GLIBC__
        malloc_trim(0);
#endif
    });
    trimTimer->start();
    app.setApplicationName("cutefish-photos");
    app.setApplicationDisplayName("Photos");
    app.setOrganizationName("CutefishOS");
    app.setOrganizationDomain("cutefishos.com");

    QIcon appIcon = QIcon::fromTheme("cutefish-photos", QIcon(":/images/cutefish-photos.svg"));
    app.setWindowIcon(appIcon);

    // Parse command line arguments (e.g. image file to open)
    QCommandLineParser parser;
    parser.setApplicationDescription("mimo OS Photos and Image Viewer");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "Image file to view");
    parser.process(app);

    QString initialImagePath;
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        QString arg = args.first();
        if (arg.startsWith("file://")) {
            arg = QUrl(arg).toLocalFile();
        }
        QFileInfo fi(arg);
        if (fi.exists() && fi.isFile()) {
            initialImagePath = fi.absoluteFilePath();
        }
    }

    PhotosApp *photosApp = new PhotosApp(&app);

    qmlRegisterType<PhotoModel>("Cutefish.Photos", 1, 0, "PhotoModel");
    qmlRegisterType<FolderModel>("Cutefish.Photos", 1, 0, "FolderModel");
    qmlRegisterSingletonInstance("Cutefish.Photos", 1, 0, "PhotosApp", photosApp);

    QQmlApplicationEngine engine;

    engine.addImageProvider(QStringLiteral("photothumb"), new ThumbnailProvider());

    // Translations
    QLocale locale;
    QString qmFilePath = QString("/usr/share/cutefish-photos/translations/%1.qm").arg(locale.name());
    if (QFile::exists(qmFilePath)) {
        QTranslator *translator = new QTranslator(&app);
        if (translator->load(qmFilePath)) {
            QApplication::installTranslator(translator);
        } else {
            translator->deleteLater();
        }
    }

    engine.rootContext()->setContextProperty("initialImagePath", initialImagePath);

    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
