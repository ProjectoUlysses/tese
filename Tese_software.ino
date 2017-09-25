//Bibliotecas adicionadas
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <MAX17043.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal.h>
#include <EEPROM.h>

//Quantidade de tempo para cada amostra de tempo, 1 dia equivale a 90060 segundos
//aplicando este tempo, tera o dado de consumo diario guardado na eeprom
uint16_t Tempo_Amostras = 90060; //ciclos de interrupcao WDT
//Visor 16x2
LiquidCrystal lcd(8,9,4,5,6,7); 
/*
8 -> RS 
9 -> E - habilita
4 ->  Digital
5 -> Digital
6 -> Digital
7 -> Digital
outra coisa e a luz de fundo
10 -> controle da luz de fundo
A0 -> controle dos botoes
*/

//Sensor LM35
const byte temp_lm = 3;

//Limite do SOC
const int superior = 99;
const int inferior = 20;

//Real Time Clock 
RTC_DS3231 rtc;

//Sensor de corrente ACS712_1&2
const byte a1 = 1;
const byte a2 = 2;

//Interrupcao externa
const byte dig2 = 2;
const byte dig3 = 3;

//MOSFET DESCARGA 
const byte mos_des = 13;

//MOSFET CARGA
const byte mos_car = 11;

//Interrupcao WDT
byte WDT = 1;

//MAX17043 da bateria
MAX17043 bateria;

//*****************************************************
//************Prototipos das funcões*******************
//*****************************************************
float ACS712_1();
float ACS712_2();
float temp_lm35();
void data();
void lcd_print();
void modo_sleep_wdt(void);
void modo_sleep_interrupcao(void);
void configuracao_wdt();
void carga_nominal();
ISR(WDT_vect);

//*****************************************************
//*******************SETUP*****************************
//*****************************************************
void setup() 
{

  //Primeira coisa a se fazer antes de gravar os dados na EEPROM é limpar ela
  int cont;
  for(cont = 0 ; cont<EEPROM.length() ; cont++) 
  {
    EEPROM.write(cont, 0);
  }

  //Configuracao mosfet Carga/descarga como saida
  pinMode(mos_car,OUTPUT);
  pinMode(mos_des,OUTPUT);
  
  //Configuracao inicial LCD
  lcd.begin(16,2); 
  pinMode(10,OUTPUT); //pino da luz de fundo do lcd
  
  //Configuracao pino de interrupcao
  pinMode(dig2,INPUT);
  pinMode(dig3,INPUT);
    
  //Configuracao do WDT
  configuracao_wdt();
 
  //Configuraçao RTC
   if(!rtc.begin())
   {
    Serial.println("Nao encontrou RTC");
    while(1);
   }
   if(rtc.lostPower())
   {
    Serial.println("RTC sem energia");
    //ajustar a data
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); //ajusta a data atual de acordo com o sistema
   }
   
  //Inicializacao MAX17043
  digitalWrite(mos_car,HIGH);
  delay(500);
  //inicializar a medição do estado da carga MAX17043
  bateria.reset();
  bateria.quickStart();
  //bateria.setAlertThreshold(20);  //definindo a X% de carga minima
  delay(500);
  digitalWrite(mos_des,LOW);        //liga os MOSFET's, carga e descarga.
  digitalWrite(mos_car,LOW);
  
  Serial.begin(9600);  
}
//****************************************************
//*****************Programa principal*****************
//****************************************************
int flag_x = 0;
void loop()
{
float SOC = bateria.getSoC();
float Temperatura = temp_lm35();
float corrente_des = ACS712_2(); 
if((WDT == 1) && digitalRead(2) == 1) //Verifica se a porta INT0 esta em nivel alto (comparador)
{                                    
  if(SOC <= inferior)
  {
    digitalWrite(mos_des,HIGH); //desliga a descarga
    digitalWrite(mos_car,LOW); //liga a carga
  }
  if(SOC >= superior)
  {
     digitalWrite(mos_car,HIGH); //desliga a carga
     digitalWrite(mos_des,LOW); //liga a descarga
  }
  if(SOC < superior && SOC > inferior)
  {
     digitalWrite(mos_car,LOW); //liga a carga
     digitalWrite(mos_des,LOW); //liga a descarga
  }
   
  descarga();       //função para somar as descargas feita na base de x tempo
  
  WDT = 0;          //Flag da interrupção da funcao sleep
  float total;
  if(flag_x == 1)  //so foi possivel imprimir o SOC e o VCeel no loop()
  {
     lcd.clear();
     lcd.setCursor(0,0);
     lcd.print(bateria.getVCell());
     lcd.setCursor(4,0);
     lcd.print("V");
     lcd.setCursor(6,0);
     total = SOC*(7/1);       // Em minutos... Exemplo corrente das baterias totais/corrente de descarga
     lcd.print((int)total);
     lcd.setCursor(9,0);
     lcd.print("mi");
     lcd.setCursor(12,0);
     lcd.print(corrente_des);
     lcd.setCursor(15,0);
     lcd.print("A");
     lcd.setCursor(0,1);
     lcd.print("SOC:");
     lcd.setCursor(5,1);
     lcd.print(SOC);
     lcd.setCursor(9,1);
     lcd.print("%");
     lcd.setCursor(11,1);
     lcd.print(Temperatura);
     lcd.print("C");
     flag_x = 0;
  }
  
  modo_sleep_wdt(); //chama funcao sleep 8 em 8 segundos WDT

 }
 else
 {
    digitalWrite(mos_car,LOW);     //liga o MOSFET
    digitalWrite(10,LOW);
    WDT = 1; 
    
    EIFR |= (1<<INTF0);            //limpa flag de interrupcao externa
    
    modo_sleep_interrupcao();       //funcao sleep com interrupcao
 }
}
//*****************************************************
//******************** MODO SLEEP *********************
//*****************************************************

