// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "pineditorlayout.h"

#include <QPainter>

PinEditorLayout createPinEditorLayout(const QPixmap& content,
                                      const QRect& globalContentGeometry,
                                      const QRect& screenGeometry,
                                      qreal devicePixelRatio,
                                      int windowMargin)
{
    const qreal dpr = devicePixelRatio > 0.0 ? devicePixelRatio : 1.0;
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

    QPixmap canvas(qRound(windowGeometry.width() * dpr),
                   qRound(windowGeometry.height() * dpr));
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(Qt::transparent);

    const QRect contentGeometry(QPoint(margin, margin),
                                contentWindowGeometry.size());

    if (!content.isNull() && !contentGeometry.isEmpty()) {
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(contentGeometry, content, content.rect());
    }

    const QRect initialSelection(qRound(contentGeometry.x() * dpr),
                                 qRound(contentGeometry.y() * dpr),
                                 qRound(contentGeometry.width() * dpr),
                                 qRound(contentGeometry.height() * dpr));
    return { canvas, windowGeometry, contentGeometry, initialSelection };
}
