// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024 Flameshot Contributors

#pragma once

#include "tools/abstractpathtool.h"

class EraserTool : public AbstractPathTool
{
    Q_OBJECT
public:
    enum class Mode
    {
        WholeStroke = 0,
        Pixel = 1,
    };

    explicit EraserTool(QObject* parent = nullptr);

    QIcon icon(const QColor& background, bool inEditor) const override;
    QString name() const override;
    QString description() const override;
    QString info() override;

    CaptureTool* copy(QObject* parent = nullptr) override;

    bool isValid() const override;
    void process(QPainter& painter, const QPixmap& pixmap) override;
    void paintMousePreview(QPainter& painter,
                           const CaptureContext& context) override;
    void drawSearchArea(QPainter& painter, const QPixmap& pixmap) override;
    QWidget* configurationWidget() override;

    Mode mode() const;

protected:
    CaptureTool::Type type() const override;

public slots:
    void drawStart(const CaptureContext& context) override;
    void pressed(CaptureContext& context) override;

private slots:
    void setMode(int mode);

private:
    Mode m_mode{ Mode::WholeStroke };
};
