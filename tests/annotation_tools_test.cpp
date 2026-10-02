// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "tools/abstracttwopointtool.h"
#include "tools/circle/circletool.h"
#include "tools/eraser/erasertool.h"
#include "tools/line/linetool.h"
#include "tools/pencil/penciltool.h"
#include "tools/rectangle/rectangletool.h"
#include "widgets/capture/capturetoolobjects.h"

#include <QApplication>
#include <QPainter>
#include <QRadioButton>

#include <cstdlib>
#include <memory>
#include <utility>

// CaptureContext is part of CaptureTool's Qt meta-object signature. Unit tests
// do not need configuration-backed CaptureRequest initialization, so provide
// the minimal constructor here instead of linking the entire config subsystem.
CaptureRequest::CaptureRequest(CaptureRequest::CaptureMode mode,
                               const uint delay,
                               QVariant data,
                               CaptureRequest::ExportTask tasks)
  : m_mode(mode)
  , m_delay(delay)
  , m_tasks(tasks)
  , m_data(std::move(data))
  , m_selectedMonitor(-1)
  , m_hasSelectedMonitor(false)
{}

namespace {

class TestEraserTool : public EraserTool
{
public:
    using AbstractPathTool::addPoint;
};

class TestPencilTool : public PencilTool
{
public:
    using AbstractPathTool::addPoint;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        qCritical() << message;
        std::exit(EXIT_FAILURE);
    }
}

std::unique_ptr<TestPencilTool> makePencil(const QPoint& from,
                                           const QPoint& to)
{
    auto pencil = std::make_unique<TestPencilTool>();
    pencil->onColorChanged(Qt::red);
    pencil->onSizeChanged(4);
    pencil->addPoint(from);
    pencil->addPoint(to);
    return pencil;
}

template<typename Tool>
void verifyFixedShape(const char* shape,
                      const QPoint& from,
                      const QPoint& to,
                      const QPoint& hitPoint,
                      const QPoint& missPoint)
{
    CaptureToolObjects objects;
    Tool source(&objects);
    CaptureContext context;
    context.mousePos = from;
    context.color = Qt::magenta;
    context.toolSize = 4;
    context.penStyle = 0;
    source.drawStart(context);
    source.drawMove(to);
    source.drawEnd(to);

    require(source.isValid(), "fixed-shape fixture is invalid");
    objects.append(QPointer<CaptureTool>(&source));
    require(objects.size() == 1, "fixed shape was not stored");
    require(objects.find(hitPoint, QSize(256, 256)) == 0,
            "fixed shape cannot be selected on its painted pixels");
    require(objects.find(missPoint, QSize(256, 256)) == -1,
            "fixed shape was selected away from its painted pixels");

    CaptureToolObjects beforeMove;
    beforeMove = objects;
    auto* stored =
      qobject_cast<AbstractTwoPointTool*>(objects.at(0).data());
    require(stored != nullptr, "stored fixed shape lost its geometry type");
    const auto originalPoints = stored->points();
    const QRect originalBounds = stored->boundingRect();
    const QPoint delta(100, 80);
    stored->move(*stored->pos() + delta);

    require(stored->points().first == originalPoints.first + delta &&
              stored->points().second == originalPoints.second + delta,
            "fixed shape endpoints did not move by the same delta");
    require(stored->boundingRect() == originalBounds.translated(delta),
            "fixed shape bounds did not move by the same delta");
    require(objects.find(hitPoint, QSize(256, 256)) == -1,
            "fixed shape remained selectable at its old position");
    require(objects.find(hitPoint + delta, QSize(256, 256)) == 0,
            "fixed shape is not selectable at its new position");

    CaptureToolObjects afterMove;
    afterMove = objects;
    objects = beforeMove;
    auto* restored =
      qobject_cast<AbstractTwoPointTool*>(objects.at(0).data());
    require(restored && restored->points() == originalPoints,
            "undo snapshot did not restore fixed-shape geometry");
    require(objects.find(hitPoint, QSize(256, 256)) == 0,
            "undo snapshot did not restore fixed-shape hit testing");

    objects = afterMove;
    auto* redone = qobject_cast<AbstractTwoPointTool*>(objects.at(0).data());
    require(redone &&
              redone->points().first == originalPoints.first + delta &&
              redone->points().second == originalPoints.second + delta,
            "redo snapshot did not restore moved fixed-shape geometry");
    require(objects.find(hitPoint + delta, QSize(256, 256)) == 0,
            "redo snapshot did not restore moved fixed-shape hit testing");

    qInfo() << shape << "selection and movement passed";
}

}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    TestEraserTool eraser;
    require(eraser.mode() == EraserTool::Mode::WholeStroke,
            "whole-stroke mode must be the safe default");
    require(!eraser.isValid(),
            "whole-stroke eraser must never become a stored pixel mask");

    std::unique_ptr<QWidget> configuration(eraser.configurationWidget());
    QRadioButton* pixelButton = nullptr;
    for (auto* button : configuration->findChildren<QRadioButton*>()) {
        if (button->text().contains("Pixel")) {
            pixelButton = button;
            break;
        }
    }
    require(pixelButton != nullptr, "pixel eraser mode is missing from UI");
    pixelButton->click();
    require(eraser.mode() == EraserTool::Mode::Pixel,
            "pixel eraser mode did not activate");

    eraser.onColorChanged(Qt::red);
    eraser.onSizeChanged(12);
    eraser.addPoint(QPoint(16, 16));
    require(eraser.isValid(), "a pixel-eraser click must create a valid mask");
    std::unique_ptr<CaptureTool> eraserCopy(eraser.copy());
    require(static_cast<EraserTool*>(eraserCopy.get())->mode() ==
              EraserTool::Mode::Pixel,
            "eraser copy lost its selected mode");

    QPixmap preview(32, 32);
    preview.fill(Qt::transparent);
    {
        QPainter painter(&preview);
        eraser.process(painter, preview);
    }
    require(preview.toImage().pixelColor(16, 16).alpha() > 0,
            "single-click pixel eraser preview was not rendered");

    auto lower = makePencil(QPoint(0, 5), QPoint(24, 5));
    auto upper = makePencil(QPoint(0, 20), QPoint(24, 20));
    CaptureToolObjects objects;
    objects.append(QPointer<CaptureTool>(lower.get()));
    objects.append(QPointer<CaptureTool>(upper.get()));
    require(objects.find(QPoint(0, 5), QSize(32, 32), 5) == 0,
            "edge-safe hit testing failed for lower annotation");
    require(objects.find(QPoint(12, 20), QSize(32, 32), 5) == 1,
            "hit testing did not select the top annotation");
    require(objects.find(QPoint(31, 31), QSize(32, 32), 5) == -1,
            "outlying hit test returned a false annotation");

    verifyFixedShape<RectangleTool>("rectangle",
                                    QPoint(20, 20),
                                    QPoint(80, 80),
                                    QPoint(50, 20),
                                    QPoint(10, 50));
    verifyFixedShape<CircleTool>("circle",
                                 QPoint(20, 20),
                                 QPoint(80, 80),
                                 QPoint(50, 20),
                                 QPoint(50, 50));
    verifyFixedShape<LineTool>("line",
                               QPoint(20, 20),
                               QPoint(80, 80),
                               QPoint(50, 50),
                               QPoint(20, 80));

    qInfo() << "annotation tool tests passed";
    return EXIT_SUCCESS;
}
