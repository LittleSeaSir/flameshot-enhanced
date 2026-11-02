// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QTextEdit>
#include <QCursor>

class QTimer;

class TextWidget : public QTextEdit
{
    Q_OBJECT
public:
    explicit TextWidget(QWidget* parent = nullptr);

    void adjustSize();
    void setFont(const QFont& f);
    QRect firstLineCursorRect() const;
    QRect firstLineTextRect() const;
    // Visible insertion caret, in viewport coordinates.
    QRect visibleCaretRect() const;
    static int fontTextHeight(const QFont& font);
    static QCursor insertionCursor(int lineHeight, qreal devicePixelRatio);

protected:
    void showEvent(QShowEvent* e);
    void resizeEvent(QResizeEvent* e);
    bool viewportEvent(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void inputMethodEvent(QInputMethodEvent* e) override;

signals:
    void textUpdated(const QString& s);
    void textLayoutChanged();

public slots:
    void setTextColor(const QColor& c);
    void setAlignment(Qt::AlignmentFlag alignment);

private slots:
    void emitTextUpdated();

private:
    QRect textLineInkRect(const QTextCursor& cursor) const;
    void restartCaretBlink();

    QSize m_baseSize;
    QSize m_minSize;
    QTimer* m_caretTimer;
    bool m_caretVisible{ true };
    bool m_preeditHidesCaret{ false };
};
