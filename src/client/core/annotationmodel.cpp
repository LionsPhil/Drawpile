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
#include "annotationmodel.h"

#include <QTextDocument>
#include <QPainter>
#include <QImage>
#include <QSet> // for old-style change notifications

namespace paintcore {

AnnotationModel::AnnotationModel(QObject *parent)
	: QAbstractListModel(parent)
{
}

int AnnotationModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_annotations.size();
}

QVariant AnnotationModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_annotations.size()) {
		const Annotation &a = m_annotations.at(index.row());
		switch(role) {
			case Qt::DisplayRole: return a.text;
			case IdRole: return a.id;
			case RectRole: return a.rect;
			case BgColorRole: return a.background;
			default: break;
		}
	}
	return QVariant();
}

QHash<int, QByteArray> AnnotationModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[Qt::DisplayRole] = "display";
	roles[IdRole] = "annotationId";
	roles[RectRole] = "rect";
	roles[BgColorRole] = "background";
	return roles;
}

void AnnotationModel::addAnnotation(const Annotation &annotation)
{
	// Make sure ID is unique
	if(findById(annotation.id)>=0) {
		qWarning("Cannot add annotation: ID (%d) not unique!", annotation.id);
		return;
	}

	beginInsertRows(QModelIndex(), m_annotations.size(), m_annotations.size());
	m_annotations.append(annotation);
	endInsertRows();
}

void AnnotationModel::addAnnotation(int id, const QRect &rect)
{
	addAnnotation(Annotation {id, QString(), rect, QColor(Qt::transparent)});
}

void AnnotationModel::deleteAnnotation(int id)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot remove annotation: ID %d not found!", id);
		return;
	}

	beginRemoveRows(QModelIndex(), idx, idx);
	m_annotations.removeAt(idx);
	endRemoveRows();
}

void AnnotationModel::reshapeAnnotation(int id, const QRect &newrect)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot reshape annotation: ID %d not found!", id);
		return;
	}

	m_annotations[idx].rect = newrect;
	emit dataChanged(index(idx), index(idx), QVector<int>() << RectRole);
}

void AnnotationModel::changeAnnotation(int id, const QString &newtext, const QColor &bgcolor)
{
	int idx = findById(id);
	if(idx<0) {
		qWarning("Cannot change annotation: ID %d not found!", id);
		return;
	}
	m_annotations[idx].text = newtext;
	m_annotations[idx].background = bgcolor;

	emit dataChanged(index(idx), index(idx), QVector<int>() << Qt::DisplayRole << BgColorRole);
}

void AnnotationModel::setAnnotations(const QList<Annotation> &annotations)
{
	beginResetModel();
	m_annotations = annotations;
	endResetModel();
}

const Annotation *AnnotationModel::getById(int id) const
{
	for(const Annotation &a : m_annotations)
		if(a.id == id)
			return &a;
	return nullptr;
}

int AnnotationModel::findById(int id) const
{
	for(int i=0;i<m_annotations.size();++i)
		if(m_annotations.at(i).id == id)
			return i;
	return -1;
}

/**
 * @brief Find the annotation at the given coordinates.
 * @param pos point in canvas coordinates
 * @return annotation ID or 0 if none found
 */
int AnnotationModel::annotationAtPos(const QPoint &pos, qreal zoom) const
{
	const int H = qRound(qMax(qreal(Annotation::HANDLE_SIZE), Annotation::HANDLE_SIZE / zoom) / 2.0);
	for(const Annotation &a : m_annotations) {
		if(a.rect.adjusted(-H, -H, H, H).contains(pos))
			return a.id;
	}
	return 0;
}

Annotation::Handle AnnotationModel::annotationHandleAt(int id, const QPoint &point, qreal zoom) const
{
	const Annotation *a = getById(id);
	if(a)
		return a->handleAt(point, zoom);
	return Annotation::OUTSIDE;
}

Annotation::Handle AnnotationModel::annotationAdjustGeometry(int id, Annotation::Handle handle, const QPoint &delta)
{
	for(int idx=0;idx<m_annotations.size();++idx) {
		if(m_annotations.at(idx).id == id) {
			handle = m_annotations[idx].adjustGeometry(handle, delta);
			emit dataChanged(index(idx), index(idx), QVector<int>() << RectRole);
			return handle;
		}
	}
	return Annotation::OUTSIDE;
}

QList<int> AnnotationModel::getEmptyIds() const
{
	QList<int> ids;
	for(const Annotation &a : m_annotations) {
		if(a.isEmpty())
			ids << a.id;
	}
	return ids;
}

