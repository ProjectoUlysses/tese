//Bibliotecas adicionadas
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal.h>
#include <EEPROM.h>

//Quantidade de tempo para cada amostra de tempo, 1 dia equivale a 90060 segundos
//aplicando este tempo, tera o dado de consumo diario guardado na eeprom
uint16_t Tempo_Amostras = 300; //ciclos de interrupcao WDT 5min
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
//Determinado pelo técnico com o programa em visual studio 
float corrente_descarga = EEPROM.read(1);
float corrente_nominal = EEPROM.read(2);
float corrente_bateria = corrente_nominal;
float limite_descarga = EEPROM.read(3);       //limite da descarga em %
float limite_carga = EEPROM.read(4);      //limite da carga em %
float temperatura_nominal = EEPROM.read(5); //limite de temperatura

//Sensor LM35
const byte temp_lm = 3;

//Real Time Clock 
RTC_DS3231 rtc;

//Sensor de corrente ACS712_1
const byte a1 = 1;

//Interrupcao externa
const byte dig2 = 2;
const byte dig3 = 3;

//MOSFET DESCARGA 
const byte mos_des = 11;
//MOSFET CARGA
const byte mos_car = 13;

//Interrupcao WDT
byte WDT = 1;

//*****************************************************
//************Prototipos das funcões*******************
//*****************************************************
float ACS712_1();
float temp_lm35();
void data();
void lcd_print();
void modo_sleep_wdt(void);
void modo_sleep_interrupcao(void);
void configuracao_wdt();
ISR(WDT_vect);
String leStringSerial();
void verifica_serial();
void imprime_eeprom();
void descarga();
//*****************************************************
//*******************SETUP*****************************
//*****************************************************
void setup() 
{
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
  //inicialização dos MOSFET's 
  digitalWrite(mos_des,LOW);        
  digitalWrite(mos_car,LOW);

  Serial.begin(9600);  
}
//****************************************************
//*****************Programa principal*****************
//****************************************************
double tempo_anterior = 0;
byte set = 0;
byte flag_serial = 0;
byte flag_serial_on = 0;

void loop()
{  
float Temperatura = temp_lm35();
//Verifica se houve comunicacao serial, aguarda 30 segundos
//se houve comunicacao, aguarda os dados serem salvos
//se nao houve, aguarda 30 segundos e continua o programa
if(flag_serial == 0)
{
  verifica_serial();
  if(flag_serial_on == 0)
  {
    if(set == 0)
    {
       tempo_anterior = millis();
       set = 1;
    }
    if(millis() - tempo_anterior >= 30000)
    {
     flag_serial = 1;
    }
  }
}
//verifica temperatura do banco de células
  if(Temperatura <= temperatura_nominal)
  {
  if((WDT == 1) && (digitalRead(2) == 1) && flag_serial == 1) //Verifica se a porta INT0 esta em nivel alto (comparador)
  {                                    
    if(corrente_nominal <= (corrente_bateria*(limite_descarga/100)))
    {
      digitalWrite(mos_des,HIGH); //desliga a descarga
      digitalWrite(mos_car,LOW); //liga a carga
    }
    if(corrente_nominal >= (corrente_bateria*(limite_carga/100))) 
    {
      
       digitalWrite(mos_car,HIGH); //desliga a carga
       digitalWrite(mos_des,LOW); //liga a descarga
    }
    if((corrente_nominal >= (corrente_bateria*(limite_descarga/100))) && (corrente_nominal <= (corrente_bateria*(limite_carga/100))))
    {
       digitalWrite(mos_car,LOW); //liga a carga
       digitalWrite(mos_des,LOW); //liga a descarga
    }
     
    WDT = 0;          //Flag da interrupção da funcao sleep
    
    descarga();       //função para somar as descargas feita na base de x tempo
    
    modo_sleep_wdt(); //chama funcao sleep 8 em 8 segundos WDT
   }
   else
     if((WDT == 0) && digitalRead(2) == 0 && flag_serial == 1) //modo sleep interrupcao externa
     {
        WDT = 1; 
        
        EIFR |= (1<<INTF0);            //limpa flag de interrupcao externa
      
        modo_sleep_interrupcao();       //funcao sleep com interrupcao
     }
  }
  else //temperatura esta alta, desliga carga e descarga
  {
    digitalWrite(mos_des,HIGH);
    digitalWrite(mos_car,HIGH);
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
}

//Sleep WDT interrupcao interna
void modo_sleep_wdt(void)
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // desliga os clocks da cpu, flash, io, adc, 
  sleep_enable();                      //configura o modo sleep
  attachInterrupt(digitalPinToInterrupt(3),acorda_wdt,LOW);
  sleep_mode();                        //ativa o modo sleep
  //o programa vai continuar daqui assim que acabar o tempo de WDT preescaler
  sleep_disable();
  interrupts();
  digitalWrite(10,LOW); 
  delay(500);
}

