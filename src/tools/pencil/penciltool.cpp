// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "penciltool.h"
#include "strokesmoother.h"

#include <QCache>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

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

QCursor PencilTool::drawingCursor(qreal devicePixelRatio)
{
    // Wayland cursor buffer scales must be integers; a fractional pixmap DPR
    // can make the compositor enlarge the pointer on a scaled desktop.
    const int scale = qMax(1, qCeil(devicePixelRatio));
    static QCache<int, QCursor> cursors;
    if (const auto* cached = cursors.object(scale)) {
        return *cached;
    }

    QPixmap pixmap(28 * scale, 28 * scale);
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
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
    }
    // The pencil points down-left, with its graphite tip at the drawing point.
    const QCursor cursor(pixmap, 3, 25);
    cursors.insert(scale, new QCursor(cursor));
    return cursor;
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
    if (m_points.size() < 2) {
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
