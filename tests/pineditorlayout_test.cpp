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
      2.0);

    require(layout.canvas.size() == QSize(40, 20),
            "editor canvas physical size does not respect DPR");
    require(qFuzzyCompare(layout.canvas.devicePixelRatio(), 2.0),
            "editor canvas lost DPR");
    require(layout.windowGeometry == QRect(120, 70, 20, 10),
            "pin editor did not preserve the pinned window geometry");
    require(layout.contentGeometry == QRect(0, 0, 20, 10),
            "pin content does not fill the editor window");
    require(layout.initialSelection == QRect(0, 0, 40, 20),
            "initial selection does not fill the editor window");

    const QImage image = layout.canvas.toImage();
    require(image.pixelColor(0, 0) == QColor(Qt::red) &&
              image.pixelColor(39, 19) == QColor(Qt::red),
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

    qInfo() << "pin editor layout tests passed";
    return EXIT_SUCCESS;
}