void Annotation::paint(QPainter *painter) const
{
	paint(painter, rect);
}

void Annotation::paint(QPainter *painter, const QRectF &paintrect) const
{
	painter->save();
	painter->translate(paintrect.topLeft());

	const QRectF rect0(QPointF(), paintrect.size());

	painter->fillRect(rect0, background);

	QTextDocument doc;
	doc.setHtml(text);
	doc.setTextWidth(rect0.width());
	doc.drawContents(painter, rect0);

	painter->restore();
}

QImage Annotation::toImage() const
{
	QImage img(rect.size(), QImage::Format_ARGB32);
	img.fill(0);
	QPainter painter(&img);
	paint(&painter, QRectF(0, 0, rect.width(), rect.height()));
	return img;
}

/**
 * Note. Assumes point is inside the text box.
 */
Annotation::Handle Annotation::handleAt(const QPoint &point, qreal zoom) const
{
	const qreal H = qMax(qreal(HANDLE_SIZE), HANDLE_SIZE / zoom);

	const QRectF R = QRectF(rect.x()-H/2, rect.y()-H/2, rect.width()+H, rect.height()+H);

	if(!R.contains(point))
		return OUTSIDE;

	QPointF p = point - R.topLeft();

	if(p.x() < H) {
		if(p.y() < H)
			return RS_TOPLEFT;
		else if(p.y() > R.height()-H)
			return RS_BOTTOMLEFT;
		return RS_LEFT;
	} else if(p.x() > R.width() - H) {
		if(p.y() < H)
			return RS_TOPRIGHT;
		else if(p.y() > R.height()-H)
			return RS_BOTTOMRIGHT;
		return RS_RIGHT;
	} else if(p.y() < H)
		return RS_TOP;
	else if(p.y() > R.height()-H)
		return RS_BOTTOM;

	return TRANSLATE;
}

Annotation::Handle Annotation::adjustGeometry(Handle handle, const QPoint &delta)
{
	switch(handle) {
	case OUTSIDE: return handle;
	case TRANSLATE: rect.translate(delta); break;
	case RS_TOPLEFT: rect.adjust(delta.x(), delta.y(), 0, 0); break;
	case RS_TOPRIGHT: rect.adjust(0, delta.y(), delta.x(), 0); break;
	case RS_BOTTOMRIGHT: rect.adjust(0, 0, delta.x(), delta.y()); break;
	case RS_BOTTOMLEFT: rect.adjust(delta.x(), 0, 0, delta.y()); break;
	case RS_TOP: rect.adjust(0, delta.y(), 0, 0); break;
	case RS_RIGHT: rect.adjust(0, 0, delta.x(), 0); break;
	case RS_BOTTOM: rect.adjust(0, 0, 0, delta.y()); break;
	case RS_LEFT: rect.adjust(delta.x(), 0, 0, 0); break;
	}

	if(rect.left() > rect.right() || rect.top() > rect.bottom()) {

		if(rect.left() > rect.right()) {
			switch(handle) {
				case RS_TOPLEFT: handle = RS_TOPRIGHT; break;
				case RS_TOPRIGHT: handle = RS_TOPLEFT; break;
				case RS_BOTTOMRIGHT: handle = RS_BOTTOMLEFT; break;
				case RS_BOTTOMLEFT: handle = RS_BOTTOMRIGHT; break;
				case RS_LEFT: handle = RS_RIGHT; break;
				case RS_RIGHT: handle = RS_LEFT; break;
				default: break;
			}
		}
		if(rect.top() > rect.bottom()) {
			switch(handle) {
				case RS_TOPLEFT: handle = RS_BOTTOMLEFT; break;
				case RS_TOPRIGHT: handle = RS_BOTTOMRIGHT; break;
				case RS_BOTTOMRIGHT: handle = RS_TOPRIGHT; break;
				case RS_BOTTOMLEFT: handle = RS_TOPRIGHT; break;
				case RS_TOP: handle = RS_BOTTOM; break;
				case RS_BOTTOM: handle = RS_TOP; break;
				default: break;
			}
		}

		rect = rect.normalized();
	}

	return handle;
}

void Annotation::toDataStream(QDataStream &out) const
{
	// Write ID
	out << quint16(id);

	// Write position and size
	out << rect;

	// Write content
	out << background;
	out << text;
}

Annotation Annotation::fromDataStream(QDataStream &in)
{
	quint16 id;
	in >> id;

	QRect rect;
	in >> rect;

	QColor color;
	in >> color;

	QString text;
	in >> text;
	return Annotation {id, text, rect, color};
}

}

