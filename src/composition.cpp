#include "composition.h"

Composition::Composition()

{

}

int Composition::getFrequency() const {
    return mFrequency;
}

void Composition::setFrequency(int f ) {
    mFrequency = f;
}
QString Composition::getCompositionName( ) const {
    return mCompositionName;
}
void Composition::setCompositionName(QString name )  {
    mCompositionName = name;
}
