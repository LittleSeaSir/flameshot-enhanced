// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "windowsnap.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>
#endif

namespace WindowSnap {

QRectF selectGeometryAt(const QPointF& globalPos,
                        const QRectF& screenGeometry,
                        const QVector<QRectF>& bottomToTopWindows)
{
    if (!screenGeometry.isValid()) {
        return {};
    }

    for (auto it = bottomToTopWindows.crbegin();
         it != bottomToTopWindows.crend();
         ++it) {
        if (!it->isValid() || !it->contains(globalPos)) {
            continue;
        }

        const QRectF visibleGeometry = it->intersected(screenGeometry);
        if (visibleGeometry.isValid()) {
            return visibleGeometry;
        }
    }

    return screenGeometry;
}

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
namespace {

constexpr auto KWIN_SERVICE = "org.kde.KWin";
constexpr auto KWIN_SCRIPTING_PATH = "/Scripting";
constexpr auto KWIN_SCRIPTING_INTERFACE = "org.kde.kwin.Scripting";
constexpr auto SNAP_INTERFACE = "org.flameshot.WindowSnap";
constexpr auto SNAP_METHOD = "Deliver";

class WindowStackReceiver final : public QDBusVirtualObject
{
public:
    QString introspect(const QString& path) const override
    {
        Q_UNUSED(path)
        return QStringLiteral(
          "<interface name=\"org.flameshot.WindowSnap\">"
          "<method name=\"Deliver\">"
          "<arg name=\"payload\" type=\"s\" direction=\"in\"/>"
          "</method>"
          "</interface>");
    }

    bool handleMessage(const QDBusMessage& message,
                       const QDBusConnection& connection) override
    {
        if (message.type() != QDBusMessage::MethodCallMessage ||
            message.interface() != QLatin1String(SNAP_INTERFACE) ||
            message.member() != QLatin1String(SNAP_METHOD) ||
            message.arguments().size() != 1 ||
            !message.arguments().constFirst().canConvert<QString>()) {
            return false;
        }

        m_payload = message.arguments().constFirst().toString().toUtf8();
        m_received = true;
        connection.send(message.createReply());
        if (m_eventLoop) {
            m_eventLoop->quit();
        }
        return true;
    }

    void setEventLoop(QEventLoop* eventLoop) { m_eventLoop = eventLoop; }
    bool received() const { return m_received; }
    const QByteArray& payload() const { return m_payload; }

private:
    QEventLoop* m_eventLoop{ nullptr };
    QByteArray m_payload;
    bool m_received{ false };
};

bool isKdeWaylandSession()
{
    if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance()) ||
        QGuiApplication::platformName().compare(QLatin1String("wayland"),
                                                Qt::CaseInsensitive) != 0) {
        return false;
    }

    const QString currentDesktop =
      qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    return currentDesktop.contains(QLatin1String("KDE"),
                                   Qt::CaseInsensitive) ||
           !qEnvironmentVariableIsEmpty("KDE_FULL_SESSION");
}

QString javascriptString(const QString& value)
{
    QJsonArray wrapper;
    wrapper.append(value);
    QByteArray encoded = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    encoded.remove(0, 1);
    encoded.chop(1);
    return QString::fromUtf8(encoded);
}

QString windowStackScript(const QString& callbackService,
                          const QString& callbackPath)
{
    return QStringLiteral(R"JS((function() {
    var result = [];
    var windows = workspace.stackingOrder;
    var currentDesktop = workspace.currentDesktop;
    var currentDesktopId = currentDesktop && currentDesktop.id !== undefined
        ? String(currentDesktop.id) : "";
    var currentActivity = workspace.currentActivity
        ? String(workspace.currentActivity) : "";
    var ownPid = %1;

    for (var i = 0; i < windows.length; ++i) {
        var window = windows[i];
        if (!window || window.minimized || window.deleted ||
            !window.managed || window.skipSwitcher ||
            window.excludeFromCapture ||
            Number(window.pid) === ownPid) {
            continue;
        }

        // Dialogs are useful snap targets too. Some KWin versions classify
        // particular dialogs as special, so preserve them explicitly.
        if ((window.specialWindow && !window.dialog) ||
            (!window.normalWindow && !window.dialog)) {
            continue;
        }

        if (!window.onAllDesktops && currentDesktopId !== "") {
            var onCurrentDesktop = false;
            var desktops = window.desktops || [];
            for (var desktopIndex = 0;
                 desktopIndex < desktops.length;
                 ++desktopIndex) {
                if (desktops[desktopIndex] &&
                    String(desktops[desktopIndex].id) === currentDesktopId) {
                    onCurrentDesktop = true;
                    break;
                }
            }
            if (!onCurrentDesktop) {
                continue;
            }
        }

        var activities = window.activities || [];
        if (currentActivity !== "" && activities.length > 0 &&
            activities.indexOf(currentActivity) === -1) {
            continue;
        }

        var geometry = window.frameGeometry;
        if (!geometry || Number(geometry.width) <= 0 ||
            Number(geometry.height) <= 0) {
            continue;
        }
        result.push({
            x: Number(geometry.x),
            y: Number(geometry.y),
            width: Number(geometry.width),
            height: Number(geometry.height)
        });
    }

    callDBus(%2, %3, %4, %5, JSON.stringify(result), function() {});
})();)JS")
      .arg(QCoreApplication::applicationPid())
      .arg(javascriptString(callbackService),
           javascriptString(callbackPath),
           javascriptString(QLatin1String(SNAP_INTERFACE)),
           javascriptString(QLatin1String(SNAP_METHOD)));
}

