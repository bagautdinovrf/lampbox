#ifndef TYPES_H
#define TYPES_H
namespace lampproject{
    //дни недели по крону
    enum DayOfWeek{
        Sunday = 0,
        Monday = 1,
        Tuesday = 2,
        Wednesday = 3,
        Thursday = 4,
        Friday = 5,
        Saturday = 6
    };
    enum Month{
        January = 1,
        February = 2,
        March = 3,
        April = 4,
        May = 5,
        June = 6,
        July = 7,
        August = 8,
        September = 9,
        October = 10,
        November = 11,
        December = 12
    };
    enum CronTracks {
        CT_ID = 0,
        CT_TRACK_NAME,        //1
        CT_FREQUENCY ,      //3
        CT_HOURS ,          //4
        CT_DAY_OF_WEEK ,    //5
        CT_START_DATE ,     //6
        CT_FINISHED_DATE ,  //7
        CT_MB_ID ,          //8
        CT_CREATION_TIME ,  //9
        CT_STATUS ,         //10
        CT_VOLUME,          //11
        CT_LOAD_STATUS,       //12
        CT_TRACK_ID
    };
}
#endif // TYPES_H
