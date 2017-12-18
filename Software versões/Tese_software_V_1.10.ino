#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <Wire.h>
#include <EEPROM.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"

#define I2C_ADDRESS 0x3C
SSD1306AsciiAvrI2c oled;

//endereco de leitura i2c

//******************************************************
//********Definicoes do pinos a serem usados************
//******************************************************

//pino A0 corrente ACS712
const char a0 = 0;

//pino A1 LM35
const char a1 = 1;

//pinos de carga e descarga mosfet
const char carga = 5;
const char descarga = 6;

//Tempo que o OLED fica ligado em segundos
int tempo_lcd = 8;

//Tempo comunicacao serial
int tempo_serial = 5000; 

//Tensao SPPM
const char painel = 3;

// Estado de carga
float SOCX = 0.0;
float corrente = 0.0;
float gravar = 0.0;
float total_temp = 0.0;

//******************************************************
//**************Variaveis globais***********************
//******************************************************
//Determinado pelo t�cnico com o programa em visual studio 

float corrente_descarga = EEPROM.read(1);
float corrente_nominal = EEPROM.read(2);
float corrente_bateria = corrente_nominal;
float limite_descarga = EEPROM.read(3);       //limite da descarga em %
float limite_carga = EEPROM.read(4);      //limite da carga em %
float temperatura_nominal = EEPROM.read(5); //limite de temperatura

//dia mes ano
int dia = EEPROM.read(6);
int mes = EEPROM.read(7);
int ano = EEPROM.read(8);

/*float corrente_descarga = 1;
float corrente_nominal = 2;
float corrente_bateria = corrente_nominal;
float limite_descarga = 10;       //limite da descarga em %
float limite_carga = 99;      //limite da carga em %
float temperatura_nominal =60; //limite de temperatura

//dia mes ano
int dia = 12;
int mes = 12;
int ano = 17;
*/
//******************************************************
//*********************Setup****************************
//******************************************************
void setup ()
{
  //inicializacao do OLED
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(Adafruit5x7);
  
  //configuracao dos pinos do mosfet
  pinMode(carga,OUTPUT);
  pinMode(descarga,OUTPUT);

  digitalWrite(carga,HIGH);
  digitalWrite(descarga,HIGH);

  //Configuracao pinos interrupcao externa
  pinMode(2,INPUT);
  pinMode(3,INPUT);

  // LED ou BUZZER
  pinMode(13,OUTPUT);
  
  Serial.begin(9600);
  delay(100);
}


//******************************************************
//*********************LOOP*****************************
//******************************************************
//flag que controla a maquina de estados do programa
int flag = 1;
byte flag_lcd = 0;
byte serial = 0;
byte flag_begin = 0;

unsigned int aux = 0;
byte byte1, byte2, byte3, byte4;

int comando = 0;

void loop()
{
 
 detachInterrupt (digitalPinToInterrupt (3));     
 detachInterrupt (digitalPinToInterrupt (2));     
byte flag_x = 0;
 switch(flag)
 {
  case 1: //tratamento Serial
    trata_serial();
  break;
  
  case 2: // temperatura
   int total1, total2;
   Wire.beginTransmission(8);
   delay(1000);
   
   if(corrente_nominal > corrente_bateria)
    corrente_nominal = corrente_bateria;
    
   SOCX = (100*(corrente_nominal/corrente_bateria));
   delay(100);
   Wire.write((int)SOCX);
   delay(100);
   total1 = corrente*100;
   if(total1 < 0)
    total1 = total1*(-1);
   delay(100);
   Wire.write((int)total1);
   delay(100);
   
   total2 = gravar*100;
   if(total2 < 0)
    total2 = total2*(-1);   
   delay(100);
   Wire.write((int)total2);
   delay(100);
   
   Wire.write((int)total_temp);
   delay(100);
   
   Wire.endTransmission();
   delay(100);
 
   estado_temp();
  
  
  break;
  
  case 3: // Sleep, acorda somente com interrupcao externa
   EIFR = bit(INTF1); //flag da interrupcao externa
    modo_sleep_interrupcao();
  break;
  
  case 4: //contabiliza carga
    contabiliza_carga(); //contabiliza carga e descarga sendo colocada nas celulas
  break;
  
  case 5: //sleep wdt 
    modo_sleep_wdt(0b100001); //sleep wdt 8 segundos
  break;
  
  case 6: //imprime lcd e apaga, tempo a ser definido nas variaveis globais
    lcd_print();
  break;
 }
}
//*****************************************************
//****************Tratamento Serial********************
//*****************************************************

