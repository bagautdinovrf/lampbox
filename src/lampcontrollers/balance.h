/**
    @file    Содержит в себе набор функций для равномерного распределения треков
    @author  Климов А.В.
*/
#ifndef BALANCE_H
#define BALANCE_H
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <QString>
#include <QDebug>

//#include <QLMObjects>
//#include <QLMWidgets>

#include <QString>
#include <QObject>

#define TRACKS 4
#define MAX_POS       4
#define ZERO    QString("Z")

using namespace std;
#include <stdexcept>
typedef QString idTrack;

//! @var сырой список имя трека - частота
typedef map<QString, unsigned int> TracksFreq;
//! @var список для хранения вида [МИНУТА ЗАПУСКА:ТРЭК]
typedef map<unsigned int, QString>  minuteOfTrack;
typedef minuteOfTrack::iterator iminuteOfTrack;
//! @var список для хранения вида [МИНУТА ЗАПУСКА:ТРЭК]
typedef map<unsigned int, QString>  preferedMinute;

typedef preferedMinute::iterator ipreferedMinute;
typedef set<unsigned int>     minutesSet;
typedef vector<QString>   tracksNames;
typedef tracksNames::iterator itracksNames;
typedef QString MsgError;
typedef vector<idTrack> idTracks;
typedef TracksFreq::iterator iTracksFreq;

class Balance  {

    public:
        // @var - если включена отладка все пишем в лог
        Balance (const bool &dbg);
        //! @fn GenerateUnbalance - ф-я для генерации тестового списка
        //map<idTrack, unsigned int> GenerateUnbalance(unsigned int numTracks , unsigned int maxFrequency  );

        //! Класс состоит из двух функций
        //! 1. Выстраиваем треки в порядке очередности запуска 1,2,3,4 и т.д.
        //! 2. Затем присваиваем каждому из них определенную минуту

        //! @fn MakeBalanced - создает список треков стоящих друг за другом
        //! @var unbalance - <название ролика:частота запуска>
        //! @var lStatus - результат построения сбалансированного списка
        //! @ErrorStr - сообщение о ошибке
        //! @ret vector<idTrack> - список треков следующих друг за другом
        vector<QString> MakeBalanced( TracksFreq unbalance,
                                      bool &lStatus);

        //! @fn MakeStartMinutes - присваивает каждому треку в списке его минуты запуска
        //! @var balance - список треков в порядке из запуска(минуты)
        //! @var minuteOfTrack - список вида [МИНУТА ЗАПУСКА:ТРЭК]
        //! @ret результат выполнения
        bool MakeStartMinutes(vector<QString> balance,
                              minuteOfTrack &,
                              preferedMinute
                          );
        void ShowResult(minuteOfTrack tr);
        MsgError getErr() const;
        //! @var - отладка да/нет
        bool mDbg;
private:
        MsgError lErrorMsg;
};
#endif // BALANCE_H
