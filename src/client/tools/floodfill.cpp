/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include "tools/toolcontroller.h"
#include "tools/floodfill.h"
#include "tools/toolsettings.h"

#include "docks/toolsettingsdock.h"
#include "core/floodfill.h"
#include "canvas/canvasmodel.h"
#include "net/client.h"

#include <QApplication>

namespace tools {

FloodFill::FloodFill(ToolController &owner)
	: Tool(owner, FLOODFILL, QCursor(QPixmap(":cursors/bucket.png"), 2, 29))
{
}

void FloodFill::begin(const paintcore::Point &point, float zoom)
{
	Q_UNUSED(zoom);
	FillSettings *ts = owner.toolSettings()->getFillSettings();
	QColor color = owner.toolSettings()->foregroundColor();

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	paintcore::FillResult fill = paintcore::floodfill(
		owner.model()->layerStack(),
		QPoint(point.x(), point.y()),
		color,
		ts->fillTolerance(),
		owner.activeLayer(),
		ts->sampleMerged()
	);

	fill = paintcore::expandFill(fill, ts->fillExpansion(), color);

	if(fill.image.isNull()) {
		QApplication::restoreOverrideCursor();
		return;
	}

	// If the target area is transparent, use the BEHIND compositing mode.
	// This results in nice smooth blending with soft outlines, when the
	// outline has different color than the fill.
	paintcore::BlendMode::Mode mode = paintcore::BlendMode::MODE_NORMAL;
	if(ts->underFill() && (fill.layerSeedColor & 0xff000000) == 0)
		mode = paintcore::BlendMode::MODE_BEHIND;

	// Flood fill is implemented using PutImage rather than a native command.
	// This has the following advantages:
	// - backward and forward compatibility: changes in the algorithm can be made freely
	// - tolerates out-of-sync canvases (shouldn't normally happen, but...)
	// - bugs don't crash/freeze other clients
	//
	// The disadvantage is increased bandwith consumption. However, this is not as bad
	// as one might think: the effective bit-depth of the bitmap is 1bpp and most fills
	// consist of large solid areas, meaning they should compress ridiculously well.
	owner.client()->sendUndopoint();
	owner.client()->sendImage(owner.activeLayer(), fill.x, fill.y, fill.image, mode);

	QApplication::restoreOverrideCursor();
}

void FloodFill::motion(const paintcore::Point &point, bool constrain, bool center)
{
	Q_UNUSED(point);
	Q_UNUSED(constrain);
	Q_UNUSED(center);
}

void FloodFill::end()
{
}

}
