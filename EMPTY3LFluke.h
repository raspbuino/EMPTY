//EMPTY3L
//weiterentwicklung von EMPTY3, nun mit Ladungsberechnung
//Voltage loggern:
//VBA Versorgungsbatterie (war VA0)
//VA1 Testbatterie A1 (C, AA oder AAA)
//VA2 Testbatterie A2 (C, AA oder AAA)
//VA3 Testbatterie A3 (C, AA oder AAA) (neu)
//VCC geregelte Spannung 3.3V, dient auch als Referenzspannung (war VA3)
//Verwendung einer Konstantspannungsquelle
//externer Oszi 8Mhz
//Schreiben auf microSD bei 3.3V
//es werden AnzMessungen Einzelmessungen gemessen, gemittelt und ausgegeben
//wegen 8kHz sind die Millis nur halbe echte Zeit und müssen umgerechnet werden:
//Umrechnung in Stunden ist: t["ms"]/(1000*60*60/2) = .../1800000
//Header: [teins|voltsBT1|voltsBT2|voltsBT3|voltsBTV|voltsVCC|qBT1sum|qBT2sum|qBT3sum] 


//********************   INCLUDE   *********************************************
#include <Wire.h>
#include <SD.h>

//********************   DEFINITION KONSTANTEN   *******************************
const byte pinLED1 = 6; //für VA1 blau
const byte pinLED2 = 7; //für VA2 blau
const byte pinLED3 = 8; //für VA3 blau
const byte pinLED4 = 9; //für VA0 orange
const int DauerBlink = 30; //Blinkdauer
const int DauerPause = 200;  //Blinkpause
const int DauerMess = 5;   //Blinkdauer bei Messung
const int AnzMessungen = 16; //Anzahl der Messungsdurchläufe
const int chipSelect = 10;  //Definition CS-Line für SD-Card
const float herrohm = 33; //Widerstand ist als const angenommen 
const float vinkrement = 3.238769531; //( = 3.33165V:1024), Fluke am ATmega AREF = 3.316-3.317 (MW=3.3165)
File logfile; //das Logfile


//********************   DEFINITION VARIABLEN   ********************************
int MessNr = 1;         //Zähler für die Anzahl der gemessenen Einzelwerte
int indicatorBT1 = 0;	//Blinker für Voltage-Level VA1 ([6])
int indicatorBT2 = 0;	//Blinker für Voltage-Level VA2 ([7])
int indicatorBT3 = 0;	//Blinker für Voltage-Level VA3 ([8])
int indicatorBTV = 0;   //Blinker für Voltage-Level VA0 ([9])

float voltsBT1 = 0;	    //Voltage [mV] an der Testbatterie 1
float voltsBT2 = 0;  	//Voltage [mV] an der Testbatterie 2
float voltsBT3 = 0;  	//Voltage [mV] an der Testbatterie 3
float voltsBTV = 0;	    //Voltage [mV] Versorgungsbatterie vor StepUp-Regler
float voltsVCC = 0;  	//Voltage [mV] geregelte Versorgungsspanung 3.3V

float voltsBT1null = 0;	//Zwischenspeicher um delta U berechnen zu können
float voltsBT2null = 0; //dito
float voltsBT3null = 0; //dito

float voltsBT1sum = 0;  //Summierer VA1-Readings 
float voltsBT2sum = 0;  //Summierer VA2-Readings
float voltsBT3sum = 0;  //Summierer VA3-Readings
float voltsBTVsum = 0;  //Summierer VBA-Readings
float voltsVCCsum = 0;  //Summierer VCC-Readings
float tnull = 0;        //zur Berechnung von delta t, Initial ist tnull = 0
float teins = 0;	    //zur Umrechnung in Dezimalstunden der millis-Zeit etc
float deltat = 0;       //Zeitdifferenz zwischen zwei (gemittelten) Messungen
float qBT1sum = 0;      //entnommene Ladung in [mAh] Batterie 1 aufsummiert
float qBT2sum = 0;      //entnommene Ladung in [mAh] Batterie 2 aufsummiert
float qBT3sum = 0;      //entnommene Ladung in [mAh] Batterie 3 aufsummiert

//********************   SUBROUTINEN   *****************************************
void error(char *str)
{
	//Serial.print("error: ");
	//Serial.println(str);
	digitalWrite(pinLED4, HIGH); //red LED indicates error
	while(1);
}

