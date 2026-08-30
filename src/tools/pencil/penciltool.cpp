// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "penciltool.h"
#include "strokesmoother.h"

#include <QPainter>

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
