#include "balance.h"
using namespace std;

Balance::Balance(const bool &dbg) : mDbg(dbg){

}
/*Тестовая функция, генерирующая разбалансированный список, в проекте не используется
std::map<QString, unsigned int> Balance::GenerateUnbalance(unsigned int numTracks , unsigned int maxFrequency  ){

    int * ads = new int[ numTracks ];


    iTracksFreq it;
    TracksFreq unbalance;

    for(int i=0; i<TRACKS; i++){
        (ads[i]) = (rand()%maxFrequency + 1 );
        unbalance.insert( pair<QString, unsigned int>(  QString( "A%1" ).arg( i ) , ads[i]  ) );
    }

    #ifdef SHOW_DEBUG_OUTPUT
    for(it = unbalance.begin(); it!=unbalance.end(); it++){
        qDebug()<<"name:"<<(*it).first<<" "<<"value:"<<(*it).second<<endl;
    }
    #endif
    delete []ads;
return unbalance;
}
*/
MsgError  Balance::getErr() const{

    return lErrorMsg;
}

idTracks Balance::MakeBalanced( TracksFreq unbalance,
                              bool &lStatus){

    lStatus = true;
    //Защита от пустого вектора
    idTracks tst;
    if( unbalance.size()==0 ) {
        //то что не прислали данные не наша проблема, просто пусть верхний модуль их пришлет
        lStatus = true;
        lErrorMsg=QObject::tr("balance::MakeBalanced> Нет данных для анализа.\n");
        return tst ;
    }

   TracksFreq::iterator it;

   unsigned int arr5 = 0;
   unsigned int arr4 = 0;
   unsigned int arr2 = 0;
   unsigned int arr3 = 0;
   unsigned int arr1 = 0;

   for(it = unbalance.begin(); it!=unbalance.end(); ++it){
       if ( it->second == 5 )  arr5++;
       if ( it->second == 4 )  arr4++;
       if ( it->second == 3 )  arr3++;
       if ( it->second == 2 )  arr2++;
       if ( it->second == 1 )  arr1++;

   }
   //так как у нас интервал дискретизации 4(столбцы в матрице), то 5 запусков трека поместятся на двух строчках
   const unsigned int ROWS_FOR_5 = 2;
   // у нас ролики x и y запускаются по 5 раз каждый, расположим их по матрице
   // x x x x
   // x 0 0 0
   // y y y y
   // y 0 0 0
   // считывая сверху вниз получим
   // позиции 1 2 3 4 5 6 7 8 9 10 - порядок следования роликов
   //         x x y y x y x y x y что конечно плохо из-за двух роликво идущих в начале
   // xx yy в позициях 1 2 3 4

   const unsigned int row  = ROWS_FOR_5*arr5+arr4+arr3+arr2+arr1;
   //Суммарное количество запусков в течении часа
   const unsigned int numStarts = 5*arr5+4*arr4+3*arr3+2*arr2+arr1;

   QString **M = new QString*[row];
   for(uint i=0; i<row; i++){
         M[i] = new QString[MAX_POS];
   }
   //Заполняем
   int Mrow=0;
   //трек a - 5 раз
   //трек b - 4 раза
   //трек с - 3 раза
   //сперва 4
   //       1 2 3 4
   // line1 a a a a
   // line2 z z z a
   // line3 b b b b
   // line4 c c c z
   // итог собираем строчки сверху вниз
   // a b c a b c a b a a b
   // есть погрешность, но достигнута относительная разреженность!
   // MakeRow(4,unbalance , M, Mrow);
   // qDebug()<<"==============="<<"\n";
   // переберем все треки в поиски частоты 5
   for( it=unbalance.begin(); it!=unbalance.end(); ++it){
        if (it->second==5){
            //первую строчку заполним полностью
            for( int line =1; line<3; line++){
               if (line == 1){
                for(int i=0;i<MAX_POS;i++){
                  (*(M[Mrow]+i))= it->first;
                }
               }
               //вторую строчку заполним лишь в одном месте
               if (line == 2){
                   for(int i=0;i<MAX_POS;i++){
                       if(i == 3){
                           (*(M[Mrow]+i))=it->first;
                        }else{
                           *(M[Mrow]+i)=ZERO;
                        }
                   }
               }
               //увеличиваем количество строк на 1
               Mrow++;
           }

       }

   }

   for(it =unbalance.begin(); it!=unbalance.end(); ++it){
       if ( it->second==4 ){
           for(int i=0; i < MAX_POS; i++){
               QString tmp =it->first;
               (*(M[Mrow]+i)) = tmp;
               //qDebug()<<" "<<(*(M[Mrow]+i));
           }

           Mrow++;
       }

   }
   //3
   //
   int zeroPos =0;
   //qDebug()<<"==============="<<"\n";
   for(TracksFreq::iterator it=unbalance.begin() ;it!=unbalance.end(); ++it){
       if ( it->second==3 ){
           for(int i=0; i<MAX_POS; i++){
               if(i == zeroPos){
                   *(M[Mrow]+zeroPos)=ZERO;
                  // qDebug()<<" "<<(*(M[Mrow]+zeroPos));
               }
               else{
                   (*(M[Mrow]+i))= it->first;
                    //qDebug()<<" "<<(*(M[Mrow]+i));
               }
            }
           (MAX_POS - 1) == zeroPos ? zeroPos = 0: zeroPos++;

           Mrow++;

       }

   }
   //2
   int pos =0;
   qDebug()<<"==============="<<"\n";
   for( TracksFreq::iterator it = unbalance.begin(); it!=unbalance.end(); ++it){
       QString qRow;
       if ( it->second==2){
           for(int i=0;i<MAX_POS;i++){
               if( Mrow%2==0 ){
                   if( i & 1 ) *(M[Mrow]+i)= it->first;
                   else (*(M[Mrow]+i))=ZERO;
                }else{
                    if   ( i & 1 ) (*(M[Mrow]+i))=ZERO;
                    else (*(M[Mrow]+i))= it->first;
                }
                qRow+=(*(M[Mrow]+i));
                qRow+=" ";
           }
           //qDebug()<<"----"<<Mrow<<"-----";
           //qDebug()<<qRow;
           Mrow++;
           }


   }

   //1
   pos =0;
   //qDebug()<<"==============="<<"\n";
   for( it=unbalance.begin(); it!=unbalance.end(); ++it){
       QString qRow;
       if ( it->second==1){
           for(int i=0;i<MAX_POS;i++){
               if(i == pos){
                   (*(M[Mrow]+i))= it->first;
                }else{
                   *(M[Mrow]+i)=ZERO;
                }
               qRow+=(*(M[Mrow]+i));
               qRow+=" ";
           }
           //если поставили последнее значение в последнюю позицию т.е. i==3
           //начинаем с 0
           (MAX_POS-1) == pos ? pos =0: pos++;
          // qDebug()<<"----"<<Mrow<<"-----";
           //qDebug()<<qRow;
           Mrow++;
       }

   }

   //создадим 2 вектора, один резервный
   std::vector<idTrack>     balanced, savebalanced ;
   //Из матрицы создадим вектор с роликами, ролики следуют в
   //порядке их запуска.
   for(int j=0;  j<MAX_POS; j++){
       QString qRow;
       for(int i=0; i<Mrow; i++){

           if( (*(M[i]+j) )!=ZERO){
           balanced.push_back( *(M[i]+j)) ;

           }

        }

   }

   savebalanced = balanced;
   //Если два одинаковых трека стоят рядом, то они меняются местами с третьим, а если например последний и предпоследний
   //то ничего не меняется, нужно будет сделать.
   const int tracks = balanced.size();
   //если у нас 3 запуска
   if(tracks>3){
    try {
        for(unsigned int i=0; i<(tracks-2); i++){
            QString current,next, previous;
            current= balanced.at(i+1);
            previous = balanced.at(i);
            next     = balanced.at(i+2);

            if(previous == current){
                balanced.at(i+1)=next;
                balanced.at(i+2) = current;
            }
        }
    }
    catch(out_of_range& oor)
    {
        //если произошла ошибка, вернем безопасный вектор
        //qDebug((string("exception caught:")+oor.what()).data());
        lErrorMsg=QObject::tr("balance::MakeBalanced> out_of_range\n");
        lStatus = false;
        return savebalanced;
    }
    }
    qDebug()<< "Список треков по порядку\n";;
    int z=0;
    #ifdef SHOW_DEBUG_OUTPUT
    for (vector<QString>::iterator it= balanced.begin();it!=balanced.end(); ++it){
        z++;
        qDebug()<<z<<":"<<(*it);
    }
    #endif
    if(numStarts!=balanced.size()){

        lErrorMsg=QObject::tr("balance::MakeBalanced> Количество запусков не совпадает с размером вектора.\n");
        lStatus = false;
    }
    return balanced;
}
bool Balance::MakeStartMinutes( idTracks tracks, minuteOfTrack &TrackByMinute, preferedMinute BusyMinutes )
{
     QString lErrorMsg;
    int num = tracks.size();
    const int numBusyMinutes = BusyMinutes.size();
    //Превышение количества минут
    if ( (num+numBusyMinutes) > 60){
        lErrorMsg+= QObject::tr("balance::MakeStartMinutes>Превышен лимит запусков. Текущее количество запусков\n");
        qDebug()<<"Balance::MakeStartMinutes limit";
        return false;
    }
    //нет ни одного рекламного ролика
    if (!(num + numBusyMinutes )){
         qDebug()<<"Balance::MakeStartMinutes zero track";
        lErrorMsg+= QObject::tr("balance::MakeStartMinutes> Количество полученных треков равно 0\n");
        return false;
    }
    float step = (float)60/(float)num;
    if ( step < 1) {
          qDebug()<<"Balance::MakeStartMinutes step error";
        lErrorMsg+= QObject::tr("balance::MakeStartMinutes>Шаг запуска меньше минуты\n");
        return false;
    }
    //float s=0;

    //заполняем множество всех минут от 0 до 59
    minutesSet mSet;
    for (int i=0; i<60; i++){
        mSet.insert(i);
    }

    //Четко указанные минуты
    for(ipreferedMinute itP = BusyMinutes.begin(); itP!=BusyMinutes.end(); itP++){
        mSet.erase( itP->first);
        TrackByMinute.insert( pair<int, QString> (static_cast<int>( itP->first), itP->second) );
    }
    int m=0;
    float initS = 0;
    for (itracksNames it= tracks.begin(); it!=tracks.end(); ++it, initS+=step){
             //опорная минута
             m = (int)initS;
             //поведение поиска сама точка
             int behavior=0;

             while(mSet.find(m)== mSet.end()){
                if ( behavior==0 ){
                    //шаг вперед
                    m=initS+1;
                    behavior++;
                    continue;
                }
                if (behavior == 1 ){
                    //два шага вперед
                    m=initS+2;
                    behavior++;
                    continue;
                }
                if (behavior == 2){
                    //шаг назад
                    m=initS-1;
                    behavior++;
                }

                if (behavior == 3){
                    //два шага назад
                    m=initS-2;
                    behavior++;
                    continue;
                }
                //берем любую
                if  ( behavior == 4 ){
                    m = *(mSet.begin());
                    continue;
                }
             }
             TrackByMinute.insert(pair< int, QString>( static_cast<int>(m), *it));
             mSet.erase(m);
             //если минута свободна
    }


    if ( mDbg ) {
        ShowResult(TrackByMinute);
    }

    return true;
}

void Balance::ShowResult(minuteOfTrack trByMinute){
    for( iminuteOfTrack itr = trByMinute.begin(); itr!=trByMinute.end(); ++itr) {
        qDebug()<<"minute :"<< itr->first<<" track_name:"<< itr->second;
    }

}

