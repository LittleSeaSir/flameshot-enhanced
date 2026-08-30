// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Flameshot Contributors

#include "erasertool.h"
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QRadioButton>

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
    return tr("Erase a whole annotation or erase pixels");
}

QString EraserTool::info()
{
    return m_mode == Mode::WholeStroke ? tr("Eraser (whole stroke)")
                                       : tr("Eraser (pixel)");
}

CaptureTool* EraserTool::copy(QObject* parent)
{
    auto* tool = new EraserTool(parent);
    copyParams(this, tool);
    tool->m_mode = m_mode;
    return tool;
}

bool EraserTool::isValid() const
{
    return m_mode == Mode::Pixel && !m_points.isEmpty();
}

void EraserTool::process(QPainter& painter, const QPixmap& pixmap)
{
    // Interactive preview: show a translucent white-red path where the eraser
    // will cut through annotations. The actual erasing happens in
    // CaptureWidget::drawToolsData() using CompositionMode_DestinationOut.
    if (m_mode != Mode::Pixel || m_points.isEmpty())
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
    if (m_points.size() == 1) {
        painter.setPen(QPen(previewColor, size(), Qt::SolidLine, Qt::RoundCap));
        painter.drawPoint(m_points.first());
    } else {
        painter.drawPath(stroker.createStroke(path));
    }
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
    const int previewSize = m_mode == Mode::WholeStroke
                              ? qMax(10, context.toolSize)
                              : context.toolSize + 2;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 80, 80, 150),
                        previewSize,
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

QWidget* EraserTool::configurationWidget()
{
    auto* widget = new QWidget();
    auto* layout = new QHBoxLayout(widget);
    auto* label = new QLabel(tr("Eraser mode:"), widget);
    auto* wholeStroke = new QRadioButton(tr("Whole stroke"), widget);
    auto* pixel = new QRadioButton(tr("Pixel"), widget);
    auto* group = new QButtonGroup(widget);

    group->addButton(wholeStroke, static_cast<int>(Mode::WholeStroke));
    group->addButton(pixel, static_cast<int>(Mode::Pixel));
    (m_mode == Mode::WholeStroke ? wholeStroke : pixel)->setChecked(true);

    connect(group,
            &QButtonGroup::idClicked,
            this,
            &EraserTool::setMode);
    layout->addWidget(label);
    layout->addWidget(wholeStroke);
    layout->addWidget(pixel);
    layout->addStretch();
    return widget;
}

EraserTool::Mode EraserTool::mode() const
{
    return m_mode;
}

void EraserTool::setMode(int mode)
{
    m_mode = mode == static_cast<int>(Mode::Pixel) ? Mode::Pixel
                                                   : Mode::WholeStroke;
}
