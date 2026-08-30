// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include <QPainterPath>
#include <QPoint>
#include <QPointF>
#include <QVector>

namespace StrokeSmoother {

QVector<QPointF> stabilizedPoints(const QVector<QPoint>& rawPoints);
QPainterPath buildPath(const QVector<QPoint>& rawPoints);

}
