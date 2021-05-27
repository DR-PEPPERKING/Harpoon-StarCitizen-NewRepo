// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QViewportPane.h"

#include "EditorFramework/Events.h"
#include "Objects/BaseObject.h"
#include "GameEngine.h"
#include "IViewportManager.h"
#include "QFullScreenWidget.h"
#include "QtUtil.h"
#include "RenderViewport.h"
#include "Viewport.h"

#include <IEditor.h>

#include <CryInput/IHardwareMouse.h>

#include <QKeyEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QVBoxLayout>

namespace
{

inline uint32 GetMouseFlags(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
	uint32 nFlags = 0;
	if (buttons & Qt::MouseButton::LeftButton)
		nFlags |= MK_LBUTTON;
	if (buttons & Qt::MouseButton::RightButton)
		nFlags |= MK_RBUTTON;
	if (buttons & Qt::MouseButton::MiddleButton)
		nFlags |= MK_MBUTTON;
	if (modifiers & Qt::Modifier::CTRL)
		nFlags |= MK_CONTROL;
	if (modifiers & Qt::Modifier::SHIFT)
		nFlags |= MK_SHIFT;
	if (modifiers & Qt::Modifier::ALT)
		nFlags |= MK_ALT;
	return nFlags;
}

inline uint32 GetWheelFlags(QWheelEvent* event)
{
	uint32 nFlags = 0;
	if (event->buttons() & Qt::MouseButton::LeftButton)
		nFlags |= MK_LBUTTON;
	if (event->buttons() & Qt::MouseButton::RightButton)
		nFlags |= MK_RBUTTON;
	if (event->buttons() & Qt::MouseButton::MiddleButton)
		nFlags |= MK_MBUTTON;
	if (event->modifiers() & Qt::Modifier::CTRL)
		nFlags |= MK_CONTROL;
	if (event->modifiers() & Qt::Modifier::SHIFT)
		nFlags |= MK_SHIFT;
	if (event->modifiers() & Qt::Modifier::ALT)
		nFlags |= MK_ALT;
	return nFlags;
}

inline uint32 GetKeyFlags(QKeyEvent* event)
{
	uint32 nFlags = 0;
	if (event->modifiers() & Qt::Modifier::CTRL)
		nFlags |= MK_CONTROL;
	if (event->modifiers() & Qt::Modifier::SHIFT)
		nFlags |= MK_SHIFT;
	if (event->modifiers() & Qt::Modifier::ALT)
		nFlags |= MK_ALT;
	return nFlags;
}

inline uint32 GetFlagsStatic()
{
	uint32 nFlags = 0;

	if (QtUtil::IsModifierKeyDown(Qt::Modifier::CTRL))
		nFlags |= MK_CONTROL;
	if (QtUtil::IsModifierKeyDown(Qt::Modifier::SHIFT))
		nFlags |= MK_SHIFT;
	if (QtUtil::IsModifierKeyDown(Qt::Modifier::ALT))
		nFlags |= MK_ALT;
	if (QtUtil::IsMouseButtonDown(Qt::MouseButton::LeftButton))
		nFlags |= MK_LBUTTON;
	if (QtUtil::IsMouseButtonDown(Qt::MouseButton::RightButton))
		nFlags |= MK_RBUTTON;
	if (QtUtil::IsMouseButtonDown(Qt::MouseButton::MiddleButton))
		nFlags |= MK_MBUTTON;
	return nFlags;
}

bool ShouldForwardEvent()
{
	return !GetIEditor()->IsInGameMode();
}

//! Converts a QPoint to a CPoint, using local space of the provided widget.
CPoint QPointToCPointLocal(const QWidget* widget, const QPoint& point)
{
	return CPoint(QtUtil::PixelScale(widget, point.x()), QtUtil::PixelScale(widget, point.y()));
}

} // unnamed namespace

QViewportWidget::QViewportWidget(CViewport* viewport)
{
	m_viewport = viewport;

	setMouseTracking(true);
	setFocusPolicy(Qt::WheelFocus);
	setAttribute(Qt::WA_PaintOnScreen);

	setAcceptDrops(true);
}

void QViewportWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (ShouldForwardEvent())
	{
		CPoint point = QPointToCPointLocal(this, event->pos());
		if (m_viewport->MouseCallback(eMouseMove, point, GetMouseFlags(event->buttons(), event->modifiers())))
			return;
	}
}

void QViewportWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	EMouseEvent key;
	if (event->button() == Qt::MouseButton::LeftButton)
		key = eMouseLDblClick;
	else if (event->button() == Qt::MouseButton::RightButton)
		key = eMouseRDblClick;
	else if (event->button() == Qt::MouseButton::MiddleButton)
		key = eMouseMDblClick;

	if (ShouldForwardEvent())
	{
		if (m_viewport->MouseCallback(key, QPointToCPointLocal(this, event->pos()), GetMouseFlags(event->buttons(), event->modifiers())))
			return;
	}

	//TODO : all key and mouse events must be correctly ignored if not handled
	//event->ignore();
}

void QViewportWidget::mousePressEvent(QMouseEvent* event)
{
	if (ShouldForwardEvent())
	{
		EMouseEvent key;
		if (event->button() == Qt::MouseButton::LeftButton)
			key = eMouseLDown;
		else if (event->button() == Qt::MouseButton::RightButton)
			key = eMouseRDown;
		else if (event->button() == Qt::MouseButton::MiddleButton)
			key = eMouseMDown;

		m_viewport->MouseCallback(key, QPointToCPointLocal(this, event->pos()), GetMouseFlags(event->buttons(), event->modifiers()));
	}
	else
	{
		if (GetIEditor()->GetViewportManager()->GetGameViewport() == m_viewport)
		{
			// if game mode is suspended, unsuspend now
			if (GetIEditor()->IsGameInputSuspended())
			{
				GetIEditor()->ToggleGameInputSuspended();
			}
		}

		if (gEnv->pHardwareMouse)
		{
			if (event->button() == Qt::MouseButton::LeftButton)
				gEnv->pHardwareMouse->Event(QtUtil::PixelScale(this, event->x()), QtUtil::PixelScale(this, event->y()), HARDWAREMOUSEEVENT_LBUTTONDOWN);
			else if (event->button() == Qt::MouseButton::RightButton)
				gEnv->pHardwareMouse->Event(QtUtil::PixelScale(this, event->x()), QtUtil::PixelScale(this, event->y()), HARDWAREMOUSEEVENT_RBUTTONDOWN);
			else if (event->button() == Qt::MouseButton::MiddleButton)
				gEnv->pHardwareMouse->Event(QtUtil::PixelScale(this, event->x()), QtUtil::PixelScale(this, event->y()), HARDWAREMOUSEEVENT_MBUTTONDOWN);
		}
	}
}

void QViewportWidget::mouseReleaseEvent(QMouseEvent* event)
{
	if (ShouldForwardEvent())
	{
		EMouseEvent key;
		if (event->button() == Qt::MouseButton::LeftButton)
			key = eMouseLUp;
		else if (event->button() == Qt::MouseButton::RightButton)
			key = eMouseRUp;
		else if (event->button() == Qt::MouseButton::MiddleButton)
			key = eMouseMUp;

		m_viewport->MouseCallback(key, QPointToCPointLocal(this, event->pos()), GetMouseFlags(event->buttons(), event->modifiers()));
	}
	else if (gEnv->pHardwareMouse)
	{
		if (event->button() == Qt::MouseButton::LeftButton)
			gEnv->pHardwareMouse->Event(QtUtil::PixelScale(this, event->x()), QtUtil::PixelScale(this, event->y()), HARDWAREMOUSEEVENT_LBUTTONUP);
		else if (event->button() == Qt::MouseButton::RightButton)
			gEnv->pHardwareMouse->Event(QtUtil::PixelScale(this, event->x()), QtUtil::PixelScale(this, event->y()), HARDWAREMOUSEEVENT_RBUTTONUP);
		else if (event->button() == Qt::MouseButton::MiddleButton)
			gEnv->pHardwareMouse->Event(QtUtil::PixelScale(this, event->x()), QtUtil::PixelScale(this, event->y()), HARDWAREMOUSEEVENT_MBUTTONUP);
	}
}

void QViewportWidget::keyPressEvent(QKeyEvent* event)
{
	if (ShouldForwardEvent())
	{
		int nChar = event->key();
		int nRepCnt = event->isAutoRepeat() ? 1 : 0;

		if (m_viewport->OnKeyDown(nChar, nRepCnt, GetKeyFlags(event)))
			return;
		else if (nChar == Qt::Key_Escape)
		{
			GetIEditor()->GetLevelEditorSharedState()->SetEditTool(nullptr);
			return;
		}
		event->ignore();
	}
}

