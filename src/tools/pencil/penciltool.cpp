// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "penciltool.h"
#include "strokesmoother.h"

#include <QCache>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

void drawBrushDot(QPainter& painter,
                  const QPointF& center,
                  qreal width,
                  const QColor& color)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    QPainterPath dot;
    dot.addEllipse(center, width / 2.0, width / 2.0);
    painter.drawPath(dot);
}

}

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
    return tr("Draw a smooth freehand stroke");
}

QCursor PencilTool::drawingCursor(int strokeWidth,
                                  qreal devicePixelRatio,
                                  const QColor& color)
{
    // Wayland cursor buffer scales must be integers; a fractional pixmap DPR
    // can make the compositor enlarge the pointer on a scaled desktop.
    const int scale = qMax(1, qCeil(devicePixelRatio));
    strokeWidth = qBound(1, strokeWidth, 50);
    const int extent = 20 + 2 * strokeWidth;
    const qreal magnification = extent / 28.0;
    // The compositor moves the pencil and its size dot as a single cursor.
    const int hotspot = qCeil(strokeWidth / 2.0) + 1;
    const int tip = qCeil((strokeWidth / 2.0 + 1.5 * magnification + 2) /
                         qSqrt(2.0));
    const int canvasExtent =
      hotspot + extent + qMax(0, tip - qRound(3 * magnification));
    const quint64 key = (quint64(scale) << 48) | (quint64(strokeWidth) << 32) |
                        color.rgba();
    static QCache<quint64, QCursor> cursors(64);
    if (const auto* cached = cursors.object(key)) {
        return *cached;
    }

    QPixmap pixmap(canvasExtent * scale, canvasExtent * scale);
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        // Point toward the upper-left dot, centered on the hotspot.
        painter.save();
        painter.translate(hotspot + tip, hotspot + tip);
        painter.scale(magnification, magnification);
        painter.rotate(90);
        painter.translate(-3, -25);
        QPainterPath outline;
        outline.moveTo(3, 25);
        outline.lineTo(5, 18);
        outline.lineTo(20, 3);
        outline.quadTo(21, 2, 22, 3);
        outline.lineTo(25, 6);
        outline.quadTo(26, 7, 25, 8);
        outline.lineTo(11, 24);
        outline.closeSubpath();

        // A white rim and dark edges keep the pencil visible on any capture.
        painter.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.setBrush(QColor(255, 205, 74));
        painter.drawPath(outline);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(235, 145, 148));
        painter.drawPolygon(QPolygonF{ { 18, 5 }, { 20, 3 }, { 21, 2.5 },
                                       { 25.5, 7 }, { 25, 8 }, { 24, 9 },
                                       { 23, 10 } });
        painter.setBrush(QColor(239, 211, 169));
        painter.drawPolygon(QPolygonF{ { 3, 25 }, { 5, 18 }, { 11, 24 } });
        painter.setPen(QPen(QColor(45, 45, 45), 1, Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(outline);
        painter.drawLine(QPointF(18, 5), QPointF(24, 11));
        painter.drawLine(QPointF(5, 18), QPointF(11, 24));
        painter.setPen(QPen(QColor(255, 241, 167), 1.5, Qt::SolidLine,
                            Qt::RoundCap));
        painter.drawLine(QPointF(8, 18), QPointF(19, 7));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(45, 45, 45));
        painter.drawPolygon(QPolygonF{ { 3, 25 }, { 4, 22 }, { 6, 24 } });
        painter.restore();
        drawBrushDot(painter, QPointF(hotspot, hotspot), strokeWidth, color);
    }
    const QCursor cursor(pixmap, hotspot, hotspot);
    cursors.insert(key, new QCursor(cursor));
    return cursor;
}

CaptureTool* PencilTool::copy(QObject* parent)
{
    auto* tool = new PencilTool(parent);
    copyParams(this, tool);
    return tool;
}

bool PencilTool::isValid() const
{
    return !m_points.isEmpty();
}

bool PencilTool::showMousePreview() const
{
    return false;
}

void PencilTool::process(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    if (m_points.isEmpty()) {
        return;
    }
    if (m_points.size() == 1) {
        drawBrushDot(painter, m_points.first(), size(), m_color);
        return;
    }

    // QPainter tessellates this compact cubic path at output resolution. This
    // avoids rebuilding a dense one-pixel outline on every pointer event.
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(
      QPen(m_color, size(), penStyle(), Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(StrokeSmoother::buildPath(m_points));
}

void PencilTool::paintMousePreview(QPainter& painter,
                                   const CaptureContext& context)
{
    Q_UNUSED(painter)
    Q_UNUSED(context)
}

void PencilTool::drawStart(const CaptureContext& context)
{
    drawStart(context, context.mousePos);
}

void PencilTool::drawStart(const CaptureContext& context,
                           const QPointF& position)
{
    m_color = context.color;
    m_penStyle = context.penStyle;
    onSizeChanged(context.toolSize);
    addPoint(position);
}

void PencilTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
