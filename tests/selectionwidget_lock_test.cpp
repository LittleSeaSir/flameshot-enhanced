// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgets/capture/selectionwidget.h"

#include <QApplication>
#include <QMouseEvent>
#include <QWidget>

#include <cstdlib>
#include <iostream>

namespace
{
class MouseCountingWidget : public QWidget
{
public:
    void resetCounts()
    {
        presses = moves = releases = 0;
    }

    int presses{ 0 };
    int moves{ 0 };
    int releases{ 0 };

protected:
    void mousePressEvent(QMouseEvent*) override { ++presses; }
    void mouseMoveEvent(QMouseEvent*) override { ++moves; }
    void mouseReleaseEvent(QMouseEvent*) override { ++releases; }
};

void sendDrag(QWidget& target, const QPoint& from, const QPoint& to)
{
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(from),
                      QPointF(from),
                      QPointF(from),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&target, &press);

    QMouseEvent move(QEvent::MouseMove,
                     QPointF(to),
                     QPointF(to),
                     QPointF(to),
                     Qt::NoButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(&target, &move);

    QMouseEvent release(QEvent::MouseButtonRelease,
                        QPointF(to),
                        QPointF(to),
                        QPointF(to),
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(&target, &release);
}

void sendHover(QWidget& target, const QPoint& pos)
{
    QMouseEvent move(QEvent::MouseMove,
                     QPointF(pos),
                     QPointF(pos),
                     QPointF(pos),
                     Qt::NoButton,
                     Qt::NoButton,
                     Qt::NoModifier);
    QApplication::sendEvent(&target, &move);
}

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MouseCountingWidget editor;
    editor.resize(400, 300);
    editor.setMouseTracking(true);
    editor.show();

    SelectionWidget selection(Qt::red, &editor);
    selection.setIdleCentralCursor(Qt::ArrowCursor);
    int settledCount = 0;
    QObject::connect(&selection,
                     &SelectionWidget::geometrySettled,
                     [&settledCount]() { ++settledCount; });
    const QRect originalGeometry(50, 50, 160, 120);
    selection.setGeometry(originalGeometry);
    selection.show();
    QApplication::processEvents();

    const QPoint center = originalGeometry.center();
    const QPoint dragTarget = center + QPoint(30, 20);
    settledCount = 0;
    sendDrag(editor, center, dragTarget);
    require(selection.geometry().topLeft() ==
              originalGeometry.topLeft() + QPoint(30, 20),
            "unlocked selection did not move; regression test is invalid");
    require(settledCount == 1,
            "full selection gesture did not settle exactly once");

    selection.setGeometry(originalGeometry);
    selection.setMouseInteraction(
      SelectionWidget::MouseInteraction::ResizeOnly);
    editor.resetCounts();
    settledCount = 0;
    sendDrag(editor, center, dragTarget);
    require(selection.geometry() == originalGeometry,
            "resize-only selection moved from its center");
    require(editor.presses == 1 && editor.moves == 1 && editor.releases == 1,
            "resize-only center gesture did not reach the annotation canvas");
    require(settledCount == 0,
            "resize-only center gesture unexpectedly settled the selection");

    struct ResizeCase
    {
        SelectionWidget::SideType side;
        QPoint start;
        QPoint target;
        QRect expectedGeometry;
        const char* message;
    };
    const QVector<ResizeCase> resizeCases{
        { SelectionWidget::TOPLEFT_SIDE,
          originalGeometry.topLeft(),
          originalGeometry.topLeft() + QPoint(-30, -24),
          originalGeometry.adjusted(-30, -24, 0, 0),
          "top-left handle did not resize" },
        { SelectionWidget::TOPRIGHT_SIDE,
          originalGeometry.topRight(),
          originalGeometry.topRight() + QPoint(30, -24),
          originalGeometry.adjusted(0, -24, 30, 0),
          "top-right handle did not resize" },
        { SelectionWidget::BOTTOMLEFT_SIDE,
          originalGeometry.bottomLeft(),
          originalGeometry.bottomLeft() + QPoint(-30, 24),
          originalGeometry.adjusted(-30, 0, 0, 24),
          "bottom-left handle did not resize" },
        { SelectionWidget::BOTTOMRIGHT_SIDE,
          originalGeometry.bottomRight(),
          originalGeometry.bottomRight() + QPoint(30, 24),
          originalGeometry.adjusted(0, 0, 30, 24),
          "bottom-right handle did not resize" },
        { SelectionWidget::LEFT_SIDE,
          QPoint(originalGeometry.left(), originalGeometry.center().y()),
          QPoint(originalGeometry.left() - 30, originalGeometry.center().y()),
          originalGeometry.adjusted(-30, 0, 0, 0),
          "left handle did not resize" },
        { SelectionWidget::RIGHT_SIDE,
          QPoint(originalGeometry.right(), originalGeometry.center().y()),
          QPoint(originalGeometry.right() + 30,
                 originalGeometry.center().y()),
          originalGeometry.adjusted(0, 0, 30, 0),
          "right handle did not resize" },
        { SelectionWidget::TOP_SIDE,
          QPoint(originalGeometry.center().x(), originalGeometry.top()),
          QPoint(originalGeometry.center().x(), originalGeometry.top() - 24),
          originalGeometry.adjusted(0, -24, 0, 0),
          "top handle did not resize" },
        { SelectionWidget::BOTTOM_SIDE,
          QPoint(originalGeometry.center().x(), originalGeometry.bottom()),
          QPoint(originalGeometry.center().x(),
                 originalGeometry.bottom() + 24),
          originalGeometry.adjusted(0, 0, 0, 24),
          "bottom handle did not resize" },
    };

    for (const SelectionWidget::MouseInteraction interaction :
         { SelectionWidget::MouseInteraction::Full,
           SelectionWidget::MouseInteraction::ResizeOnly }) {
        selection.setMouseInteraction(interaction);
        for (const ResizeCase& resizeCase : resizeCases) {
            selection.setGeometry(originalGeometry);
            require(selection.getMouseSide(resizeCase.start) ==
                      resizeCase.side,
                    "resize test did not hit the expected handle");
            editor.resetCounts();
            settledCount = 0;
            sendDrag(editor, resizeCase.start, resizeCase.target);
            require(selection.geometry() == resizeCase.expectedGeometry,
                    resizeCase.message);
            const int expectedParentEvents =
              interaction == SelectionWidget::MouseInteraction::Full ? 1 : 0;
            require(editor.presses == expectedParentEvents &&
                      editor.moves == expectedParentEvents &&
                      editor.releases == expectedParentEvents,
                    "resize gesture used the wrong parent event policy");
            require(settledCount == 1,
                    "resize gesture did not settle exactly once");
        }
    }

    selection.setGeometry(originalGeometry);
    selection.setMouseInteraction(
      SelectionWidget::MouseInteraction::ResizeOnly);
    editor.resetCounts();
    sendHover(editor, originalGeometry.topLeft());
    require(selection.cursor().shape() == Qt::SizeFDiagCursor,
            "resize-only handle hover did not show a resize cursor");
    sendHover(editor, originalGeometry.center());
    require(selection.cursor().shape() == Qt::ArrowCursor,
            "resize cursor remained stuck after leaving the handle");
    require(editor.moves == 2,
            "resize-only hover did not reach the annotation canvas");

    selection.setGeometry(originalGeometry);
    selection.setMouseInteraction(
      SelectionWidget::MouseInteraction::Disabled);
    editor.resetCounts();
    settledCount = 0;
    sendDrag(editor,
             originalGeometry.bottomRight(),
             originalGeometry.bottomRight() + QPoint(20, 20));
    require(selection.geometry() == originalGeometry,
            "disabled pin editor selection resized with the mouse");
    require(editor.presses == 1 && editor.moves == 1 && editor.releases == 1,
            "disabled selection unexpectedly consumed parent mouse events");
    require(settledCount == 0,
            "disabled selection unexpectedly emitted geometrySettled");

    return EXIT_SUCCESS;
}
