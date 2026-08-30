// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "pinwidget.h"
#include "core/flameshot.h"
#include "core/qguiappcurrentscreen.h"
#include "utils/confighandler.h"
#include "utils/globalvalues.h"
#include "utils/screenshotsaver.h"
#include "utils/waylandwindowpositioner.h"

#include <QCursor>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPinchGesture>
#include <QScreen>
#include <QShortcut>
#include <QShowEvent>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>

namespace {
constexpr int MARGIN = 7;
constexpr int BLUR_RADIUS = 2 * MARGIN;
constexpr qreal STEP = 0.03;
constexpr qreal MIN_SIZE = 100.0;
}

PinWidget::PinWidget(const QPixmap& pixmap,
                     const QRect& geometry,
                     QWidget* parent)
  : QWidget(parent)
  , m_pixmap(pixmap)
  , m_layout(new QVBoxLayout(this))
  , m_label(new QLabel())
  , m_shadowEffect(new QGraphicsDropShadowEffect(this))
{
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint |
                   Qt::Tool);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("flameshot-pin-%1").arg(
      QUuid::createUuid().toString(QUuid::WithoutBraces)));
    ConfigHandler conf;
    m_baseColor = conf.uiColor();
    m_hoverColor = conf.contrastUiColor();

    m_layout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);

    m_shadowEffect->setColor(m_baseColor);
    m_shadowEffect->setBlurRadius(BLUR_RADIUS);
    m_shadowEffect->setOffset(0, 0);
    setGraphicsEffect(m_shadowEffect);
    setWindowOpacity(m_opacity);

    m_label->setPixmap(m_pixmap);
    m_layout->addWidget(m_label);

    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q), this, SLOT(close()));
    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
    auto* editShortcut = new QShortcut(Qt::Key_Space, this);
    connect(editShortcut, &QShortcut::activated, this, &PinWidget::reEdit);

    // The `geometry` passed in is in device pixels (×DPR). Convert to logical
    // and store — it will be applied in showEvent() when the Wayland platform
    // window handle exists and can accept positioning.
    qreal dpr = 1.0;
    QScreen* currentScreen = QGuiAppCurrentScreen().currentScreen();
    QPoint screenTopLeft(0, 0);
    if (currentScreen != nullptr) {
        dpr = currentScreen->devicePixelRatio();
        screenTopLeft = currentScreen->geometry().topLeft();
    }

    QRect logicalGeom;
    logicalGeom.setX(
      static_cast<int>((geometry.x() - screenTopLeft.x()) / dpr + screenTopLeft.x()));
    logicalGeom.setY(
      static_cast<int>((geometry.y() - screenTopLeft.y()) / dpr + screenTopLeft.y()));
    logicalGeom.setWidth(static_cast<int>(geometry.width() / dpr));
    logicalGeom.setHeight(static_cast<int>(geometry.height() / dpr));

    m_pinGeometry = logicalGeom.adjusted(-MARGIN, -MARGIN, MARGIN, MARGIN);

    grabGesture(Qt::PinchGesture);

    this->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this,
            &QWidget::customContextMenuRequested,
            this,
            &PinWidget::showContextMenu);

    setGeometry(m_pinGeometry);
    const WaylandWindowPositioning positioning =
      positionWaylandWindow(this, m_pinGeometry);
    m_waylandPositioned =
      positioning != WaylandWindowPositioning::Unavailable;
    m_waylandLayerPositioned =
      positioning == WaylandWindowPositioning::LayerShell;
}

void PinWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // On Wayland the platform window handle is created during show();
    // setGeometry before show() is unreliable — apply it here instead.
    if (!m_waylandPositioned && !m_pinGeometry.isNull()) {
        setGeometry(m_pinGeometry);
    }
}