bool QViewportWidget::focusNextPrevChild(bool next)
{
	return false;
}

void QViewportWidget::keyReleaseEvent(QKeyEvent* event)
{
	if (ShouldForwardEvent())
	{
		int nChar = event->key();
		int nRepCnt = event->isAutoRepeat() ? 1 : 0;
		if (m_viewport->OnKeyUp(nChar, nRepCnt, GetKeyFlags(event)))
			return;
	}

	//TODO : all key and mouse events must be correctly ignored if not handled
	//event->ignore();
}

void QViewportWidget::wheelEvent(QWheelEvent* event)
{
	if (ShouldForwardEvent())
	{
		int zDelta = event->delta();
		if (m_viewport->MouseCallback(eMouseWheel, CPoint(zDelta, zDelta), GetWheelFlags(event)))
			return;
	}
	else if (gEnv->pHardwareMouse)
	{
		gEnv->pHardwareMouse->Event(QtUtil::PixelScale(this, event->x()), QtUtil::PixelScale(this, event->y()), HARDWAREMOUSEEVENT_WHEEL, event->delta());
	}

	//TODO : all key and mouse events must be correctly ignored if not handled
	//event->ignore();
}

void QViewportWidget::enterEvent(QEvent* pEvent)
{
	// These events are dispatched immediately. There are cases in which an enter frame event will be
	// fired/received before the previous has been digested.
	if (ShouldForwardEvent())
	{
		QPoint point = QtUtil::GetMousePosition(this);
		m_viewport->MouseCallback(eMouseEnter, QPointToCPointLocal(this, point), GetFlagsStatic());
	}
	else
	{
		if (!GetIEditor()->IsGameInputSuspended() && GetIEditor()->GetViewportManager()->GetGameViewport() == m_viewport)
		{
			m_viewport->SetCurrentCursor(STD_CURSOR_CRYSIS);
		}
	}
}

void QViewportWidget::leaveEvent(QEvent*)
{
	if (ShouldForwardEvent())
	{
		QPoint point = QtUtil::GetMousePosition(this);
		m_viewport->MouseCallback(eMouseLeave, QPointToCPointLocal(this, point), GetFlagsStatic());
	}
}

void QViewportWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	m_viewport->OnResize();
	setAttribute(Qt::WA_PaintOnScreen, false);
}

void QViewportWidget::paintEvent(QPaintEvent* event)
{
	if (GetIEditor()->IsUpdateSuspended())
		return;

	setAttribute(Qt::WA_PaintOnScreen, true);
	m_viewport->OnPaint();
}

void QViewportWidget::focusInEvent(QFocusEvent* event)
{
	m_viewport->SetFocus(true);

	if (ShouldForwardEvent())
	{
		QPoint point = QtUtil::GetMousePosition(this);
		m_viewport->MouseCallback(eMouseFocusEnter, QPointToCPointLocal(this, point), GetFlagsStatic());
	}
	else
	{
		if (GetIEditor()->GetViewportManager()->GetGameViewport() == m_viewport)
		{
			GetIEditor()->GetSystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAMEWINDOW_ACTIVATE, true, 0);
		}
	}
}

void QViewportWidget::focusOutEvent(QFocusEvent* event)
{
	m_viewport->SetFocus(false);

	if (ShouldForwardEvent())
	{
		QPoint point = QtUtil::GetMousePosition(this);
		m_viewport->MouseCallback(eMouseFocusLeave, QPointToCPointLocal(this, point), GetFlagsStatic());
	}
	else
	{
		if (GetIEditor()->GetViewportManager()->GetGameViewport() == m_viewport)
		{
			GetIEditor()->GetSystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_GAMEWINDOW_ACTIVATE, false, 0);
		}
	}
}

template<typename T>
class ScaledDropEvent : public T
{
public:
	QPointF scale(QWidget* scaleSource)
	{
		QPointF oldP = p;
		p = QtUtil::PixelScale(scaleSource, p);
		return oldP;
	}

	void restore(QPointF oldP)
	{
		p = oldP;
	}
};

