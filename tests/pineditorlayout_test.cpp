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

    require(layout.canvas.size() == QSize(400, 200),
            "editor canvas physical size does not respect DPR");
    require(qFuzzyCompare(layout.canvas.devicePixelRatio(), 2.0),
            "editor canvas lost DPR");
    require(layout.contentGeometry == QRect(20, 20, 20, 10),
            "global pin geometry was not converted to screen-local geometry");
    require(layout.initialSelection == QRect(40, 40, 40, 20),
            "initial selection is not in physical screen-local coordinates");

    const QImage image = layout.canvas.toImage();
    require(image.pixelColor(50, 50) == QColor(Qt::red),
            "pinned image was not placed at the expected canvas position");
    require(image.pixelColor(10, 10) == QColor(32, 32, 32),
            "editor background was unexpectedly modified");

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
    require(highDpiCanvas.pixelColor(45, 50) == QColor(Qt::red) &&
              highDpiCanvas.pixelColor(75, 50) == QColor(Qt::blue),
            "high-DPI pin content was cropped or scaled incorrectly");

    qInfo() << "pin editor layout tests passed";
    return EXIT_SUCCESS;
}
