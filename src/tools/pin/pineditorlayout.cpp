// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "pineditorlayout.h"

#include <QColor>
#include <QPainter>

PinEditorLayout createPinEditorLayout(const QPixmap& content,
                                      const QRect& globalContentGeometry,
                                      const QRect& screenGeometry,
                                      qreal devicePixelRatio)
{
    const qreal dpr = devicePixelRatio > 0.0 ? devicePixelRatio : 1.0;
    QPixmap canvas(qRound(screenGeometry.width() * dpr),
                   qRound(screenGeometry.height() * dpr));
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(QColor(32, 32, 32));

    QRect requestedGeometry = globalContentGeometry;
    if (requestedGeometry.isNull()) {
        requestedGeometry = QRect(
          QPoint(0, 0),
          QSize(qRound(content.width() / content.devicePixelRatio()),
                qRound(content.height() / content.devicePixelRatio())));
    } else {
        requestedGeometry.translate(-screenGeometry.topLeft());
    }
    const QRect contentGeometry = requestedGeometry.intersected(
      QRect(QPoint(0, 0), screenGeometry.size()));

    if (!content.isNull() && !contentGeometry.isEmpty()) {
        const qreal scaleX = static_cast<qreal>(content.width()) /
                             qMax(1, requestedGeometry.width());
        const qreal scaleY = static_cast<qreal>(content.height()) /
                             qMax(1, requestedGeometry.height());
        const QRectF source((contentGeometry.x() - requestedGeometry.x()) *
                              scaleX,
                            (contentGeometry.y() - requestedGeometry.y()) *
                              scaleY,
                            contentGeometry.width() * scaleX,
                            contentGeometry.height() * scaleY);
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(contentGeometry, content, source);
    }

    const QRect initialSelection(qRound(contentGeometry.x() * dpr),
                                 qRound(contentGeometry.y() * dpr),
                                 qRound(contentGeometry.width() * dpr),
                                 qRound(contentGeometry.height() * dpr));
    return { canvas, contentGeometry, initialSelection };
}
