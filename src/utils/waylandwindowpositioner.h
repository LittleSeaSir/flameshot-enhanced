// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include <QMargins>
#include <QRect>

class QWidget;

struct AnchoredWindowPlacement
{
    QMargins margins;
    QSize size;
};

AnchoredWindowPlacement anchoredWindowPlacement(
  const QRect& globalWindowGeometry,
  const QRect& screenGeometry);

// Ordinary xdg-toplevel windows cannot choose their position on Wayland.
// When KDE's optional LayerShellQt runtime is available, turn this widget
// into a top-layer surface anchored at the requested global geometry.
bool positionWaylandWindow(QWidget* widget,
                           const QRect& globalWindowGeometry);
