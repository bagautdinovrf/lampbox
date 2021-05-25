
#ifndef QTMWIDGETS__COLOR_HPP__INCLUDED
#define QTMWIDGETS__COLOR_HPP__INCLUDED

// Qt include.
#include <QColor>


namespace lampproject {

//
// lighterColor
//

//! \return Lighter color with HSV value bias \a b.
QColor lighterColor( const QColor & c, int b );


//
// darkerColor
//

//! \return Darker color with HSV value bias \a b.
QColor darkerColor( const QColor & c, int b );

} /* namespace QtMWidgets */

#endif // QTMWIDGETS__COLOR_HPP__INCLUDED
