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

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QRegularExpression>
#endif

namespace {

constexpr int LAYER_SHELL_ANCHOR_TOP = 1;
constexpr int LAYER_SHELL_ANCHOR_LEFT = 4;
constexpr int LAYER_SHELL_LAYER_TOP = 2;
constexpr int LAYER_SHELL_KEYBOARD_ON_DEMAND = 2;

using GetLayerShellWindow = QObject* (*)(QWindow*);
using SetLayerShellInt = void (*)(QObject*, int);
using SetLayerShellMargins = void (*)(QObject*, const QMargins&);
using SetLayerShellSize = void (*)(QObject*, const QSize&);
using GetPlasmaShellWindow = QObject* (*)(QWindow*);
using SetPlasmaShellPosition = void (*)(QObject*, const QPoint&);

struct PlasmaShellApi
{
    GetPlasmaShellWindow getWindow{ nullptr };
    SetPlasmaShellPosition setPosition{ nullptr };

    bool available() const { return getWindow && setPosition; }
};

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

PlasmaShellApi& plasmaShellApi()
{
    static PlasmaShellApi api;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    static QLibrary library;
    static bool resolved = false;
    if (resolved) {
        return api;
    }
    resolved = true;

    // Plasma 6 currently exposes this helper from libPlasmaQuick.so.7.
    // Keep it runtime-only so non-Plasma desktops need no KDE dependency.
    library.setFileNameAndVersion(QStringLiteral("PlasmaQuick"), 7);
    if (!library.load()) {
        return api;
    }
    api.getWindow = reinterpret_cast<GetPlasmaShellWindow>(
      library.resolve("_ZN29PlasmaShellWaylandIntegration3getEP7QWindow"));
    api.setPosition = reinterpret_cast<SetPlasmaShellPosition>(library.resolve(
      "_ZN29PlasmaShellWaylandIntegration11setPositionERK6QPoint"));
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

WaylandWindowPositioning positionWaylandWindow(
  QWidget* widget,
  const QRect& globalWindowGeometry)
{
    if (!widget || globalWindowGeometry.isEmpty() ||
        QGuiApplication::platformName() != QLatin1String("wayland")) {
        return WaylandWindowPositioning::Unavailable;
    }

    QScreen* screen = QGuiApplication::screenAt(globalWindowGeometry.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return WaylandWindowPositioning::Unavailable;
    }

    widget->setGeometry(globalWindowGeometry);
    widget->winId();
    QWindow* nativeWindow = widget->windowHandle();
    if (!nativeWindow) {
        return WaylandWindowPositioning::Unavailable;
    }
    nativeWindow->setScreen(screen);

    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    PlasmaShellApi& plasmaApi = plasmaShellApi();
    if (desktop.contains(QLatin1String("KDE"), Qt::CaseInsensitive) &&
        plasmaApi.available()) {
        QObject* plasmaWindow = plasmaApi.getWindow(nativeWindow);
        if (plasmaWindow) {
            plasmaApi.setPosition(plasmaWindow,
                                  globalWindowGeometry.topLeft());
            return WaylandWindowPositioning::PlasmaShell;
        }
    }

    LayerShellApi& api = layerShellApi();
    if (!api.available()) {
        qWarning() << QObject::tr(
          "No supported Wayland window-positioning protocol is available");
        return WaylandWindowPositioning::Unavailable;
    }

    QObject* layerWindow = api.getWindow(nativeWindow);
    if (!layerWindow) {
        return WaylandWindowPositioning::Unavailable;
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
    return WaylandWindowPositioning::LayerShell;
}

std::optional<QPoint> kdeWindowTopLeft(const QString& windowTitle)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (windowTitle.isEmpty() ||
        QGuiApplication::platformName() != QLatin1String("wayland") ||
        !qEnvironmentVariable("XDG_CURRENT_DESKTOP")
           .contains(QLatin1String("KDE"), Qt::CaseInsensitive)) {
        return std::nullopt;
    }

    QDBusMessage match = QDBusMessage::createMethodCall(
      QStringLiteral("org.kde.KWin"),
      QStringLiteral("/WindowsRunner"),
      QStringLiteral("org.kde.krunner1"),
      QStringLiteral("Match"));
    match << windowTitle;
    const QDBusMessage matchReply =
      QDBusConnection::sessionBus().call(match, QDBus::Block, 1000);
    if (matchReply.type() != QDBusMessage::ReplyMessage ||
        matchReply.arguments().isEmpty()) {
        return std::nullopt;
    }

    QString windowUuid;
    const QDBusArgument matches =
      matchReply.arguments().constFirst().value<QDBusArgument>();
    matches.beginArray();
    while (!matches.atEnd()) {
        QString id;
        QString text;
        QString subtext;
        int category = 0;
        double relevance = 0.0;
        QVariantMap properties;
        matches.beginStructure();
        matches >> id >> text >> subtext >> category >> relevance >> properties;
        matches.endStructure();
        if (text == windowTitle) {
            const QRegularExpressionMatch uuidMatch =
              QRegularExpression(QStringLiteral("\\{([^}]+)\\}"))
                .match(id);
            if (uuidMatch.hasMatch()) {
                windowUuid = uuidMatch.captured(1);
                break;
            }
        }
    }
    matches.endArray();
    if (windowUuid.isEmpty()) {
        return std::nullopt;
    }

    QDBusMessage info = QDBusMessage::createMethodCall(
      QStringLiteral("org.kde.KWin"),
      QStringLiteral("/KWin"),
      QStringLiteral("org.kde.KWin"),
      QStringLiteral("getWindowInfo"));
    info << windowUuid;
    const QDBusMessage infoReply =
      QDBusConnection::sessionBus().call(info, QDBus::Block, 1000);
    if (infoReply.type() != QDBusMessage::ReplyMessage ||
        infoReply.arguments().isEmpty()) {
        return std::nullopt;
    }
    const QVariantMap windowInfo = qdbus_cast<QVariantMap>(
      infoReply.arguments().constFirst());
    if (!windowInfo.contains(QStringLiteral("x")) ||
        !windowInfo.contains(QStringLiteral("y"))) {
        return std::nullopt;
    }
    return QPoint(qRound(windowInfo.value(QStringLiteral("x")).toDouble()),
                  qRound(windowInfo.value(QStringLiteral("y")).toDouble()));
#else
    Q_UNUSED(windowTitle)
    return std::nullopt;
#endif
}
