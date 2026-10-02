// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "screenshotscale.h"

#include <QtMath>

qreal screenshotDevicePixelRatio(const QSize& physicalSize,
                                 const QSize& logicalSize)
{
    if (physicalSize.isEmpty() || logicalSize.isEmpty()) {
        return 1.0;
    }

    const qreal scaleX = static_cast<qreal>(physicalSize.width()) /
                         static_cast<qreal>(logicalSize.width());
    const qreal scaleY = static_cast<qreal>(physicalSize.height()) /
                         static_cast<qreal>(logicalSize.height());

    // Wayland fractional scale is expressed in 1/120 increments. Screen
    // geometry is integer-valued, so the raw ratios differ slightly between
    // axes (for example 3840/2194 and 2160/1234 for a 175% screen).
    const qreal averageScale = (scaleX + scaleY) / 2.0;
    return qMax(1.0 / 120.0, qRound(averageScale * 120.0) / 120.0);
}