//Quando acontece a interrupcao pow WDT ele faz o teste

ISR(WDT_vect) //Funcao usada depois que ocorre o estouro no WDT (ISR - Interrupt Service Routine)
{
  if(WDT == 0)
  {
    WDT = 1;
  }
  else
  {
    Serial.println("WDT RODANDO!");//não ocorreu o tempo suficiente para o estouro do WDT
  }
}

//Sleep WDT interrupcao interna
void modo_sleep_wdt(void)
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // desliga os clocks da cpu, flash, io, adc, 
  sleep_enable();                      //configura o modo sleep
  attachInterrupt(digitalPinToInterrupt(3),acorda_wdt,RISING);
  sleep_mode();                        //ativa o modo sleep
  //o programa vai continuar daqui assim que acabar o tempo de WDT preescaler
  sleep_disable();
  interrupts();
  digitalWrite(10,LOW); 
}

//Sleep Interrupcao externa
void modo_sleep_interrupcao(void)
{
  wdt_disable();                       //desativa WDT, se não desativado ele vai acordar
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //modo mais economico de energia
  //noInterrupts();                      //nao havera interrupcao antes de dormir
  sleep_enable();                      //habilita o bit do registrador MCUCR do modo escolhido
  attachInterrupt(digitalPinToInterrupt(2),acorda_int,CHANGE); //acorda do modo sleep quando ha mudanca na entrada digital 2 INT0
  sleep_mode();
  interrupts();                        //interrupcao ativa
}

void acorda_int()
{
  if(analogRead(0) == 0)
  {
    sleep_disable();
    lcd_print();
  }
  configuracao_wdt();                         //Ativa WDT 
  sleep_disable();                            //modo sleep desativado
  detachInterrupt(digitalPinToInterrupt(2));  //limpa a flag de interrupcao no pino Digital 2
}

void acorda_wdt()
{

  while(digitalRead(3)== 1);
  
  noInterrupts();
  wdt_disable();                            //desliga para recomeçar a contagem de 8segundos
  sleep_disable();
   
  lcd_print();                              
  
  configuracao_wdt();                       //reinicializa o wdt
  
  WDT = 1;
  detachInterrupt(digitalPinToInterrupt(3));  //limpa a flag de interrupcao no pino Digital 3
}

//*****************************************************
//******************* Funcoes de medicao ************** 
//*****************************************************
//Sensor de carga (ficou para ser decidido, antes ou depois do TP4056)
float ACS712_1()
{
  int corrente = analogRead(a1);
 
  if(corrente >= 508 && corrente <= 512)
    corrente = 512;
  
  float total;
  //regular empiricamente a corrente
  total = (corrente - 512)/5.681;
 
  return total;
}