void QViewportWidget::dragEnterEvent(QDragEnterEvent* event)
{
	if (ShouldForwardEvent())
	{
		ScaledDropEvent<QDragEnterEvent>* e2 = (ScaledDropEvent<QDragEnterEvent>*)event;
		QPointF p = e2->scale(this);
		m_viewport->DragEvent(eDragEnter, e2, GetFlagsStatic());
		e2->restore(p);
	}
}

void QViewportWidget::dragMoveEvent(QDragMoveEvent* event)
{
	if (ShouldForwardEvent())
	{
		ScaledDropEvent<QDragMoveEvent>* e2 = (ScaledDropEvent<QDragMoveEvent>*)event;
		QPointF p = e2->scale(this);
		m_viewport->DragEvent(eDragMove, e2, GetFlagsStatic());
		e2->restore(p);
	}
}

void QViewportWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
	if (ShouldForwardEvent())
	{
		m_viewport->DragEvent(eDragLeave, event, GetFlagsStatic());
	}
}

void QViewportWidget::dropEvent(QDropEvent* event)
{
	if (ShouldForwardEvent())
	{
		ScaledDropEvent<QDropEvent>* e2 = (ScaledDropEvent<QDropEvent>*)event;
		QPointF p = e2->scale(this);
		m_viewport->DragEvent(eDrop, e2, GetFlagsStatic());
		e2->restore(p);
	}
}

void QViewportWidget::customEvent(QEvent* pEvent)
{
	if (pEvent->type() == SandboxEvent::CameraMovement)
	{
		CameraTransformEvent* mouse3devt = static_cast<CameraTransformEvent*>(pEvent);
		m_viewport->OnCameraTransformEvent(mouse3devt);
		pEvent->setAccepted(true);
	}

	if (pEvent->type() == SandboxEvent::CryInput)
	{
		CryInputEvent* e = static_cast<CryInputEvent*>(pEvent);
		m_viewport->OnFilterCryInputEvent(e);
	}

	if (pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* pCommandEvent = static_cast<CommandEvent*>(pEvent);
		const string& command = pCommandEvent->GetCommand();
		if (command == "viewport.toggle_helpers")
		{
			auto& settings = m_viewport->GetHelperSettings();
			settings.enabled = !settings.enabled;
			settings.signalStateChanged();

			pEvent->setAccepted(true);
		}
	}
}

QAspectRatioWidget::QAspectRatioWidget(QViewportWidget* pViewport)
{
	setFocusPolicy(Qt::NoFocus);
	m_layout = new QAspectLayout;
	setLayout(m_layout);
	layout()->addWidget(pViewport);

	if (pViewport->GetViewport() && pViewport->GetViewport()->IsRenderViewport())
	{
		CRenderViewport* rv = (CRenderViewport*)pViewport->GetViewport();
		rv->signalResolutionChanged.Connect(this, &QAspectRatioWidget::updateAspect);
	}
}

// Copied from http://www.qtcentre.org/threads/37976-Q_OBJECT-and-CSS-background-image
void QAspectRatioWidget::paintEvent(QPaintEvent* pe)
{
	QStyleOption o;
	o.initFrom(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &o, &p, this);
}

void QAspectRatioWidget::updateAspect()
{
	const QRect geomRect = geometry();
	// own rect has position according to parent, adjust here
	m_layout->setGeometry(QRect(0, 0, geomRect.width(), geomRect.height()));
}

QAspectLayout::QAspectLayout() : m_child(nullptr)
{
}

QAspectLayout::~QAspectLayout()
{
	if (m_child)
	{
		delete m_child;
	}
}

void QAspectLayout::addItem(QLayoutItem* item)
{
	m_child = item;
}

QSize QAspectLayout::sizeHint() const
{
	if (m_child)
	{
		return m_child->sizeHint();
	}
	else
	{
		return QSize(400, 300);
	}
}

QSize QAspectLayout::minimumSize() const
{
	if (m_child)
	{
		return m_child->minimumSize();
	}
	else
	{
		return QSize(400, 300);
	}
}

int QAspectLayout::count() const
{
	return (m_child != nullptr) ? 1 : 0;
}

QLayoutItem* QAspectLayout::itemAt(int i) const
{
	return (i == 0) ? m_child : nullptr;
}

QLayoutItem* QAspectLayout::takeAt(int i)
{
	if (i == 0)
	{
		QLayoutItem* r = m_child;
		m_child = nullptr;
		return r;
	}
	else
	{
		return nullptr;
	}
}

