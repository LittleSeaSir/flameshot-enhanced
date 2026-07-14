// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Flameshot Contributors

#include "erasertool.h"
#include <QPainter>
#include <QPainterPath>

EraserTool::EraserTool(QObject* parent)
  : AbstractPathTool(parent)
{}

QIcon EraserTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "eraser.svg");
}

QString EraserTool::name() const
{
    return tr("Eraser");
}

CaptureTool::Type EraserTool::type() const
{
    return CaptureTool::TYPE_ERASER;
}

QString EraserTool::description() const
{
    return tr("Click to delete object, drag to pixel-erase");
}

CaptureTool* EraserTool::copy(QObject* parent)
{
    auto* tool = new EraserTool(parent);
    copyParams(this, tool);
    return tool;
}

void EraserTool::process(QPainter& painter, const QPixmap& pixmap)
{
    // Interactive preview: show a translucent white-red path where the eraser
    // will cut through annotations. The actual erasing happens in
    // CaptureWidget::drawToolsData() using CompositionMode_DestinationOut.
    if (m_points.size() < 2)
        return;

    QPainterPath path;
    path.moveTo(m_points[0]);
    for (int i = 1; i < m_points.size(); i++)
        path.lineTo(m_points[i]);

    QPainterPathStroker stroker;
    stroker.setWidth(size());
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);

    QColor previewColor(255, 255, 255, 90);
    painter.save();
    painter.setPen(QPen(previewColor, 1));
    painter.setBrush(previewColor);
    painter.drawPath(stroker.createStroke(path));
    painter.restore();
}

void EraserTool::drawSearchArea(QPainter& painter, const QPixmap& pixmap)
{
    // Deliberately empty: eraser tool objects must be invisible to
    // CaptureToolObjects::find(), otherwise they block clicks from
    // reaching the actual annotations underneath them.
    Q_UNUSED(painter)
    Q_UNUSED(pixmap)
}

void EraserTool::paintMousePreview(QPainter& painter,
                                   const CaptureContext& context)
{
    // Show a translucent red dot to indicate eraser position and size
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 80, 80, 150),
                        context.toolSize + 2,
                        Qt::SolidLine,
                        Qt::RoundCap));
    painter.drawLine(context.mousePos, context.mousePos);
    painter.restore();
}

void EraserTool::drawStart(const CaptureContext& context)
{
    m_color = context.color;
    m_penStyle = context.penStyle;
    onSizeChanged(context.toolSize);
    m_points.append(context.mousePos);
    m_pathArea.setTopLeft(context.mousePos);
    m_pathArea.setBottomRight(context.mousePos);
}

void EraserTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
