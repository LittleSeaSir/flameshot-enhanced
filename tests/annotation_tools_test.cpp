// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "tools/eraser/erasertool.h"
#include "tools/pencil/penciltool.h"
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

    qInfo() << "annotation tool tests passed";
    return EXIT_SUCCESS;
}
