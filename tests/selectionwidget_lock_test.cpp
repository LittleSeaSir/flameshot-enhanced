// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgets/capture/selectionwidget.h"

#include <QApplication>
#include <QMouseEvent>
#include <QWidget>

#include <cstdlib>
#include <iostream>

namespace
{
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

    QWidget editor;
    editor.resize(400, 300);
    editor.show();

    SelectionWidget selection(Qt::red, &editor);
    const QRect originalGeometry(50, 50, 160, 120);
    selection.setGeometry(originalGeometry);
    selection.show();
    QApplication::processEvents();

    const QPoint center = originalGeometry.center();
    const QPoint dragTarget = center + QPoint(30, 20);
    sendDrag(editor, center, dragTarget);
    require(selection.geometry().topLeft() ==
              originalGeometry.topLeft() + QPoint(30, 20),
            "unlocked selection did not move; regression test is invalid");

    selection.setGeometry(originalGeometry);
    selection.setIgnoreMouse(true);
    sendDrag(editor, center, dragTarget);
    require(selection.geometry() == originalGeometry,
            "locked pin editor selection moved with the mouse");

    return EXIT_SUCCESS;
}
