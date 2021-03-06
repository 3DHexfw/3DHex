/*
----------------------------------------------------------------------
COPYRIGHT NOTICE FOR 3DHex:
----------------------------------------------------------------------

3DHex - 3D Printer Firmware

Copyright (c) 2019 Panagiotis Menounos
Contact: 3DHexfw@gmail.com


3DHex is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

3DHex is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with 3DHex.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <SPI.h>
#include "SdFat.h"
#include <TimerThree.h>
#include <thermistor.h>
#include <LiquidCrystal.h>
SdFat SD;
File myFile;

#define SERIAL_BAUD_RATE 400000
#define BUTTON_TIME_DURATION 1000
#define LCD_16x4 true
#define ENCODER_PIN 35
#define SD_ENABLE 53
#define MIN_TEMP_SAFETY 5
#define MAX_TEMP_NOZZLE 250
#define MAX_TEMP_BED 80
#define HOME_XMIN_PIN 3
#define HOME_YMIN_PIN 14
#define HOME_ZMIN_PIN 18
#define USB_SETTING_BYTES 52
#define BUFFERSIZE 3300
#define X_EN 38
#define Y_EN A2
#define Z_EN A8
#define E_EN 24 
#define X_STEP A0        //PF0
#define Y_STEP A6        //PF6
#define Z_STEP 46        //PL3
#define E_STEP 26        //PA4
#define X_DIR A1         //PF1
#define Y_DIR A7         //PF7
#define Z_DIR 48         //PL1
#define E_DIR 28         //PA6
#define NOZZ_THRMSTR A13
#define BED_THRMSTR A14
#define NOZZ_HEATER 10
#define BED_HEATER 8

void service_routine();
void temperature_control();
void get_USB_settings();
void get_SD_settings();
void check_steppers();
void read_serial();
void read_sd();
int SD_setup();
int check_inputs();
void check_USB_terminate();
void check_BUTTON_terminate();
void terminate_process();
void homing_routine();

struct data1 { 
   volatile byte byte_1[BUFFERSIZE];
};

struct data2 { 
   volatile byte byte_2[BUFFERSIZE];
};

struct data3{ 
   volatile uint16_t CORE_FREQ;
   volatile uint8_t X_ENABLE;
   volatile uint8_t Y_ENABLE;
   volatile uint8_t Z_ENABLE;
   volatile uint8_t E_ENABLE;
   volatile uint16_t NOZZLE_TEMP;
   volatile uint16_t BED_TEMP;
   volatile uint16_t CYCLE_NOZZ;
   volatile uint16_t CYCLE_BED;
   volatile uint16_t P_NOZZ_MIRROR;
   volatile uint16_t I_NOZZ_MIRROR;
   volatile uint16_t D_NOZZ_MIRROR;
   volatile uint16_t P_BED_MIRROR;
   volatile uint16_t I_BED_MIRROR;
   volatile uint16_t D_BED_MIRROR;
   volatile uint8_t Wait_nozz;
   volatile uint8_t Wait_bed;
   volatile int8_t THERMISTOR_TYPE_NOZZLE;
   volatile int8_t THERMISTOR_TYPE_BED;
   volatile uint8_t HEATED_NOZZLE;
   volatile uint8_t HEATED_BED;
   volatile uint8_t PID_nozz;
   volatile uint8_t PID_bed;
   volatile uint8_t DIFFERENTIAL_NOZZ;
   volatile uint8_t DIFFERENTIAL_BED;
   volatile uint16_t HOME_X_DURATION;
   volatile uint16_t HOME_Y_DURATION;
   volatile uint16_t HOME_Z_DURATION;
   volatile uint8_t HOME_X_ENABLE;
   volatile uint8_t HOME_Y_ENABLE;
   volatile uint8_t HOME_Z_ENABLE;
   volatile uint8_t HOME_X_DIR;
   volatile uint8_t HOME_Y_DIR;
   volatile uint8_t HOME_Z_DIR;
   volatile uint8_t HOME_X_STATE;
   volatile uint8_t HOME_Y_STATE;
   volatile uint8_t HOME_Z_STATE_MIRROR;
   volatile uint8_t HOME_Z_STATE;//MUST BE WRITTEN TWICE
};

struct data1 buffer1;
struct data2 buffer2;
struct data3 buffer3;

volatile double temp1,temp2;
volatile boolean bufferstate=true,readstate=false,setoff=false,nozz_block=true,bed_block=true,once=false,PRINTING=false;
volatile unsigned int i=0,j=2,cc;
volatile byte buf[USB_SETTING_BYTES],PRINT_STATE;
volatile float time_duration;
volatile unsigned long currentMillis=0,previousMillis_Nozz = 0,previousMillis_Bed = 0,signal_duration=0,off=0,on=0;
volatile int XMIN_READ,YMIN_READ,ZMIN_READ;

LiquidCrystal lcd(16, 17, 23, 25, 27, 29);

void setup() {
   pinMode(ENCODER_PIN,INPUT_PULLUP);
   pinMode(SD_ENABLE,OUTPUT);
   pinMode(X_EN,OUTPUT);
   pinMode(Y_EN,OUTPUT);
   pinMode(Z_EN,OUTPUT);
   pinMode(E_EN,OUTPUT); 
   pinMode(X_STEP,OUTPUT);
   pinMode(Y_STEP,OUTPUT);
   pinMode(Z_STEP,OUTPUT);
   pinMode(E_STEP,OUTPUT);
   pinMode(X_DIR,OUTPUT);
   pinMode(Y_DIR,OUTPUT);
   pinMode(Z_DIR,OUTPUT);
   pinMode(E_DIR,OUTPUT);
   pinMode(NOZZ_THRMSTR,INPUT);
   pinMode(BED_THRMSTR,INPUT);
   pinMode(NOZZ_HEATER,OUTPUT);
   pinMode(BED_HEATER,OUTPUT);
   pinMode(HOME_XMIN_PIN,INPUT);
   pinMode(HOME_YMIN_PIN,INPUT);
   pinMode(HOME_ZMIN_PIN,INPUT);
   pinMode(SD_ENABLE,OUTPUT);
   digitalWrite(SD_ENABLE,HIGH);
   digitalWrite(SD_ENABLE,HIGH);
   digitalWrite(BED_HEATER,LOW);
   digitalWrite(NOZZ_HEATER,LOW);
   Serial.begin(SERIAL_BAUD_RATE);
   i=0;j=2;signal_duration=0;off=0;on=0;bufferstate=true;readstate=false;
   setoff=false;nozz_block=true;bed_block=true;once=false,PRINTING=false;
   if(LCD_16x4==true){lcd.begin(20,4);delay(100);}
   delay(100); 
   do{// blocking if no input
      if(LCD_16x4==true){lcd.setCursor(0, 0);lcd.print("MODE: CHECK INPUT");lcd.blink();}    
   }while(check_inputs());
   PRINTING=true; //printing process has started
   if(PRINT_STATE==0){get_USB_settings();}else{get_SD_settings();}
   if(LCD_16x4==true){lcd.noBlink();}
   check_steppers();
   homing_routine();
   if((buffer3.Wait_nozz==1 && buffer3.HEATED_NOZZLE==1) || (buffer3.Wait_bed==1 && buffer3.HEATED_BED==1)){
      if(LCD_16x4==true && setoff==false){lcd.setCursor(0, 2);lcd.print("HEAT UP");lcd.blink();delay(1200);}
   }else{
      if(LCD_16x4==true && setoff==false){lcd.setCursor(0, 2);lcd.print("HEAT UP.");delay(1200);}
   }
   temperature_control(); //blocking if heat up first is enabled
   if(LCD_16x4==true && setoff==false){lcd.noBlink();lcd.setCursor(0, 3);lcd.print("PRINTING..");lcd.blink();delay(1200);}
   Timer3.initialize(time_duration); //[microseconds]
   Timer3.attachInterrupt(service_routine);
}

void loop() {
  if(setoff==false){
     if(PRINT_STATE==0){read_serial();}else{read_sd();}
     temperature_control();
     check_BUTTON_terminate();
  }else{ 
     terminate_process();
  }
}

void service_routine(){ //Timer interrupted service routine for pushing out the bytes
   cli();
   i++;
   if(bufferstate==true && i==buffer1.byte_1[j]){
      j++;
      PORTF = (bitRead(buffer1.byte_1[j],0)<<PF1)|(bitRead(buffer1.byte_1[j],2)<<PF7);
      PORTF = (bitRead(buffer1.byte_1[j],0)<<PF1)|(bitRead(buffer1.byte_1[j],1)<<PF0)|(bitRead(buffer1.byte_1[j],2)<<PF7)|(bitRead(buffer1.byte_1[j],3)<<PF6);
      PORTL = (bitRead(buffer1.byte_1[j],4)<<PL1);
      PORTL = (bitRead(buffer1.byte_1[j],4)<<PL1)|(bitRead(buffer1.byte_1[j],5)<<PL3);
      PORTA = (bitRead(buffer1.byte_1[j],6)<<PA6);
      PORTA = (bitRead(buffer1.byte_1[j],6)<<PA6)|(bitRead(buffer1.byte_1[j],7)<<PA4);
      j++;
      i=0;
   }else if(bufferstate==false && i==buffer2.byte_2[j]){
      j++;
      PORTF = (bitRead(buffer2.byte_2[j],0)<<PF1)|(bitRead(buffer2.byte_2[j],2)<<PF7);
      PORTF = (bitRead(buffer2.byte_2[j],0)<<PF1)|(bitRead(buffer2.byte_2[j],1)<<PF0)|(bitRead(buffer2.byte_2[j],2)<<PF7)|(bitRead(buffer2.byte_2[j],3)<<PF6);
      PORTL = (bitRead(buffer2.byte_2[j],4)<<PL1);
      PORTL = (bitRead(buffer2.byte_2[j],4)<<PL1)|(bitRead(buffer2.byte_2[j],5)<<PL3);
      PORTA = (bitRead(buffer2.byte_2[j],6)<<PA6);
      PORTA = (bitRead(buffer2.byte_2[j],6)<<PA6)|(bitRead(buffer2.byte_2[j],7)<<PA4);
      j++;
      i=0;
   }else{
      PORTF = (0<<PF0)|(0<<PF6);
      PORTL = (0<<PL3);
      PORTA = (0<<PA4);
   }
   if (j==BUFFERSIZE){ //just reset to zero so the buffer locate again at the start point (first object e.g a[0])
      bufferstate=!bufferstate;
      j=0;
      readstate=true; //one of the buffers has pushed all the way so fill it with new values
   }
   sei();
   check_USB_terminate();
}

void homing_routine(){ 
   if(LCD_16x4 == true && (buffer3.HOME_X_ENABLE==true || buffer3.HOME_Y_ENABLE==true || buffer3.HOME_Z_ENABLE==true)){lcd.setCursor(0, 2);lcd.print("HOMING");lcd.blink();}
   delay(800);
   XMIN_READ=digitalRead(HOME_XMIN_PIN);
   while(buffer3.HOME_X_ENABLE==true && XMIN_READ==buffer3.HOME_X_STATE && setoff==false){
      PORTF = (buffer3.HOME_X_DIR<<PF1);
      PORTF = (buffer3.HOME_X_DIR<<PF1)|(1<<PF0);
      delayMicroseconds(buffer3.HOME_X_DURATION);
      PORTF = (buffer3.HOME_X_DIR<<PF1);
      PORTF = (buffer3.HOME_X_DIR<<PF1)|(0<<PF0);
      delayMicroseconds(buffer3.HOME_X_DURATION);
      XMIN_READ=digitalRead(HOME_XMIN_PIN);
      if(digitalRead(ENCODER_PIN)==LOW){setoff=true;}
   }
   YMIN_READ=digitalRead(HOME_YMIN_PIN);
   while(buffer3.HOME_Y_ENABLE==true && YMIN_READ==buffer3.HOME_Y_STATE && setoff==false){
      PORTF = (buffer3.HOME_Y_DIR<<PF7);
      PORTF = (buffer3.HOME_Y_DIR<<PF7)|(1<<PF6);
      delayMicroseconds(buffer3.HOME_Y_DURATION);
      PORTF = (buffer3.HOME_Y_DIR<<PF7);
      PORTF = (buffer3.HOME_Y_DIR<<PF7)|(0<<PF6);
      delayMicroseconds(buffer3.HOME_Y_DURATION);
      YMIN_READ=digitalRead(HOME_YMIN_PIN);
      if(digitalRead(ENCODER_PIN)==LOW){setoff=true;}
   }
   ZMIN_READ=digitalRead(HOME_ZMIN_PIN);
   while(buffer3.HOME_Z_ENABLE==true && ZMIN_READ==buffer3.HOME_Z_STATE && setoff==false){
      PORTL = (buffer3.HOME_Z_DIR<<PL1);
      PORTL = (buffer3.HOME_Z_DIR<<PL1)|(1<<PL3);
      delayMicroseconds(buffer3.HOME_Z_DURATION);
      PORTL = (buffer3.HOME_Z_DIR<<PL1);
      PORTL = (buffer3.HOME_Z_DIR<<PL1)|(0<<PL3);
      delayMicroseconds(buffer3.HOME_Z_DURATION);
      ZMIN_READ=digitalRead(HOME_ZMIN_PIN);
      if(digitalRead(ENCODER_PIN)==LOW){setoff=true;}
   }
}

void terminate_process(){
    if(PRINTING==true){
       Timer3.stop();
       if(PRINT_STATE==0){
          Serial.write('e');
          Serial.readBytes((uint8_t *)&buffer1, sizeof(buffer1));
          Serial.write('e');
       }else{
          myFile.close();
       }
       memset(&buffer1, 0, sizeof(buffer1));
       memset(&buffer2, 0, sizeof(buffer2));
       lcd.clear();
       if(LCD_16x4==true){lcd.setCursor(0, 3);lcd.print("TERMINATED");lcd.blink();}
       delay(3000);
       lcd.clear();
       setup();  //locate again at startup
    } 
}

void check_USB_terminate(){
   if(bufferstate==true && buffer1.byte_1[j]==0){
      setoff=true;}
   else if(bufferstate==false && buffer2.byte_2[j]==0){
      setoff=true;
   }
}

void check_BUTTON_terminate(){
   if(digitalRead(ENCODER_PIN)==LOW){
      off=millis();
      signal_duration=off-on;}
   else{
      on=millis();
      signal_duration=0;
   } 
   if(signal_duration >= BUTTON_TIME_DURATION && PRINTING==true){ 
      setoff=true;
   } 
}

void read_serial(){
   if(readstate==true){ 
      if(bufferstate==false){ 
         memset(&buffer1, 0, sizeof(buffer1));
         Serial.read();
         Serial.write('c');  
         Serial.readBytes((uint8_t *)&buffer1, sizeof(buffer1));
      }else{
         memset(&buffer2, 0, sizeof(buffer2));
         Serial.read();
         Serial.write('c');  
         Serial.readBytes((uint8_t *)&buffer2, sizeof(buffer2));     
      }
      readstate=false;
   } 
}

void read_sd(){
   if(readstate==true){ 
      if(bufferstate==false){ 
         myFile.read(&buffer1,BUFFERSIZE);
      }else{
         myFile.read(&buffer2,BUFFERSIZE);    
      }
      readstate=false;
   } 
}

void get_USB_settings(){
   if(LCD_16x4==true){lcd.setCursor(0, 0);lcd.print("MODE: USB        ");lcd.noBlink();delay(1200);}
   if(LCD_16x4==true){lcd.setCursor(0, 1);lcd.print("CONNECTED!");delay(1200);}
   Serial.read(); //Clears Serial
   Serial.write('c');//Signal to python
   Serial.readBytes((uint8_t *)&buffer1, sizeof(buffer1));
   Serial.read();
   Serial.write('c');
   Serial.readBytes((uint8_t *)&buffer2, sizeof(buffer2));
   for(cc=0;cc<USB_SETTING_BYTES;cc++){//total 52 bytes
      buf[cc]=buffer1.byte_1[cc];
      j++;
   }
   memmove(&buffer3,buf,sizeof(buffer3));
   time_duration=1000/buffer3.CORE_FREQ;
}

int SD_setup()
{
  volatile char savefile[12]="test000.bin";
  volatile int centi,mod_file_num,deca,mona,file_num=0;
  boolean sucess=true;
  if(LCD_16x4==true){lcd.setCursor(0, 0);lcd.print("MODE: SD CARD    ");lcd.noBlink();delay(1200);}
  if(LCD_16x4==true){lcd.setCursor(0, 1);lcd.print("INITIALIZING");lcd.blink();delay(1200);}
  if (!SD.begin(SD_ENABLE)) {
       sucess=false;
  }
  delay(1200);
  if(sucess==true){
      if(LCD_16x4==true){lcd.noBlink();lcd.setCursor(0, 1);lcd.print("SD DETECTED!");delay(1200);}
      if(LCD_16x4==true){lcd.setCursor(0, 2);lcd.print("SEARCH");lcd.blink();delay(1200);}
  }else{
       if(LCD_16x4==true){lcd.noBlink();lcd.setCursor(0, 1);lcd.print("SD ERROR!   ");delay(2000);lcd.clear();}
       return 1;
   }
   do {
      myFile = SD.open(savefile);
      file_num++;
      centi=file_num/100;
      mod_file_num=file_num%100;
      deca=mod_file_num/10;
      mona=mod_file_num%10;
      savefile[4]= centi +'0';
      savefile[5]= deca+'0';
      savefile[6]= mona+'0';            
   } while (!myFile && file_num<=999);
   if (myFile) {
      PRINT_STATE=1;
      return 0;
   }else{
      if(LCD_16x4==true){lcd.setCursor(0, 2);lcd.print("FILE ERROR");delay(2000);lcd.clear();}
      return 1;
   }
}

void get_SD_settings(){
   myFile.read(&buffer3,sizeof(buffer3));
   myFile.read(&buffer1,BUFFERSIZE);
   myFile.read(&buffer2,BUFFERSIZE);
   time_duration=1000/buffer3.CORE_FREQ;   
}

void check_steppers(){
   if(buffer3.X_ENABLE==0){pinMode(X_EN,INPUT);}
   if(buffer3.Y_ENABLE==0){pinMode(Y_EN,INPUT);}
   if(buffer3.Z_ENABLE==0){pinMode(Z_EN,INPUT);}
   if(buffer3.E_ENABLE==0){pinMode(E_EN,INPUT);}
}

void temperature_control(){
   thermistor therm1(NOZZ_THRMSTR,buffer3.THERMISTOR_TYPE_NOZZLE);// nozzle
   thermistor therm2(BED_THRMSTR,buffer3.THERMISTOR_TYPE_BED);  // bed
   do{
      currentMillis = millis();
      if (currentMillis - previousMillis_Bed >= buffer3.CYCLE_BED) {
         previousMillis_Bed = currentMillis;
         temp2 = therm2.analog2temp(); // bed
         if((temp2<MIN_TEMP_SAFETY || temp2>MAX_TEMP_BED) && buffer3.HEATED_BED==1){digitalWrite(BED_HEATER,LOW);digitalWrite(NOZZ_HEATER,LOW);setoff=true;}
         if(temp2<=buffer3.BED_TEMP && buffer3.HEATED_BED==1){digitalWrite(BED_HEATER,HIGH);}else{digitalWrite(BED_HEATER,LOW); bed_block=false;}
      }
      if (currentMillis - previousMillis_Nozz >= buffer3.CYCLE_NOZZ) {
         previousMillis_Nozz = currentMillis;
         temp1 = therm1.analog2temp(); // nozzle
         if((temp1<MIN_TEMP_SAFETY || temp1>MAX_TEMP_NOZZLE) && buffer3.HEATED_NOZZLE==1){digitalWrite(BED_HEATER,LOW);digitalWrite(NOZZ_HEATER,LOW);setoff=true;}
         if(temp1<=buffer3.NOZZLE_TEMP && buffer3.HEATED_NOZZLE==1){if(bed_block==false){digitalWrite(NOZZ_HEATER,HIGH);}else{digitalWrite(NOZZ_HEATER,LOW);}
         }else{digitalWrite(NOZZ_HEATER,LOW); nozz_block=false;}
      }
      if((digitalRead(ENCODER_PIN)==LOW) && ((nozz_block==1 && buffer3.Wait_nozz==1) || (bed_block==1 && buffer3.Wait_bed==1))){setoff=true;} 
   }while(((nozz_block==1 && buffer3.Wait_nozz==1) || (bed_block==1 && buffer3.Wait_bed==1)) && setoff==false);
}

int check_inputs(){
   while(signal_duration < BUTTON_TIME_DURATION && !Serial.available()){
      if(digitalRead(ENCODER_PIN)==LOW){
         off=millis();
         signal_duration=off-on;
      }else{
         on=millis();
         signal_duration=0;
      }
   }
   if(signal_duration >= BUTTON_TIME_DURATION && !Serial.available()){
      signal_duration=0;
      return SD_setup();
   }
   if(Serial.available() && signal_duration < BUTTON_TIME_DURATION){
      PRINT_STATE=0;
      return 0;
   }
    return 1;
}