byte flag_tempo_comunicacao = 0;
uint16_t tempo_anterior = 0;
byte flag_recebido = 0;
void trata_serial() //estado 1
{
  verifica_serial();
  uint16_t tempo = millis();
  if(flag_tempo_comunicacao == 0)
  {
    tempo_anterior = tempo;
    flag_tempo_comunicacao = 1;
  }
  if((tempo - tempo_anterior >= tempo_serial) && flag_recebido == 0 )//30 segundos de espera na comunica��o, casou houver comunicacao este parte � iguinorada
  {                                                     //espera salvar os dados
    flag = 2;
    flag_tempo_comunicacao = 0;
    Serial.end();
  }
}
String leStringSerial()
{
  String conteudo = "";
  char caractere;

  while(Serial.available () > 0)
  {
    caractere = Serial.read();
    if(caractere != '\n')
    {
      conteudo.concat(caractere);
    }
    delay(10);
  }
  return conteudo;
}

int Ahdes = 0;
int Ah = 0;
int desc_cut = 0;
int car_cut = 0;
int temp_lim = 0;
int contador_leitura = 0;
void verifica_serial()
{
  int x = 0;
  int caso = 0;
 
  while(Serial.available() > 0)
  {
    String recebido = leStringSerial();
    x = recebido.toInt();
    flag_recebido = 1;
    if(x < 100)
      caso = x/10;
    if(x >= 100 && x <10000)
      caso = x/100;
      
    switch(caso)
    {
      case 1: //Corrente de descarga
      if(x < 100)
        EEPROM.write(1,(x%10));
      else
        EEPROM.write(1,(x%100));
      delay(10);
      break;
      
      case 2: //corrente do banco
      if(x < 100)
      {
        EEPROM.write(2,(x%10));
      }
      else
        EEPROM.write(2,(x%100));
      delay(10);
      break;
      
      case 3: //cut off descarga
      EEPROM.write(3,(x%100));
      delay(10);
      break;
      
      case 4: //cut off carga
      EEPROM.write(4,(x%100));
      delay(10);
      break;

      case 5: //temperatura
      EEPROM.write(5,(x%100));
      delay(10);
      break;
      
      case 6://imprime na eeprom
      imprime_eeprom();
      break;
      
      case 7://reseta eeprom
      reset_eeprom();
      break;
      
      case 8://"salva os dados"
      //fecha a inser��o do programa
      flag = 2;
      flag_recebido = 0;
      Serial.end();
      break;

      case 9: //Corrente de descarga
      if(x < 100)
        EEPROM.write(6,(x%10));
      else
        EEPROM.write(6,(x%100));
      delay(10);
      break;

      case 10://mes
      EEPROM.write(7,(x%1000));
      delay(10);
      break;

      case 11://ano
       EEPROM.write(8,(x%1100));
      break;
      
    }
  }
}

//******************************************************
//*************** Reset EEPROM no programa *************
//******************************************************

void reset_eeprom()
{
  int cont;
  for(cont = 0 ; cont<EEPROM.length() ; cont++) 
  {
    EEPROM.write(cont, 0);
  }
  contador_leitura = 0;
}

