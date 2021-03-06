/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "toolslotbutton.h"
#include "utils/icon.h"

#include <QStylePainter>
#include <QStyleOptionToolButton>
#include <QMouseEvent>

namespace widgets {

ToolSlotButton::ToolSlotButton(QWidget *parent) :
	QToolButton(parent), _isHovering(false)
{
	_highlight = palette().color(QPalette::Highlight);
	_hover = _highlight;
	_hover.setAlphaF(0.5);
}

void ToolSlotButton::setColors(const QColor &fg, const QColor &bg)
{
	_fg = fg;
	_bg = bg;
	update();
}

void ToolSlotButton::setIcons(const QIcon &light, const QIcon &dark)
{
	_lightIcon = light;
	_darkIcon = dark;
	update();
}

void ToolSlotButton::setHighlightColor(const QColor &c)
{
	_highlight = c;
	update();
}

void ToolSlotButton::setHoverColor(const QColor &c)
{
	_hover = c;
	update();
}

void ToolSlotButton::paintEvent(QPaintEvent *)
{
	const int UNDERLINE = 5;
	QPainter p(this);
	const QRect rect(0, 0, width(), height());
	// Draw background
	p.fillRect(rect, _fg);

	// Draw icon
	const int iconSize = qMax(16, qMin(rect.width(), rect.height()) / 2);
	const QRect iconRect(
		(rect.width() - iconSize)/2,
		(rect.height() - iconSize)/2,
		iconSize, iconSize);

	const QIcon &icon = icon::isDark(_fg) ? _darkIcon : _lightIcon;
	icon.paint(&p, iconRect);

	// Draw selection highlight
	if(isChecked() || _isHovering) {
		const QColor c = isChecked() ? _highlight : _hover;
		p.fillRect(0, height() - UNDERLINE, width(), UNDERLINE, c);
	}

	// Draw bottom border
	p.fillRect(0, height() - 1, width(), 1, _highlight);
}

bool ToolSlotButton::event(QEvent *e)
{
	switch(e->type()) {
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonRelease:
	case QEvent::MouseMove: {
		// We let these propagate to the parent dock widget
		// since we are using toolslotbuttons as the title
		// However, we still need to respond to mouse clicks here,
		// so we make a copy of the event for our own use.
		QMouseEvent *me = static_cast<QMouseEvent*>(e);
		QMouseEvent ce(me->type(), me->localPos(), me->button(), me->buttons(), me->modifiers());
		QToolButton::event(&ce);
		return false;
	}

	case QEvent::MouseButtonDblClick:
		// Double click event toggles floating mode
		return false;

	case QEvent::Enter:
		_isHovering = true;
		break;
	case QEvent::Leave:
		_isHovering = false;
		break;

	default: break;
	}
	return QToolButton::event(e);
}

}
