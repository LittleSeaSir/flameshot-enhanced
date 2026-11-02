// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "tools/text/texttool.h"
#include "tools/text/textwidget.h"
#include "widgets/capture/capturewidget.h"

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QInputMethodEvent>
#include <QEventLoop>
#include <QMouseEvent>
#include <QPainter>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTimer>

#include <cmath>
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

QRect redInkBounds(const QImage& original)
{
    const QImage image = original.convertToFormat(QImage::Format_ARGB32);
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb color = pixels[x];
            if (qAlpha(color) > 0 && qRed(color) > 150 &&
                qGreen(color) < 120 && qBlue(color) < 120) {
                bounds = bounds.united(QRect(x, y, 1, 1));
            }
        }
    }
    return bounds;
}

QRect alphaBounds(const QImage& original)
{
    const QImage image = original.convertToFormat(QImage::Format_ARGB32);
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(pixels[x]) > 0) {
                bounds = bounds.united(QRect(x, y, 1, 1));
            }
        }
    }
    return bounds;
}

QImage renderEditor(TextWidget& editor)
{
    // Capture only the actual text pixels, without the selected text palette
    // or the insertion caret changing the apparent ink bounds.
    QTextCursor selection = editor.textCursor();
    selection.clearSelection();
    editor.setTextCursor(selection);
    editor.setCursorWidth(0);
    editor.setReadOnly(true);
    editor.clearFocus();
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    editor.render(&image);
    return image;
}

bool savePreview(const QString& directory,
                 const QString& mode,
                 int pointSize,
                 const QImage& editorImage,
                 const QPoint& editorPos,
                 const QPoint& anchor,
                 const QRect& line,
                 const QCursor& cursor)
{
    if (directory.isEmpty()) {
        return true;
    }
    QDir output(directory);
    if (!output.mkpath(QStringLiteral("."))) {
        return false;
    }

    QImage preview(720, 160, QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::white);
    QPainter painter(&preview);
    painter.setPen(Qt::black);
    painter.drawText(15,
                     22,
                     QStringLiteral("%1 pt | visible glyphs: %2 px | %3")
                       .arg(pointSize)
                       .arg(line.height())
                       .arg(mode));
    const QPoint previewAnchor(110, 90);
    const QPoint origin = previewAnchor + editorPos - anchor;
    painter.drawImage(origin, editorImage);

    // Put the real cursor immediately to the left of the same anchor height,
    // so its caps and the text are visible together without covering a glyph.
    const QPoint cursorAnchor = previewAnchor - QPoint(18, 0);
    painter.drawPixmap(cursorAnchor - cursor.hotSpot(), cursor.pixmap());
    const QRect mappedLine = line.translated(origin);
    QPen guide(QColor(160, 170, 185));
    guide.setStyle(Qt::DotLine);
    painter.setPen(guide);
    painter.drawLine(60, mappedLine.top(), 650, mappedLine.top());
    painter.drawLine(60, mappedLine.bottom(), 650, mappedLine.bottom());
    painter.end();
    return preview.save(output.filePath(
      QStringLiteral("text-cursor-%1-%2pt.png").arg(mode).arg(pointSize)));
}

} // namespace

