// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "core/capturerequest.h"

#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QRect>

struct CaptureContext
{
    // screenshot with modifications
    QPixmap screenshot;
    // unmodified screenshot
    QPixmap origScreenshot;
    // Selection area
    QRect selection;
    // Selected tool color
    QColor color;
    // Path where the content has to be saved
    QString savePath;
    // Offset of the capture widget based on the system's screen (top-left)
    QPoint widgetOffset;
    // Mouse position inside the widget
    QPoint mousePos;
    // Size of the active tool
    int toolSize;
    // Pen style for dashed/dotted lines: 0=Solid, 1=Dash, 2=Dot, 3=DashDot
    int penStyle;
    // Current circle count
    int circleCount;
    // Mode of the capture widget
    bool fullscreen;
    CaptureRequest request = CaptureRequest::GRAPHICAL_MODE;

    QPixmap selectedScreenshotArea() const;
};
