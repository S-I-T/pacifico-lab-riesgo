#include <ESP8266WiFi.h>    //Instrucciones para instalar en https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/all
#include <FirebaseArduino.h>

// Credenciales de acceso a Firebase
#define FIREBASE_HOST         "<firebase id>.firebaseio.com"
#define FIREBASE_AUTH         "<auth-secret-key>"

#define WIFI_SSID               "<your ssid>"
#define WIFI_PASSWORD           "<ssid pass|blank"

#define SOFT_LIMIT_LEVEL      200  //Estos son los mm de altura desde el piso hasta el primer limite (Amarillo) VAlor minimo = 50
#define HARD_LIMIT_LEVEL      400 //Esto marca el limete Rojo   . Valor minimo = 80 pero mayor que el anterior

#define SOFT_LIMIT_RAIN_GAUGE 10
#define HARD_LIMIT_RAIN_GAUGE 50

#define OFFSET_LEVEL          50
#define BUCKET_VOLUME         2

#define LED_PIN       5
#define RAIN_PIN      15
#define LEVEL_PIN     A0

#define TRIGGER_READ_QRE        70
#define TRIGGER_GOTAS           2
#define TIME_OUT_VALIDA_GOTAS   20 // milisegundos
#define TIME_OUT_BLOQUEO_GOTAS  80 // milisegundos


String mSensorRoute;
String mSensorLimitsRoute;
String mLevelMeasureRoute;
String mRainGaugeMeasureRoute;
bool   mSetupLimits = true;
int    mLevel;
int    mCurLevel = -1;
int    mRainGauge;
int    mCurRainGauge  = -1;
int SumaTotal=0;

int Maqestado=0;
long timer_maqest=0;
long diff=0;
long timeIR;
int QRE1113_Diff = 0;
long tmr_maqctagotas=0;
int MaqCtaGotas=0;

int Gotas=0;


StaticJsonBuffer<50> jsonBuffer;
JsonObject& mData = jsonBuffer.createObject();

StaticJsonBuffer<50> jsonBufferTimestamp;
JsonObject& mTimestamp = jsonBufferTimestamp.createObject();


void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(RAIN_PIN, INPUT);
  pinMode(LEVEL_PIN, INPUT);

  Serial.begin(9600);
  delay(1000);

  Serial.println("Configurando...");

  Serial.print("ID sensor: ");
  Serial.println(WiFi.macAddress());

  //Inicializamos WiFi
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);


  //Inicializamos datos
  mTimestamp[".sv"]  = "timestamp";
  mData["value"]     = "0";
  mData["timestamp"] = mTimestamp;


  //Inicializamos Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  mSensorRoute           = "sensors/" + WiFi.macAddress() + "/";
  mSensorLimitsRoute     = "sensors/" + WiFi.macAddress() + "/limits/";
  mLevelMeasureRoute     = "measures/" + WiFi.macAddress() + "/level/";
  mRainGaugeMeasureRoute = "measures/" + WiFi.macAddress() + "/rain_gauge/";

  Serial.println("Iniciando...");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN,HIGH);

    if(mSetupLimits){
        Serial.println("Seteando limites...");

        Firebase.setInt(mSensorLimitsRoute+"soft_limit_level",SOFT_LIMIT_LEVEL);
        if (Firebase.failed()) {
            Serial.print("Error al setear soft_limit_level:");
            Serial.println(Firebase.error());
        }

        Firebase.setInt(mSensorLimitsRoute+"hard_limit_level",HARD_LIMIT_LEVEL);
        if (Firebase.failed()) {
            Serial.print("Error al setear hard_limit_level:");
            Serial.println(Firebase.error());
        }

        Firebase.setInt(mSensorLimitsRoute+"soft_limit_rain_gauge",SOFT_LIMIT_RAIN_GAUGE);
        if (Firebase.failed()) {
            Serial.print("Error al setear soft_limit_rain_gauge:");
            Serial.println(Firebase.error());
        }

        Firebase.setInt(mSensorLimitsRoute+"hard_limit_rain_gauge",HARD_LIMIT_RAIN_GAUGE);
        if (Firebase.failed()) {
            Serial.print("Error al setear hard_limit_rain_gauge:");
            Serial.println(Firebase.error());
        }

        Firebase.set(mSensorRoute+"last_boot",mTimestamp);
        if (Firebase.failed()) {
            Serial.print("Error al setear last boot:");
            Serial.println(Firebase.error());
        }

        Serial.println("Ready");

        mSetupLimits = false;
    }


     //Level
     mLevel = readLevel();
     if(mLevel > mCurLevel + OFFSET_LEVEL || mLevel < mCurLevel - OFFSET_LEVEL){
        //Serial.println("Sending level="+mLevel);

        int mLevelMap;
        mLevelMap=map(mLevel,400,1000,130,0);

        MuestraDatos(mLevelMap,SumaTotal);

        mData["value"] = mLevelMap;

        Firebase.push(mLevelMeasureRoute,mData);
        if (Firebase.failed()) {
            Serial.print("Error al enviar nivel: ");
            Serial.println(Firebase.error());
        }

        mCurLevel = mLevel;
     }

     //Rain gauge
     mRainGauge = readRainGauge();
     if(mRainGauge ==0)
      MuestraDatos(mLevel,++SumaTotal);
     /*if(readQD(RAIN_PIN)){
        if(QRE1113_Diff < TRIGGER_READ_QRE){
            Serial.print("Dif:");Serial.println(QRE1113_Diff);
        }
     }
     */

     if(mRainGauge==0 && mCurRainGauge==1){

        //MuestraDatos(mLevel,++SumaTotal);

        //Serial.println("Sending rain gauge cycle");
        mData["value"] = BUCKET_VOLUME;
        Firebase.push(mRainGaugeMeasureRoute,mData);
        if (Firebase.failed()) {
            Serial.print("Error al enviar lluvia: ");
            Serial.println(Firebase.error());
        }
     }
     mCurRainGauge = mRainGauge;

  }
  else{
    digitalWrite(LED_PIN,LOW);
  }
  //delay(1);
}

