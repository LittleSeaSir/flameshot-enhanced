// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QTextEdit>
#include <QCursor>

class TextWidget : public QTextEdit
{
    Q_OBJECT
public:
    explicit TextWidget(QWidget* parent = nullptr);

    void adjustSize();
    void setFont(const QFont& f);
    QRect firstLineCursorRect() const;
    QRect firstLineTextRect() const;
    static int fontTextHeight(const QFont& font);
    static QCursor insertionCursor(int lineHeight, qreal devicePixelRatio);

protected:
    void showEvent(QShowEvent* e);
    void resizeEvent(QResizeEvent* e);

signals:
    void textUpdated(const QString& s);
    void textLayoutChanged();

public slots:
    void setTextColor(const QColor& c);
    void setAlignment(Qt::AlignmentFlag alignment);

private slots:
    void emitTextUpdated();

private:
    QSize m_baseSize;
    QSize m_minSize;
};