//******************************************************
//**** Imprime dados da EEPROM no programa *************
//******************************************************
int adress = 9;
int cont = 0;
void imprime_eeprom()
{
   delay(50);
   Ahdes = EEPROM.read(1);
   Serial.print("AhDescarga: ");
   Serial.println(Ahdes);
   delay(50);
   
   Ah = EEPROM.read(2);
   Serial.print("Ah: ");
   Serial.println(Ah);
   delay(50);
       
   desc_cut = EEPROM.read(3);
   Serial.print("Cut-off discharge: ");
   Serial.println(desc_cut);
   delay(50);
  
   car_cut = EEPROM.read(4);
   Serial.print("Cut-off charge: ");
   Serial.println(car_cut);
   delay(50);
  
   temp_lim = EEPROM.read(5);
   Serial.print("Temperature: ");
   Serial.println(temp_lim);
   delay(50);
   
   Serial.println("dia , mes , ano , inicial consumo Ah");
   Serial.println("");
   dia = EEPROM.read(6);
   mes = EEPROM.read(7);
   ano = EEPROM.read(8);
   Serial.print(dia);
   Serial.print("/");
   Serial.print(mes);
   Serial.print("/");
   Serial.println(ano);
   Serial.println("");
   delay(100);
   
   while(cont < EEPROM.read(0)) //quantidade de dias que foi gravado os dados
   {
    Serial.print(EEPROM.read(adress));
    Serial.print(",");
    Serial.print(EEPROM.read(adress+1));
    Serial.print(",");
    Serial.print(EEPROM.read(adress+2));
    Serial.print(",");
    Serial.print(EEPROM.read(adress+3));
    Serial.print(".");
    Serial.println(EEPROM.read(adress+4));
    delay(100);
    adress += 5;
    cont++;
   }
   cont = 0;
}

//******************************************************
//********************Temperatura***********************
//******************************************************
byte flag_temperatura = 0;
byte flag_sobrecarga = 0;

void estado_temp() //estado 2
{
  total_temp = LM35_temp();
  
  if(total_temp >= temperatura_nominal || flag_sobrecarga == 1)
  {
    MOSFET_OFF();
    
    if(flag_sobrecarga == 1)
      flag_temperatura = 0;
    else
      flag_temperatura = 1;
  }
  else
  {
    digitalWrite(7,LOW);
    MOSFET_ON();
    flag_temperatura = 0;
  }
}
//sensor de temperatura
float LM35_temp()
{
  int temp = analogRead(a1);
  return(temp*(5.0/1023.0)*100.0);
}
//maquina de estados 2.1

void MOSFET_OFF()
{ 
  digitalWrite(carga, LOW);
  digitalWrite(descarga, LOW);
  digitalWrite(13,HIGH);
  
  if(flag_temperatura == 1)
    flag = 5;
  else
    flag = 3;
    
}

byte flag_carga = 0;
byte flag_iniciar = 0;
//maquinas de estados 2.2
void MOSFET_ON()
{
 digitalWrite(13,LOW); //desliga LED
  
 digitalWrite(carga,HIGH);
  
 if(analogRead(painel) > 0)
 {
   if(corrente_nominal <= (corrente_bateria*(limite_descarga/100)))
     digitalWrite(descarga,LOW);
   else
     digitalWrite(descarga,HIGH);  
  }
  else
  {
     if(corrente_nominal <= (corrente_bateria*(limite_descarga/100)))
     {
      digitalWrite(descarga,LOW);
     }
     else
        digitalWrite(descarga,HIGH);
  }  
    flag = 4;
}

//******************************************************
//******************Contabiliza Carga*******************
//******************************************************
byte flag_t = 0;
float total = 0.0;
//float gravar = 0.0;
byte flag_dia = 0;
byte flag_grava = 0;

