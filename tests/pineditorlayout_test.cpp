// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "tools/pin/pineditorlayout.h"

#include <QApplication>
#include <QColor>

#include <cstdlib>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        qCritical() << message;
        std::exit(EXIT_FAILURE);
    }
}

}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QPixmap content(20, 10);
    content.fill(Qt::red);

    const PinEditorLayout layout = createPinEditorLayout(
      content,
      QRect(120, 70, 20, 10),
      QRect(100, 50, 200, 100),
      1.0);

    require(layout.canvas.size() == QSize(20, 10),
            "editor canvas physical size does not respect DPR");
    require(qFuzzyCompare(layout.canvas.devicePixelRatio(), 1.0),
            "editor canvas lost DPR");
    require(layout.windowGeometry == QRect(120, 70, 20, 10),
            "pin editor did not preserve the pinned window geometry");
    require(layout.contentGeometry == QRect(0, 0, 20, 10),
            "pin content does not fill the editor window");
    require(layout.initialSelection == QRect(0, 0, 20, 10),
            "initial selection does not fill the editor window");

    const QImage image = layout.canvas.toImage();
    require(image.pixelColor(0, 0) == QColor(Qt::red) &&
              image.pixelColor(19, 9) == QColor(Qt::red),
            "pinned image does not cover the entire editor window");

    QImage highDpiImage(40, 20, QImage::Format_ARGB32_Premultiplied);
    highDpiImage.fill(Qt::red);
    for (int x = 20; x < highDpiImage.width(); ++x) {
        for (int y = 0; y < highDpiImage.height(); ++y) {
            highDpiImage.setPixelColor(x, y, Qt::blue);
        }
    }
    QPixmap highDpiContent = QPixmap::fromImage(highDpiImage);
    highDpiContent.setDevicePixelRatio(2.0);
    const PinEditorLayout highDpiLayout = createPinEditorLayout(
      highDpiContent,
      QRect(120, 70, 20, 10),
      QRect(100, 50, 200, 100),
      2.0);
    const QImage highDpiCanvas = highDpiLayout.canvas.toImage();
    require(highDpiCanvas.pixelColor(5, 10) == QColor(Qt::red) &&
              highDpiCanvas.pixelColor(35, 10) == QColor(Qt::blue),
            "high-DPI pin content was cropped or scaled incorrectly");

    const PinEditorLayout inPlaceLayout = createPinEditorLayout(
      content,
      QRect(120, 70, 20, 10),
      QRect(100, 50, 200, 100),
      1.0,
      PIN_WINDOW_MARGIN);
    require(inPlaceLayout.windowGeometry == QRect(113, 63, 34, 24),
            "pin editor outer geometry does not preserve the pin margin");
    require(inPlaceLayout.contentGeometry == QRect(7, 7, 20, 10),
            "pin content moved inside the in-place editor");
    require(inPlaceLayout.initialSelection == QRect(7, 7, 20, 10),
            "pin editor selection does not match the original content");
    require(inPlaceLayout.windowGeometry.topLeft() +
                inPlaceLayout.contentGeometry.topLeft() ==
              QPoint(120, 70),
            "accepting a pin edit changes its global content position");

    QImage fractionalImage(
      1414, 890, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < fractionalImage.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(fractionalImage.scanLine(y));
        for (int x = 0; x < fractionalImage.width(); ++x) {
            line[x] = qRgba(x % 251, y % 241, (x + y) % 239, 255);
        }
    }
    QPixmap fractionalContent = QPixmap::fromImage(fractionalImage);
    fractionalContent.setDevicePixelRatio(1.75);
    const QRect fractionalLogicalGeometry(320, 180, 808, 509);
    const PinEditorLayout fractionalLayout = createPinEditorLayout(
      fractionalContent,
      fractionalLogicalGeometry,
      QRect(0, 0, 2194, 1234),
      2.0,
      PIN_WINDOW_MARGIN);
    require(qFuzzyCompare(fractionalLayout.canvas.devicePixelRatio(), 1.75),
            "pin editor used the rounded screen DPR instead of image DPR");
    require(qFuzzyCompare(fractionalContent.devicePixelRatio(), 1.75),
            "laying out the editor changed the source image DPR");
    require(fractionalLayout.canvas.size() == QSize(1439, 915),
            "fractional-DPR canvas changed the source pixel dimensions");
    require(fractionalLayout.initialSelection ==
              QRect(12, 12, 1414, 890),
            "fractional-DPR selection is not pixel exact");

    QImage roundTripped =
      fractionalLayout.canvas.copy(fractionalLayout.initialSelection).toImage();
    roundTripped.setDevicePixelRatio(1.0);
    fractionalImage.setDevicePixelRatio(1.0);
    require(roundTripped == fractionalImage,
            "re-edit resampled or modified the pinned image pixels");

    QPixmap firstEditedPin =
      fractionalLayout.canvas.copy(fractionalLayout.initialSelection);
    require(qFuzzyCompare(firstEditedPin.devicePixelRatio(), 1.75),
            "re-edit export lost the source DPR");
    const PinEditorLayout secondFractionalLayout = createPinEditorLayout(
      firstEditedPin,
      fractionalLogicalGeometry,
      QRect(0, 0, 2194, 1234),
      2.0,
      PIN_WINDOW_MARGIN);
    QImage secondRoundTrip = secondFractionalLayout.canvas
                               .copy(secondFractionalLayout.initialSelection)
                               .toImage();
    secondRoundTrip.setDevicePixelRatio(1.0);
    require(secondRoundTrip == fractionalImage,
            "repeated re-edit accumulated pixel resampling");

    const QRect captureGeometry = pinCaptureContentGeometry(
      fractionalLogicalGeometry,
      QPoint(0, 0),
      fractionalContent.size(),
      fractionalContent.devicePixelRatio());
    require(pinLogicalContentGeometry(captureGeometry,
                                      QPoint(0, 0),
                                      fractionalContent.devicePixelRatio()) ==
              fractionalLogicalGeometry,
            "pin position or size changed after a re-edit round trip");

    const QPoint secondaryScreenTopLeft(-2194, 120);
    const QRect secondaryLogicalGeometry(-2030, 260, 808, 509);
    const QRect secondaryCaptureGeometry = pinCaptureContentGeometry(
      secondaryLogicalGeometry,
      secondaryScreenTopLeft,
      fractionalContent.size(),
      fractionalContent.devicePixelRatio());
    require(pinLogicalContentGeometry(secondaryCaptureGeometry,
                                      secondaryScreenTopLeft,
                                      fractionalContent.devicePixelRatio()) ==
              secondaryLogicalGeometry,
            "pin position changed on an offset monitor");

    qInfo() << "pin editor layout tests passed";
    return EXIT_SUCCESS;
}
