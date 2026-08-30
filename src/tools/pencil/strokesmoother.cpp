// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "strokesmoother.h"

#include <QLineF>
#include <QtMath>

namespace {

constexpr qreal MIN_POINT_DISTANCE = 0.75;
constexpr qreal MAX_POINT_DISTANCE = 4.0;
constexpr int SMOOTHING_PASSES = 2;

QVector<QPointF> resample(const QVector<QPoint>& rawPoints)
{
    QVector<QPointF> sampled;
    if (rawPoints.isEmpty()) {
        return sampled;
    }

    sampled.reserve(rawPoints.size());
    sampled.append(QPointF(rawPoints.first()));
    for (int i = 1; i < rawPoints.size(); ++i) {
        const QPointF candidate(rawPoints.at(i));
        const QPointF previous = sampled.last();
        const qreal distance = QLineF(previous, candidate).length();
        if (distance < MIN_POINT_DISTANCE) {
            continue;
        }

        const int steps = qMax(1, qCeil(distance / MAX_POINT_DISTANCE));
        for (int step = 1; step <= steps; ++step) {
            const qreal t = static_cast<qreal>(step) / steps;
            sampled.append(previous + (candidate - previous) * t);
        }
    }
    return sampled;
}

QVector<QPointF> smoothingPass(const QVector<QPointF>& points)
{
    if (points.size() < 3) {
        return points;
    }

    QVector<QPointF> result;
    result.reserve(points.size());
    result.append(points.first());
    for (int i = 1; i < points.size() - 1; ++i) {
        const QPointF incoming = points.at(i) - points.at(i - 1);
        const QPointF outgoing = points.at(i + 1) - points.at(i);
        const qreal incomingLength =
          qSqrt(QPointF::dotProduct(incoming, incoming));
        const qreal outgoingLength =
          qSqrt(QPointF::dotProduct(outgoing, outgoing));

        qreal straightness = 0.0;
        if (incomingLength > 0.0 && outgoingLength > 0.0) {
            straightness = qBound(
              0.0,
              QPointF::dotProduct(incoming, outgoing) /
                (incomingLength * outgoingLength),
              1.0);
        }

        // Gentle motion gets stronger stabilization. Deliberate sharp turns
        // retain their original vertex instead of being rounded away.
        const qreal neighborWeight = 0.22 * straightness;
        const qreal centerWeight = 1.0 - 2.0 * neighborWeight;
        result.append(points.at(i - 1) * neighborWeight +
                      points.at(i) * centerWeight +
                      points.at(i + 1) * neighborWeight);
    }
    result.append(points.last());
    return result;
}

}

namespace StrokeSmoother {

QVector<QPointF> stabilizedPoints(const QVector<QPoint>& rawPoints)
{
    QVector<QPointF> points = resample(rawPoints);
    for (int pass = 0; pass < SMOOTHING_PASSES; ++pass) {
        points = smoothingPass(points);
    }
    return points;
}

QPainterPath buildPath(const QVector<QPoint>& rawPoints)
{
    const QVector<QPointF> points = stabilizedPoints(rawPoints);
    QPainterPath path;
    if (points.isEmpty()) {
        return path;
    }

    path.moveTo(points.first());
    if (points.size() == 1) {
        return path;
    }
    if (points.size() == 2) {
        path.lineTo(points.last());
        return path;
    }

    // Uniform Catmull-Rom converted to cubic Bezier. The bounded 1/6
    // coefficient avoids the loops produced by the old 0.4 coefficient.
    for (int i = 0; i < points.size() - 1; ++i) {
        const QPointF p0 = i > 0 ? points.at(i - 1) : points.at(i);
        const QPointF p1 = points.at(i);
        const QPointF p2 = points.at(i + 1);
        const QPointF p3 = i + 2 < points.size() ? points.at(i + 2) : p2;
        const QPointF control1 = p1 + (p2 - p0) / 6.0;
        const QPointF control2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo(control1, control2, p2);
    }
    return path;
}

}