void contabiliza_carga() //contabiliza carga e descarga estado 4
{
  corrente = ACS712();
  if(corrente != 0.0)
  {
    if(corrente >= corrente_descarga) //caso houver uma carga a mais do que a dimensionada o sistema desliga
    {
      flag_sobrecarga = 1;
      MOSFET_OFF();
      flag = 5; //repeti so para garantir
    }
    else
    {  
        total += (8.0*(-corrente))/3600.0;     //multiplicado por 8 pois s� � medido a corrente de 8 em 8 segundos no modo WDT
        corrente_nominal += total;      //verifica a quantidade de carga que ainda se tem nas celulas
        gravar += total;
        
        //limitar gravar;
        if(gravar > 0)
          gravar = 0;
        
        total = 0;
        flag_sobrecarga = 0;
        flag = 5;
        
    }
  }
  if(digitalRead(3) == 0 && flag_temperatura == 0)
    flag = 3; 
  else
    flag = 5;//o programa ficara acordado verificado a temperatura ou se há corrente na carga e descarga
 
  float tensao_painel = 20*(analogRead(painel)/1023.0);
  
  if(tensao_painel > 2 && flag_dia == 0)
  {
     flag_dia = 1; 
     if(flag_grava == 1)
     {
      
      grava_EEPROM(gravar);                   
      gravar = 0;
      contador_leitura++; 
      EEPROM.write(0,contador_leitura); //gravar na EEPROM 
      total = 0;
      calendario();
      flag_grava = 0;
     }
  }
  if(tensao_painel < 1 && flag_dia == 1)
  {
    Serial.println("Entrou-2");
    delay(100);
     
      flag_dia = 0;
      flag_grava = 1;
  }
}

//****************************************************
//************Dados a ser gravados na EEPROM**********
//****************************************************
int endereco_inicial = 9;
void grava_EEPROM(float total)
{
  
    int dado = (100*total); //corrente em decimal
    
    EEPROM.write(endereco_inicial,dia);
    
    EEPROM.write(endereco_inicial+1,mes);
   
    EEPROM.write(endereco_inicial+2, ano);
    
    EEPROM.write(endereco_inicial+3,dado/100);
    EEPROM.write(endereco_inicial+4,dado%100);
    
    endereco_inicial += 5;
}

//******************************************************
//********************Calendario************************
//******************************************************
void calendario()
{
  switch(mes)
  {
    case 1:
    dia++;
    if(dia == 32)
    {
      dia = 1;
      mes = 2;
    }
    break;    
    
    case 2:
    dia++;
    if(dia == 29 && ano%4 == 0)
    {
      dia = 1;
      mes = 3;
    }
    if(dia == 28 && ano%4 != 0)
    {
      dia = 1;
      mes = 3;
    }
    break;

    case 3:
    dia++;
    if(dia == 32)
    {
      dia = 1;
      mes = 4;
    }
    break;   

    case 4:
    dia++;
    if(dia == 31)
    {
      dia = 1;
      mes = 5;
    }
    break;   
  
    case 5:
    dia++;
    if(dia == 32)
    {
      dia = 1;
      mes = 6;
    }
    break;   

    case 6:
    dia++;
    if(dia == 31)
    {
      dia = 1;
      mes = 7;
    }
    break;   

    case 7:
    dia++;
    if(dia == 32)
    {
      dia = 1;
      mes = 8;
    }
    break;   

    case 8:
    dia++;
    if(dia == 32)
    {
      dia = 1;
      mes = 9;
    }
    break;   
    
    case 9:
    dia++;
    if(dia == 31)
    {
      dia = 1;
      mes = 10;
    }
    break;   

    case 10:
    dia++;
    if(dia == 32)
    {
      dia = 1;
      mes = 11;
    }
    break;  
    
    case 11:
    dia++;
    if(dia == 31)
    {
      dia = 1;
      mes = 12;
    }
    break;   
    
    case 12:
    dia++;
    if(dia == 32)
    {
      dia = 1;
      mes = 1;
      ano++;
    }
    break;   
  }
}

//******************************************************
//********************CORRENTE**************************
//******************************************************
const int N = 10;
int i = 0;
int valor[N];
int soma = 0;
int media = 0;
int f = 0;
float ACS712()
{  
  int analog ;
  while(f < 10)
  {
    analog = analogRead(a0);
    for(i = N-1; i >0; i--)
    {
      valor[i] = valor[i-1];
    }
    
    valor[0] = analog;
    long soma = 0;
  
    for(i = 0; i < N; i++)
    {
      soma += valor[i];
    }
    
    media = soma/N;
    f++;
  }
  f = 0;
  if(media > 508 && media < 515)
    media = 512;
  
 if(media > 512)
  return (media - 512.0)/8.0;
 else  
  return (media - 512.0)/18.5;
}
//*****************************************************
//******************** MODO SLEEP *********************
//*****************************************************
//ISR rotina de tratamento
byte flag_imprime = 0;

