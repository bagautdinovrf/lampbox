#ifndef MAININFORMWIDGET_H
#define MAININFORMWIDGET_H

#include <QWidget>
#include <QTimer>

namespace Ui {
    class MainInformWidget;
}

class MainInformWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainInformWidget(QWidget *parent = 0);
    ~MainInformWidget();

    /**
     * @brief setMessage
     * @param text
     */
    void setMessage( const QString &text );

public slots:
    /**
     * @brief inform
     * @param text
     */
    void inform( const QString &text );

    /**
     * @brief hideInform
     */
    void hideInform();

private:
    Ui::MainInformWidget *ui;

    /// Текст сообщения
    QString                     mTextMessage;

    QTimer                      mHideWidgetTimer;
};

#endif // MAININFORMWIDGET_H