class CaptureWidgetTextCursorTest
{
public:
    static int run(int argc, char* argv[])
    {
        QCoreApplication::setApplicationName(
          QStringLiteral("flameshot-text-cursor-test"));
        QCoreApplication::setOrganizationName(QStringLiteral("flameshot"));
        QStandardPaths::setTestModeEnabled(true);
        QApplication app(argc, argv);

        if (qEnvironmentVariableIsSet("FLAMESHOT_TEXT_CURSOR_LIVE_PROBE")) {
            // Keep a real, fully mapped Wayland capture open long enough for
            // its fractional-scale event and an external compositor screenshot.
            CaptureWidget widget(CaptureRequest(CaptureRequest::GRAPHICAL_MODE));
            widget.m_context.screenshot.fill(Qt::white);
            widget.m_context.origScreenshot.fill(Qt::white);
            widget.show();
            widget.activateWindow();
            QEventLoop loop;
            bool displayedCorrectly = false;
            QTimer::singleShot(1200, &loop, [&]() {
                widget.finishWindowSnapPreview();
                widget.selectAll();
                for (auto* button : widget.findChildren<CaptureToolButton*>()) {
                    if (button->tool()->type() == CaptureTool::TYPE_TEXT) {
                        widget.setState(button);
                        break;
                    }
                }
                widget.toolSizeChanged(6);
                widget.m_context.color = QColor(220, 32, 32);
                const QPoint point = widget.mapFromGlobal(QCursor::pos());
                click(widget, point);
                auto* editor = qobject_cast<TextWidget*>(widget.m_toolWidget.data());
                if (!editor) {
                    loop.quit();
                    return;
                }
                QInputMethodEvent input;
                input.setCommitString(QStringLiteral("等等Ag"));
                QApplication::sendEvent(editor, &input);
                const QCursor cursor = editor->viewport()->cursor();
                const qreal bufferScale = cursor.pixmap().devicePixelRatio();
                displayedCorrectly = bufferScale >= 1.0 &&
                  qAbs(bufferScale - std::round(bufferScale)) < 0.001 &&
                  cursor.pixmap().height() / static_cast<int>(bufferScale) ==
                    editor->firstLineTextRect().height() + 4;
                std::fprintf(stderr,
                             "LIVE cursor point=%d,%d windowDpr=%.3f editorDpr=%.3f "
                             "pixmap=%dx%d pixmapDpr=%.3f glyphHeight=%d "
                             "widgetFont=%d currentFont=%d\n",
                             point.x(), point.y(), widget.devicePixelRatioF(),
                             editor->devicePixelRatioF(), cursor.pixmap().width(),
                             cursor.pixmap().height(), cursor.pixmap().devicePixelRatio(),
                             editor->firstLineTextRect().height(), editor->font().pointSize(),
                             editor->currentCharFormat().font().pointSize());
                std::fflush(stderr);
            });
            QTimer::singleShot(9000, &loop, &QEventLoop::quit);
            loop.exec();
            widget.hide();
            return displayedCorrectly ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        bool passed = true;
        auto require = [&passed](bool condition, const char* message) {
            if (!condition) {
                std::fprintf(stderr, "%s\n", message);
                passed = false;
            }
            return condition;
        };
        const QString outputDirectory = QString::fromLocal8Bit(
          qgetenv("FLAMESHOT_TEXT_CURSOR_TEST_OUTPUT_DIR"));
        const QString content = QStringLiteral("等等Ag");

        for (bool pinMode : { true, false }) {
            int previousPreviewHeight = 0;
            int previousTypedHeight = 0;
            for (int pointSize : { 9, 20, 40 }) {
                QPixmap source(900, 600);
                source.fill(Qt::white);
                CaptureWidget widget(source, QRect(20, 20, 900, 600));
                widget.show();
                QApplication::processEvents();
                const auto hideBeforeDestruction = qScopeGuard([&widget]() {
                    widget.hide();
                });
                widget.m_pinEditMode = pinMode;
                widget.m_context.fullscreen = !pinMode;
                widget.updateSelectionState();
                widget.m_context.color = QColor(220, 32, 32);

                CaptureToolButton* textButton = nullptr;
                for (auto* button : widget.findChildren<CaptureToolButton*>()) {
                    if (button->tool()->type() == CaptureTool::TYPE_TEXT) {
                        textButton = button;
                        break;
                    }
                }
                if (!require(textButton, "text cursor fixture has no Text tool")) {
                    return EXIT_FAILURE;
                }
                widget.setState(textButton);
                widget.toolSizeChanged(pointSize - 8);
                auto* prototype = qobject_cast<TextTool*>(textButton->tool());
                const QCursor preview = prototype->insertionCursor(1.0);
                require(!preview.pixmap().isNull(),
                        "Text tool still uses a fixed system cursor");
                require(prototype->size() == pointSize - 8,
                        "Text tool did not receive the selected font size");
                const int previewHeight = preview.pixmap().height();
                require(previewHeight > previousPreviewHeight,
                        "increasing the font did not increase the preview cursor");
                previousPreviewHeight = previewHeight;

                const QPoint anchor(150, 180);
                click(widget, anchor);
                auto* editor = qobject_cast<TextWidget*>(widget.m_toolWidget.data());
                if (!require(editor, "text cursor fixture did not open an input")) {
                    return EXIT_FAILURE;
                }
                editor->setPlainText(content);
                QApplication::processEvents();
                const QRect firstLine = editor->firstLineTextRect();
                const QPoint insertionPoint(editor->firstLineCursorRect().left(),
                                             firstLine.center().y());
                require(editor->pos() + insertionPoint == anchor,
                        "visible text is not centered at the insertion anchor");
                require(firstLine.height() > previousTypedHeight,
                        "visible glyph height did not track the requested font size");
                previousTypedHeight = firstLine.height();

                auto* text = qobject_cast<TextTool*>(widget.m_activeTool.data());
                if (!require(text, "active input lost its Text tool")) {
                    return EXIT_FAILURE;
                }
                for (qreal dpr : { 1.0, 1.75, 2.0 }) {
                    const QCursor cursor = text->insertionCursor(dpr);
                    const QPixmap pixmap = cursor.pixmap();
                    const qreal bufferScale = std::ceil(dpr);
                    require(!pixmap.isNull() &&
                              qAbs(pixmap.devicePixelRatio() - bufferScale) < 0.001,
                            "text cursor buffer scale is not Wayland-compatible");
                    require(pixmap.height() ==
                              static_cast<int>((firstLine.height() + 4) * bufferScale),
                            "text cursor physical height does not match the visible glyphs");
                    // Reproduce what the Wayland compositor actually sees,
                    // rather than only drawing QPixmap with its float DPR.
                    const int protocolScale = static_cast<int>(pixmap.devicePixelRatio());
                    require(protocolScale > 0 &&
                              pixmap.height() / protocolScale == firstLine.height() + 4,
                            "Wayland would display an enlarged cursor buffer");
                    require(cursor.hotSpot().y() == 2 + (firstLine.height() - 1) / 2,
                            "text cursor hotspot is not the logical vertical center");
                    require(cursor.hotSpot().x() >= 0 &&
                              cursor.hotSpot().x() < pixmap.width() / bufferScale,
                            "text cursor hotspot escaped its logical image");
                }

                // QTextEdit can reset its viewport cursor while selecting a
                // word or paragraph. Keep the size-aware pointer after the
                // double-click/release/third-click gesture as well.
                QWidget* viewport = editor->viewport();
                const QPoint viewportPoint = viewport->mapFrom(
                  editor,
                  QPoint(editor->firstLineCursorRect().left() + 2,
                         firstLine.center().y()));
                for (auto type : { QEvent::MouseButtonDblClick,
                                   QEvent::MouseButtonRelease,
                                   QEvent::MouseButtonPress,
                                   QEvent::MouseButtonRelease }) {
                    QMouseEvent event(type,
                                      QPointF(viewportPoint),
                                      QPointF(viewport->mapToGlobal(viewportPoint)),
                                      Qt::LeftButton,
                                      type == QEvent::MouseButtonRelease
                                        ? Qt::NoButton
                                        : Qt::LeftButton,
                                      Qt::NoModifier);
                    QApplication::sendEvent(viewport, &event);
                }
                const QPixmap viewportCursor = editor->viewport()->cursor().pixmap();
                require(editor->viewport()->cursor().shape() == Qt::BitmapCursor &&
                          !viewportCursor.isNull() &&
                          qAbs(viewportCursor.height() /
                                 viewportCursor.devicePixelRatio() -
                               (firstLine.height() + 4)) <= 1.0,
                        "input viewport restored a fixed-size system I-beam");

                // A live size change must refresh both cursors without moving
                // the mouse. Restore the size before comparing text rendering.
                const int otherPointSize = pointSize == 40 ? 9 : pointSize + 5;
                widget.toolSizeChanged(otherPointSize - 8);
                const int changedHeight = editor->firstLineTextRect().height();
                const QPixmap changedCursor = editor->viewport()->cursor().pixmap();
                require(changedHeight != firstLine.height() &&
                          !changedCursor.isNull() &&
                          qAbs(changedCursor.height() / changedCursor.devicePixelRatio() -
                               (changedHeight + 4)) <= 1.0,
                        "live font changes did not refresh the input cursor");
                if (pinMode) {
                    const QPixmap canvasCursor = widget.cursor().pixmap();
                    require(!canvasCursor.isNull() &&
                              qAbs(canvasCursor.height() / canvasCursor.devicePixelRatio() -
                                   (changedHeight + 4)) <= 1.0,
                            "live font changes did not refresh the canvas cursor");
                }
                widget.toolSizeChanged(pointSize - 8);
                QApplication::processEvents();
                const QPoint origin = editor->pos();
                const QRect restoredLine = editor->firstLineTextRect();
                require(origin + QPoint(editor->firstLineCursorRect().left(),
                                        restoredLine.center().y()) ==
                          anchor,
                        "font changes detached the text line from its insertion point");

                const QCursor actualCursor = text->insertionCursor(1.0);
                const QImage editImage = renderEditor(*editor);
                const QRect editInk = redInkBounds(editImage).translated(origin);
                require(!editInk.isEmpty(), "input render contains no colored text");
                const QRect cursorInk = alphaBounds(actualCursor.pixmap().toImage())
                                          .translated(anchor - actualCursor.hotSpot());
                require(qAbs(cursorInk.top() - editInk.top()) <= 2 &&
                          qAbs(cursorInk.bottom() - editInk.bottom()) <= 2,
                        "I-beam caps do not align with the visible text ink");
                require(savePreview(outputDirectory,
                                    pinMode ? QStringLiteral("pin") : QStringLiteral("capture"),
                                    pointSize,
                                    editImage,
                                    origin,
                                    anchor,
                                    restoredLine,
                                    actualCursor),
                        "failed to save text cursor visual verification image");

                widget.commitCurrentTool();
                const QRect committedInk = redInkBounds(widget.m_context.screenshot.toImage());
                require(!committedInk.isEmpty(), "committing text lost its visible pixels");
                require(qAbs(editInk.top() - committedInk.top()) <= 1 &&
                          qAbs(editInk.bottom() - committedInk.bottom()) <= 1,
                        "input and committed text do not share the same vertical ink bounds");
                require(qAbs(editInk.left() - committedInk.left()) <= 1,
                        "input and committed text do not share the same left edge");
                require(widget.m_captureToolObjects.size() == 1 &&
                          *widget.m_captureToolObjects.at(0)->pos() == origin,
                        "text commit changed the insertion origin or added a duplicate layer");

                click(widget, widget.m_captureToolObjects.at(0)->boundingRect().center());
                editor = qobject_cast<TextWidget*>(widget.m_toolWidget.data());
                require(editor && editor->pos() == origin && editor->toPlainText() == content,
                        "reopening text reapplied the creation offset");
                widget.commitCurrentTool();
                std::fprintf(stderr,
                             "Text cursor %s %d pt: glyphs %d px, ink y %d..%d\n",
                             pinMode ? "pin" : "capture",
                             pointSize,
                             restoredLine.height(),
                             committedInk.top(),
                             committedInk.bottom());
            }
        }
        std::fprintf(stderr,
                     "Text cursor pixel test %s\n",
                     passed ? "passed" : "failed");
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    }
};

int runCaptureWidgetTextCursorTest(int argc, char* argv[])
{
    return CaptureWidgetTextCursorTest::run(argc, argv);
}
