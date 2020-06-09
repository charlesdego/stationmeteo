/*
  Station Meteo Pro
  avec : 
     - Arduino MKR1200-SIGFOX
     - Anémomètre Lextronic   LEXCA003
     - Girouette Lextronic    LEXCA002
     - Pluviomètre Lextronic  LEXCA001
     - DHT22

 *  Fichiers d'entête des librairies
 */

#include "DHT.h"   // Librairie des capteurs DHT
#include <SigFox.h>

// Temperature et Humidité
#define DHTPIN 5   // Changer le pin sur lequel est branché le DHT
#define DHTTYPE DHT22       // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE); 

/* 
 *  Variables statiques
 */
#define GIROUETTE   A1  //port analogique A1
#define ANEMOMETRE  1   //pin D3, interruption n°1
#define PLUVIOMETRE 0   //pin D2, interruption n°0
#define VALEUR_PLUVIOMETRE 0.2794 //valeur en mm d'eau à chaque bascule d'auget
#define PI        3.1415
#define RAYON     0.08  //rayon en mètre de l'anémomètre en mètre à vérifier


/* 
 *  Variables globales
 */
unsigned long previousMillis=   0;
unsigned long previousMillis2=  0;
unsigned long delaiAnemometre = 3000L;    //3 secondes
unsigned long delaiProgramme =  900000L;   //15min
float gust(0);        //vent max cumulé sur 1 min
float wind(0);        //vent moyen cumulé sur 1 min
int nbAnemo = 0;      //nb d'occurence de mesure Anemo
float gir(0);         //direction moyenne de la girouette sur 1 min (en degrés)
int nbGir = 0;        //nb d'occurence de mesure Anemo
float pluvio1min(0);  //pluie sur 1 min
float vitesseVent(0); //vent moyen cumulé sur 1 min
float temp(0);        //température moyenne sur 1 min
float hum(0);         //humidité moyenne sur 1 min
int nbBME280 = 0;     //nb d'occurence d'appel du capteur BME280
volatile unsigned int countAnemometre = 0;  //variable pour l'interruption de l'anémomètre pour compter les impulsions
volatile unsigned int countPluviometre = 0; //variable pour l'interruption du pluviomètre pour compter les impulsions
boolean debug = true;  //TRUE = ecriture du programme, active tous les Serial.Println ; FALSE = aucun println affichés

// sigfox
typedef struct __attribute__ ((packed)) sigfox_message {
float t15;
int8_t h15;
int8_t p15;
int8_t v15;
int8_t r15;
int16_t g15;


} SigfoxMessage;

// stub for message which will be sent
SigfoxMessage msg;

// =================== UTILITIES ===================
void reboot() {
NVIC_SystemReset();
while (1);
}


void setup()
{
  
  delay(2000);   //initialisation de la carte ethernet 
  Serial.begin(9600);
  Serial.println("démarrage ...");
  Serial.println("DHTxx test!");

 dht.begin();

  pinMode(PLUVIOMETRE, INPUT_PULLUP);
  pinMode(ANEMOMETRE, INPUT_PULLUP);
  
 attachInterrupt(PLUVIOMETRE,interruptPluviometre,RISING) ;
 attachInterrupt(ANEMOMETRE,interruptAnemometre,RISING) ;


}
void interruptAnemometre(){
  countAnemometre++;
}

/* 
 *  Fonction d'interruption du pluviomètre qui incrémente un compteur à chaque impulsion
 */
void interruptPluviometre(){
  countPluviometre++;
}


/* 
 *  Fonction qui converti en angle la valeur analogique renvoyée par l'arduino (valeurs fixes)
 *  RETURN : angle en degré
 */
float getGirouetteAngle(int value){
  float angle = 0;
   if (value > 940 && value < 949) angle = 0;
  if (value > 791 && value < 795) angle = 45;
  if (value > 845 && value < 850) angle = 90;
  if (value > 750 && value < 754) angle = 135;
  if (value > 885 && value < 892) angle = 180;
  if (value > 815 && value < 822) angle = 225;
  if (value > 925 && value < 938) angle = 270;
  if (value > 865 && value < 869) angle = 315;
  return angle;
}