int readLevel(){
  return analogRead(LEVEL_PIN);
}

int readRainGauge(){
int Valor=1;
  switch(MaqCtaGotas){
    case 0:
      if(readQD(RAIN_PIN)){
        if(QRE1113_Diff < TRIGGER_READ_QRE){
          Gotas++;
          MaqCtaGotas=1;
          tmr_maqctagotas=millis()+TIME_OUT_VALIDA_GOTAS;
          //Serial.print("E0");
        }
      }
    break;
    case 1:
      if(readQD(RAIN_PIN)){
        if(QRE1113_Diff < TRIGGER_READ_QRE)
          Gotas++;
          //Serial.print("E1");
          if(Gotas>TRIGGER_GOTAS){
            Valor=0;
            tmr_maqctagotas=millis()+TIME_OUT_BLOQUEO_GOTAS;
            MaqCtaGotas=2;
            //Serial.print("E2");
          }
        }
      if(millis()>tmr_maqctagotas){
        MaqCtaGotas=0;
        Gotas=0;
      }
    break;
    case 2:
      if(millis()>tmr_maqctagotas){
        MaqCtaGotas=0;
        Gotas=0;
      }
    break;
  }
  return(Valor);
}

//***********************************************************///

bool readQD(int id_pin){
  bool flagread=false;
  switch (Maqestado) {
      case 0:
        pinMode( id_pin, OUTPUT );
        digitalWrite( id_pin, HIGH );
        timer_maqest=micros()+10;
        Maqestado=1;
        break;
      case 1:
        if(micros()>timer_maqest){
          pinMode( id_pin, INPUT_PULLUP );//ESTE ES EL TRUCO!!!!
          Maqestado=0;
          timeIR = micros();
          while (digitalRead(id_pin) == HIGH && micros()- timeIR < 500);
          QRE1113_Diff = micros() - timeIR;
          flagread=true;
        }
        break;
  }
  return(flagread);
}

void MuestraDatos(int Val_Nivel ,int Val_Bascula){
  Serial.print("Nivel:");Serial.print(Val_Nivel);Serial.print("\tBascula:");Serial.println(Val_Bascula);
}


