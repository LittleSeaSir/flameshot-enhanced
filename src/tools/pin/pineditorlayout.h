// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include <QPixmap>
#include <QRect>

inline constexpr int PIN_WINDOW_MARGIN = 7;

struct PinEditorLayout
{
    QPixmap canvas;
    QRect windowGeometry;
    QRect contentGeometry;
    QRect initialSelection;
};

QRect pinLogicalContentGeometry(const QRect& captureGeometry,
                                const QPoint& screenTopLeft,
                                qreal devicePixelRatio);

QRect pinCaptureContentGeometry(const QRect& logicalGeometry,
                                const QPoint& screenTopLeft,
                                const QSize& physicalSize,
                                qreal devicePixelRatio);

PinEditorLayout createPinEditorLayout(const QPixmap& content,
                                      const QRect& globalContentGeometry,
                                      const QRect& screenGeometry,
                                      qreal devicePixelRatio,
                                      int windowMargin = 0);
