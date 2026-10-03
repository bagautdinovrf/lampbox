#include "hours.h"
#include <algorithm>
#include <utility>

Hours::Hours(HoursList _hoursList) : mHoursList(std::move(_hoursList)){
    mMaxFrequency=0;
}

QString Hours::GetHoursString(){
    QString hoursText;
    for (const auto &[hour, frequency] : mHoursList) {
        if (frequency != 0) {
            mMaxFrequency = std::max(mMaxFrequency, frequency);
            if (!hoursText.isEmpty())
                hoursText += ',';
            hoursText += hour;
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
