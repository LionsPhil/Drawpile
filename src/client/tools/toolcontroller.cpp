/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "toolcontroller.h"
#include "docks/toolsettingsdock.h"

#include "annotation.h"
#include "brushes.h"
#include "colorpicker.h"
#include "laser.h"
#include "selection.h"
#include "shapetools.h"
#include "floodfill.h"

#include "core/point.h"
#include "core/layerstack.h"
#include "core/annotationmodel.h"
#include "canvas/canvasmodel.h"
#include "canvas/statetracker.h"

namespace tools {

ToolController::ToolController(net::Client *client, docks::ToolSettings *toolSettings, QObject *parent)
	: QObject(parent),
	m_client(client), m_toolsettings(toolSettings),
	m_model(nullptr), m_activeTool(nullptr), m_smoothing(0)
{
	Q_ASSERT(client);
	Q_ASSERT(toolSettings);

	registerTool(new Pen(*this));
	registerTool(new Brush(*this));
	registerTool(new Smudge(*this));
	registerTool(new Eraser(*this));
	registerTool(new ColorPicker(*this));
	registerTool(new Line(*this));
	registerTool(new Rectangle(*this));
	registerTool(new Ellipse(*this));
	registerTool(new FloodFill(*this));
	registerTool(new Annotation(*this));
	registerTool(new LaserPointer(*this));
	registerTool(new RectangleSelection(*this));
	registerTool(new PolygonSelection(*this));

	m_activeTool = m_toolbox[Tool::PEN];
	m_activeLayer = 0;
	m_activeAnnotation = 0;
}

void ToolController::registerTool(Tool *tool)
{
	Q_ASSERT(!m_toolbox.contains(tool->type()));
	m_toolbox[tool->type()] = tool;
}

ToolController::~ToolController()
{
	for(Tool *t : m_toolbox)
		delete t;
}

Tool *ToolController::getTool(Tool::Type type) const
{
	Tool *t = m_toolbox[type];
	Q_ASSERT(t);
	return t;
}

void ToolController::setActiveTool(Tool::Type tool)
{
	if(activeTool() != tool) {
		m_activeTool = getTool(tool);
		emit activeToolChanged(tool);
		emit toolCursorChanged(activeToolCursor());
	}
}

void ToolController::setActiveAnnotation(int id)
{
	if(m_activeAnnotation != id) {
		m_activeAnnotation = id;
		emit activeAnnotationChanged(id);
	}
}

Tool::Type ToolController::activeTool() const
{
	Q_ASSERT(m_activeTool);
	return m_activeTool->type();
}

QCursor ToolController::activeToolCursor() const
{
	Q_ASSERT(m_activeTool);
	return m_activeTool->cursor();
}

void ToolController::setActiveLayer(int id)
{
	if(m_activeLayer != id) {
		m_activeLayer = id;
		if(m_model)
			m_model->layerStack()->setViewLayer(id);

		emit activeLayerChanged(id);
	}
}

paintcore::Brush ToolController::activeBrush() const
{
	return m_toolsettings->getBrush();
}

void ToolController::setModel(canvas::CanvasModel *model)
{
	if(m_model != model) {
		if(m_model) {
			disconnect(m_model->stateTracker(), &canvas::StateTracker::myAnnotationCreated, this, &ToolController::setActiveAnnotation);
			disconnect(m_model->layerStack()->annotations(), &paintcore::AnnotationModel::rowsAboutToBeRemoved, this, &ToolController::onAnnotationRowDelete);
		}

		m_model = model;

		connect(m_model->stateTracker(), &canvas::StateTracker::myAnnotationCreated, this, &ToolController::setActiveAnnotation);
		connect(m_model->layerStack()->annotations(), &paintcore::AnnotationModel::rowsAboutToBeRemoved, this, &ToolController::onAnnotationRowDelete);

		emit modelChanged(model);
	}
}

void ToolController::onAnnotationRowDelete(const QModelIndex&, int first, int last)
{
	for(int i=first;i<=last;++i) {
		const QModelIndex &a = m_model->layerStack()->annotations()->index(i);
		if(a.data(paintcore::AnnotationModel::IdRole).toInt() == activeAnnotation())
			setActiveAnnotation(0);
	}
}

void ToolController::setSmoothing(int smoothing)
{
	if(m_smoothing != smoothing) {
		m_smoothing = smoothing;
		if(smoothing>0)
			m_smoother.setSmoothing(smoothing);
		emit smoothingChanged(smoothing);
	}
}

void ToolController::startDrawing(const QPointF &point, qreal pressure)
{
	Q_ASSERT(m_activeTool);

	if(!m_model) {
		qWarning("ToolController::startDrawing: no model set!");
		return;
	}

	if(m_smoothing>0 && m_activeTool->allowSmoothing()) {
		m_smoother.reset();
		m_smoother.addPoint(paintcore::Point(point, pressure));
	}
	// TODO handle hasSmoothPoint() == false
	m_activeTool->begin(paintcore::Point(point, pressure), /*_zoom (TODO)*/ 1.0);
}

void ToolController::continueDrawing(const QPointF &point, qreal pressure, bool shift, bool alt)
{
	Q_ASSERT(m_activeTool);

	if(!m_model) {
		qWarning("ToolController::continueDrawing: no model set!");
		return;
	}

	if(m_smoothing>0 && m_activeTool->allowSmoothing()) {
		m_smoother.addPoint(paintcore::Point(point, pressure));

		if(m_smoother.hasSmoothPoint()) {
			m_activeTool->motion(m_smoother.smoothPoint(), shift, alt);
		}

	} else {
		m_activeTool->motion(paintcore::Point(point, pressure), shift, alt);
	}
}

void ToolController::endDrawing()
{
	Q_ASSERT(m_activeTool);

	if(!m_model) {
		qWarning("ToolController::endDrawing: no model set!");
		return;
	}

	m_activeTool->end();
}

}