/* 
 *  Programme principal
 */
void loop(){  
  
  unsigned long currentMillis = millis(); // read time passed

  //Récupération des infos de l'anémomètre et girouette toutes les 3 sec
  //Enregistrement cumulé des valeurs
  if (currentMillis - previousMillis > delaiAnemometre){
    previousMillis = millis();
    vitesseVent = (PI * RAYON * 2 * countAnemometre)/3*3.6; //3 = durée de prise de mesure (sec)
    
    if(vitesseVent>gust) gust = vitesseVent;
    wind += vitesseVent;
    nbAnemo++;
    countAnemometre = 0;

    int gdir = analogRead(GIROUETTE);
    gir += getGirouetteAngle(gdir);
    nbGir++;

 
delay(2000);
float temperature = dht.readTemperature();
temp += temperature;

float humidity = dht.readHumidity();
hum += humidity;
  
nbBME280++;

 

    if(debug == true){
      Serial.println("------------------------");
      Serial.print("#");
      Serial.println(nbGir);
      Serial.println("------------------------");
      Serial.print("Temp:");
      Serial.println(temperature);
       Serial.print("Humidit:");
      Serial.println(humidity);
      Serial.print("Vent:");
      Serial.println(vitesseVent);
      Serial.print("Pluvio:");
      Serial.println(countPluviometre);
      Serial.print("Girouette:");
      Serial.println(getGirouetteAngle(gdir));
      Serial.println("------------------------");
    }    
  }

  //Toutes les minutes, compilation des valeurs et envoi au serveur  
  if (currentMillis - previousMillis2 > delaiProgramme){
    previousMillis2 = millis();
    float avgwind = wind / nbAnemo;
    int8_t v15 = avgwind;
    float avggir = gir / nbGir;
    int16_t g15 = avggir;

float r15 = gust;

float avgtemp = temp / nbBME280;
float t15 = avgtemp;

   
float avghum = hum / nbBME280; //avghum = 0;
int8_t h15 = avghum;


pluvio1min = countPluviometre*VALEUR_PLUVIOMETRE;
countPluviometre = 0;
int8_t p15=pluvio1min;

if (!SigFox.begin()) {
Serial.println("SigFox error, rebooting");
reboot();
}
  SigFox.debug();
msg.t15 = (float) (t15);
msg.h15 = (int8_t) (h15);
msg.p15 = (int8_t) (p15);
msg.v15 = (int8_t) (v15);
msg.r15 = (int8_t) (r15);
msg.g15 = (int16_t) (g15);
 
Serial.println("envoi a sigfox");

// Clears all pending interrupts
SigFox.status();
delay(1);

// Send the data
SigFox.beginPacket();
SigFox.write((uint8_t*)&msg, sizeof(SigfoxMessage));
// SigFox.write((uint8_t*)&poids);
Serial.print("Status: ");
Serial.println(SigFox.endPacket());

SigFox.end();

   if(debug == true){
      Serial.println("------------------------");
      Serial.println("------------------------");
      Serial.print("Wind AVG : ");
      Serial.println(avgwind);
      Serial.print("Gust : ");
      Serial.println(gust);
      Serial.print("Girouette : ");
      Serial.println(avggir);    
      Serial.print("pluvio1min : ");
      Serial.println(pluvio1min);
      Serial.print("temp1min : ");
     Serial.println(avgtemp);
     Serial.print("hum1min : ");
     Serial.println(avghum);
      Serial.println("------------------------");
      Serial.println("------------------------");}
 
     //20/10/2019
    //Modification de l'emplacement des RAZ : fin de programme -> avant insertion BDD et carte SD pour ne plus avoir la latence.
    //RAZ des compteurs qui ont servi a calculé les valeurs moyennes sur 1 min
    wind = 0;
    gust = 0;
    nbAnemo = 0;
    gir = 0;
    nbGir = 0;
    temp = 0;
    hum = 0;
    nbBME280 = 0;
    t15 = 0;
    h15 =0;
    p15 = 0;
    v15 = 0;
    r15 = 0;
    g15 = 0;
    
  }
} 
