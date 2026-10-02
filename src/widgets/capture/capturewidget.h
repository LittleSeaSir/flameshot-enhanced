// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

// Based on Lightscreen areadialog.h, Copyright 2017  Christian Kaiser
// <info@ckaiser.com.ar> released under the GNU GPL2
// <https://www.gnu.org/licenses/gpl-2.0.txt>

// Based on KDE's KSnapshot regiongrabber.cpp, revision 796531, Copyright 2007
// Luca Gugelmann <lucag@student.ethz.ch> released under the GNU LGPL
// <http://www.gnu.org/licenses/old-licenses/library.txt>

#pragma once

#include "tools/capturecontext.h"
#include "tools/capturetool.h"
#include "utils/confighandler.h"
#include "widgets/capture/buttonhandler.h"
#include "widgets/capture/capturetoolbutton.h"
#include "widgets/capture/capturetoolobjects.h"
#include "widgets/capture/magnifierwidget.h"
#include "widgets/capture/selectionwidget.h"

#include <QMessageBox>
#include <QPointer>
#include <QRectF>
#include <QTimer>
#include <QUndoStack>
#include <QVector>
#include <QWidget>

class QLabel;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QMouseEvent;
class QShortcut;
class QNetworkAccessManager;
class QNetworkReply;
class ColorPicker;
class NotifierBox;
class HoverEventFilter;
#if !defined(DISABLE_UPDATE_CHECKER)
class UpdateNotificationWidget;
#endif
class UtilityPanel;
class SidePanelWidget;

class CaptureWidget : public QWidget
{
    Q_OBJECT

#ifdef FLAMESHOT_BUILD_TESTS
    friend class CaptureWidgetShapeReselectionTest;
    friend class CaptureWidgetWindowSnapTest;
#endif

public:
    explicit CaptureWidget(const CaptureRequest& req,
                           bool fullScreen = true,
                           QWidget* parent = nullptr);
    explicit CaptureWidget(const QPixmap& preloaded,
                           const QRect& pinGeometry = QRect(),
                           QWidget* parent = nullptr);
    ~CaptureWidget();

    QPixmap pixmap();
    void setCaptureToolObjects(const CaptureToolObjects& captureToolObjects);
#if !defined(DISABLE_UPDATE_CHECKER)
    void showAppUpdateNotification(const QString& appLatestVersion,
                                   const QString& appLatestUrl);
#endif

public slots:
    bool commitCurrentTool();
    void deleteToolWidgetOrClose();

signals:
    void colorChanged(const QColor& c);
    void toolSizeChanged(int size);
    void captureFinished(bool accepted);

private slots:
    void undo();
    void redo();
    void cancel();
    void togglePanel();
    void childEnter();
    void childLeave();

    void deleteCurrentTool();

    void setState(CaptureToolButton* b);
    void handleToolSignal(CaptureTool::Request r);
    void handleButtonLeftClick(CaptureToolButton* b);
    void handleButtonRightClick(CaptureToolButton* b);
    void setDrawColor(const QColor& c);
    void onToolSizeChanged(int size);
    void onToolSizeSettled(int size);
    void updateActiveLayer(int layer);
    void onMoveCaptureToolUp(int captureToolIndex);
    void onMoveCaptureToolDown(int captureToolIndex);
    void selectAll();
    void xywhTick();
    void onDisplayGridChanged(bool display);
    void onGridSizeChanged(int size);

