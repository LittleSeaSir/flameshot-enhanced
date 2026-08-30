// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "tools/pencil/strokesmoother.h"

#include <QDebug>
#include <QLineF>

#include <cstdlib>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        qCritical() << message;
        std::exit(EXIT_FAILURE);
    }
}

bool fuzzyPointEqual(const QPointF& left, const QPointF& right)
{
    return QLineF(left, right).length() < 0.001;
}

}

int main()
{
    require(StrokeSmoother::buildPath({}).isEmpty(),
            "empty input must produce an empty path");

    const QVector<QPoint> sparseLine{ { 0, 0 }, { 0, 0 }, { 12, 0 } };
    const auto sampledLine = StrokeSmoother::stabilizedPoints(sparseLine);
    require(fuzzyPointEqual(sampledLine.first(), QPointF(0, 0)),
            "stroke start moved during stabilization");
    require(fuzzyPointEqual(sampledLine.last(), QPointF(12, 0)),
            "stroke end moved during stabilization");
    for (int i = 1; i < sampledLine.size(); ++i) {
        require(QLineF(sampledLine.at(i - 1), sampledLine.at(i)).length() <=
                  4.001,
                "resampled segment exceeded maximum spacing");
    }

    const QVector<QPoint> jitter{ { 0, 0 },  { 2, 1 },  { 4, -1 },
                                  { 6, 1 },  { 8, -1 }, { 10, 0 } };
    const auto stabilized = StrokeSmoother::stabilizedPoints(jitter);
    qreal rawVariation = 0.0;
    qreal stabilizedVariation = 0.0;
    for (const QPoint& point : jitter) {
        rawVariation += qAbs(point.y());
    }
    for (const QPointF& point : stabilized) {
        stabilizedVariation += qAbs(point.y());
    }
    require(stabilizedVariation < rawVariation,
            "stabilizer did not reduce straight-line jitter");

    const QVector<QPoint> corner{ { 0, 0 }, { 10, 0 }, { 10, 10 } };
    const QPainterPath cornerPath = StrokeSmoother::buildPath(corner);
    require(fuzzyPointEqual(cornerPath.pointAtPercent(0.0), QPointF(0, 0)),
            "curve does not start at the first input point");
    require(fuzzyPointEqual(cornerPath.pointAtPercent(1.0), QPointF(10, 10)),
            "curve does not finish at the last input point");
    require(cornerPath.boundingRect().adjusted(-1, -1, 1, 1).contains(
              QRectF(QPointF(0, 0), QPointF(10, 10))),
            "corner path escaped its expected bounds");

    QVector<QPoint> longStroke;
    longStroke.reserve(10000);
    for (int x = 0; x < 10000; ++x) {
        longStroke.append(QPoint(x, x % 3));
    }
    const QPainterPath longPath = StrokeSmoother::buildPath(longStroke);
    require(longPath.elementCount() < longStroke.size() * 4,
            "path representation grew faster than linearly");

    qInfo() << "strokesmoother tests passed";
    return EXIT_SUCCESS;
}
