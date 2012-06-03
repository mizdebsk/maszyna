//---------------------------------------------------------------------------

#include    "system.hpp"
#include    "classes.hpp"
#pragma hdrstop

#include "Timer.h"
#include "PoliSound.h"
#include "AdvSound.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

// yB: sa skroty AA/AM/FA/FM, ktore znacza
// A - Attenuating - narastanie
// F - Falling - opadanie
// A - Attenuating - narastanie
// M - Maximum - maksimum
// co odpowiada kolejnosci (zaleznosci)
// AA < AM < FM < FA
// nie znam angielskiego, ale mi sie dobrze kojarzy :) Wykorzystuje stare zmienne
// do nowych celow, bo nie widze potrzeby przekopywania/kopiowania klasy.

__fastcall TPoliSound::TPoliSound()
{
//    SoundStart=SoundCommencing=SoundShut= NULL;
 State=ss_Off;
 fTime=0;
 fStartLength=0;
 fShutLength=0;
}

__fastcall TPoliSound::~TPoliSound()
{//Ra: stopowanie si� sypie
 //SoundStart.Stop();
 //SoundCommencing.Stop();
 //SoundShut.Stop();
}

void __fastcall TPoliSound::Free()
{
}

void __fastcall TPoliSound::Load(TQueryParserComp *Parser, vector3 pPosition)
{
//ladowanie dzwieku startu
 AnsiString NameOn= Parser->GetNextSymbol().LowerCase();
 double DistanceAttenuation= Parser->GetNextSymbol().ToDouble();
 SoundStart.Init(NameOn.c_str(),DistanceAttenuation,pPosition.x,pPosition.y,pPosition.z,true);
 fStartLength=SoundStart.GetWaveTime();
 SoundStart.AM=1.0;
 SoundStart.AA=0.0;
 SoundStart.FM=1.0;
 SoundStart.FA=0.0;
//ladowanie
 max_i= Parser->GetNextSymbol().ToInt();
 for(int i=0;i<max_i;i++)
 {
  AnsiString Name= Parser->GetNextSymbol().LowerCase();
  SoundCommencing[i].Init(Name.c_str(),DistanceAttenuation,pPosition.x,pPosition.y,pPosition.z,true);
  freqmod[i]= Parser->GetNextSymbol().ToDouble();
 }
 //dla pierwszego inaczej poczatek
 SoundCommencing[0].AA= -freqmod[0]; //pelne wylaczenie w 1/3
 SoundCommencing[0].AM= 1; //pelne wlaczenie w 2/3
 for(int i=1;i<max_i;i++)
 {
  SoundCommencing[i].AA= 0.1f*(9*freqmod[i-1]+freqmod[i]); //pelne wylaczenie w 1/10 (dalej)
  SoundCommencing[i].AM= 0.1f*(9*freqmod[i]+freqmod[i-1]); //pelne wlaczenie w 9/10 (blizej)
  SoundCommencing[i-1].FA= SoundCommencing[i].AM;              //wlaczenie przy wylaczeniu
  SoundCommencing[i-1].FM= SoundCommencing[i].AA;              //wylaczenie przy wlaczeniu
 }
 //dla ostatniego inaczej koniec
 SoundCommencing[max_i-1].FM= 2*freqmod[max_i-1]; //pelne wylaczenie w 1/3
 SoundCommencing[max_i-1].FA= 3*freqmod[max_i-1]; //pelne wlaczenie w 2/3

 AnsiString NameOff= Parser->GetNextSymbol().LowerCase();
 SoundShut.Init(NameOff.c_str(),DistanceAttenuation,pPosition.x,pPosition.y,pPosition.z,true);
 fShutLength=SoundShut.GetWaveTime();
 SoundShut.AM=1.0;
 SoundShut.AA=0.0;
 SoundShut.FM=1.0;
 SoundShut.FA=0.0;
}

void __fastcall TPoliSound::TurnOn(bool ListenerInside, vector3 NewPosition)
{
    //hunter-311211: nie trzeba czekac na ponowne odtworzenie dzwieku, az sie wylaczy
    if ((State==ss_Off || State==ss_ShuttingDown) && (SoundStart.AM>0))
    {
        SoundStart.ResetPosition();
        for (int i=0; i<max_i; i++) SoundCommencing[i].ResetPosition();
        SoundShut.Stop();
        SoundStart.Play(1,0,ListenerInside,NewPosition);
//        SoundStart->SetVolume(-10000);
        State= ss_Starting;
        fTime= 0;
    }
}

void __fastcall TPoliSound::TurnOff(bool ListenerInside, vector3 NewPosition)
{
    if ((State==ss_Commencing || State==ss_Starting)  && (SoundShut.AM>0))
    {
        SoundStart.Stop();
        SoundShut.ResetPosition();
        SoundShut.Play(1,0,ListenerInside,NewPosition);
        for (int i=0; i<max_i; i++) SoundCommencing[i].Stop();        
        State= ss_ShuttingDown;
        fTime= fShutLength;
//        SoundShut->SetVolume(0);
    }
}

void __fastcall TPoliSound::UpdateAF(double A, double F, bool ListenerInside, vector3 NewPosition)
{   //update, ale z amplituda i czestotliwoscia
    if (State==ss_Starting)
    {
        fTime+= Timer::GetDeltaTime();
//        SoundStart->SetVolume(-1000*(4-fTime)/4);
        if (fTime>=fStartLength)
        {
            State= ss_Commencing;
            SoundStart.Stop();
            for(int i=0; i<max_i; i++) SoundCommencing[i].ResetPosition();

        }
        else
           SoundStart.Play(A,0,ListenerInside,NewPosition);
    }
    else
    if (State==ss_ShuttingDown)
    {
        fTime-= Timer::GetDeltaTime();
//        SoundShut->SetVolume(-1000*(4-fTime)/4);
        if (fTime<=0)
        {
            State= ss_Off;
            SoundShut.Stop();
        }
        else
         SoundShut.Play(A,0,ListenerInside,NewPosition);
    }
    if ((State==ss_Commencing))
    {
        for (int i=0; i<max_i; i++)
         {
          float vol;
          if(F<SoundCommencing[i].AM)
            vol=(F-SoundCommencing[i].AA)/(SoundCommencing[i].AM-SoundCommencing[i].AA);
          else if (F>SoundCommencing[i].FM)
            vol=(F-SoundCommencing[i].FA)/(SoundCommencing[i].FM-SoundCommencing[i].FA);
          else
            vol=1;
          if(vol>0)
           {
            vol= 1+0.2*log10(vol);
            SoundCommencing[i].AdjFreq(F/freqmod[i],Timer::GetDeltaTime());
            SoundCommencing[i].Play(A*vol,DSBPLAY_LOOPING,ListenerInside,NewPosition);
           }
          else
            SoundCommencing[i].Stop();
         }
    }
}

void __fastcall TPoliSound::CopyIfEmpty(TPoliSound &s)
{//skopiowanie, gdyby by� potrzebny, a nie zosta� wczytany
 if ((fStartLength>0.0)||(fShutLength>0.0)) return; //co� jest
 SoundStart=s.SoundStart;
 for (int i=0; i<max_i; i++)
   SoundCommencing[i]=s.SoundCommencing[i];
 SoundShut=s.SoundShut;
 State=s.State;
 fStartLength=s.fStartLength;
 fShutLength=s.fShutLength;
 defAM=s.defAM;
 defFM=s.defFM;
};

