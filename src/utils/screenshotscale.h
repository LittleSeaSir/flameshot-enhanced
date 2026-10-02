// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include <QSize>

qreal screenshotDevicePixelRatio(const QSize& physicalSize,
                                 const QSize& logicalSize);
