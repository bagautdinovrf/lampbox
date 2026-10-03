// QtMWidgets include.
#include "drawing.hpp"
#include "color.hpp"
#include "restyletheme.h"

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
    Q_UNUSED(roundLeftCorner);
    Q_UNUSED(roundRightCorner);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    Restyle::paintSurface(*p, r, QStringLiteral("inset"));
    p->restore();
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