//********************   VOID SETUP   ******************************************
void setup()
{
	
//interne Referenzspannung für Analoge Kanäle (VCC, Bat)	
	analogReference(EXTERNAL); //etwas 3.3V am Ausgang vom Reglerboard
	
//AUSGANGSPINS DEFINIEREN
	pinMode(pinLED1, OUTPUT);
	pinMode(pinLED2, OUTPUT);
	pinMode(pinLED3, OUTPUT);
	pinMode(pinLED4, OUTPUT);
	
//SD-CARD INITIalisieren
	pinMode(10, OUTPUT); //SPI Chip Select als Ausgang setzen
  
//CARD VORHANDEN? DANN INITIALISIEREN
	if (SD.begin(chipSelect)) //alles gut...also initialisieren
	{	//orange LED blinken lassen
		for (byte i=0; i< 5; i++)
		{
			digitalWrite(pinLED4, HIGH);
			delay(DauerMess);
			digitalWrite(pinLED4, LOW);
			delay(DauerPause);
		}
	}	
	else //Errorblinken
	{
		for (byte i=0; i<10; i++)
		{
			digitalWrite(pinLED4, HIGH);
			delay(DauerMess);
			digitalWrite(pinLED4, LOW);
			delay(DauerMess*10);
		}
		return;
	}

//CREATE NEW FILE, MAXIMAL 8+3 ZEICHEN...wie zu DOS-Zeiten
	char filename[] = "EMPTY-00.csv";
	for (uint8_t i = 0; i < 100; i++)
	{
		filename[6] = i/10 + '0';
		filename[7] = i%10 + '0';
		if (!SD.exists(filename))
		{
			//only open a new file if it doesn't exist
			logfile = SD.open(filename, FILE_WRITE); 
			break;  //leave the loop!
		}
	}	

//HEADER IN DATEI SCHREIBEN, [teins|voltsBT1|voltsBT2|voltsBT3|voltsBTV|voltsVCC|qBT1sum|qBT2sum|qBT3sum]
	if (logfile)
	{
		logfile.println("EMPTY3LFluke-33Ohm.1%-3.3165V");  //schreiben
		logfile.print("Anzahl Messloops pro Datensatz: ");
		logfile.println(AnzMessungen);
		logfile.println("teins[ms],t[h],BT1[mV],BT2[mV],BT3[mV],BTV[mV],VCC[mV],qBT1[mAh],qBT2[mAh],qBT3[mAh]");
		delay(200);	
	}
	else //Errorblinken
	{	
		for (byte i=0; i<5; i++)
		{
			digitalWrite(pinLED4, HIGH);
			delay(DauerMess);
			digitalWrite(pinLED4, LOW);
			delay(DauerMess*10);
		}
		return;
	}
	delay(1000);
}