void PinWidget::closePin()
{
    update();
    close();
}
bool PinWidget::scrollEvent(QWheelEvent* event)
{
    const auto phase = event->phase();
    if (phase == Qt::ScrollPhase::ScrollUpdate
#if defined(Q_OS_UNIX) || defined(Q_OS_WINDOWS)
        || phase == Qt::ScrollPhase::NoScrollPhase
#endif
    ) {
        const auto angle = event->angleDelta();
        if (angle.y() == 0) {
            return true;
        }
        m_currentStepScaleFactor = angle.y() > 0
                                     ? m_currentStepScaleFactor + STEP
                                     : m_currentStepScaleFactor - STEP;
        m_expanding = m_currentStepScaleFactor >= 1.0;
    }
#if defined(Q_OS_MACOS)
    // ScrollEnd is currently supported only on Mac OSX
    if (phase == Qt::ScrollPhase::ScrollEnd) {
#else
    else {
#endif
        m_scaleFactor *= m_currentStepScaleFactor;
        m_currentStepScaleFactor = 1.0;
        m_expanding = false;
    }

    m_sizeChanged = true;
    update();
    return true;
}

void PinWidget::enterEvent(QEnterEvent*)
{
    m_shadowEffect->setColor(m_hoverColor);
}

void PinWidget::leaveEvent(QEvent*)
{
    m_shadowEffect->setColor(m_baseColor);
}

void PinWidget::mouseDoubleClickEvent(QMouseEvent*)
{
    closePin();
}

void PinWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    if (m_waylandLayerPositioned) {
        m_dragging = true;
        m_dragStartGlobal = QCursor::pos();
        m_dragStartGeometry = m_pinGeometry;
        grabMouse();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    if (QWindow* window = windowHandle(); window != nullptr) {
        window->startSystemMove();
        return;
    }
}

void PinWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (m_waylandLayerPositioned && m_dragging) {
        m_pinGeometry = m_dragStartGeometry.translated(
          QCursor::pos() - m_dragStartGlobal);
        positionWaylandWindow(this, m_pinGeometry);
        e->accept();
        return;
    }
    QWidget::mouseMoveEvent(e);
}

void PinWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_waylandLayerPositioned && m_dragging &&
        e->button() == Qt::LeftButton) {
        m_pinGeometry = m_dragStartGeometry.translated(
          QCursor::pos() - m_dragStartGlobal);
        positionWaylandWindow(this, m_pinGeometry);
        m_dragging = false;
        if (mouseGrabber() == this) {
            releaseMouse();
        }
        unsetCursor();
        e->accept();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void PinWidget::moveEvent(QMoveEvent* e)
{
    QWidget::moveEvent(e);
    if (!m_waylandLayerPositioned) {
        m_pinGeometry.moveTopLeft(e->pos());
    }
}

void PinWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_0) {
        m_opacity = 1.0;
    } else if (event->key() == Qt::Key_9) {
        m_opacity = 0.9;
    } else if (event->key() == Qt::Key_8) {
        m_opacity = 0.8;
    } else if (event->key() == Qt::Key_7) {
        m_opacity = 0.7;
    } else if (event->key() == Qt::Key_6) {
        m_opacity = 0.6;
    } else if (event->key() == Qt::Key_5) {
        m_opacity = 0.5;
    } else if (event->key() == Qt::Key_4) {
        m_opacity = 0.4;
    } else if (event->key() == Qt::Key_3) {
        m_opacity = 0.3;
    } else if (event->key() == Qt::Key_2) {
        m_opacity = 0.2;
    } else if (event->key() == Qt::Key_1) {
        m_opacity = 0.1;
    }

    setWindowOpacity(m_opacity);
}
bool PinWidget::gestureEvent(QGestureEvent* event)
{
    if (QGesture* pinch = event->gesture(Qt::PinchGesture)) {
        pinchTriggered(static_cast<QPinchGesture*>(pinch));
    }
    return true;
}

void PinWidget::rotateLeft()
{
    m_sizeChanged = true;

    auto rotateTransform = QTransform().rotate(270);
    m_pixmap = m_pixmap.transformed(rotateTransform);
    update();
}

void PinWidget::rotateRight()
{
    m_sizeChanged = true;

    auto rotateTransform = QTransform().rotate(90);
    m_pixmap = m_pixmap.transformed(rotateTransform);
    update();
}

void PinWidget::reEdit()
{
    if (m_editing) {
        return;
    }

    const QPixmap displayedPixmap = m_label->pixmap();
    if (displayedPixmap.isNull()) {
        return;
    }

    if (m_waylandPositioned && !m_waylandLayerPositioned) {
        if (const std::optional<QPoint> actualTopLeft =
              kdeWindowTopLeft(windowTitle())) {
            m_pinGeometry.moveTopLeft(*actualTopLeft);
        }
    }

    m_editing = true;
    const QPoint contentTopLeft =
      m_waylandPositioned
        ? m_pinGeometry.topLeft() + QPoint(MARGIN, MARGIN)
        : m_label->mapToGlobal(QPoint(0, 0));
    const QRect contentGeometry(contentTopLeft, m_label->size());
    hide();
    CaptureWidget* editor =
      Flameshot::instance()->pinEdit(displayedPixmap, contentGeometry);
    if (!editor) {
        m_editing = false;
        show();
        return;
    }

    // Keep the original pin alive while editing. Cancel restores it; accepting
    // creates the replacement pin and then closes this one.
    connect(editor,
            &CaptureWidget::captureFinished,
            this,
            [this](bool accepted) {
                m_editing = false;
                if (accepted) {
                    close();
                } else {
                    show();
                    activateWindow();
                    raise();
                }
            });
}