QVector<QRectF> parseWindowStack(const QByteArray& payload)
{
    QJsonParseError parseError;
    const QJsonDocument document =
      QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        return {};
    }

    QVector<QRectF> windows;
    windows.reserve(document.array().size());
    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        const QJsonValue xValue = object.value(QLatin1String("x"));
        const QJsonValue yValue = object.value(QLatin1String("y"));
        const QJsonValue widthValue = object.value(QLatin1String("width"));
        const QJsonValue heightValue = object.value(QLatin1String("height"));
        if (!xValue.isDouble() || !yValue.isDouble() ||
            !widthValue.isDouble() || !heightValue.isDouble()) {
            continue;
        }

        const qreal x = xValue.toDouble();
        const qreal y = yValue.toDouble();
        const qreal width = widthValue.toDouble();
        const qreal height = heightValue.toDouble();
        if (!std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(width) || !std::isfinite(height) || width <= 0 ||
            height <= 0) {
            continue;
        }
        windows.emplaceBack(x, y, width, height);
    }
    return windows;
}

void unloadKWinScript(const QDBusConnection& connection,
                      const QString& pluginName)
{
    QDBusMessage unload = QDBusMessage::createMethodCall(
      QLatin1String(KWIN_SERVICE),
      QLatin1String(KWIN_SCRIPTING_PATH),
      QLatin1String(KWIN_SCRIPTING_INTERFACE),
      QStringLiteral("unloadScript"));
    unload << pluginName;
    connection.call(unload, QDBus::Block, 100);
}

}
#endif

QVector<QRectF> queryKdeWaylandWindows(int timeoutMs)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (!isKdeWaylandSession()) {
        return {};
    }

    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.isConnected() || connection.baseService().isEmpty()) {
        return {};
    }

    const QString uniqueSuffix =
      QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
    const QString callbackPath =
      QStringLiteral("/org/flameshot/WindowSnap/s%1").arg(uniqueSuffix);
    const QString pluginName =
      QStringLiteral("flameshot-window-snap-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(uniqueSuffix);

    QTemporaryFile scriptFile(
      QDir::tempPath() + QStringLiteral("/flameshot-window-snap-XXXXXX.js"));
    scriptFile.setAutoRemove(true);
    if (!scriptFile.open()) {
        return {};
    }
    const QByteArray script =
      windowStackScript(connection.baseService(), callbackPath).toUtf8();
    if (scriptFile.write(script) != script.size() || !scriptFile.flush()) {
        return {};
    }
    scriptFile.close();

    WindowStackReceiver receiver;
    if (!connection.registerVirtualObject(
          callbackPath, &receiver, QDBusConnection::SingleNode)) {
        return {};
    }

    QVector<QRectF> result;
    const int boundedTimeout = timeoutMs > 0 ? timeoutMs : 750;
    QElapsedTimer elapsed;
    elapsed.start();

    QDBusMessage load = QDBusMessage::createMethodCall(
      QLatin1String(KWIN_SERVICE),
      QLatin1String(KWIN_SCRIPTING_PATH),
      QLatin1String(KWIN_SCRIPTING_INTERFACE),
      QStringLiteral("loadScript"));
    load << scriptFile.fileName() << pluginName;
    const QDBusMessage loadReply =
      connection.call(load, QDBus::Block, boundedTimeout);

    bool idOk = false;
    const int scriptId =
      loadReply.type() == QDBusMessage::ReplyMessage &&
          !loadReply.arguments().isEmpty()
        ? loadReply.arguments().constFirst().toInt(&idOk)
        : -1;

    if (idOk && scriptId >= 0) {
        QDBusMessage run = QDBusMessage::createMethodCall(
          QLatin1String(KWIN_SERVICE),
          QStringLiteral("/Scripting/Script%1").arg(scriptId),
          QStringLiteral("org.kde.kwin.Script"),
          QStringLiteral("run"));
        connection.call(run,
                        QDBus::NoBlock,
                        qMax(1, boundedTimeout - int(elapsed.elapsed())));

        if (!receiver.received() && elapsed.elapsed() < boundedTimeout) {
            QEventLoop eventLoop;
            receiver.setEventLoop(&eventLoop);
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer,
                             &QTimer::timeout,
                             &eventLoop,
                             &QEventLoop::quit);
            timer.start(qMax(1,
                             boundedTimeout - int(elapsed.elapsed())));
            eventLoop.exec();
            receiver.setEventLoop(nullptr);
        }

        if (receiver.received()) {
            result = parseWindowStack(receiver.payload());
        }
    }

    // Unload by the unique plugin name even when loadScript timed out or
    // returned a malformed reply: KWin may still have accepted the request.
    unloadKWinScript(connection, pluginName);
    connection.unregisterObject(callbackPath, QDBusConnection::UnregisterNode);
    return result;
#else
    Q_UNUSED(timeoutMs)
    return {};
#endif
}

}
