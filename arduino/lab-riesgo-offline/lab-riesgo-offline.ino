#define TRIGGER_READ_QRE  87
#define QRE1113_Pin       15
#define RANGONUEVALECTURA 30
#define TRIGGER_GOTAS     2   
#define TIME_OUT_VALIDA_GOTAS 5 // milisegundos
#define TIME_OUT_BLOQUEO_GOTAS 50 // milisegundos

int Maqestado=0;
long timer_maqest=0;
long diff=0;
long timeIR ;
long QRE1113_Diff = 0;
int SensorNivel=0;  
int SensorNivel_Old=0;  
int MaqCtaGotas=0;
int Nivel=0;
long tmr_maqctagotas=0;
int Gotas=0;
int SumaTotal=0;
int Id_Pin_QRE=0;
const int PIN_MIDENIVEL_H2O = A0; 


void setup(){
  Serial.begin(9600);
  Serial.println("Programa de Validacion Pluviometro");
  delay(2000);
}

void loop(){
  
   if(CuentaGotas()>0)
   {
    SumaTotal++;
    MuestraDatos(SensorNivel,SumaTotal);
   }
   if (Serial.available() > 0) {     
     Nivel = Serial.read()-48;
     if(Nivel==0){
      SumaTotal=0;
     }
   } 
    
   SensorNivel=analogRead(PIN_MIDENIVEL_H2O);
   if(abs(SensorNivel-SensorNivel_Old)>RANGONUEVALECTURA){
     SensorNivel_Old=SensorNivel;
     MuestraDatos(SensorNivel,SumaTotal);
   }
   delay(1);
}


bool readQD(int id_pin){
  bool flagread=false;
  switch (Maqestado) {
      case 0:
        pinMode( QRE1113_Pin, OUTPUT );
        digitalWrite( QRE1113_Pin, HIGH );
        timer_maqest=micros()+10;
        Maqestado=1;
        break;
      case 1:
        if(micros()>timer_maqest){
          pinMode( QRE1113_Pin, INPUT_PULLUP );//ESTE ES EL TRUCO!!!!
          Maqestado=0;
          timeIR = micros();
          while (digitalRead(QRE1113_Pin) == HIGH && micros()- timeIR < 500); 
          QRE1113_Diff = micros() - timeIR;
          flagread=true;
        }
        break;
  }
  return(flagread);
} 

void MuestraDatos(int Val_Nivel ,int Val_Bascula){
  int NivelMapeado;
  NivelMapeado=map(Val_Nivel,400,1000,130,0);
  Serial.print("Nivel:");Serial.print(NivelMapeado);Serial.print("\tBascula:");Serial.println(Val_Bascula);
}


int CuentaGotas(){
int Valor=0;
  switch(MaqCtaGotas){
    case 0:
      if(readQD(Id_Pin_QRE)){
        if(QRE1113_Diff < TRIGGER_READ_QRE){
          Gotas++;
          MaqCtaGotas=1;
          tmr_maqctagotas=millis()+TIME_OUT_VALIDA_GOTAS;
        }
      }
    break;
    case 1:
      if(readQD(Id_Pin_QRE)){
        if(QRE1113_Diff < TRIGGER_READ_QRE)
          Gotas++;
          if(Gotas>TRIGGER_GOTAS){
            Valor=1;          
            tmr_maqctagotas=millis()+TIME_OUT_BLOQUEO_GOTAS;
            MaqCtaGotas=2;
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

