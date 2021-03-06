// QtMWidgets include.
#include "drawing.hpp"
#include "color.hpp"

// Qt include.
#include <QPainter>
#include <QStyleOption>
#include <QLinearGradient>
#include <QBrush>
#include <QPen>
#include <QPainterPath>


namespace lampproject {

//
// drawCylinder
//

void
drawCylinder( QPainter * p, const QRect & r,
	bool roundLeftCorner, bool roundRightCorner )
{
	QLinearGradient firstVertLineGradient( QPointF( 0.0, 0.0 ),
		QPointF( 0.0, 1.0 ) );
	firstVertLineGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
	firstVertLineGradient.setColorAt( 0.0, QColor( 50, 50, 60 ) );
	firstVertLineGradient.setColorAt( 0.5, QColor( 140, 140, 150 ) );
	firstVertLineGradient.setColorAt( 1.0, QColor( 50, 50, 60 ) );

	QLinearGradient secondVertLineGradient( QPointF( 0.0, 0.0 ),
		QPointF( 0.0, 1.0 ) );
	secondVertLineGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
	secondVertLineGradient.setColorAt( 0.0, QColor( 60, 60, 80 ) );
	secondVertLineGradient.setColorAt( 0.5, QColor( 200, 205, 225 ) );
	secondVertLineGradient.setColorAt( 1.0, QColor( 60, 60, 80 ) );

	p->setPen( Qt::NoPen );
	p->setBrush( firstVertLineGradient );

	p->drawRect( r.x(), roundLeftCorner ? 2 : 0,
		1, roundLeftCorner ? r.height() - 4 : r.height() );
	p->drawRect( r.x() + r.width() - 1, roundRightCorner ? 2 : 0,
		1, roundRightCorner ? r.height() - 4 : r.height() );

	p->setBrush( secondVertLineGradient );

	p->drawRect( r.x() + 1, roundLeftCorner ? 1 : 0,
		1, roundLeftCorner ? r.height() - 2 : r.height() );
	p->drawRect( r.x() + r.width() - 2, roundRightCorner ? 1 : 0,
		1, roundRightCorner ? r.height() - 2 : r.height() );

	p->drawRect( r.x() + 2, 0, 1, r.height() );
	p->drawRect( r.x() + r.width() - 3, 0,
		1, r.height() );

	QLinearGradient backgroundGradient( QPointF( 0.0, 0.0 ),
		QPointF( 0.0, 1.0 ) );
	backgroundGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
	backgroundGradient.setColorAt( 0.0, QColor( 80, 80, 80 ) );
	backgroundGradient.setColorAt( 0.15, QColor( 215, 215, 220 ) );
	backgroundGradient.setColorAt( 0.5, QColor( 255, 255, 255 ) );
	backgroundGradient.setColorAt( 0.85, QColor( 215, 215, 220 ) );
	backgroundGradient.setColorAt( 1.0, QColor( 80, 80, 80 ) );

	p->setPen( Qt::NoPen );
	p->setBrush( backgroundGradient );
	p->drawRect( r.x() + 3, 0, r.width() - 2 * 3, r.height() );
}


//
// drawSliderHandle
//

void drawSliderHandle( QPainter * p, const QRect & r,
	int xRadius, int yRadius, const QColor & borderColor,
	const QColor & lightColor )
{
	p->setPen( borderColor );
	p->setBrush( lightColor );
	p->drawRoundedRect( r, xRadius, yRadius );

	QLinearGradient g( QPointF( 0.0, 0.0 ), QPointF( 0.0, 1.0 ) );
	g.setCoordinateMode( QGradient::ObjectBoundingMode );
	g.setColorAt( 0.0, darkerColor( lightColor, 75 ) );
	g.setColorAt( 1.0, darkerColor( lightColor, 10 ) );

	p->setPen( Qt::NoPen );
	p->setBrush( g );

	p->drawRoundedRect( r.marginsRemoved( QMargins( 2, 2, 2, 2 ) ),
		xRadius - 4, yRadius - 4 );
}


//
// drawArrow
//

void drawArrow( QPainter * p, const QRect & r,
	const QColor & color )
{
	const qreal width = r.width() / 3;
	const qreal middle = r.height() / 2;

	QPainterPath path;
	path.moveTo( r.x(), r.y() );
	path.lineTo( r.x() + width, r.y() );
	path.lineTo( r.x() + r.width(), r.y() + middle );
	path.lineTo( r.x() + width, r.y() + r.height() );
	path.lineTo( r.x(), r.y() + r.height() );
	path.lineTo( r.x() + r.width() - width, r.y() + middle );
	path.lineTo( r.x(), r.y() );

	p->setPen( color );
	p->setBrush( color );
	p->drawPath( path );
}

} /* namespace QtMWidgets */
