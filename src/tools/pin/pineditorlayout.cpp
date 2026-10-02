// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "pineditorlayout.h"

#include <QPainter>
#include <QtMath>

namespace {

qreal validDevicePixelRatio(qreal devicePixelRatio)
{
    return devicePixelRatio > 0.0 ? devicePixelRatio : 1.0;
}

}

QRect pinLogicalContentGeometry(const QRect& captureGeometry,
                                const QPoint& screenTopLeft,
                                qreal devicePixelRatio)
{
    const qreal dpr = validDevicePixelRatio(devicePixelRatio);
    return { screenTopLeft.x() +
               qRound((captureGeometry.x() - screenTopLeft.x()) / dpr),
             screenTopLeft.y() +
               qRound((captureGeometry.y() - screenTopLeft.y()) / dpr),
             qRound(captureGeometry.width() / dpr),
             qRound(captureGeometry.height() / dpr) };
}

QRect pinCaptureContentGeometry(const QRect& logicalGeometry,
                                const QPoint& screenTopLeft,
                                const QSize& physicalSize,
                                qreal devicePixelRatio)
{
    const qreal dpr = validDevicePixelRatio(devicePixelRatio);
    return { screenTopLeft.x() +
               qRound((logicalGeometry.x() - screenTopLeft.x()) * dpr),
             screenTopLeft.y() +
               qRound((logicalGeometry.y() - screenTopLeft.y()) * dpr),
             physicalSize.width(),
             physicalSize.height() };
}

PinEditorLayout createPinEditorLayout(const QPixmap& content,
                                      const QRect& globalContentGeometry,
                                      const QRect& screenGeometry,
                                      qreal devicePixelRatio,
                                      int windowMargin)
{
    const qreal contentDpr = content.devicePixelRatio();
    const qreal dpr = !content.isNull() && contentDpr > 0.0
                        ? contentDpr
                        : validDevicePixelRatio(devicePixelRatio);
    const int margin = qMax(0, windowMargin);
    QRect contentWindowGeometry = globalContentGeometry;
    if (contentWindowGeometry.isNull()) {
        contentWindowGeometry = QRect(
          screenGeometry.topLeft(),
          QSize(qRound(content.width() / content.devicePixelRatio()),
                qRound(content.height() / content.devicePixelRatio())));
    }
    const QRect windowGeometry = contentWindowGeometry.adjusted(
      -margin, -margin, margin, margin);

    const int physicalMargin = qRound(margin * dpr);
    QSize canvasSize(qRound(windowGeometry.width() * dpr),
                     qRound(windowGeometry.height() * dpr));
    if (!content.isNull()) {
        canvasSize = canvasSize.expandedTo(
          content.size() +
          QSize(physicalMargin * 2, physicalMargin * 2));
    }
    QPixmap canvas(canvasSize);
    canvas.fill(Qt::transparent);

    const QRect contentGeometry(QPoint(margin, margin),
                                contentWindowGeometry.size());

    if (!content.isNull() && !contentGeometry.isEmpty()) {
        // Paint while both pixmaps use physical-pixel coordinates. Supplying a
        // target rectangle here would resample fractional-DPR content (for
        // example 1414x890 @ 1.75 becomes 1414x891).
        QPixmap physicalContent = content;
        physicalContent.setDevicePixelRatio(1.0);
        QPainter painter(&canvas);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawPixmap(QPoint(physicalMargin, physicalMargin),
                           physicalContent);
    }
    canvas.setDevicePixelRatio(dpr);

    const QRect initialSelection(
      QPoint(physicalMargin, physicalMargin),
      content.isNull()
        ? QSize(qRound(contentGeometry.width() * dpr),
                qRound(contentGeometry.height() * dpr))
        : content.size());
    return { canvas, windowGeometry, contentGeometry, initialSelection };
}