//Sensor de corrente da carga, para coletar os dados de consumo
float ACS712_2()
{
  
 int corrente = analogRead(a2);

if(corrente >= 500 && corrente <= 513)
    corrente = 512;
 
  float total;
  //regular empiricamente a corrente
  total = (corrente - 512)/17.804;
  
  return total;
}
//*****************************************************
//*********Quanto de Ah foi drenado********************
//*****************************************************
byte flag = 0;
uint16_t tempo_ant = 0;
float total = 0;

void descarga()
{
  uint16_t tempo = segundos();
  float corrente = ACS712_2();
  
  if(flag == 0)
  {
    tempo_ant = segundos();
    flag = 1;
  }
  
  total += 8*corrente; //multiplicado por 8 pois só é medido a corrente de 8 em 8 segundos no modo WDT
  if((tempo - tempo_ant) >= Tempo_Amostras) //dependende da quantidade de amostras por dia que se deseja
  {
    total = total/3600.0;                  // divide o total pela quantidade de segundos que tem uma hora
    flag = 0;
    grava_EEPROM(total);                     //gravar na EEPROM 
    total = 0;                            //nao preciso 0 a flag e nem total, pois o programa ira começar do começo depois de executado a função abaixo
  }
}
//*****************************************************
//********** Verificar Carga Nominal*******************
//*****************************************************
//Funcao que converte horas, minutos em segundos
uint16_t segundos()
{
  DateTime now = rtc.now();
  
  int hora    = now.hour();
  int minuto  = now.minute();
  int segundo = now.second();
    
  return ((hora*3600)+(minuto*60)+ segundo);
}
//****************************************************
//************Dados a ser gravados na EEPROM**********
//****************************************************
int endereco = 0;
void grava_EEPROM(float total)
{
    DateTime now = rtc.now();
    
    int dado = (100*total); //corrente em decimal
    
    int dia = now.day();
    EEPROM.write(endereco,dia);
    
    int mes = now.month();
    EEPROM.write(endereco+1,mes);
   
    int ano = now.year();
    EEPROM.write(endereco+2,ano/256);
    EEPROM.write(endereco+3,ano%256);
    
    EEPROM.write(endereco+4,dado/100);
    EEPROM.write(endereco+5,dado%100);
    
    endereco += 6;
}
//********************Temperatura***********************
float temp_lm35()
{
  int temp = analogRead(temp_lm);
  return(temp*(5.0/1023.0)*100);
}

void configuracao_wdt()
{
  //WDE esta sempre setato qndo WDRF esta setado, para limpar precisa ser a sequencia
  //abaixo, limpa WDRF e depos o WDE, serve para uma reinicialização mais segura
  MCUSR &= ~(1<<WDRF); //forca o bit WDRF do registrador MCUSR ser 0, flag de reset do WDT, seta quando há ocorrencia de reset
  WDTCSR |= (1<<WDCE) | (1<<WDE); //força os bits especificos do registrador WDTCSR ser 1 
  //WDCE Usado para modificar nosso preescaler, depois de 4 ciclos de clock o hardware coloca o pino em '0'
  //WDE  sempre setado quando WDRF esta setado, para limpar precisa limpar o WDRF antes. Responsavel por multiplos resets quando a falha no sistema, e ativa o sistema de maneira segura depois de falhar
  WDTCSR = (1<<WDP0) | (1<<WDP3);//preescalar de 8 segundos
  //para acessar um bit especifico em um byte usa-se _BV
  WDTCSR |= _BV(WDIE);
  //WDTCSR |= (1<<WDIE); //forca o bit WDIR ser 1, ativando a interrupcao do WDT e desativa a interrupcao do WDT
}

//Funcao para imprimir no LCD
int flag_f = 0;
void lcd_print()
{
  digitalWrite(10,HIGH); 
  if(flag_f == 0)
  {
    lcd.setCursor(0,0);
    lcd.print("Loading in 8 seg");
    lcd.setCursor(0,1);
    lcd.print("Wait and press"); 
    flag_f = 1;
  }
  flag_x = 1;
}