//Sleep Interrupcao externa
void modo_sleep_interrupcao(void)
{
  wdt_disable();                       //desativa WDT, se não desativado ele vai acordar
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //modo mais economico de energia
  sleep_enable();                      //habilita o bit do registrador MCUCR do modo escolhido
  attachInterrupt(digitalPinToInterrupt(2),acorda_int,CHANGE); //acorda do modo sleep quando ha mudanca na entrada digital 2 INT0
  sleep_mode();                         //entrou no modo sleep
 //continua daqui
  interrupts();                        //interrupcao ativa
}

void acorda_int() //funcao para limpar a flag de interrupcao em INT0 e verificar se o botao ligado a A0 foi pressionado
{
  if(analogRead(0) == 0)
    lcd_print();
    
  configuracao_wdt();                         //Ativa WDT 
  sleep_disable();                            //modo sleep desativado
  detachInterrupt(digitalPinToInterrupt(2));  //limpa a flag de interrupcao no pino Digital 
}

void acorda_wdt()           //funcao para limpar a flag de interrupcao externa na funcao WDT
{
  noInterrupts();
  sleep_disable();  
  wdt_disable();
  configuracao_wdt();
  WDT = 1;
  lcd_print();
  detachInterrupt(digitalPinToInterrupt(3));  //limpa a flag de interrupcao no pino Digital 3
}

//*****************************************************
//******************* Funcoes de medicao ************** 
//*****************************************************
//Sensor de corrente 
float ACS712_1()
{  
  return (analogRead(a1) - 512.0)/15.38;
}

//*****************************************************
//*********Quanto de Ah foi drenado********************
//*****************************************************
byte flag = 0;
uint16_t tempo_ant = 0;
float total = 0.0;
int contador_leitura = 0;

void descarga()
{
  uint16_t tempo = segundos();
  float corrente = ACS712_1();
  if(flag == 0)
  {
    tempo_ant = segundos();
    flag = 1;
  }
  if(corrente != 0)
  {
    total += (8.0*(-corrente))/3600.0;     //multiplicado por 8 pois só é medido a corrente de 8 em 8 segundos no modo WDT
    delay(10);
    corrente_nominal += total;      //verifica a quantidade de carga que ainda se tem nas celulas
    delay(10);
  }
  else
    total = 0;
  if((tempo - tempo_ant) >= Tempo_Amostras) 
  {
    flag = 0;
    grava_EEPROM(total);                   
    contador_leitura++; 
    EEPROM.write(0,contador_leitura); //gravar na EEPROM 
    total = 0;            
  }
}

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
int endereco = 6;
void grava_EEPROM(float total)
{
    DateTime now = rtc.now();
    
    int dado = (100*total); //corrente em decimal
    
    int dia = now.day();
    EEPROM.write(endereco,dia);
    
    int mes = now.month();
    EEPROM.write(endereco+1,mes);
   
    int ano = now.year();
    EEPROM.write(endereco+2, ano%2000);
    
    EEPROM.write(endereco+3,dado/100);
    EEPROM.write(endereco+4,dado%100);
    
    endereco += 5;
}
//******************************************************
//********************Temperatura***********************
//******************************************************
float temp_lm35()
{
  int temp = analogRead(temp_lm);
  return(temp*(5.0/1023.0)*100);
}
//******************************************************
//***************Inicializacao WDT**********************
//******************************************************
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
//********************************************
//********* Leitura porta serial**************
//********************************************
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

void verifica_serial()
{
  int x = 0;
  int caso = 0;
  int Ah = 0;
  while(Serial.available() > 0)
  {
    flag_serial_on= 1;
    String recebido = leStringSerial();
    x = recebido.toInt();
    
    if(x < 100)
      caso = x/10;
    else
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
        EEPROM.write(2,(x%10));
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
      
      case 6:
      imprime_eeprom();
      break;
      
      case 7:
      reset_eeprom();
      break;
      
      case 8:
      //fecha a inserção do programa
      flag_serial = 1;
      flag_serial_on = 0;
      break;
    }
  }
}

//******************************************************
//********************Limpa EEPROM**********************
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
 int adress = 6;
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
   

   Serial.println("dia , mes , ano , consumo Ah");
   Serial.println("");
   while(cont < EEPROM.read(0))
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
//************** Imprime LCD ***************************
//******************************************************
void lcd_print()
{
       float Autonomia = corrente_nominal/corrente_descarga;
       float Corrente = ACS712_1(); 
       float Temperatura = temp_lm35();
       float SOC = (100*(corrente_nominal/corrente_bateria));

       digitalWrite(10,HIGH);
       delay(100);
       
       lcd.setCursor(0,0);
       lcd.print("Autonomia: ");
       lcd.setCursor(10,0);
       lcd.print(Autonomia);
       lcd.setCursor(14,0);
       lcd.print("hr");
       
       lcd.setCursor(0,1);
       lcd.print("Temp:");
       lcd.setCursor(6,1);
       lcd.print(Temperatura);
       lcd.setCursor(10,1);
       lcd.print("C");
      
       lcd.setCursor(12,1);
       lcd.print(SOC);
}