void PinWidget::increaseOpacity()
{
    m_opacity += 0.1;
    if (m_opacity > 1.0) {
        m_opacity = 1.0;
    }
    setWindowOpacity(m_opacity);
}

void PinWidget::decreaseOpacity()
{
    m_opacity -= 0.1;
    if (m_opacity < 0.0) {
        m_opacity = 0.0;
    }

    setWindowOpacity(m_opacity);
}

bool PinWidget::event(QEvent* event)
{
    if (event->type() == QEvent::Gesture) {
        return gestureEvent(static_cast<QGestureEvent*>(event));
    } else if (event->type() == QEvent::Wheel) {
        return scrollEvent(static_cast<QWheelEvent*>(event));
    }
    return QWidget::event(event);
}

void PinWidget::paintEvent(QPaintEvent* event)
{
    if (m_sizeChanged) {
        const auto aspectRatio =
          m_expanding ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
        const auto transformType = ConfigHandler().antialiasingPinZoom()
                                     ? Qt::SmoothTransformation
                                     : Qt::FastTransformation;
        const qreal iw = m_pixmap.width();
        const qreal ih = m_pixmap.height();
        const qreal nw = qBound(MIN_SIZE,
                                iw * m_currentStepScaleFactor * m_scaleFactor,
                                static_cast<qreal>(maximumWidth()));
        const qreal nh = qBound(MIN_SIZE,
                                ih * m_currentStepScaleFactor * m_scaleFactor,
                                static_cast<qreal>(maximumHeight()));

        const QPixmap pix = m_pixmap.scaled(nw, nh, aspectRatio, transformType);

        m_label->setPixmap(pix);
        adjustSize();
        m_pinGeometry.setSize(size());
        if (m_waylandLayerPositioned) {
            positionWaylandWindow(this, m_pinGeometry);
        }
        m_sizeChanged = false;
    }
}

void PinWidget::pinchTriggered(QPinchGesture* gesture)
{
    const QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        m_currentStepScaleFactor = gesture->totalScaleFactor();
        m_expanding = m_currentStepScaleFactor > gesture->lastScaleFactor();
    }
    if (gesture->state() == Qt::GestureFinished) {
        m_scaleFactor *= m_currentStepScaleFactor;
        m_currentStepScaleFactor = 1;
        m_expanding = false;
    }
    m_sizeChanged = true;
    update();
}

void PinWidget::showContextMenu(const QPoint& pos)
{
    QMenu contextMenu(tr("Context menu"), this);

    QAction reeditAction(tr("Re-edit"), this);
    reeditAction.setShortcut(Qt::Key_Space);
    connect(&reeditAction, &QAction::triggered, this, &PinWidget::reEdit);
    contextMenu.addAction(&reeditAction);
    contextMenu.addSeparator();

    QAction copyToClipboardAction(tr("Copy to clipboard"), this);
    connect(&copyToClipboardAction,
            &QAction::triggered,
            this,
            &PinWidget::copyToClipboard);
    contextMenu.addAction(&copyToClipboardAction);

    QAction saveToFileAction(tr("Save to file"), this);
    connect(
      &saveToFileAction, &QAction::triggered, this, &PinWidget::saveToFile);
    contextMenu.addAction(&saveToFileAction);

    contextMenu.addSeparator();

    QAction rotateRightAction(tr("Rotate Right"), this);
    connect(
      &rotateRightAction, &QAction::triggered, this, &PinWidget::rotateRight);
    contextMenu.addAction(&rotateRightAction);

    QAction rotateLeftAction(tr("Rotate Left"), this);
    connect(
      &rotateLeftAction, &QAction::triggered, this, &PinWidget::rotateLeft);
    contextMenu.addAction(&rotateLeftAction);

    QAction increaseOpacityAction(tr("Increase Opacity"), this);
    connect(&increaseOpacityAction,
            &QAction::triggered,
            this,
            &PinWidget::increaseOpacity);
    contextMenu.addAction(&increaseOpacityAction);

    QAction decreaseOpacityAction(tr("Decrease Opacity"), this);
    connect(&decreaseOpacityAction,
            &QAction::triggered,
            this,
            &PinWidget::decreaseOpacity);
    contextMenu.addAction(&decreaseOpacityAction);

    QAction closePinAction(tr("Close"), this);
    connect(&closePinAction, &QAction::triggered, this, &PinWidget::closePin);
    contextMenu.addSeparator();
    contextMenu.addAction(&closePinAction);

    contextMenu.exec(mapToGlobal(pos));
}

void PinWidget::copyToClipboard()
{
    saveToClipboard(m_pixmap);
}
void PinWidget::saveToFile()
{
    hide();
    saveToFilesystemGUI(m_pixmap);
    show();
}
