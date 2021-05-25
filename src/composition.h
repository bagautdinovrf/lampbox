#ifndef COMPOSITION_H
#define COMPOSITION_H
#include <QString>


class Composition
{

public:
    explicit Composition();
    int getFrequency() const;
    void setFrequency(int );
    QString getCompositionName( ) const ;
    void setCompositionName(QString name);

private:
    int mFrequency;
    QString mCompositionName;

    
};

#endif // COMPOSITION_H
