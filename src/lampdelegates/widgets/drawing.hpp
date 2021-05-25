
#ifndef QTMWIDGETS__DRAWING_HPP__INCLUDED
#define QTMWIDGETS__DRAWING_HPP__INCLUDED

// Qt include.
#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QPainter;
class QRect;
class QColor;
QT_END_NAMESPACE


namespace lampproject {

//
// drawCylinder
//

//! Draw cylinder with rect \a r.
void drawCylinder( QPainter * p, const QRect & r,
	bool roundLeftCorner = true, bool roundRightCorner = true );


//
// drawSliderHandle
//

void drawSliderHandle( QPainter * p, const QRect & r,
	int xRadius, int yRadius, const QColor & borderColor,
	const QColor & lightColor );


//
// drawArrow
//

void drawArrow( QPainter * p, const QRect & r,
	const QColor & color );

} /* namespace QtMWidgets */

#endif // QTMWIDGETS__DRAWING_HPP__INCLUDED