void int_wdt()                            
{
  wdt_disable();  
  if(flag_lcd == 0)
  {
    flag = 6;  //vai para o estado lcd
    flag_lcd = 1;
  }
  else
  {
    flag_lcd = 0;
    flag = 5;
  }
}

//Rotina de tratamento estouro WDT
ISR (WDT_vect) 
{
  wdt_disable(); //desativa wdt
  flag = 2; //volta para temperatura 
}

//modo sleep WDT com interrupcao externa
void modo_sleep_wdt(const byte interval) //estado 5
{ 

  noInterrupts ();   // timed sequence below

  MCUSR = 0;                          // reset as flags
  WDTCSR |= 0b00011000;               // WDCE, WDE
  WDTCSR =  0b01000000 | interval;    // set WDIE, tempo configurado de cada leitura 
  wdt_reset();
    
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);   // sleep mode setado
  sleep_enable();

    
  attachInterrupt (digitalPinToInterrupt (2), int_wdt, RISING);   
  interrupts ();
  sleep_cpu ();            // dorme aqui
  
  detachInterrupt (digitalPinToInterrupt (2));     
} 

//ISR interrupcao externa
void int_ext()
{
  sleep_disable ();         
  flag = 5; //vai para o wdt
  detachInterrupt (digitalPinToInterrupt (3));     
} 
 
//interrupcao externa
void modo_sleep_interrupcao()
{
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);   
  noInterrupts ();          
  sleep_enable ();          
  attachInterrupt (digitalPinToInterrupt (3), int_ext, CHANGE); 
  interrupts ();          
  sleep_cpu ();           
} 

//******************************************************
//********************LCD IMPRIME***********************
//******************************************************
void lcd_print()
{
  flag = 5;
  int tempo = 0;
  float Auto;
  float Temp = LM35_temp();
  
  if(flag_lcd == 1 && flag_temperatura == 0 && flag_sobrecarga == 0)
  {
    SOCX = (100*(corrente_nominal/corrente_bateria));
    if(SOCX > 100)
      SOCX = 100;

    if(Temp < 60)
      digitalWrite(13,LOW);
    
    if(corrente_nominal > corrente_bateria)
      Auto = corrente_bateria/corrente_descarga;
    else  
      Auto = corrente_nominal/corrente_descarga;
        
    oled.set2X();
    oled.print("Auto: ");
    oled.print(Auto);
    oled.println(" h");
    
    oled.set1X();
    oled.println("                 o");
    
    oled.set2X();
    oled.print("Temp: ");
    oled.print((int)Temp);
    oled.println(" C");
        
    oled.set1X();
    oled.println("");
 
    oled.set2X();
    oled.print("SoC: ");
    oled.print((int)SOCX);
    oled.println(" %");
    
    tempo = tempo_lcd*1000;
    delay(tempo);
    flag_lcd = 2;
  }
  
  if(flag_temperatura == 1)
  {
     
    if(Temp < 60)
      digitalWrite(13,LOW);
    
    oled.set2X();
    
    oled.println(" TEMP ALTA");
   
    oled.set1X();
    oled.println("");
 
    oled.set2X();
    
    oled.println(" PERIGO!!!");
    
    oled.set1X();
    oled.println("");
 
    oled.set2X();
    
    oled.println("AGUARDE...");

   
    tempo = tempo_lcd*1000;
    delay(tempo);
    flag_lcd = 2;      
  }

  if(flag_sobrecarga == 1)
  {
    oled.set2X();
    oled.println(" CORRENTE");
    
    oled.set1X();
    oled.println("");
 
    oled.set2X();
    oled.println("  ALTA!!!");

    oled.set1X();
    oled.println("");
 
    oled.set2X();
    
    oled.println("AGUARDANDO");
    
    tempo = tempo_lcd*1000;
    delay(tempo);
    flag_lcd = 2;
    flag_sobrecarga = 0;    
  }
    lcd_apaga();
}

//******************************************************
//********************LCD APAGA************************
//******************************************************
void lcd_apaga()
{
  flag = 5;
  oled.clear();
}
