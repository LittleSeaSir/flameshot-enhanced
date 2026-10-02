// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/screenshotscale.h"

#include <QtMath>

#include <cstdlib>
#include <iostream>

namespace
{
void requireScale(const QSize& physical,
                  const QSize& logical,
                  qreal expected,
                  const char* message)
{
    if (qAbs(screenshotDevicePixelRatio(physical, logical) - expected) >
        0.0001) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    requireScale(QSize(3840, 2160),
                 QSize(2194, 1234),
                 1.75,
                 "175% Wayland scale was rounded to the 2x buffer DPR");
    requireScale(QSize(2560, 1440),
                 QSize(1280, 720),
                 2.0,
                 "integer 2x scale was not preserved");
    requireScale(QSize(1920, 1080),
                 QSize(1920, 1080),
                 1.0,
                 "1x scale was not preserved");
    requireScale(QSize(), QSize(1920, 1080), 1.0, "empty input is invalid");
    return EXIT_SUCCESS;
}
