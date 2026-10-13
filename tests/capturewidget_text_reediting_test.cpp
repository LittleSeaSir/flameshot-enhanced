// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "tools/text/textwidget.h"
#include "widgets/capture/capturewidget.h"
#include "widgets/panel/utilitypanel.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTextCursor>

#include <cstdio>
#include <cstdlib>

namespace {

void click(CaptureWidget& widget, const QPoint& pos)
{
    for (auto type : { QEvent::MouseButtonPress, QEvent::MouseButtonRelease }) {
        QMouseEvent event(type,
                          QPointF(pos),
                          QPointF(widget.mapToGlobal(pos)),
                          Qt::LeftButton,
                          type == QEvent::MouseButtonPress ? Qt::LeftButton
                                                          : Qt::NoButton,
                          Qt::NoModifier);
        QApplication::sendEvent(&widget, &event);
    }
}

} // namespace

class CaptureWidgetTextReeditingTest
{
public:
    static int run(int argc, char* argv[])
    {
        QCoreApplication::setApplicationName(
          QStringLiteral("flameshot-text-reediting-test"));
        QCoreApplication::setOrganizationName(QStringLiteral("flameshot"));
        QStandardPaths::setTestModeEnabled(true);
        QApplication app(argc, argv);

        bool passed = true;
        auto require = [&passed](bool condition, const char* message) {
            if (!condition) {
                std::fprintf(stderr, "%s\n", message);
                passed = false;
            }
            return condition;
        };

        for (bool pinEditMode : { true, false }) {
            QPixmap source(500, 350);
            source.fill(Qt::white);
            CaptureWidget widget(source, QRect(20, 20, 500, 350));
            widget.show();
            QApplication::processEvents();
            const auto hideBeforeDestruction = qScopeGuard([&widget]() {
                widget.hide();
            });
            widget.m_pinEditMode = pinEditMode;
            widget.m_context.fullscreen = !pinEditMode;
            widget.updateSelectionState();
            const QRect pinGeometry = widget.geometry();
            const QRect selection = widget.m_selection->geometry();
            const QImage background = widget.m_context.origScreenshot.toImage();
            CaptureToolButton* textButton = nullptr;
            for (auto* button : widget.findChildren<CaptureToolButton*>()) {
                if (button->tool()->type() == CaptureTool::TYPE_TEXT) {
                    textButton = button;
                    break;
                }
            }
            if (!require(textButton, "text button was not initialized")) {
                return EXIT_FAILURE;
            }
            widget.setState(textButton);
            const QPoint firstPos(40, 45);
            const QPoint secondPos(40, 220);
            // The center of this multi-line box is blank. Text must be selected
            // by its input area, including the gaps between lines and characters.
            const QString original = QStringLiteral("原始文字 abc\n\n第二行");
            const QString edited = QStringLiteral("原始文字 ABC，添加内容\n\n第二行");

            auto editor = [&widget]() {
                return qobject_cast<TextWidget*>(widget.m_toolWidget.data());
            };
            auto open = [&](const QPoint& pos, const QString& expected) {
                click(widget, pos);
                return require(editor() && widget.m_activeTool &&
                                 widget.m_activeTool->editMode() &&
                                 editor()->toPlainText() == expected,
                               "text click did not reopen the existing input box");
            };

            click(widget, firstPos);
            if (!require(editor(), "new text input box was not created")) {
                return EXIT_FAILURE;
            }
            editor()->setPlainText(original);
            widget.commitCurrentTool();
            require(widget.m_captureToolObjects.size() == 1 &&
                      widget.m_undoStack.count() == 1,
                    "creating text did not produce one object and one undo step");
            const QRect firstBounds =
              widget.m_captureToolObjects.at(0)->boundingRect();
            const QPoint firstHit = firstBounds.center();

            if (!open(firstHit, original)) {
                return EXIT_FAILURE;
            }
            require(editor()->pos() == firstPos &&
                      widget.m_captureToolObjects.size() == 1 &&
                      widget.m_activeButton == textButton,
                    "reopening text moved it, duplicated it, or disabled Text");
            auto cursor = editor()->textCursor();
            cursor.movePosition(QTextCursor::End);
            editor()->setTextCursor(cursor);
            editor()->insertPlainText(QStringLiteral("，添加内容"));
            const QString appended = original + QStringLiteral("，添加内容");
            auto* sameEditor = editor();
            click(widget, firstPos + QPoint(10, 10));
            require(editor() == sameEditor && editor()->toPlainText() == appended,
                    "clicking the same input box reset the pending edit");
            editor()->setPlainText(edited);
            widget.commitCurrentTool();
            require(widget.m_captureToolObjects.size() == 1 &&
                      widget.m_undoStack.count() == 2 &&
                      *widget.m_captureToolObjects.at(0)->pos() == firstPos &&
                      widget.m_activeButton == textButton,
                    "editing text duplicated/moved it or lost its undo step");
            widget.undo();
            if (!open(firstHit, original)) {
                return EXIT_FAILURE;
            }
            widget.commitCurrentTool();
            require(widget.m_undoStack.count() == 2,
                    "opening and closing unchanged text added an undo step");
            widget.redo();
            if (!open(firstHit, edited)) {
                return EXIT_FAILURE;
            }
            widget.commitCurrentTool();

            // A change only in letter case must also remain undoable.
            if (!open(firstHit, edited)) {
                return EXIT_FAILURE;
            }
            const QString lowercase = QString(edited).replace(
              QStringLiteral("ABC"), QStringLiteral("abc"));
            editor()->setPlainText(lowercase);
            widget.commitCurrentTool();
            require(widget.m_undoStack.count() == 3,
                    "case-only edit was not recorded");
            widget.undo();
            if (!open(firstHit, edited)) {
                return EXIT_FAILURE;
            }
            widget.commitCurrentTool();
            widget.redo();

            // Clicking empty canvas starts another input immediately. Switching
            // between existing inputs commits the old one before looking up the new.
            click(widget, secondPos);
            if (!require(editor(), "blank click did not start a second text box")) {
                return EXIT_FAILURE;
            }
            const QString second = QStringLiteral("第二段文字");
            editor()->setPlainText(second);
            if (!open(firstHit, lowercase)) {
                return EXIT_FAILURE;
            }
            require(widget.m_captureToolObjects.size() == 2,
                    "switching text boxes lost the newly created second text");
            editor()->setPlainText(QStringLiteral("修改第一段"));
            const QPoint secondHit =
              widget.m_captureToolObjects.at(1)->boundingRect().center();
            if (!open(secondHit, second)) {
                return EXIT_FAILURE;
            }
            widget.commitCurrentTool();

            // Delete all of A, then click B: deleting A shifts B's layer index.
            if (!open(firstPos + QPoint(10, 10), QStringLiteral("修改第一段"))) {
                return EXIT_FAILURE;
            }
            editor()->selectAll();
            QKeyEvent removeText(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
            QApplication::sendEvent(editor(), &removeText);
            if (!open(secondHit, second)) {
                return EXIT_FAILURE;
            }
            require(widget.m_captureToolObjects.size() == 1 &&
                      widget.m_panel->activeLayerIndex() == 0,
                    "deleting all text left an empty layer or stale selection index");
            widget.commitCurrentTool();
            widget.undo();
            require(widget.m_captureToolObjects.size() == 2,
                    "undo did not restore the deleted text layer");
            if (!open(firstPos + QPoint(10, 10), QStringLiteral("修改第一段"))) {
                return EXIT_FAILURE;
            }
            widget.commitCurrentTool();
            widget.redo();
            require(widget.m_captureToolObjects.size() == 1,
                    "redo did not remove the deleted text layer again");

            // Outside Text mode, the existing double-click gesture still edits.
            widget.setState(textButton);
            click(widget, secondHit);
            QMouseEvent doubleClick(QEvent::MouseButtonDblClick,
                                    QPointF(secondHit),
                                    QPointF(widget.mapToGlobal(secondHit)),
                                    Qt::LeftButton,
                                    Qt::LeftButton,
                                    Qt::NoModifier);
            QApplication::sendEvent(&widget, &doubleClick);
            require(editor() && editor()->toPlainText() == second,
                    "double-click editing stopped working outside Text mode");
            widget.commitCurrentTool();
            require(widget.geometry() == pinGeometry &&
                      widget.m_selection->geometry() == selection &&
                      widget.m_context.origScreenshot.toImage() == background,
                    "text editing changed the pinned image position or background");
        }

        std::fprintf(stderr,
                     "Text re-editing test %s\n",
                     passed ? "passed" : "failed");
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    }
};

int runCaptureWidgetTextReeditingTest(int argc, char* argv[])
{
    return CaptureWidgetTextReeditingTest::run(argc, argv);
}
