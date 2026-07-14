// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "penciltool.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

PencilTool::PencilTool(QObject* parent)
  : AbstractPathTool(parent)
{}

QIcon PencilTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "pencil.svg");
}
QString PencilTool::name() const
{
    return tr("Pencil");
}

CaptureTool::Type PencilTool::type() const
{
    return CaptureTool::TYPE_PENCIL;
}

QString PencilTool::description() const
{
    return tr("Set the Pencil as the paint tool");
}

CaptureTool* PencilTool::copy(QObject* parent)
{
    auto* tool = new PencilTool(parent);
    copyParams(this, tool);
    return tool;
}

void PencilTool::process(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    if (m_points.size() < 2)
        return;

    // ── Fast path: 2 points = straight line, no smoothing needed ────
    if (m_points.size() == 2) {
        painter.setPen(
          QPen(m_color, size(), penStyle(), Qt::RoundCap, Qt::RoundJoin));
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawLine(m_points[0], m_points[1]);
        return;
    }

    // ── Step 1: Near-collinear point downweighting ──────────────────
    // Compute a per-point "keep weight": points that are almost on a
    // straight line between their neighbors get low weight, so the
    // Gaussian pass can pull them toward the line center.
    QVector<double> keepW(m_points.size(), 1.0);
    for (int i = 2; i < m_points.size(); i++) {
        const QPoint& a = m_points[i - 2];
        const QPoint& b = m_points[i - 1];
        const QPoint& c = m_points[i];
        // Perpendicular distance from b to line ac via cross-product
        double cross =
          qAbs((c.x() - a.x()) * (a.y() - b.y()) -
               (a.x() - b.x()) * (c.y() - a.y()));
        double baseLen = QLineF(a, c).length();
        // If b is within ~0.7px of line ac, it's just hand-tremor noise
        if (baseLen > 2.0 && cross / baseLen < 0.9) {
            keepW[i - 1] = 0.2; // strongly downweight jitter points
        }
    }

    // ── Step 2: Gaussian-weighted smoothing ─────────────────────────
    // Gaussian σ=2.5, radius=5. Much stronger than the old window=3
    // moving average: produces GoodNotes-grade smooth curves while
    // preserving intentional direction changes.
    const double sigma = 2.5;
    const int radius = 5;
    double gaussWeights[11]; // 2*radius + 1
    double wSum = 0;
    for (int i = -radius; i <= radius; i++) {
        gaussWeights[i + radius] = std::exp(-(i * i) / (2 * sigma * sigma));
        wSum += gaussWeights[i + radius];
    }
    for (int i = 0; i <= 2 * radius; i++)
        gaussWeights[i] /= wSum;

    QVector<QPointF> smoothed;
    smoothed.reserve(m_points.size());
    for (int i = 0; i < m_points.size(); i++) {
        double sx = 0, sy = 0, sw = 0;
        for (int j = -radius; j <= radius; j++) {
            int idx = qBound(0, i + j, m_points.size() - 1);
            double w = gaussWeights[j + radius] * keepW[idx];
            sx += m_points[idx].x() * w;
            sy += m_points[idx].y() * w;
            sw += w;
        }
        smoothed.append(QPointF(sx / sw, sy / sw));
    }

    // ── Step 3: Catmull-Rom spline at 1.0 px spacing ────────────────
    // 1px sub-sampling + tension 0.4 per GoodNotes-grade spec.
    // Finer spacing eliminates visible stair-stepping on diagonals.
    const qreal sampleStep = 1.0;
    QPainterPath densePath;
    densePath.moveTo(smoothed[0]);

    for (int i = 0; i < smoothed.size() - 1; i++) {
        QPointF p0 = (i > 0) ? smoothed[i - 1] : smoothed[0];
        QPointF p1 = smoothed[i];
        QPointF p2 = smoothed[i + 1];
        QPointF p3 =
          (i + 2 < smoothed.size()) ? smoothed[i + 2] : smoothed.last();

        // Catmull-Rom → cubic Bezier, tension 0.4 for rounder curves
        const qreal T = 0.4;
        QPointF cp1(p1.x() + (p2.x() - p0.x()) * T,
                    p1.y() + (p2.y() - p0.y()) * T);
        QPointF cp2(p2.x() - (p3.x() - p1.x()) * T,
                    p2.y() - (p3.y() - p1.y()) * T);

        qreal segLen = QLineF(p1, p2).length();
        int steps = qMax(2, (int)(segLen / sampleStep));
        for (int s = 1; s <= steps; s++) {
            qreal t = (qreal)s / steps, t2 = t * t, t3 = t2 * t;
            qreal mt = 1.0 - t, mt2 = mt * mt, mt3 = mt2 * mt;
            qreal x = mt3 * p1.x() + 3 * mt2 * t * cp1.x() +
                      3 * mt * t2 * cp2.x() + t3 * p2.x();
            qreal y = mt3 * p1.y() + 3 * mt2 * t * cp1.y() +
                      3 * mt * t2 * cp2.y() + t3 * p2.y();
            densePath.lineTo(x, y);
        }
    }

    // ── Step 4: Soft-edge stroking ──────────────────────────────────
    QPainterPathStroker stroker;
    stroker.setWidth(size());
    stroker.setCapStyle(penStyle() == Qt::SolidLine ? Qt::RoundCap
                                                     : Qt::FlatCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    QPainterPath filledPath = stroker.createStroke(densePath);
    filledPath.setFillRule(Qt::WindingFill);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_color);
    painter.drawPath(filledPath);
}

void PencilTool::paintMousePreview(QPainter& painter,
                                   const CaptureContext& context)
{
    painter.setPen(QPen(context.color,
                        context.toolSize + 2,
                        Qt::SolidLine,
                        Qt::RoundCap));
    painter.drawLine(context.mousePos, context.mousePos);
}

void PencilTool::drawStart(const CaptureContext& context)
{
    m_color = context.color;
    m_penStyle = context.penStyle;
    onSizeChanged(context.toolSize);
    m_points.append(context.mousePos);
    m_pathArea.setTopLeft(context.mousePos);
    m_pathArea.setBottomRight(context.mousePos);
}

void PencilTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
