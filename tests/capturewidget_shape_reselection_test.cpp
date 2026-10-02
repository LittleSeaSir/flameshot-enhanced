// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "tools/abstracttwopointtool.h"
#include "widgets/capture/capturetoolbutton.h"
#include "widgets/capture/capturewidget.h"
#include "widgets/panel/utilitypanel.h"

#include <QApplication>
#include <QMouseEvent>
#include <QStandardPaths>

#include <cstdio>
#include <cstdlib>

namespace {

void sendMouseEvent(CaptureWidget& widget,
                    QEvent::Type type,
                    const QPoint& pos,
                    Qt::MouseButton button,
                    Qt::MouseButtons buttons)
{
    QMouseEvent event(type,
                      QPointF(pos),
                      QPointF(widget.mapToGlobal(pos)),
                      button,
                      buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
}

void press(CaptureWidget& widget, const QPoint& pos)
{
    sendMouseEvent(widget,
                   QEvent::MouseButtonPress,
                   pos,
                   Qt::LeftButton,
                   Qt::LeftButton);
}

void drag(CaptureWidget& widget, const QPoint& pos)
{
    sendMouseEvent(widget,
                   QEvent::MouseMove,
                   pos,
                   Qt::NoButton,
                   Qt::LeftButton);
}

void release(CaptureWidget& widget, const QPoint& pos)
{
    sendMouseEvent(widget,
                   QEvent::MouseButtonRelease,
                   pos,
                   Qt::LeftButton,
                   Qt::NoButton);
}

} // namespace

class CaptureWidgetShapeReselectionTest
{
public:
    static int run(int argc, char* argv[])
    {
        QCoreApplication::setApplicationName(
          QStringLiteral("flameshot-capturewidget-test"));
        QCoreApplication::setOrganizationName(QStringLiteral("flameshot"));
        QStandardPaths::setTestModeEnabled(true);
        QApplication app(argc, argv);

        bool passed = true;
        auto require = [&passed](bool condition, const char* message) {
            if (!condition) {
                qCritical() << message;
                std::fprintf(stderr, "%s\n", message);
                passed = false;
            }
        };

        QPixmap source(320, 240);
        source.fill(Qt::white);
        CaptureWidget widget(source, QRect(20, 20, 320, 240));
        const QRect pinSelectionGeometry = widget.m_selection->geometry();
        const QRect pinWindowGeometry = widget.geometry();
        const QImage pinBackground = widget.m_context.origScreenshot.toImage();

        CaptureToolButton* rectangleButton = nullptr;
        for (auto* button : widget.findChildren<CaptureToolButton*>()) {
            if (button->tool() &&
                button->tool()->type() == CaptureTool::TYPE_RECTANGLE) {
                rectangleButton = button;
                break;
            }
        }
        require(rectangleButton != nullptr,
                "rectangle button was not created by CaptureWidget");
        if (!rectangleButton) {
            return EXIT_FAILURE;
        }

        // Build the fixture through the same event path used in the UI. A
        // selectable tool deliberately remains active after the first shape.
        widget.setState(rectangleButton);
        const QPoint originalFrom(40, 40);
        const QPoint originalTo(100, 100);
        press(widget, originalFrom);
        drag(widget, originalTo);
        release(widget, originalTo);

        require(widget.m_captureToolObjects.size() == 1,
                "initial rectangle was not stored");
        require(widget.m_undoStack.count() == 1,
                "initial rectangle did not create exactly one undo command");
        require(widget.m_activeButton == rectangleButton,
                "rectangle tool did not remain active for continuous drawing");
        auto* original = qobject_cast<AbstractTwoPointTool*>(
          widget.m_captureToolObjects.at(0).data());
        require(original != nullptr, "stored rectangle lost two-point geometry");
        if (!original) {
            return EXIT_FAILURE;
        }
        const auto originalPoints = original->points();

        // Press a painted edge while Rectangle is still active. The same
        // gesture must reselect, uncheck the tool, and move the old object.
        const QPoint hitPoint = originalFrom;
        const QPoint delta(30, 20);
        press(widget, hitPoint);
        require(widget.m_activeButton.isNull(),
                "hitting an old rectangle did not cancel the active tool");
        require(widget.m_panel->activeLayerIndex() == 0,
                "hitting an old rectangle did not select its layer");
        require(widget.m_activeTool.isNull(),
                "reselection incorrectly started a new rectangle");

        drag(widget, hitPoint + delta);
        require(widget.cursor().shape() == Qt::ClosedHandCursor,
                "dragging a reselected shape did not show the grabbed cursor");
        release(widget, hitPoint + delta);

        auto* moved = qobject_cast<AbstractTwoPointTool*>(
          widget.m_captureToolObjects.at(0).data());
        require(widget.m_captureToolObjects.size() == 1,
                "moving an old rectangle created another object");
        require(moved && moved->points().first == originalPoints.first + delta &&
                  moved->points().second == originalPoints.second + delta,
                "same-gesture drag did not move the selected rectangle exactly");
        require(widget.m_undoStack.count() == 2,
                "one shape drag did not create exactly one undo command");
        require(widget.m_selection->geometry() == pinSelectionGeometry &&
                  widget.geometry() == pinWindowGeometry &&
                  widget.m_context.origScreenshot.toImage() == pinBackground,
                "moving an annotation changed the pinned image background");

        widget.undo();
        auto* undone = qobject_cast<AbstractTwoPointTool*>(
          widget.m_captureToolObjects.at(0).data());
        require(undone && undone->points() == originalPoints,
                "undo did not restore the rectangle before its move");

        widget.redo();
        auto* redone = qobject_cast<AbstractTwoPointTool*>(
          widget.m_captureToolObjects.at(0).data());
        require(redone &&
                  redone->points().first == originalPoints.first + delta &&
                  redone->points().second == originalPoints.second + delta,
                "redo did not restore the moved rectangle");

        // Reselect Rectangle, then press empty canvas. This must retain the
        // continuous-draw behavior rather than treating every press as a
        // selection attempt.
        widget.setState(rectangleButton);
        const QPoint blankFrom(180, 70);
        const QPoint blankTo(240, 130);
        press(widget, blankFrom);
        require(widget.m_activeButton == rectangleButton,
                "blank press unexpectedly canceled the rectangle tool");
        require(!widget.m_activeTool.isNull(),
                "blank press did not start a new rectangle");
        drag(widget, blankTo);
        release(widget, blankTo);

        require(widget.m_captureToolObjects.size() == 2,
                "blank drag did not append a new rectangle");
        require(widget.m_undoStack.count() == 3,
                "new rectangle did not create exactly one undo command");
        auto* newlyDrawn = qobject_cast<AbstractTwoPointTool*>(
          widget.m_captureToolObjects.at(1).data());
        require(newlyDrawn && newlyDrawn->points().first == blankFrom &&
                  newlyDrawn->points().second == blankTo,
                "new rectangle geometry does not match the blank drag");

        if (passed) {
            qInfo() << "CaptureWidget fixed-shape reselection test passed";
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }
};

int runCaptureWidgetShapeReselectionTest(int argc, char* argv[])
{
    return CaptureWidgetShapeReselectionTest::run(argc, argv);
}
