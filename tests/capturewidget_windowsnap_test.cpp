// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "widgets/capture/capturewidget.h"

#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPixmap>
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

void hover(CaptureWidget& widget, const QPoint& pos)
{
    sendMouseEvent(
      widget, QEvent::MouseMove, pos, Qt::NoButton, Qt::NoButton);
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

class CaptureWidgetWindowSnapTest
{
public:
    static int run(int argc, char* argv[])
    {
        QCoreApplication::setApplicationName(
          QStringLiteral("flameshot-window-snap-test"));
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

        // Model a 400 x 300 logical screen captured at 175%. The fractional
        // top window is deliberately chosen so a naive integer-preview/DPR
        // conversion loses a pixel at its right and bottom edges.
        const QRectF screenGeometry(100.0, 50.0, 400.0, 300.0);
        const QRectF bottomWindow(105.0, 65.0, 250.0, 180.0);
        const QRectF topWindow(110.5, 70.5, 100.5, 60.5);
        const QVector<QRectF> bottomToTop{ bottomWindow, topWindow };
        const QPointF overlapGlobal(150.0, 100.0);
        const QPoint overlapLocal =
          (overlapGlobal - screenGeometry.topLeft()).toPoint();

        const QRect expectedPreview(10, 20, 101, 61);
        const QRect expectedPhysical(18, 35, 177, 107);
        // Portal images and integer QScreen geometry need not divide to the
        // exact scale (for example 701 / 400). Window endpoints must use the
        // pixmap's authoritative DPR, while a full-screen fallback still
        // returns every available source pixel.
        const QRect screenshotBounds(0, 0, 701, 526);

        QImage sourceImage(screenshotBounds.size(), QImage::Format_ARGB32);
        sourceImage.fill(Qt::black);
        sourceImage.setPixelColor(expectedPhysical.topLeft(), Qt::red);
        sourceImage.setPixelColor(expectedPhysical.bottomRight(), Qt::green);
        QPixmap source = QPixmap::fromImage(sourceImage);
        source.setDevicePixelRatio(1.75);

        // The preloaded constructor gives the test a fully initialized editor.
        // Reset only the capture-specific state before invoking the same snap
        // and mouse-event paths used by a normal fullscreen capture.
        CaptureWidget widget(source, QRect(20, 20, 400, 300));
        widget.m_pinEditMode = false;
        widget.resize(screenGeometry.size().toSize());
        widget.m_context.screenshot = source;
        widget.m_context.origScreenshot = source;
        widget.m_context.fullscreen = true;
        widget.m_context.widgetOffset = screenGeometry.topLeft().toPoint();

        widget.startWindowSnapPreview(
          bottomToTop, screenGeometry, overlapGlobal);

        require(widget.m_windowSnapPreviewActive,
                "window snap did not enter preview mode");
        require(widget.m_selection->mouseInteraction() ==
                  SelectionWidget::MouseInteraction::Preview,
                "window snap did not lock selection handles during preview");
        require(widget.m_selection->geometry() == expectedPreview,
                "overlap did not select the top-most window");
        require(widget.m_windowSnapPhysicalSelection == expectedPhysical,
                "fractional window was mapped to the wrong physical pixels");
        require(widget.m_context.selection == expectedPhysical,
                "preview did not preserve the exact physical crop");
        require(expectedPhysical.right() == 194 &&
                  expectedPhysical.bottom() == 141,
                "fractional test fixture does not exercise endpoint rounding");

        // Empty space falls back to the full current screen rather than
        // retaining the most recently hovered window.
        const QPoint blankLocal(390, 290);
        hover(widget, blankLocal);
        require(widget.m_selection->geometry() == QRect(0, 0, 400, 300),
                "blank desktop space did not snap to the full screen");
        require(widget.m_windowSnapPhysicalSelection == screenshotBounds &&
                  widget.m_context.selection == screenshotBounds,
                "full-screen fallback did not use every captured pixel");

        // Return to the overlap and click once. This locks the candidate and
        // restores normal selection interaction without recomputing the crop
        // from the rounded logical preview.
        hover(widget, overlapLocal);
        require(widget.m_selection->geometry() == expectedPreview,
                "hovering back over windows did not restore the top target");
        press(widget, overlapLocal);
        release(widget, overlapLocal);

        require(!widget.m_windowSnapPreviewActive,
                "single click did not lock the snapped selection");
        require(widget.m_selection->mouseInteraction() ==
                  SelectionWidget::MouseInteraction::Full,
                "single click did not restore full selection interaction");
        require(widget.m_windowSnapPhysicalSelectionValid,
                "single click discarded the exact physical selection");
        require(widget.extendedSelection() == expectedPhysical &&
                  widget.m_context.selection == expectedPhysical,
                "locked window crop changed after leaving preview mode");

        const QImage cropped = widget.pixmap().toImage();
        require(cropped.size() == expectedPhysical.size(),
                "locked selection exported the wrong physical pixel size");
        require(cropped.pixelColor(0, 0) == QColor(Qt::red) &&
                  cropped.pixelColor(cropped.width() - 1,
                                     cropped.height() - 1) ==
                    QColor(Qt::green),
                "locked selection lost a physical edge pixel at 175% scale");

        // A movement beyond the gesture threshold switches from the automatic
        // candidate to a normal manual selection. The old exact-pixel override
        // must not leak into the newly drawn rectangle.
        widget.startWindowSnapPreview(
          bottomToTop, screenGeometry, overlapGlobal);
        const QPoint manualFrom(250, 200);
        const QPoint manualTo(300, 240);
        const QRect expectedManual =
          QRect(manualFrom, manualTo).normalized().intersected(widget.rect());
        press(widget, manualFrom);
        drag(widget, manualTo);

        require(widget.m_windowSnapManualDrag,
                "drag beyond threshold did not switch to manual selection");
        require(!widget.m_windowSnapPhysicalSelectionValid,
                "manual drag retained the snapped physical override");
        require(widget.m_selection->geometry() == expectedManual,
                "manual drag produced the wrong logical selection");

        release(widget, manualTo);
        const QRect expectedManualPhysical(
          static_cast<int>(expectedManual.x() * 1.75),
          static_cast<int>(expectedManual.y() * 1.75),
          static_cast<int>(expectedManual.width() * 1.75),
          static_cast<int>(expectedManual.height() * 1.75));
        require(!widget.m_windowSnapPreviewActive &&
                  widget.m_selection->mouseInteraction() ==
                    SelectionWidget::MouseInteraction::Full,
                "manual drag did not finish in normal selection mode");
        require(!widget.m_windowSnapPhysicalSelectionValid,
                "manual selection restored a stale snapped crop on release");
        require(widget.extendedSelection() == expectedManualPhysical &&
                  widget.m_context.selection == expectedManualPhysical,
                "manual selection did not use its normal DPR conversion");

        if (passed) {
            qInfo() << "CaptureWidget window snap test passed";
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }
};

int runCaptureWidgetWindowSnapTest(int argc, char* argv[])
{
    return CaptureWidgetWindowSnapTest::run(argc, argv);
}
