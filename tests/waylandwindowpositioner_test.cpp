// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "utils/waylandwindowpositioner.h"

#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QUuid>
#include <QWidget>

#include <cstdio>
#include <cstdlib>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        qCritical() << message;
        std::fprintf(stderr, "%s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    const AnchoredWindowPlacement placement = anchoredWindowPlacement(
      QRect(320, 190, 240, 120), QRect(100, 50, 1920, 1080));
    require(placement.margins == QMargins(220, 140, 0, 0),
            "layer-shell margins do not preserve global coordinates");
    require(placement.size == QSize(240, 120),
            "layer-shell desired size does not preserve the pin size");

    const AnchoredWindowPlacement secondaryPlacement =
      anchoredWindowPlacement(QRect(-1700, 210, 300, 160),
                              QRect(-1920, 0, 1920, 1080));
    require(secondaryPlacement.margins == QMargins(220, 210, 0, 0),
            "layer-shell margins fail on a negative-origin monitor");

    if (!qEnvironmentVariableIsSet("FLAMESHOT_TEST_LIVE_WAYLAND")) {
        qInfo() << "Wayland placement geometry tests passed";
        return EXIT_SUCCESS;
    }

    require(QGuiApplication::platformName() == QLatin1String("wayland"),
            "live Wayland test is not running on the Wayland platform");
    QScreen* screen = QGuiApplication::primaryScreen();
    require(screen != nullptr, "live Wayland test has no screen");

    QWidget window;
    window.setWindowFlags(Qt::WindowStaysOnTopHint |
                          Qt::FramelessWindowHint | Qt::Tool);
    window.setStyleSheet(QStringLiteral("background: #e03030"));
    const QRect requested(screen->geometry().topLeft() + QPoint(80, 60),
                          QSize(240, 120));
    const WaylandWindowPositioning positioning =
      positionWaylandWindow(&window, requested);
    require(positioning == WaylandWindowPositioning::PlasmaShell,
            "KDE did not select the movable Plasma shell window path");
    window.show();

    const QRect moved = requested.translated(320, 180);
    const QRect editorGeometry = moved.adjusted(7, 7, -7, -7);
    QWidget editor;
    editor.setWindowFlags(Qt::WindowStaysOnTopHint |
                          Qt::FramelessWindowHint | Qt::Tool);
    editor.setWindowTitle(QStringLiteral("flameshot-position-test-%1").arg(
      QUuid::createUuid().toString(QUuid::WithoutBraces)));
    editor.setStyleSheet(QStringLiteral("background: #3060e0"));
    QTimer::singleShot(250, &app, [&]() {
        require(positionWaylandWindow(&window, moved) !=
                  WaylandWindowPositioning::Unavailable,
                "Wayland positioning could not move the live test window");
    });
    QTimer::singleShot(500, &app, [&]() {
        window.hide();
        require(positionWaylandWindow(&editor, editorGeometry) ==
                  WaylandWindowPositioning::PlasmaShell,
                "re-edit window did not use Plasma positioning");
        editor.show();
    });
    QTimer::singleShot(800, &app, [&]() {
        require(setKdeWindowKeepAbove(editor.windowTitle(), true),
                "KWin could not enable keep-above for the pinned window");
    });
    const int duration = qEnvironmentVariableIntValue(
      "FLAMESHOT_TEST_DURATION_MS");
    QTimer::singleShot(duration > 0 ? duration : 1200, &app, [&]() {
        require(editor.geometry() == editorGeometry,
                "re-edit window did not preserve the moved pin position");
        const std::optional<QPoint> kwinTopLeft =
          kdeWindowTopLeft(editor.windowTitle());
        require(kwinTopLeft && *kwinTopLeft == editorGeometry.topLeft(),
                "KWin did not report the positioned window coordinates");
        require(kdeWindowKeepAbove(editor.windowTitle()) == true,
                "KWin did not keep the pinned window above other windows");
        qInfo() << "live Wayland re-edit configured at" << editorGeometry
                << "reported geometry" << editor.geometry()
                << "KWin top-left" << *kwinTopLeft;
        app.quit();
    });
    return app.exec();
}
