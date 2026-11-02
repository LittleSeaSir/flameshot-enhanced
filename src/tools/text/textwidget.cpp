// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "textwidget.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCache>
#include <QEvent>
#include <QFocusEvent>
#include <QGlyphRun>
#include <QInputMethodEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
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

void drawInsertionLines(QPainter& painter, const QLineF* lines, int count)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawLines(lines, count);
    painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::RoundCap));
    painter.drawLines(lines, count);
}

} // namespace

TextWidget::TextWidget(QWidget* parent)
  : QTextEdit(parent)
  , m_caretTimer(new QTimer(this))
{
    setStyleSheet(QStringLiteral("TextWidget { background: transparent; }"));
    connect(this, &TextWidget::textChanged, this, &TextWidget::adjustSize);
    connect(this, &TextWidget::textChanged, this, &TextWidget::emitTextUpdated);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setContextMenuPolicy(Qt::NoContextMenu);
    // QTextEdit's native caret uses the whole font line, including leading.
    // Paint our caret with the same glyph bounds and strokes as the pointer.
    QTextEdit::setCursorWidth(0);
    connect(m_caretTimer, &QTimer::timeout, this, [this]() {
        m_caretVisible = !m_caretVisible;
        viewport()->update();
    });
    connect(this, &QTextEdit::cursorPositionChanged,
            this, &TextWidget::restartCaretBlink);
    connect(this, &QTextEdit::selectionChanged,
            this, &TextWidget::restartCaretBlink);
    connect(this, &QTextEdit::textChanged,
            this, &TextWidget::restartCaretBlink);
}

void TextWidget::restartCaretBlink()
{
    m_caretVisible = true;
    const int flashTime = QApplication::cursorFlashTime();
    if (hasFocus() && !isReadOnly() && flashTime > 0) {
        m_caretTimer->start(qMax(1, flashTime / 2));
    } else {
        m_caretTimer->stop();
    }
    viewport()->update();
}

void TextWidget::focusInEvent(QFocusEvent* e)
{
    QTextEdit::focusInEvent(e);
    restartCaretBlink();
}

void TextWidget::focusOutEvent(QFocusEvent* e)
{
    QTextEdit::focusOutEvent(e);
    m_caretTimer->stop();
    m_caretVisible = false;
    viewport()->update();
}

void TextWidget::inputMethodEvent(QInputMethodEvent* e)
{
    QTextEdit::inputMethodEvent(e);
    m_preeditHidesCaret = false;
    for (const auto& attribute : e->attributes()) {
        if (attribute.type == QInputMethodEvent::Cursor) {
            m_preeditHidesCaret = attribute.length == 0;
        }
    }
    restartCaretBlink();
}

void TextWidget::paintEvent(QPaintEvent* e)
{
    // Width zero alone does not hide Qt's native caret at fractional scale:
    // QTextLayout turns it into a cosmetic (one physical pixel) pen. Draw the
    // document explicitly with cursorPosition=-1, then draw our own caret.
    QPainter painter(viewport());
    QAbstractTextDocumentLayout::PaintContext context;
    context.cursorPosition = -1;
    context.palette = palette();
    for (const auto& extra : extraSelections()) {
        QAbstractTextDocumentLayout::Selection selection;
        selection.cursor = extra.cursor;
        selection.format = extra.format;
        context.selections.append(selection);
    }
    if (textCursor().hasSelection()) {
        QAbstractTextDocumentLayout::Selection selection;
        selection.cursor = textCursor();
        const auto group = hasFocus() ? QPalette::Active : QPalette::Inactive;
        selection.format.setBackground(palette().brush(group, QPalette::Highlight));
        selection.format.setForeground(palette().brush(group, QPalette::HighlightedText));
        context.selections.append(selection);
    }
    const QPoint offset(horizontalScrollBar()->value(), verticalScrollBar()->value());
    context.clip = e->rect().translated(offset);
    painter.save();
    painter.translate(-offset);
    document()->documentLayout()->draw(&painter, context);
    painter.restore();
    if (!m_caretVisible || m_preeditHidesCaret || !hasFocus() || isReadOnly() ||
        textCursor().hasSelection()) {
        return;
    }
    const QRect caret = visibleCaretRect();
    const QLineF stem(caret.left(), caret.top() + 1.5,
                      caret.left(), caret.bottom() - 1.5);
    drawInsertionLines(painter, &stem, 1);
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
    QTextCursor first(document());
    first.movePosition(QTextCursor::Start);
    return textLineInkRect(first);
}

QRect TextWidget::textLineInkRect(const QTextCursor& position) const
{
    // cursorRect forces QTextDocument to finish its current layout first.
    const QRect caret = cursorRect(position);
    const QTextLayout* layout = position.block().layout();
    const QTextLine line = layout
      ? layout->lineForTextPosition(position.positionInBlock()) : QTextLine();
    if (line.isValid()) {
        const QRectF ink = lineInkBounds(*layout, line);
        if (!ink.isEmpty()) {
            return roundedInkBounds(ink.translated(
              layout->position() + QPointF(viewport()->pos()) -
              QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value())));
        }
    }
    // Before typing, preview representative CJK/Latin glyphs at this font.
    const QRectF sample = sampleInkBounds(font());
    const QPoint caretOrigin = viewport()->mapTo(this, caret.topLeft());
    return sample.isEmpty() ? QRect(caretOrigin, caret.size())
                            : roundedInkBounds(sample.translated(caretOrigin));
}

QRect TextWidget::visibleCaretRect() const
{
    const QRect ink = textLineInkRect(textCursor());
    const int height = qMax(8, ink.height());
    const int center = viewport()->mapFrom(this, ink.center()).y();
    return QRect(cursorRect().left(), center - (height - 1) / 2, 1, height);
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
        const qreal top = padding + 1.5;
        const qreal bottom = padding + height - 2.5;
        const qreal left = centerX - (capWidth - 3) / 2.0;
        const qreal right = centerX + (capWidth - 3) / 2.0;
        const qreal stemX = centerX;
        const QLineF lines[] = { { stemX, top, stemX, bottom },
                                { left, top, right, top },
                                { left, bottom, right, bottom } };
        // Use the same strokes for the mouse pointer and blinking caret.
        drawInsertionLines(painter, lines, 3);
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