//********************   VOID LOOP   *******************************************
void loop ()
{
//MESSUNG und ggf WATCHDOG
	MessNr = 1; //Zurücksetzen des Schleifenzählers für die Mittelwertbildung
	
	//MESSLOOP START ***********************************************************
	while (MessNr <= AnzMessungen)
	{
	//Messen	
		voltsBT1 = analogRead(0) * vinkrement; // ( = 3.3165V:1024), Fluke am ATmega AREF = 3.316-3.317 (MW=3.3165)
		voltsBT2 = analogRead(1) * vinkrement; //dito (alter Wert war 3.3V:1024=3.22265625)
		voltsBT3 = analogRead(2) * vinkrement; //dito
		voltsBTV = analogRead(3) * vinkrement; //dito
		voltsVCC = analogRead(4) * vinkrement; //dito
	//Summieren über alle einzelnen Messloops
		voltsBT1sum = voltsBT1sum + voltsBT1;
		voltsBT2sum = voltsBT2sum + voltsBT2;
	    voltsBT3sum = voltsBT3sum + voltsBT3;
		voltsBTVsum = voltsBTVsum + voltsBTV;
		voltsVCCsum = voltsVCCsum + voltsVCC;
		
	//LED Messblinkung
		digitalWrite(pinLED4, HIGH);
		delay(DauerMess);
		digitalWrite(pinLED4, LOW);
		
	//Schleifenzähler hochsetzen	
		MessNr++; // = MessNr +1;
	//warten bis nächste Messung
		delay(1000); //hinzugefügt statt Watchdogzeit
	} //************************************************************************
		

//Ausgabewerte Spannungen mitteln
	voltsBT1 = voltsBT1sum/AnzMessungen;
	voltsBT2 = voltsBT2sum/AnzMessungen;
	voltsBT3 = voltsBT3sum/AnzMessungen;
	voltsBTV = voltsBTVsum/AnzMessungen;
	voltsVCC = voltsVCCsum/AnzMessungen;
	
//Berechnung Delta t etc
    //teins = float(millis())/1800000;  //Umrechnung in Dezimalstunden bei 8kHz
    teins = millis()*2;               //Faktor x2 weil nur 8kHz-Kristall (statt 16)
	deltat = teins - tnull;           //Berechnung Delta t
    tnull = teins;                    //tnull bekommt den neuen tnull-Wert
	
	
	
//Ladung deltaq = 1/(2*R)*((t1-t0)/(U1+U0)) 
//Berechnung Ladung....	
	float nenner = 2 * herrohm;
	float deltaq1 = (deltat * (voltsBT1 + voltsBT1null))/nenner/3600000; //von ms*mA auf mAh
	float deltaq2 = (deltat * (voltsBT2 + voltsBT2null))/nenner/3600000;
	float deltaq3 = (deltat * (voltsBT3 + voltsBT3null))/nenner/3600000;
		
	qBT1sum = qBT1sum + deltaq1; //von ms*mA auf mAh
	qBT2sum = qBT2sum + deltaq2;
	qBT3sum = qBT3sum + deltaq3;

//Zuweisung der neuen alten Werte	
	voltsBT1null = voltsBT1;
	voltsBT2null = voltsBT2;
	voltsBT3null = voltsBT3;

//	
//Schreiben auf SD [teins[ms]|teins|voltsBT1|voltsBT2|voltsBT3|voltsBTV|voltsVCC|qBT1sum|qBT2sum|qBT3sum]
//Einheiten:       [  h  |                     mV                     |         mAh           ]
	logfile.print(teins);  //millis()*2 bei 8kHz-Kristall
	logfile.print(",");
	logfile.print(float(teins)/3600000,6);  //h (Dezimalstunden) seit Start
	logfile.print(",");
	logfile.print(voltsBT1,6); //VoltsBT1 in [mV], zwei Dezimalstellen
	logfile.print(",");
	logfile.print(voltsBT2,6); //dito BT2
	logfile.print(",");
	logfile.print(voltsBT3,6); //dito BT3
	logfile.print(",");
	logfile.print(voltsBTV,2); //dito BTV
	logfile.print(",");
	logfile.print(voltsVCC,2); //dito VCC
	logfile.print(",");
	logfile.print(qBT1sum,10);  //LadungBT1 in [mAh], zwei Dezimalstellen
	logfile.print(",");
	logfile.print(qBT2sum,10);  //dito BT2
	logfile.print(",");
	logfile.print(qBT3sum,10);  //dito BT3
	logfile.println();
	
	delay(200); //dem Schreibenvorgang noch etwas Zeit geben
	
	logfile.flush(); //uuund... schwupps rüberschieben
	delay(200);
	
//LED012 Ende Schreiben
	for (byte i=0; i<3; i++)
	{
		digitalWrite(pinLED1, HIGH);
		digitalWrite(pinLED2, HIGH);
		digitalWrite(pinLED3, HIGH);
		digitalWrite(pinLED4, HIGH);
		delay(20);
		digitalWrite(pinLED1, LOW);
		digitalWrite(pinLED2, LOW);
		digitalWrite(pinLED3, LOW);
		digitalWrite(pinLED4, LOW);
		delay(40);	
	}
		

//in welchem Bereich liegt voltsBT1 Testbatterie 1
	if (voltsBT1 <= 10){indicatorBT1 = 7;}
	else if (voltsBT1 <= 50 && voltsBT1 > 10){indicatorBT1 = 6;}
	else if (voltsBT1 <= 250 && voltsBT1 > 50){indicatorBT1 = 5;}
	else if (voltsBT1 <= 700 && voltsBT1 > 250){indicatorBT1 = 4;}
	else if (voltsBT1 <= 1100 && voltsBT1 > 700){indicatorBT1 = 3;}
	else if (voltsBT1 <= 1200 && voltsBT1 > 1100){indicatorBT1 = 2;}
	else {indicatorBT1 = 1;}
	
//in welchem Bereich liegt voltsBT2 Testbatterie 2
	if (voltsBT2 <= 10){indicatorBT2 = 7;}
	else if (voltsBT2 <= 50 && voltsBT2 > 10){indicatorBT2 = 6;}
	else if (voltsBT2 <= 250 && voltsBT2 > 50){indicatorBT2 = 5;}
	else if (voltsBT2 <= 700 && voltsBT2 > 250){indicatorBT2 = 4;}
	else if (voltsBT2 <= 1100 && voltsBT2 > 700){indicatorBT2 = 3;}
	else if (voltsBT2 <= 1200 && voltsBT2 > 1100){indicatorBT2 = 2;}
	else {indicatorBT2 = 1;}
	
//in welchem Bereich liegt voltsBT3 Testbatterie 3
	if (voltsBT3 <= 10){indicatorBT3 = 7;}
	else if (voltsBT3 <= 50 && voltsBT3 > 10){indicatorBT3 = 6;}
	else if (voltsBT3 <= 250 && voltsBT3 > 50){indicatorBT3 = 5;}
	else if (voltsBT3 <= 700 && voltsBT3 > 250){indicatorBT3 = 4;}
	else if (voltsBT3 <= 1100 && voltsBT3 > 700){indicatorBT3 = 3;}
	else if (voltsBT3 <= 1200 && voltsBT3 > 1100){indicatorBT3 = 2;}
	else {indicatorBT3 = 1;}	

//in welchem Bereich liegt voltsBTV Versorgungsbatterie 2xAA
	if (voltsBTV <= 1200){indicatorBTV = 20;}
	else if (voltsBTV <= 1300 && voltsBTV > 1200){indicatorBTV = 19;}
	else if (voltsBTV <= 1400 && voltsBTV > 1300){indicatorBTV = 18;}
	else if (voltsBTV <= 1500 && voltsBTV > 1400){indicatorBTV = 17;}
	else if (voltsBTV <= 1600 && voltsBTV > 1500){indicatorBTV = 16;}
	else if (voltsBTV <= 1700 && voltsBTV > 1600){indicatorBTV = 15;}
	else if (voltsBTV <= 1800 && voltsBTV > 1700){indicatorBTV = 14;}
	else if (voltsBTV <= 1900 && voltsBTV > 1800){indicatorBTV = 13;}
	else if (voltsBTV <= 2000 && voltsBTV > 1900){indicatorBTV = 12;}
	else if (voltsBTV <= 2100 && voltsBTV > 2000){indicatorBTV = 11;}
	else if (voltsBTV <= 2200 && voltsBTV > 2100){indicatorBTV = 10;}
	else if (voltsBTV <= 2300 && voltsBTV > 2200){indicatorBTV = 9;} 
	else if (voltsBTV <= 2400 && voltsBTV > 2300){indicatorBTV = 8;}	
	else if (voltsBTV <= 2500 && voltsBTV > 2400){indicatorBTV = 7;}
	else if (voltsBTV <= 2600 && voltsBTV > 2500){indicatorBTV = 6;}
	else if (voltsBTV <= 2700 && voltsBTV > 2600){indicatorBTV = 5;}
	else if (voltsBTV <= 2800 && voltsBTV > 2700){indicatorBTV = 4;}
	else if (voltsBTV <= 2900 && voltsBTV > 2800){indicatorBTV = 3;}
	else if (voltsBTV <= 3000 && voltsBTV > 2900){indicatorBTV = 2;}
	else {indicatorBTV = 1;}	

//LED1234 Start Blinken
	
	digitalWrite(pinLED1, HIGH);
	digitalWrite(pinLED2, HIGH);
	digitalWrite(pinLED3, HIGH);
	digitalWrite(pinLED4, HIGH);
	delay(20);
	digitalWrite(pinLED1, LOW);
	digitalWrite(pinLED2, LOW);
	digitalWrite(pinLED3, LOW);
	digitalWrite(pinLED4, LOW);
		
	delay(100);
//LED4 Sequenz
	for (byte i=0; i<indicatorBTV; i++)
	{
		digitalWrite(pinLED4, HIGH);
		delay(DauerBlink);
		digitalWrite(pinLED4, LOW);
		delay(DauerPause);
	}	
    delay(100);
	
		
//LED1 Sequenz
	for (byte i=0; i<indicatorBT1; i++)
	{
		digitalWrite(pinLED1, HIGH);
		delay(DauerBlink);
		digitalWrite(pinLED1, LOW);
		delay(DauerPause);
	}
	delay(100);
	
//LED2 Sequenz
	for (byte i=0; i<indicatorBT2; i++)
	{
		digitalWrite(pinLED2, HIGH);
		delay(DauerBlink);
		digitalWrite(pinLED2, LOW);
		delay(DauerPause);
	}
	delay(100);

//LED3 Sequenz
	for (byte i=0; i<indicatorBT3; i++)
	{
		digitalWrite(pinLED3, HIGH);
		delay(DauerBlink);
		digitalWrite(pinLED3, LOW);
		delay(DauerPause);
	}
	delay(250);

//LED1234 Ende Blinken
	for (byte i=0; i<2; i++)
	{
		digitalWrite(pinLED1, HIGH);
		digitalWrite(pinLED2, HIGH);
		digitalWrite(pinLED3, HIGH);
		digitalWrite(pinLED4, HIGH);
		delay(20);
		digitalWrite(pinLED1, LOW);
		digitalWrite(pinLED2, LOW);
		digitalWrite(pinLED3, LOW);
		digitalWrite(pinLED4, LOW);
		delay(40);	
	}

//Messsummen zurücksetzen auf Null da für nächste MW-Bildung benötigt 	
	voltsBT1sum = 0;
	voltsBT2sum = 0;
	voltsBT3sum = 0;
	voltsBTVsum = 0;
	voltsVCCsum = 0;
	delay(250);
}