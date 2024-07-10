#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BME280I2C.h>
#include <RTClib.h>
#include <stdio.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

#define ADC_ON 13 
#define ADC_R 12
#define SW 14

#define MOSI 23
#define MISO 19
#define CLK 18 //SCK
#define CS 5

#define mW 2  //minuto en los que se hace la grabación de datos

float temp(NAN), hum(NAN), pres(NAN);
float b = 200.375395304049;
float m = -116.0468780696;

char t[32];       //time formato memoria
char t_d[32];     //time formato display
char logName[32] = "/datalog.txt";   //nombre del archivo log 
char logName2[32];
char dataSD[60];

double vHR = 0.00, humS = 0.00;
double lastTS = 0;      // TS última medición [mS]
double T = 10*60*1000;  // Periodo entre mediciones [mS]
bool intSW = false, graba = false; 

RTC_DS3231 rtc;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BME280I2C bme280;

void IRAM_ATTR SW_ISR() {
  intSW = true;
}

void setup() {
  delay (5000);
  Serial.begin(119200);
  delay (100);
// setup PINES de alimentación y lectura del sensor de humedad
  pinMode(ADC_ON, OUTPUT);
  digitalWrite(ADC_ON, LOW);
  pinMode(ADC_R, INPUT);
// setup PIN de wakeup para activar display
  pinMode(SW, INPUT_PULLUP);
  pinMode(CS, OUTPUT);

//..... DISPLAY OLED.........
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();

//..... SENSOR BME280.........
  while(!bme280.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }

  switch(bme280.chipModel())
  {
     case BME280::ChipModel_BME280:
       Serial.println("Found BME280 sensor! Success.");
       break;
     case BME280::ChipModel_BMP280:
       Serial.println("Found BMP280 sensor! No Humidity available.");
       break;
     default:
       Serial.println("Found UNKNOWN sensor! Error!");
  }
//..... RTC.........
  rtc.begin();
  //rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));

//..... MEMORIA SD.........
  DateTime now = rtc.now();
  sprintf(t_d, "%02d/%02d/%02d %02d:%02d", now.day(), now.month(), now.year()-2000, now.hour(), now.minute());  
 
  Serial.print("Initializing SD card...");
  if (SD.begin(CS)) {
    //Serial.println("initialization done.");
    if(!SD.exists(logName)){
      File logFile = SD.open(logName, FILE_WRITE);
      if(logFile){
        logFile.println(t_d);
        logFile.close();
      }else Serial.println("No se pudo GENERAR el archivo");
    }else {
      File logFile = SD.open(logName, FILE_APPEND);
      if(logFile){
        logFile.println(t_d);
        logFile.close();
      } else Serial.println("No se pudo ESCRIBIR el archivo");
    }
  } else  Serial.println("NO SE INICIALIZO SD.");
//.... INTERRUPCION DISPLAY...........
  attachInterrupt(SW, SW_ISR, FALLING);

//.... WATCHDOG....................
  esp_task_wdt_init(60, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

}

void loop() {
  esp_task_wdt_reset();
  DateTime now = rtc.now();
  int minX = now.minute();
  if ((graba==false) && ((minX==mW) || (minX==(mW+10)) || (minX==(mW+20)) || (minX==(mW+30)) || (minX==(mW+40)) || (minX == ((mW+50))))){
    graba=true;
    //lastTS = millis();
    vHR = check_HR();
    humS = ((vHR*m) + b);
    getBME280Data();
    sprintf(dataSD, "%02d/%02d/%02d %02d:%02d:%02d, %.2f, %.2f, %.2f, %.0f, %.3f", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second(),temp, hum, pres, humS, vHR); 
    sprintf(t_d, "%02d/%02d/%02d %02d:%02d", now.day(), now.month(), now.year()-2000, now.hour(), now.minute());  
    sprintf(logName2, "/%02d%02d%02d.txt", now.year()-2000, now.month(), now.day());  
  
    if (SD.begin(CS)) {
      //Serial.println("initialization done.");
      if(!SD.exists(logName2)){
        File logFile = SD.open(logName2, FILE_WRITE);
        if(logFile){
          delay(100);
          logFile.println(dataSD);
          logFile.close();
          SD_ok();
        } else{
          SD_err();
          Serial.println("No se pudo GENERAR el archivo");
        }
      } else {
        File logFile = SD.open(logName2, FILE_APPEND);
        if(logFile){
          delay(100);
          logFile.println(dataSD);
          logFile.close();
          SD_ok();
        } else {
          Serial.println("No se pudo ABRIR el archivo");
          SD_err();
        }
      }
    } else  Serial.println("NO SE INICIALIZO SD.");
  }
  if (intSW == true){
    titulo();
    valores();
    delay(5000);
    display.clearDisplay();
    display.display();
    intSW = false;
    
    if (SD.begin(CS)) {
        SD_ok();
      } else SD_err();
  } 
  if ((graba==true) && ((minX==mW+1) || (minX==(mW+11)) || (minX==(mW+21)| (minX==(mW+31)) || (minX==(mW+41)) || (minX == ((mW+51)))))){
    graba = false;
  }
  delay(1000);
}

void printBME280Data(Stream* client){
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);

  bme280.read(pres, temp, hum, tempUnit, presUnit);

  client->print("Temp: ");
  client->print(temp);
  client->print(" °"+ String(tempUnit == BME280::TempUnit_Celsius ? 'C' :'F'));
  client->print("\t\tHumidity: ");
  client->print(hum);
  client->print("% RH");
  client->print("\t\tPressure: ");
  pres = pres*1.008/100.00;              //*1.012
  client->print(pres);  
  client->println("hPa");

  delay(1000);
}