void QAspectLayout::setGeometry(const QRect& rect)
{
	QLayout::setGeometry(rect);

	if (!m_child)
	{
		return;
	}

	QViewportWidget* w = qobject_cast<QViewportWidget*>(m_child->widget());
	if (!w)
	{
		return;
	}

	CViewport* viewport = w->GetViewport();
	CViewport::EResolutionMode mode = viewport->GetResolutionMode();

	int width, height;
	viewport->GetResolution(width, height);
	width = width / w->devicePixelRatioF();
	height = height / w->devicePixelRatioF();

	QPoint upleft;
	QPoint downRight;

	switch (mode)
	{
	case CViewport::EResolutionMode::Window:
		{
			upleft = rect.topLeft();
			downRight = rect.bottomRight();
			break;
		}
	case CViewport::EResolutionMode::Stretch:
		{
			// First get aspect ratio of current widget and aspect ratio of child widget
			float viewportAspect = width / (float)height;

			width = rect.width();
			height = rect.height();
			float ownAspect = width / (float)height;

			float startX, endX, startY, endY;

			if (ownAspect > viewportAspect)
			{
				startY = 0;
				endY = height;
				int screenWidth = viewportAspect * height;
				startX = (width - screenWidth) / 2;
				endX = (width + screenWidth) / 2;
			}
			else
			{
				startX = 0;
				endX = width;
				int screenHeight = width / viewportAspect;
				startY = (height - screenHeight) / 2;
				endY = (height + screenHeight) / 2;
			}

			upleft.setX(startX);
			upleft.setY(startY);
			downRight.setX(endX);
			downRight.setY(endY);
			break;
		}
	case CViewport::EResolutionMode::Center:
		{
			// Now find the difference between this resolution and the new geometry and split it into two
			upleft.setX((rect.width() - width) / 2);
			upleft.setY((rect.height() - height) / 2);
			downRight.setX(upleft.x() + width);
			downRight.setY(upleft.y() + height);
			break;
		}
	case CViewport::EResolutionMode::TopRight:
		{
			upleft.setX(rect.width() - width);
			upleft.setY(0);
			downRight.setX(rect.width());
			downRight.setY(height);
			break;
		}
	case CViewport::EResolutionMode::TopLeft:
		{
			upleft.setX(0);
			upleft.setY(0);
			downRight.setX(width);
			downRight.setY(height);
			break;
		}
	case CViewport::EResolutionMode::BottomRight:
		{
			upleft.setX(rect.width() - width);
			upleft.setY(rect.height() - height);
			downRight.setX(rect.width());
			downRight.setY(rect.height());
			break;
		}
	case CViewport::EResolutionMode::BottomLeft:
		{
			upleft.setX(0);
			upleft.setY(rect.height() - height);
			downRight.setX(width);
			downRight.setY(rect.height());
			break;
		}
	}

	QRect source(upleft, downRight);
	m_child->setGeometry(source.intersected(rect));
}

QViewportPane::QViewportPane(CViewport* viewport, QWidget* headerWidget)
{
	m_viewport = viewport;
	m_headerWidget = headerWidget;

	// Init widgets
	m_viewWidget = new QViewportWidget(viewport);

	QVBoxLayout* layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	if (m_headerWidget)
		layout->addWidget(m_headerWidget);

	// wrap renderviewports in letterboxing widget to preserve aspect ratio in custom resolutions
	if (viewport->IsRenderViewport())
	{
		QWidget* aspectWidget = new QAspectRatioWidget(m_viewWidget);
		QFullScreenWidget* fullscreen = new QFullScreenWidget(aspectWidget);
		layout->addWidget(fullscreen);
	}
	else
	{
		layout->addWidget(m_viewWidget);
	}
	setLayout(layout);

	viewport->SetViewWidget(m_viewWidget);
}

namespace Private_QViewportPane
{

void ToggleHelpers()
{
	CommandEvent("viewport.toggle_helpers").SendToKeyboardFocus();
}

}

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_QViewportPane::ToggleHelpers, viewport, toggle_helpers, CCommandDescription("Toggle display of helpers in the viewport"))
REGISTER_EDITOR_UI_COMMAND_DESC(viewport, toggle_helpers, "", "Ctrl+H", "icons:Viewport/viewport-helpers.ico", true)
REGISTER_COMMAND_REMAPPING(ui_action, actionShow_Hide_Helpers, viewport, toggle_helpers)
