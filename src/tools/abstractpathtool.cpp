// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "abstractpathtool.h"

#include <cmath>

AbstractPathTool::AbstractPathTool(QObject* parent)
  : CaptureTool(parent)
  , m_thickness(1)
  , m_padding(0)
{}

void AbstractPathTool::copyParams(const AbstractPathTool* from,
                                  AbstractPathTool* to)
{
    to->m_color = from->m_color;
    to->m_thickness = from->m_thickness;
    to->m_padding = from->m_padding;
    to->m_pos = from->m_pos;

    to->m_points = from->m_points;
    to->m_pathArea = from->m_pathArea;
    to->m_penStyle = from->m_penStyle;
}

bool AbstractPathTool::isValid() const
{
    return m_points.length() > 1;
}

bool AbstractPathTool::closeOnButtonPressed() const
{
    return false;
}

bool AbstractPathTool::isSelectable() const
{
    return true;
}

bool AbstractPathTool::showMousePreview() const
{
    return true;
}

QRect AbstractPathTool::mousePreviewRect(const CaptureContext& context) const
{
    QRect rect(0, 0, context.toolSize + 2, context.toolSize + 2);
    rect.moveCenter(context.mousePos);
    return rect;
}

QRect AbstractPathTool::boundingRect() const
{
    if (m_points.isEmpty()) {
        return {};
    }
    const int offset =
      m_thickness <= 1 ? 1 : static_cast<int>(round(m_thickness * 0.7 + 0.5));
    return m_pathArea.adjusted(-offset, -offset, offset, offset);
}

void AbstractPathTool::drawEnd(const QPoint& p)
{
    addPoint(p);
}

void AbstractPathTool::drawMove(const QPoint& p)
{
    addPoint(p);
}

void AbstractPathTool::drawMoveWithAdjustment(const QPoint& p)
{
    // Shift+draw: draw a straight line from the start point at ANY angle
    // (free angle, no snapping)
    if (!m_points.isEmpty()) {
        QPoint start = m_points.first(); // save actual start before clearing
        m_points.clear();
        addPoint(start);
        addPoint(p);
    }
}

void AbstractPathTool::onColorChanged(const QColor& c)
{
    m_color = c;
}

void AbstractPathTool::onSizeChanged(int size)
{
    m_thickness = size;
}

void AbstractPathTool::addPoint(const QPoint& point)
{
    // High-report-rate mice can deliver multiple events for the same logical
    // pixel. Keeping duplicates only increases path processing cost and can
    // create zero-length spline segments.
    if (!m_points.isEmpty() && m_points.last() == point) {
        return;
    }
    const QRect pointArea(point, QSize(1, 1));
    m_pathArea = m_points.isEmpty() ? pointArea : m_pathArea.united(pointArea);
    m_points.append(point);
}

void AbstractPathTool::move(const QPoint& mousePos)
{
    if (m_points.empty()) {
        return;
    }
    QPoint basePos = *pos();
    QPoint offset = mousePos - basePos;
    for (auto& m_point : m_points) {
        m_point += offset;
    }
    m_pathArea.translate(offset);
}

const QPoint* AbstractPathTool::pos()
{
    if (m_points.empty()) {
        m_pos = QPoint();
        return &m_pos;
    }
    m_pos = m_pathArea.topLeft();
    return &m_pos;
}