    void startColorGrab();

public:
    void removeToolObject(int index = -1);
    void showxywh();

protected:
    void paintEvent(QPaintEvent* paintEvent) override;
    void mousePressEvent(QMouseEvent* mouseEvent) override;
    void mouseMoveEvent(QMouseEvent* mouseEvent) override;
    void mouseReleaseEvent(QMouseEvent* mouseEvent) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* keyEvent) override;
    void keyReleaseEvent(QKeyEvent* keyEvent) override;
    void wheelEvent(QWheelEvent* wheelEvent) override;
    void resizeEvent(QResizeEvent* resizeEvent) override;
    void showEvent(QShowEvent* showEvent) override;
    void moveEvent(QMoveEvent* moveEvent) override;
    void changeEvent(QEvent* changeEvent) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void pushObjectsStateToUndoStack();
    void releaseActiveTool();
    void uncheckActiveTool();
    int selectToolItemAtPos(const QPoint& pos,
                            bool allowActiveTwoPointTool = false);
    void prepareToolDrag(const QPoint& pressPos);
    void showColorPicker(const QPoint& pos);
    bool startDrawObjectTool(const QPoint& pos);
    bool eraseWholeStrokesBetween(const QPoint& from, const QPoint& to);
    QPointer<CaptureTool> activeToolObject();
    void initContext(bool fullscreen, const CaptureRequest& req);
    void initPanel();
    void initSelection();
    void initShortcuts();
    void initButtons();
    void initHelpMessage();
    void initQuitPrompt();
    void startWindowSnapPreview(const QVector<QRectF>& candidates,
                                const QRectF& screenGeometry,
                                const QPointF& globalCursorPosition);
    void updateWindowSnapPreview(const QPoint& localCursorPosition);
    void applyWindowSnapTarget(const QRectF& globalTarget);
    QRect windowSnapPreviewRect(const QRectF& globalTarget) const;
    QRect windowSnapPhysicalRect(const QRectF& globalTarget) const;
    void finishWindowSnapPreview();
    void updateSizeIndicator();
    void updateCursor();
    void updateSelectionState();
    void updateTool(CaptureTool* tool);
    void updateLayersPanel();
    bool promptQuit();
    void pushToolToStack();
    void makeChild(QWidget* w);
    void restoreCircleCountState();

    QList<QShortcut*> newShortcut(const QKeySequence& key,
                                  QWidget* parent,
                                  const char* slot);

    void setToolSize(int size);

    QRect extendedSelection() const;
    QRect extendedRect(const QRect& r) const;
    QRect paddedUpdateRect(const QRect& r) const;
    void drawErrorMessage(const QString& msg, QPainter* painter);
    void drawInactiveRegion(QPainter* painter);
    void drawToolsData(bool drawSelection = true);
    void drawObjectSelection();

    void processPixmapWithTool(QPixmap* pixmap, CaptureTool* tool);

    CaptureTool* activeButtonTool() const;
    CaptureTool::Type activeButtonToolType() const;

    QPoint snapToGrid(const QPoint& point) const;

    ////////////////////////////////////////
    // Class members

    // Context information
    CaptureContext m_context;

    // Main ui color
    QColor m_uiColor;
    // Secondary ui color
    QColor m_contrastUiColor;

    // Outside selection opacity
    int m_opacity;
    int m_toolSizeByKeyboard;

    // utility flags
    bool m_mouseIsClicked;
    bool m_newSelection;
    bool m_movingSelection;
    bool m_captureDone;
    bool m_previewEnabled;
    bool m_adjustmentButtonPressed;
    bool m_shiftPressed;
    double m_currentAngle;
    bool m_showAngleIndicator;
    bool m_configError;
    bool m_configErrorResolved;

#if !defined(DISABLE_UPDATE_CHECKER)
    UpdateNotificationWidget* m_updateNotificationWidget;
#endif
    quint64 m_lastMouseWheel;
    QPointer<CaptureToolButton> m_sizeIndButton;
    // Last pressed button
    QPointer<CaptureToolButton> m_activeButton;
    QPointer<CaptureTool> m_activeTool;
    bool m_activeToolIsMoved;
    QPointer<QWidget> m_toolWidget;
    QPointer<QMessageBox> m_quitPrompt;

    ButtonHandler* m_buttonHandler;
    UtilityPanel* m_panel;
    SidePanelWidget* m_sidePanel;
    ColorPicker* m_colorPicker;
    ConfigHandler m_config;
    NotifierBox* m_notifierBox;
    HoverEventFilter* m_eventFilter;
    SelectionWidget* m_selection;
    MagnifierWidget* m_magnifier;
    QString m_helpMessage;

    SelectionWidget::SideType m_mouseOverHandle;

    QMap<CaptureTool::Type, CaptureTool*> m_tools;
    CaptureToolObjects m_captureToolObjects;
    CaptureToolObjects m_captureToolObjectsBackup;

    QPoint m_mousePressedPos;
    QPoint m_activeToolOffsetToMouseOnStart;
    bool m_activeToolOffsetToMouseOnStartValid{ false };
    QPoint m_lastWholeStrokeErasePos;
    bool m_wholeStrokeEraseChanged{ false };
    QRect m_deferredWindowGeometry;
    QRect m_pinEditPhysicalSelection;
    QRect m_pinEditExportGeometry;
    bool m_waylandPositioned{ false };
    bool m_pinEditMode{ false };

    // Smart capture selection. Window candidates use compositor-global
    // logical coordinates and remain cached while the overlay is active.
    QVector<QRectF> m_windowSnapCandidates;
    QRectF m_windowSnapScreenGeometry;
    QRect m_windowSnapPhysicalSelection;
    QPoint m_windowSnapPressPos;
    bool m_windowSnapPreviewActive{ false };
    bool m_windowSnapPressed{ false };
    bool m_windowSnapManualDrag{ false };
    bool m_windowSnapApplyingCandidate{ false };
    bool m_windowSnapPhysicalSelectionValid{ false };

    // XYWH display position and timer
    bool m_xywhDisplay;
    QTimer m_xywhTimer;

    QUndoStack m_undoStack;

    bool m_existingObjectIsChanged;

    // For start moving after more than X offset
    QPoint m_startMovePos;
    bool m_startMovePosValid{ false };
    bool m_startMove;

    // Grid
    bool m_displayGrid{ false };
    int m_gridSize{ 10 };

    bool m_clipboardWorkaroundDone{ false };
};
