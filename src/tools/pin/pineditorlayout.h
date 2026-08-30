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

PinEditorLayout createPinEditorLayout(const QPixmap& content,
                                      const QRect& globalContentGeometry,
                                      const QRect& screenGeometry,
                                      qreal devicePixelRatio,
                                      int windowMargin = 0);
