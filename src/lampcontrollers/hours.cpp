#include "hours.h"

Hours::Hours(HoursList _hoursList) : mHoursList(_hoursList){
    mMaxFrequency=0;
}

QString Hours::GetHoursString(){
    QString hoursText;
    for (HoursList::iterator it= mHoursList.begin();it!=mHoursList.end();++it){
        if ((*it).second!=0){
             if ( (*it).second>mMaxFrequency ) mMaxFrequency=(*it).second;
            (hoursText.isEmpty()) ? hoursText=(*it).first : hoursText+=(QString(",")+(*it).first );
        }
    }
    return hoursText;
}

QString Hours::GetCronStyleString(){
    return QString("");
}

int Hours::GetFrequency() const {

    return mMaxFrequency;
}
