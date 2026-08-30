// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "utils/waylandwindowpositioner.h"

#include <QApplication>
#include <QScreen>
#include <QTimer>
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
    require(positionWaylandWindow(&window, requested),
            "LayerShellQt could not configure the live test window");
    window.show();

    const int duration = qEnvironmentVariableIntValue(
      "FLAMESHOT_TEST_DURATION_MS");
    QTimer::singleShot(duration > 0 ? duration : 750, &app, [&]() {
        require(window.geometry() == requested,
                "Wayland compositor changed the requested pin geometry");
        qInfo() << "live Wayland placement configured at" << requested
                << "reported geometry" << window.geometry();
        app.quit();
    });
    return app.exec();
}
