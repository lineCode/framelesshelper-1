/*
 * MIT License
 *
 * Copyright (C) 2020 by wangwenx190 (Yuhang Zhao)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "framelesshelper.h"

#include <QDebug>
#include <QGuiApplication>
#include <QMargins>
#include <QScreen>
#ifdef QT_WIDGETS_LIB
#include <QWidget>
#endif
#ifdef QT_QUICK_LIB
#include <QQuickItem>
#endif
#include <QEvent>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

Q_DECLARE_METATYPE(QMargins)

namespace {

QWindow *getWindowHandle(QObject *const val) {
    if (val) {
        const auto validWindow = [](QWindow *const window) -> QWindow * {
            return (window && window->handle()) ? window : nullptr;
        };
        if (val->isWindowType()) {
            return validWindow(qobject_cast<QWindow *>(val));
        }
#ifdef QT_WIDGETS_LIB
        else if (val->isWidgetType()) {
            const auto widget = qobject_cast<QWidget *>(val);
            if (widget && widget->isTopLevel()) {
                return validWindow(widget->windowHandle());
            }
        }
#endif
        else {
            qWarning().noquote()
                << "Can't acquire the window handle: only "
                   "top level QWidget and QWindow are accepted.";
        }
    }
    return nullptr;
}

} // namespace

FramelessHelper::FramelessHelper(QObject *parent) : QObject(parent) {}

void FramelessHelper::updateQtFrame(QWindow *const window,
                                    const int titleBarHeight) {
    if (window && (titleBarHeight > 0)) {
        // Reduce top frame to zero since we paint it ourselves. Use
        // device pixel to avoid rounding errors.
        const QMargins margins = {0, -titleBarHeight, 0, 0};
        const QVariant marginsVar = QVariant::fromValue(margins);
        // The dynamic property takes effect when creating the platform
        // window.
        window->setProperty("_q_windowsCustomMargins", marginsVar);
        // If a platform window exists, change via native interface.
        QPlatformWindow *const platformWindow = window->handle();
        if (platformWindow) {
            QGuiApplication::platformNativeInterface()->setWindowProperty(
                platformWindow, QString::fromUtf8("WindowsCustomMargins"),
                marginsVar);
        }
    }
}

void FramelessHelper::moveWindowToDesktopCenter(QObject *const obj) {
    if (!obj) {
        return;
    }
    if (obj->isWindowType()) {
        const auto window = qobject_cast<QWindow *>(obj);
        if (window) {
            const QSize ss = window->screen()->size();
            const int sw = ss.width();
            const int sh = ss.height();
            const int ww = window->width();
            const int wh = window->height();
            window->setX(qRound(static_cast<qreal>(sw - ww) / 2.0));
            window->setY(qRound(static_cast<qreal>(sh - wh) / 2.0));
        }
    }
#ifdef QT_WIDGETS_LIB
    else if (obj->isWidgetType()) {
        const auto widget = qobject_cast<QWidget *>(obj);
        if (widget && widget->isTopLevel()) {
            const QSize ss = widget->screen()->size();
            const int sw = ss.width();
            const int sh = ss.height();
            const int ww = widget->width();
            const int wh = widget->height();
            widget->move(qRound(static_cast<qreal>(sw - ww) / 2.0),
                         qRound(static_cast<qreal>(sh - wh) / 2.0));
        }
    }
#endif
    else {
        qWarning().noquote() << "The given QObject is not a top level window.";
    }
}

int FramelessHelper::getBorderWidth() const { return m_borderWidth; }

void FramelessHelper::setBorderWidth(const int val) { m_borderWidth = val; }

int FramelessHelper::getBorderHeight() const { return m_borderHeight; }

void FramelessHelper::setBorderHeight(const int val) { m_borderHeight = val; }

int FramelessHelper::getTitleBarHeight() const { return m_titleBarHeight; }

void FramelessHelper::setTitleBarHeight(const int val) {
    m_titleBarHeight = val;
}

QVector<QRect> FramelessHelper::getIgnoreAreas(QObject *const obj) const {
    if (!obj) {
        return {};
    }
    return m_ignoreAreas.value(obj);
}

void FramelessHelper::setIgnoreAreas(QObject *const obj,
                                     const QVector<QRect> &val) {
    if (obj) {
        m_ignoreAreas[obj] = val;
    }
}

void FramelessHelper::addIgnoreArea(QObject *const obj, const QRect &val) {
    if (obj) {
        QVector<QRect> areas = m_ignoreAreas[obj];
        areas.append(val);
        m_ignoreAreas[obj] = areas;
    }
}

void FramelessHelper::clearIgnoreAreas(QObject *const obj) {
    if (obj) {
        m_ignoreAreas[obj] = {};
    }
}

QVector<QRect> FramelessHelper::getDraggableAreas(QObject *const obj) const {
    if (!obj) {
        return {};
    }
    return m_draggableAreas.value(obj);
}

void FramelessHelper::setDraggableAreas(QObject *const obj,
                                        const QVector<QRect> &val) {
    if (obj) {
        m_draggableAreas[obj] = val;
    }
}

void FramelessHelper::addDraggableArea(QObject *const obj, const QRect &val) {
    if (obj) {
        QVector<QRect> areas = m_draggableAreas[obj];
        areas.append(val);
        m_draggableAreas[obj] = areas;
    }
}

void FramelessHelper::clearDraggableAreas(QObject *const obj) {
    if (obj) {
        m_draggableAreas[obj] = {};
    }
}

QVector<QObject *> FramelessHelper::getIgnoreObjects(QObject *const obj) const {
    if (!obj) {
        return {};
    }
    QVector<QObject *> ret{};
    const QVector<QPointer<QObject>> objs = m_ignoreObjects.value(obj);
    if (!objs.isEmpty()) {
        for (auto &&_obj : qAsConst(objs)) {
            if (_obj) {
                ret.append(_obj);
            }
        }
    }
    return ret;
}

void FramelessHelper::setIgnoreObjects(QObject *const obj,
                                       const QVector<QObject *> &val) {
    if (obj) {
        QVector<QPointer<QObject>> objs{};
        if (!val.isEmpty()) {
            for (auto &&_obj : qAsConst(val)) {
                objs.append(_obj);
            }
        }
        m_ignoreObjects[obj] = objs;
    }
}

void FramelessHelper::addIgnoreObject(QObject *const obj, QObject *val) {
    if (obj) {
        QVector<QPointer<QObject>> objs = m_ignoreObjects[obj];
        objs.append(val);
        m_ignoreObjects[obj] = objs;
    }
}

void FramelessHelper::clearIgnoreObjects(QObject *const obj) {
    if (obj) {
        m_ignoreObjects[obj] = {};
    }
}

QVector<QObject *>
FramelessHelper::getDraggableObjects(QObject *const obj) const {
    if (!obj) {
        return {};
    }
    QVector<QObject *> ret{};
    const QVector<QPointer<QObject>> objs = m_draggableObjects.value(obj);
    if (!objs.isEmpty()) {
        for (auto &&_obj : qAsConst(objs)) {
            if (_obj) {
                ret.append(_obj);
            }
        }
    }
    return ret;
}

void FramelessHelper::setDraggableObjects(QObject *const obj,
                                          const QVector<QObject *> &val) {
    if (obj) {
        QVector<QPointer<QObject>> objs{};
        if (!val.isEmpty()) {
            for (auto &&_obj : qAsConst(val)) {
                objs.append(_obj);
            }
        }
        m_draggableObjects[obj] = objs;
    }
}

void FramelessHelper::addDraggableObject(QObject *const obj, QObject *val) {
    if (obj) {
        QVector<QPointer<QObject>> objs = m_draggableObjects[obj];
        objs.append(val);
        m_draggableObjects[obj] = objs;
    }
}

void FramelessHelper::clearDraggableObjects(QObject *const obj) {
    if (obj) {
        m_draggableObjects[obj] = {};
    }
}

bool FramelessHelper::getResizable(QObject *const obj) const {
    if (!obj) {
        return true;
    }
    return !m_fixedSize.value(obj);
}

void FramelessHelper::setResizable(QObject *const obj, const bool val) {
    if (obj) {
        m_fixedSize[obj] = !val;
    }
}

bool FramelessHelper::getTitleBarEnabled(QObject *const obj) const {
    if (!obj) {
        return true;
    }
    return !m_disableTitleBar.value(obj);
}

void FramelessHelper::setTitleBarEnabled(QObject *const obj, const bool val) {
    if (obj) {
        m_disableTitleBar[obj] = !val;
    }
}

void FramelessHelper::removeWindowFrame(QObject *const obj) {
    if (obj) {
        // Don't miss the Qt::Window flag.
        const Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint;
        const auto window = qobject_cast<QWindow *>(obj);
        if (window) {
            window->setFlags(flags);
            // MouseTracking is always enabled for QWindow.
            window->installEventFilter(this);
        }
#ifdef QT_WIDGETS_LIB
        else {
            const auto widget = qobject_cast<QWidget *>(obj);
            if (widget && widget->isTopLevel()) {
                widget->setWindowFlags(flags);
                // We can't get MouseMove events if MouseTracking is
                // disabled.
                widget->setMouseTracking(true);
                widget->installEventFilter(this);
                updateQtFrame(widget->windowHandle(), m_titleBarHeight);
            }
        }
#endif
    }
}

bool FramelessHelper::eventFilter(QObject *object, QEvent *event) {
    const auto isWindowTopLevel = [](QObject *const window) -> bool {
        if (window) {
            if (window->isWindowType()) {
                const auto win = qobject_cast<QWindow *>(window);
                if (win) {
                    return win->isTopLevel();
                }
            }
#ifdef QT_WIDGETS_LIB
            else if (window->isWidgetType()) {
                const auto widget = qobject_cast<QWidget *>(window);
                if (widget) {
                    return widget->isTopLevel();
                }
            }
#endif
        }
        return false;
    };
    if (!object || !isWindowTopLevel(object)) {
        return false;
    }
    const auto getWindowEdges = [this](const QPointF &point, const int ww,
                                       const int wh) -> Qt::Edges {
        if (point.y() <= m_borderHeight) {
            if (point.x() <= m_borderWidth) {
                return Qt::Edge::TopEdge | Qt::Edge::LeftEdge;
            }
            if (point.x() >= (ww - m_borderWidth)) {
                return Qt::Edge::TopEdge | Qt::Edge::RightEdge;
            }
            return Qt::Edge::TopEdge;
        }
        if (point.y() >= (wh - m_borderHeight)) {
            if (point.x() <= m_borderWidth) {
                return Qt::Edge::BottomEdge | Qt::Edge::LeftEdge;
            }
            if (point.x() >= (ww - m_borderWidth)) {
                return Qt::Edge::BottomEdge | Qt::Edge::RightEdge;
            }
            return Qt::Edge::BottomEdge;
        }
        if (point.x() <= m_borderWidth) {
            return Qt::Edge::LeftEdge;
        }
        if (point.x() >= (ww - m_borderWidth)) {
            return Qt::Edge::RightEdge;
        }
        return {};
    };
    const auto getCursorShape = [](const Qt::Edges edges) -> Qt::CursorShape {
        if ((edges.testFlag(Qt::Edge::TopEdge) &&
             edges.testFlag(Qt::Edge::LeftEdge)) ||
            (edges.testFlag(Qt::Edge::BottomEdge) &&
             edges.testFlag(Qt::Edge::RightEdge))) {
            return Qt::CursorShape::SizeFDiagCursor;
        }
        if ((edges.testFlag(Qt::Edge::TopEdge) &&
             edges.testFlag(Qt::Edge::RightEdge)) ||
            (edges.testFlag(Qt::Edge::BottomEdge) &&
             edges.testFlag(Qt::Edge::LeftEdge))) {
            return Qt::CursorShape::SizeBDiagCursor;
        }
        if (edges.testFlag(Qt::Edge::TopEdge) ||
            edges.testFlag(Qt::Edge::BottomEdge)) {
            return Qt::CursorShape::SizeVerCursor;
        }
        if (edges.testFlag(Qt::Edge::LeftEdge) ||
            edges.testFlag(Qt::Edge::RightEdge)) {
            return Qt::CursorShape::SizeHorCursor;
        }
        return Qt::CursorShape::ArrowCursor;
    };
    const auto isInSpecificAreas = [](const int x, const int y,
                                      const QVector<QRect> &areas) -> bool {
        if (!areas.isEmpty()) {
            for (auto &&area : qAsConst(areas)) {
                if (area.contains(x, y)) {
                    return true;
                }
            }
        }
        return false;
    };
    const auto isInSpecificObjects =
        [](const int x, const int y,
           const QVector<QObject *> &objects) -> bool {
        if (!objects.isEmpty()) {
            for (auto &&obj : qAsConst(objects)) {
                if (!obj) {
                    continue;
                }
#ifdef QT_WIDGETS_LIB
                const auto widget = qobject_cast<QWidget *>(obj);
                if (widget) {
                    const QPoint pos = widget->mapToGlobal({0, 0});
                    if (QRect(pos.x(), pos.y(), widget->width(),
                              widget->height())
                            .contains(x, y)) {
                        return true;
                    }
                }
#endif
#ifdef QT_QUICK_LIB
                const auto quickItem = qobject_cast<QQuickItem *>(obj);
                if (quickItem) {
                    const QPointF pos = quickItem->mapToGlobal({0, 0});
                    if (QRectF(pos.x(), pos.y(), quickItem->width(),
                               quickItem->height())
                            .contains(x, y)) {
                        return true;
                    }
                }
#endif
            }
        }
        return false;
    };
    const auto isInIgnoreAreas =
        [this, &isInSpecificAreas](const QPointF &point,
                                   QObject *const window) -> bool {
        if (!window) {
            return false;
        }
        return isInSpecificAreas(point.x(), point.y(), getIgnoreAreas(window));
    };
    const auto isInIgnoreObjects =
        [this, &isInSpecificObjects](const QPointF &point,
                                     QObject *const window) -> bool {
#if defined(QT_WIDGETS_LIB) || defined(QT_QUICK_LIB)
        if (!window) {
            return false;
        }
        return isInSpecificObjects(point.x(), point.y(),
                                   getIgnoreObjects(window));
#else
        Q_UNUSED(point)
        Q_UNUSED(window)
        return false;
#endif
    };
    const auto isInDraggableAreas =
        [this, &isInSpecificAreas](const QPointF &point,
                                   QObject *const window) -> bool {
        if (!window) {
            return false;
        }
        const auto areas = getDraggableAreas(window);
        return (areas.isEmpty()
                    ? true
                    : isInSpecificAreas(point.x(), point.y(), areas));
    };
    const auto isInDraggableObjects =
        [this, &isInSpecificObjects](const QPointF &point,
                                     QObject *const window) -> bool {
#if defined(QT_WIDGETS_LIB) || defined(QT_QUICK_LIB)
        if (!window) {
            return false;
        }
        const auto objs = getDraggableObjects(window);
        return (objs.isEmpty()
                    ? true
                    : isInSpecificObjects(point.x(), point.y(), objs));
#else
        Q_UNUSED(point)
        Q_UNUSED(window)
        return true;
#endif
    };
    const auto isResizePermitted =
        [&isInIgnoreAreas, &isInIgnoreObjects](const QPointF &globalPoint,
                                               const QPointF &point,
                                               QObject *const window) -> bool {
        if (!window) {
            return false;
        }
        return (!isInIgnoreAreas(point, window) &&
                !isInIgnoreObjects(globalPoint, window));
    };
    const auto isInTitlebarArea =
        [this, &isInDraggableAreas, &isInDraggableObjects,
         &isResizePermitted](const QPointF &globalPoint, const QPointF &point,
                             QObject *const window) -> bool {
        if (window) {
            return ((point.y() <= m_titleBarHeight) &&
                    isInDraggableAreas(point, window) &&
                    isInDraggableObjects(globalPoint, window) &&
                    isResizePermitted(globalPoint, point, window) &&
                    getTitleBarEnabled(window));
        }
        return false;
    };
    const auto moveOrResize = [this, &getWindowEdges, &isResizePermitted,
                               &isInTitlebarArea](const QPointF &globalPoint,
                                                  const QPointF &point,
                                                  QObject *const object) {
        QWindow *const window = getWindowHandle(object);
        if (window) {
            const Qt::Edges edges =
                getWindowEdges(point, window->width(), window->height());
            if (edges == Qt::Edges{}) {
                if (isInTitlebarArea(globalPoint, point, object)) {
                    window->startSystemMove();
                }
            } else {
                if (window->windowStates().testFlag(
                        Qt::WindowState::WindowNoState) &&
                    isResizePermitted(globalPoint, point, object) &&
                    getResizable(object)) {
                    window->startSystemResize(edges);
                }
            }
        } else {
            qWarning().noquote() << "Can't move or resize the window: failed "
                                    "to acquire the window handle.";
        }
    };
    switch (event->type()) {
    case QEvent::MouseButtonDblClick: {
        const auto mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent) {
            if (mouseEvent->button() != Qt::MouseButton::LeftButton) {
                break;
            }
            if (isInTitlebarArea(mouseEvent->screenPos(),
                                 mouseEvent->windowPos(), object)) {
                // ### FIXME: If the current object is a QWidget, we can use
                // getWindowHandle(object) to get the window handle, but if we
                // call showMaximized() of that window, it will not be
                // maximized, it will be moved to the top-left edge of the
                // screen without changing it's size instead. Why? Convert the
                // object to QWidget and call showMaximized() doesn't have this
                // issue.
                if (object->isWindowType()) {
                    const auto window = qobject_cast<QWindow *>(object);
                    if (window) {
                        if (window->windowStates().testFlag(
                                Qt::WindowState::WindowFullScreen)) {
                            break;
                        }
                        if (window->windowStates().testFlag(
                                Qt::WindowState::WindowMaximized)) {
                            window->showNormal();
                        } else {
                            window->showMaximized();
                        }
                        window->setCursor(Qt::CursorShape::ArrowCursor);
                    }
                }
#ifdef QT_WIDGETS_LIB
                else if (object->isWidgetType()) {
                    const auto widget = qobject_cast<QWidget *>(object);
                    if (widget) {
                        if (widget->isFullScreen()) {
                            break;
                        }
                        if (widget->isMaximized()) {
                            widget->showNormal();
                        } else {
                            widget->showMaximized();
                        }
                        widget->setCursor(Qt::CursorShape::ArrowCursor);
                    }
                }
#endif
            }
        }
    } break;
    case QEvent::MouseButtonPress: {
        const auto mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent) {
            if (mouseEvent->button() != Qt::MouseButton::LeftButton) {
                break;
            }
            moveOrResize(mouseEvent->screenPos(), mouseEvent->windowPos(),
                         object);
        }
    } break;
    case QEvent::MouseMove: {
        const auto mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent) {
            QWindow *const window = getWindowHandle(object);
            if (window) {
                if (window->windowStates().testFlag(
                        Qt::WindowState::WindowNoState) &&
                    getResizable(object)) {
                    window->setCursor(getCursorShape(
                        getWindowEdges(mouseEvent->windowPos(), window->width(),
                                       window->height())));
                }
            }
#ifdef QT_WIDGETS_LIB
            else {
                const auto widget = qobject_cast<QWidget *>(object);
                if (widget) {
                    if (!widget->isMinimized() && !widget->isMaximized() &&
                        !widget->isFullScreen() && getResizable(object)) {
                        widget->setCursor(getCursorShape(
                            getWindowEdges(mouseEvent->windowPos(),
                                           widget->width(), widget->height())));
                    }
                }
            }
#endif
        }
    } break;
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate: {
        const auto point =
            static_cast<QTouchEvent *>(event)->touchPoints().first();
        moveOrResize(point.screenPos(), point.pos(), object);
    } break;
    default:
        break;
    }
    return false;
}
