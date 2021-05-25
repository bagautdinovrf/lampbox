#ifndef QTMWIDGETS__SWITCH_HPP__INCLUDED
#define QTMWIDGETS__SWITCH_HPP__INCLUDED

// Qt include.
#include <QWidget>
#include <QScopedPointer>


namespace lampproject {

class SwitchPrivate;


class Switch
	:	public QWidget
{
	Q_OBJECT

	Q_ENUMS( State )

	Q_PROPERTY( State state READ state WRITE setState NOTIFY stateChanged USER true )

	Q_PROPERTY( bool checked READ isChecked NOTIFY toggled )

	Q_PROPERTY( QString onText READ onText WRITE setOnText )

	Q_PROPERTY( QString offText READ offText WRITE setOffText )

	Q_PROPERTY( QColor onColor READ onColor WRITE setOnColor )

signals:

	void toggled( bool checked );

	void stateChanged( int state );

public:
	/*!
		State of the button.
	*/
	enum State {
		//! Button is in not accepted unchecked state.
		NotAcceptedUncheck = 0,
		//! Button is in accepted unchecked state.
		AcceptedUncheck = 1,
		//! Button is in not accepted checked state.
		NotAcceptedCheck = 2,
		//! Button is in accepted checked state.
		AcceptedCheck = 3
	}; // enum State

public:
	Switch( QWidget * parent = 0 );
	explicit Switch( State st, QWidget * parent = 0 );

	virtual ~Switch();

	//! \return State of the button.
	State state() const;

	//! \return Is button checked?
	bool isChecked() const;

	//! \return Text that displays in checked state.
	QString onText() const;
	//! Set text that will be displayed in checked state.
	void setOnText( const QString & text );

	//! \return Text that displays in unchecked state.
	QString offText() const;
	//! Set text that will be displayed in unchecked state.
	void setOffText( const QString & text );

	//! \return Color to highlight checked state.
	QColor onColor() const;
	//! Set color to highlight checked state.
	void setOnColor( const QColor & c );

	virtual QSize sizeHint() const;

public slots:
	/*!
		Set state of the button.
	*/
	void setState( State st );

protected:
	virtual void paintEvent( QPaintEvent * event );
	virtual void mousePressEvent( QMouseEvent * event );
	virtual void mouseReleaseEvent( QMouseEvent * event );
	virtual void mouseMoveEvent( QMouseEvent * event );
    virtual void mouseDoubleClickEvent(QMouseEvent * event);
private:
	friend class SwitchPrivate;

	Q_DISABLE_COPY( Switch )

	QScopedPointer< SwitchPrivate > d;
}; // class Switch

} /* namespace QtMWidgets */

#endif // QTMWIDGETS__SWITCH_HPP__INCLUDED
