// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "waylandwindowpositioner.h"

#include <QDebug>
#include <QGuiApplication>
#include <QLibrary>
#include <QObject>
#include <QScreen>
#include <QWidget>
#include <QWindow>

namespace {

constexpr int LAYER_SHELL_ANCHOR_TOP = 1;
constexpr int LAYER_SHELL_ANCHOR_LEFT = 4;
constexpr int LAYER_SHELL_LAYER_TOP = 2;
constexpr int LAYER_SHELL_KEYBOARD_ON_DEMAND = 2;

using GetLayerShellWindow = QObject* (*)(QWindow*);
using SetLayerShellInt = void (*)(QObject*, int);
using SetLayerShellMargins = void (*)(QObject*, const QMargins&);
using SetLayerShellSize = void (*)(QObject*, const QSize&);

struct LayerShellApi
{
    GetLayerShellWindow getWindow{ nullptr };
    SetLayerShellInt setAnchors{ nullptr };
    SetLayerShellMargins setMargins{ nullptr };
    SetLayerShellSize setDesiredSize{ nullptr };
    SetLayerShellInt setExclusiveZone{ nullptr };
    SetLayerShellInt setLayer{ nullptr };
    SetLayerShellInt setKeyboardInteractivity{ nullptr };

    bool available() const
    {
        return getWindow && setAnchors && setMargins && setDesiredSize &&
               setExclusiveZone && setLayer && setKeyboardInteractivity;
    }
};

LayerShellApi& layerShellApi()
{
    static LayerShellApi api;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    static QLibrary library;
    static bool resolved = false;
    if (resolved) {
        return api;
    }
    resolved = true;

    library.setFileNameAndVersion(QStringLiteral("LayerShellQtInterface"),
                                  QT_VERSION_MAJOR);
    if (!library.load()) {
        return api;
    }

    // LayerShellQt does not expose a C ABI. Resolve its small Window API at
    // runtime so Flameshot remains usable when the optional library is absent.
    api.getWindow = reinterpret_cast<GetLayerShellWindow>(
      library.resolve("_ZN12LayerShellQt6Window3getEP7QWindow"));
    api.setAnchors = reinterpret_cast<SetLayerShellInt>(library.resolve(
      "_ZN12LayerShellQt6Window10setAnchorsE6QFlagsINS0_6AnchorEE"));
    api.setMargins = reinterpret_cast<SetLayerShellMargins>(library.resolve(
      "_ZN12LayerShellQt6Window10setMarginsERK8QMargins"));
    api.setDesiredSize = reinterpret_cast<SetLayerShellSize>(library.resolve(
      "_ZN12LayerShellQt6Window14setDesiredSizeERK5QSize"));
    api.setExclusiveZone = reinterpret_cast<SetLayerShellInt>(library.resolve(
      "_ZN12LayerShellQt6Window16setExclusiveZoneEi"));
    api.setLayer = reinterpret_cast<SetLayerShellInt>(library.resolve(
      "_ZN12LayerShellQt6Window8setLayerENS0_5LayerE"));
    api.setKeyboardInteractivity =
      reinterpret_cast<SetLayerShellInt>(library.resolve(
        "_ZN12LayerShellQt6Window24setKeyboardInteractivityENS0_21KeyboardInteractivityE"));
#endif
    return api;
}

}

AnchoredWindowPlacement anchoredWindowPlacement(
  const QRect& globalWindowGeometry,
  const QRect& screenGeometry)
{
    return { QMargins(globalWindowGeometry.left() - screenGeometry.left(),
                      globalWindowGeometry.top() - screenGeometry.top(),
                      0,
                      0),
             globalWindowGeometry.size() };
}

bool positionWaylandWindow(QWidget* widget,
                           const QRect& globalWindowGeometry)
{
    if (!widget || globalWindowGeometry.isEmpty() ||
        QGuiApplication::platformName() != QLatin1String("wayland")) {
        return false;
    }

    LayerShellApi& api = layerShellApi();
    if (!api.available()) {
        qWarning() << QObject::tr(
          "LayerShellQt is unavailable; Wayland may place the pinned window "
          "automatically");
        return false;
    }

    QScreen* screen = QGuiApplication::screenAt(globalWindowGeometry.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return false;
    }

    widget->setGeometry(globalWindowGeometry);
    widget->winId();
    QWindow* nativeWindow = widget->windowHandle();
    if (!nativeWindow) {
        return false;
    }
    nativeWindow->setScreen(screen);

    QObject* layerWindow = api.getWindow(nativeWindow);
    if (!layerWindow) {
        return false;
    }

    const AnchoredWindowPlacement placement =
      anchoredWindowPlacement(globalWindowGeometry, screen->geometry());
    api.setAnchors(layerWindow,
                   LAYER_SHELL_ANCHOR_TOP | LAYER_SHELL_ANCHOR_LEFT);
    api.setMargins(layerWindow, placement.margins);
    api.setDesiredSize(layerWindow, placement.size);
    api.setExclusiveZone(layerWindow, -1);
    api.setLayer(layerWindow, LAYER_SHELL_LAYER_TOP);
    api.setKeyboardInteractivity(layerWindow,
                                 LAYER_SHELL_KEYBOARD_ON_DEMAND);
    return true;
}