void getBME280Data(){
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  bme280.read(pres, temp, hum, tempUnit, presUnit);
  pres = pres*1.008/100.00;
}

double check_HR(){
  int val_foam = 0;
  digitalWrite (ADC_ON, HIGH);
  delay(1000);
  for (int i=0; i<5; i++){
    delay(100);
    val_foam += analogRead(ADC_R);

  }
  digitalWrite (ADC_ON, LOW);
  val_foam = val_foam/5;
  //Serial.println("ADC: " + String(val_foam) + "\t");

  double v_foam = 0.119 + double(map(val_foam, 0, 4095, 0, 3300))/1000.00;  //ADC_ATTEN_DB_11	150 mV ~ 2450 mV
  
  return (v_foam);
} 


void titulo(void) {
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(7, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.print("DATALOGGER - Ing.GC");

  display.drawLine(0,10,128,10,SSD1306_WHITE);

  display.setCursor(0, 13);
  display.print("TIME:");
  display.setCursor(0, 25);
  display.print("TEMPERATURA:");
  display.setCursor(0, 35);
  display.print("PRESION:");
  display.setCursor(0, 45);
  display.print("HUM.AMBIENTE:");

  display.setCursor(0, 55);
  display.print("vHR:");
  display.setCursor(72, 55);
  display.print("HR:");

  display.setCursor(108, 25);
  display.print("C");
  display.setCursor(108, 35);
  display.print("hPa");
  display.setCursor(108, 45);
  display.print("%");

  display.setCursor(54, 55);
  display.print("V");
  display.setCursor(108, 55);
  display.print("%");

  display.display();
}

void valores(void) {
  display.fillRect(33, 13, 96, 6, SSD1306_BLACK);
  display.setCursor(33, 13);
  display.print(t_d);

  display.fillRect(77, 25, 24, 24, SSD1306_BLACK);
  display.setCursor(77, 25);
  display.printf("%4.0f", temp);
  display.setCursor(77, 35);
  display.printf("%4.0f", pres);
  display.setCursor(77, 45);
  display.printf("%4.0f", hum);
  display.fillRect(24, 55, 30, 8, SSD1306_BLACK);
  display.setCursor(24, 55);
  display.printf("%.3f", vHR);

  display.fillRect(90, 55, 12, 8, SSD1306_BLACK);
  display.setCursor(90, 55);
  display.printf("%3d", (int(humS)>0 ? int(humS) : 0));
  display.display();
}

void SD_ok(void){
  display.clearDisplay();
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(7, 10);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.print("SD: OK");
  display.display();
  delay(20000);
  display.clearDisplay();
  display.display();
}

void SD_err(void){
  display.clearDisplay();
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(7, 10);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.print("SD: ERROR");
  display.display();
  delay(20000);
  display.clearDisplay();
  display.display();
}