// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Yurii Puchkov & Contributors

#include "capturetoolobjects.h"

#define SEARCH_RADIUS_NEAR 3
#define SEARCH_RADIUS_TEXT_HANDICAP 5

CaptureToolObjects::CaptureToolObjects(QObject* parent)
  : QObject(parent)
{}

void CaptureToolObjects::append(const QPointer<CaptureTool>& captureTool)
{
    if (!captureTool.isNull()) {
        m_captureToolObjects.append(captureTool->copy(captureTool->parent()));
    }
}

void CaptureToolObjects::insert(int index,
                                const QPointer<CaptureTool>& captureTool)
{
    if (!captureTool.isNull() && index >= 0 &&
        index <= m_captureToolObjects.size()) {
        m_captureToolObjects.insert(index,
                                    captureTool->copy(captureTool->parent()));
    }
}

QPointer<CaptureTool> CaptureToolObjects::at(int index)
{
    if (index >= 0 && index < m_captureToolObjects.size()) {
        return m_captureToolObjects[index];
    }
    return nullptr;
}

void CaptureToolObjects::clear()
{
    m_captureToolObjects.clear();
}

QList<QPointer<CaptureTool>> CaptureToolObjects::captureToolObjects()
{
    return m_captureToolObjects;
}

int CaptureToolObjects::size()
{
    return m_captureToolObjects.size();
}

void CaptureToolObjects::removeAt(int index)
{
    if (index >= 0 && index < m_captureToolObjects.size()) {
        m_captureToolObjects.removeAt(index);
    }
}

int CaptureToolObjects::find(const QPoint& pos,
                             QSize captureSize,
                             int radius)
{
    if (m_captureToolObjects.empty()) {
        return -1;
    }
    radius = qMax(0, radius);
    QPixmap pixmap(captureSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    // Keep the legacy two-stage lookup for normal object selection. Whole
    // stroke erasing supplies its brush radius directly.
    int index = findWithRadius(
      painter, pixmap, pos, qMin(radius, SEARCH_RADIUS_NEAR));
    if (-1 == index && radius > SEARCH_RADIUS_NEAR) {
        // second attempt to find at position with radius
        index = findWithRadius(painter, pixmap, pos, radius);
    }
    return index;
}

int CaptureToolObjects::findWithRadius(QPainter& painter,
                                       QPixmap& pixmap,
                                       const QPoint& pos,
                                       int radius)
{
    int index = m_captureToolObjects.size() - 1;
    for (; index >= 0; --index) {
        int currentRadius = radius;
        auto toolItem = m_captureToolObjects.at(index);
        if (!toolItem ||
            !toolItem->boundingRect()
               .adjusted(-currentRadius,
                         -currentRadius,
                         currentRadius,
                         currentRadius)
               .contains(pos)) {
            continue;
        }

        // Each candidate needs an isolated buffer; otherwise pixels from a
        // higher layer can cause a false hit on every layer below it.
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(pixmap.rect(), Qt::transparent);
        painter.restore();
        toolItem->drawSearchArea(painter, pixmap);
        painter.end();
        const QImage image = pixmap.toImage();
        painter.begin(&pixmap);

        if (toolItem->type() == CaptureTool::TYPE_TEXT) {
            if (currentRadius > SEARCH_RADIUS_NEAR) {
                // Text already has a big currentRadius and no need to search
                // with a bit bigger currentRadius than
                // SEARCH_RADIUS_TEXT_HANDICAP + SEARCH_RADIUS_NEAR
                continue;
            }

            // Text has spaces inside to need to take a bigger currentRadius for
            // text objects search
            currentRadius += SEARCH_RADIUS_TEXT_HANDICAP;
        }

        const int left = qMax(0, pos.x() - currentRadius);
        const int right = qMin(image.width() - 1, pos.x() + currentRadius);
        const int top = qMax(0, pos.y() - currentRadius);
        const int bottom = qMin(image.height() - 1, pos.y() + currentRadius);
        for (int x = left; x <= right;
             ++x) {
            for (int y = top; y <= bottom;
                 ++y) {
                QRgb rgb = image.pixel(x, y);
                if (rgb != 0) {
                    // object was found, return it index (layer index)
                    return index;
                }
            }
        }
    }
    // no object at current pos found
    return -1;
}

CaptureToolObjects& CaptureToolObjects::operator=(
  const CaptureToolObjects& other)
{
    // remove extra items for this if size is bigger
    while (this->m_captureToolObjects.size() >
           other.m_captureToolObjects.size()) {
        this->m_captureToolObjects.removeLast();
    }

    int count = 0;
    for (const auto& item : other.m_captureToolObjects) {
        QPointer<CaptureTool> itemCopy = item->copy(item->parent());
        if (count < this->m_captureToolObjects.size()) {
            this->m_captureToolObjects[count] = itemCopy;
        } else {
            this->m_captureToolObjects.append(itemCopy);
        }
        count++;
    }
    return *this;
}
