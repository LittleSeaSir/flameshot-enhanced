// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector>

namespace WindowSnap {

// The input stack is ordered from bottom to top. The returned geometry is
// clipped to the current screen; when no window is hit, the screen itself is
// returned. Keeping this operation in logical QRectF coordinates avoids losing
// pixels on fractionally scaled displays.
QRectF selectGeometryAt(const QPointF& globalPos,
                        const QRectF& screenGeometry,
                        const QVector<QRectF>& bottomToTopWindows);

// Returns the visible KDE Wayland normal/dialog window stack in bottom-to-top
// order. Failure, timeout, and unsupported desktops are represented by an
// empty vector. Geometries are KWin's logical frameGeometry values.
QVector<QRectF> queryKdeWaylandWindows(int timeoutMs = 750);

}
