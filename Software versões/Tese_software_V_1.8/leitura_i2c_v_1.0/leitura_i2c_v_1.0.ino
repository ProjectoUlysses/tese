#include <Wire.h>

void setup() {
  // put your setup code here, to run once:

  Wire.begin(8);
  Wire.onReceive(receiveEvent);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
}
byte byte1, byte2, byte3, byte4;
char c;
unsigned int aux;
int x;
float aux_saida;

void receiveEvent(int bytes) {
  
  x = Wire.read();    // recebe o byte como inteiro
  Serial.print("SOC: ");
  Serial.print(x);
  Serial.println("%"); 
  
  Serial.print("Corrente: ");
   int corrente = Wire.read();
   float resultado = (float)corrente/100;
     
   Serial.print(resultado);
   Serial.println(" A");
   
  Serial.print("Corrente consumida: ");
   int correntec = Wire.read();
   float resultado2 = (float)correntec/100;
   
   Serial.print(resultado2);
   Serial.println(" Ah");
   
  Serial.print("Temperatura da bateria: ");
  x = Wire.read();    // recebe o byte como inteiro
  Serial.print(x); 
  Serial.println(" Graus");
}
