// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "textwidget.h"

#include <QCache>
#include <QEvent>
#include <QPainter>
#include <QGlyphRun>
#include <QTextBlock>
#include <QTextLayout>
#include <QtMath>

namespace {

QRectF lineInkBounds(const QTextLayout& layout, const QTextLine& line)
{
    QRectF bounds;
    for (const QGlyphRun& run : layout.glyphRuns(line.textStart(), line.textLength())) {
        const auto glyphs = run.glyphIndexes();
        const auto positions = run.positions();
        const QRawFont font = run.rawFont();
        for (qsizetype i = 0; i < glyphs.size(); ++i) {
            const QRectF ink = font.boundingRect(glyphs.at(i));
            if (!ink.isEmpty()) {
                bounds = bounds.united(ink.translated(positions.at(i)));
            }
        }
    }
    return bounds;
}

QRectF sampleInkBounds(const QFont& font)
{
    QTextLayout layout(QStringLiteral("国Ag"), font);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (line.isValid()) {
        line.setLineWidth(10000);
    }
    layout.endLayout();
    return line.isValid() ? lineInkBounds(layout, line) : QRectF();
}

QRect roundedInkBounds(const QRectF& bounds)
{
    // Glyph metrics contain fractional baselines. Round pixel boundaries
    // rather than expanding them, which would add a spurious row of pixels.
    const int left = qRound(bounds.left());
    const int top = qRound(bounds.top());
    return QRect(left, top,
                 qMax(1, qRound(bounds.right()) - left),
                 qMax(1, qRound(bounds.bottom()) - top));
}

} // namespace

TextWidget::TextWidget(QWidget* parent)
  : QTextEdit(parent)
{
    setStyleSheet(QStringLiteral("TextWidget { background: transparent; }"));
    connect(this, &TextWidget::textChanged, this, &TextWidget::adjustSize);
    connect(this, &TextWidget::textChanged, this, &TextWidget::emitTextUpdated);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setContextMenuPolicy(Qt::NoContextMenu);
}

void TextWidget::showEvent(QShowEvent* e)
{
    QFont font;
    QFontMetrics fm(font);
    setFixedWidth(fm.lineSpacing() * 6);
    setFixedHeight(fm.lineSpacing() * 2.5);
    m_baseSize = size();
    m_minSize = m_baseSize;
    QTextEdit::showEvent(e);
    adjustSize();
}

void TextWidget::resizeEvent(QResizeEvent* e)
{
    m_minSize.setHeight(qMin(m_baseSize.height(), height()));
    m_minSize.setWidth(qMin(m_baseSize.width(), width()));
    QTextEdit::resizeEvent(e);
}

bool TextWidget::viewportEvent(QEvent* e)
{
    const bool handled = QTextEdit::viewportEvent(e);
    switch (e->type()) {
        case QEvent::Enter:
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
            // QTextEdit restores its fixed-size cursor after paragraph/list
            // selection. Preserve the glyph-sized pointer after that logic.
            viewport()->setCursor(insertionCursor(firstLineTextRect().height(),
                                                   devicePixelRatioF()));
            break;
        default:
            break;
    }
    return handled;
}

void TextWidget::setFont(const QFont& f)
{
    QTextEdit::setFont(f);
    adjustSize();
}

QRect TextWidget::firstLineCursorRect() const
{
    QTextCursor first(document());
    first.movePosition(QTextCursor::Start);
    const QRect caret = cursorRect(first);
    return QRect(viewport()->mapTo(this, caret.topLeft()), caret.size());
}

QRect TextWidget::firstLineTextRect() const
{
    // cursorRect forces QTextDocument to finish its current layout first.
    const QRect caret = firstLineCursorRect();
    const QTextLayout* layout = document()->firstBlock().layout();
    if (layout && layout->lineCount() > 0) {
        const QRectF ink = lineInkBounds(*layout, layout->lineAt(0));
        if (!ink.isEmpty()) {
            return roundedInkBounds(ink.translated(
              layout->position() + QPointF(viewport()->pos())));
        }
    }
    // Before typing, preview representative CJK/Latin glyphs at this font.
    const QRectF sample = sampleInkBounds(font());
    return sample.isEmpty() ? caret
                            : roundedInkBounds(sample.translated(caret.topLeft()));
}

int TextWidget::fontTextHeight(const QFont& font)
{
    const QRectF sample = sampleInkBounds(font);
    return sample.isEmpty() ? QFontMetrics(font).height()
                            : roundedInkBounds(sample).height();
}

QCursor TextWidget::insertionCursor(int lineHeight, qreal devicePixelRatio)
{
    const int height = qMax(8, lineHeight);
    // Wayland wl_surface.buffer_scale is an integer. Qt 6.11 truncates a
    // bitmap cursor's fractional DPR when sending it, without resampling the
    // image: a 1.75x bitmap then appears 1.75x too large. Supersample at the
    // next whole scale so both Qt's hotspot and Wayland's buffer agree.
    const qreal scale = qMax(1, qCeil(devicePixelRatio));
    const QString key = QStringLiteral("%1:%2").arg(height).arg(scale);
    static QCache<QString, QCursor> cursors(32);
    if (const auto* cached = cursors.object(key)) {
        return *cached;
    }

    constexpr int padding = 2;
    const int capWidth = qMax(5, qRound(height * 0.25)) | 1;
    const int width = capWidth + padding * 2;
    const int centerX = width / 2;
    QPixmap pixmap(qCeil(width * scale),
                   qCeil((height + padding * 2) * scale));
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal top = padding + 1.5;
        const qreal bottom = padding + height - 2.5;
        const qreal left = centerX - (capWidth - 3) / 2.0;
        const qreal right = centerX + (capWidth - 3) / 2.0;
        const qreal stemX = centerX;
        const QLineF lines[] = { { stemX, top, stemX, bottom },
                                { left, top, right, top },
                                { left, bottom, right, bottom } };
        // A contrasting outline stays legible on light and dark captures.
        painter.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap));
        painter.drawLines(lines, 3);
        painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::RoundCap));
        painter.drawLines(lines, 3);
    }
    // QCursor hotspots use logical pixels even for a high-DPI pixmap.
    const QCursor cursor(pixmap, centerX, padding + (height - 1) / 2);
    cursors.insert(key, new QCursor(cursor));
    return cursor;
}

void TextWidget::setAlignment(Qt::AlignmentFlag alignment)
{
    QTextEdit::setAlignment(alignment);
    adjustSize();
}
void TextWidget::setTextColor(const QColor& c)
{
    QString s(
      QStringLiteral("TextWidget { background: transparent; color: %1; }"));
    setStyleSheet(s.arg(c.name()));
}

void TextWidget::adjustSize()
{
    QString&& text = this->toPlainText();

    QFontMetrics fm(font());
    QRect bounds = fm.boundingRect(QRect(), 0, text);
    int pixelsWide = bounds.width() + fm.lineSpacing();
    int pixelsHigh = bounds.height() * 1.15 + fm.lineSpacing();
    if (pixelsWide < m_minSize.width()) {
        pixelsWide = m_minSize.width();
    }
    if (pixelsHigh < m_minSize.height()) {
        pixelsHigh = m_minSize.height();
    }

    this->setFixedSize(pixelsWide, pixelsHigh);
    const QCursor pointer = insertionCursor(firstLineTextRect().height(),
                                            devicePixelRatioF());
    setCursor(pointer);
    viewport()->setCursor(pointer);
    emit textLayoutChanged();
}

void TextWidget::emitTextUpdated()
{
    emit textUpdated(this->toPlainText());
}
