
/// LIBRERIAS COMPLEMENTARIAS
#include <PRIA_LSTM_TEST_inferencing.h>
#include <BME280I2C.h>
#include <Wire.h>

#define ADC_ON 1   //D0 
#define ADC_R 2    //D1
#define LED_R 8    //D9 GPIO8
#define LED_Y 9    //D8 GPIO9


/// DEFINICIÓN DE VARIABLES GLOBALES
const int samplesPerSensor = 144;
const int numSensors = 4;

const float foamHRScalerMax = 106.37742406767299;
const float foamHRScalerMin = 71.09917313451459;

const float tempScalerMax = 28.02;
const float tempScalerMin = 1.11;

const float airHRScalerMax = 86.18;
const float airHRScalerMin = 30.46;

const float paScalerMIn = 1001.54;
const float paScalerMax = 1033.13;

float lastInference = 0.0;
float ALARMA_RED = 65.0;
float ALARMA_YELOW = 75.0;

float sensorBuffer[numSensors][samplesPerSensor];
float features[numSensors * samplesPerSensor];

  
float temp=0.0, hum=0.0, pres=0.0, humS=0.0;
float b = 200.375395304049;
float m = -116.0468780696;

uint countBuffer = 0;
bool fullBuffer = false, update  = false;


// OBEJETOS DE CLASES 
BME280I2C bme280;

// DECLARACIONES DE FUNCIONES DE USUARIO
void updateBuffer(float TA_n, float HR_n, float PA_n, float HRfoam_n);
void flattenBuffer();
void cleanFeatures();
void cleanBuffer();
void printBuffer();

float reScaleHumS(float nHR);
float normTemp(float temp);
float normHum(float hum);
float normPres(float pres);
float normHumS(float humS);

float predictLSTM();
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr);

void getBME280Data();
void check_HR();

void ledRED(int estado);
void ledYELLOW(int estado);

///////////////////////////////////////////////////////////////////////////
// FUNCIÓN SETUP PRINCIPAL

void setup() {
  Serial.begin(115200); 
  delay(500);

  // setup PINES de alimentación y lectura del sensor de humedad
  pinMode(ADC_ON, OUTPUT);
  digitalWrite(ADC_ON, LOW);
  pinMode(ADC_R, INPUT);

  // setup PINES de LED de señalización
  pinMode (LED_R, OUTPUT);
  digitalWrite(LED_R,LOW);
  pinMode (LED_Y, OUTPUT);
  digitalWrite(LED_Y,LOW);

  cleanBuffer();
  cleanFeatures();

  Wire.begin();
  while(!bme280.begin()){
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }
}

////////////////////////////////////////////////////////////////////////
// FUNCION LOOP PRINCIPAL

void loop() {
  static double ts = millis();
  static double tLed = millis();

  if(!fullBuffer){
    if ((millis()-tLed) > (10*1000)){
      tLed = millis();
      ledRED(HIGH);
      delay(75);
      ledRED(LOW);
    }
  }

  if((millis()-ts)>(10*60*1000)){
    ts = millis();
    getBME280Data();
    check_HR();
    Serial.println(String(temp)+" "+String(hum)+" "+String(pres)+" "+String(humS));
    updateBuffer(normTemp(temp),normHum(hum),normPres(pres),normHumS(humS));

    if(!fullBuffer){
      countBuffer++;
      if (countBuffer==144) fullBuffer=true;
    }

    //printBuffer();

    if(fullBuffer){
      flattenBuffer();
      lastInference = reScaleHumS(predictLSTM());
      Serial.println("Predicción re-escalada: " + String(lastInference));
    }  
  }
  if (fullBuffer){
    if(lastInference > ALARMA_RED && lastInference<ALARMA_YELOW){
      ledYELLOW(HIGH);
      ledRED(LOW);
    }else if(lastInference<ALARMA_RED){
      ledYELLOW(LOW);
      ledRED(HIGH);
    }else{
      ledRED(LOW);
      if ((millis()-tLed) > (5*1000)){
        tLed = millis();
        ledYELLOW(HIGH);
        delay(75);
        ledYELLOW(LOW);
        delay(120);
        ledYELLOW(HIGH);
        delay(75);
        ledYELLOW(LOW);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////
// DEFINICIONES DE FUNCIONES DE USUARIO
float predictLSTM(){
  ei_impulse_result_t result = { 0 };
  signal_t features_signal;

  if (sizeof(features) / sizeof(float) != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
      ei_printf("The size of your 'features' array is not correct. Expected %lu items, but had %lu\n",
          EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, sizeof(features) / sizeof(float));
      delay(1000);
      return 0;
  }

  features_signal.total_length = sizeof(features) / sizeof(features[0]);
  features_signal.get_data = &raw_feature_get_data;

  EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false /* debug */);
  
  if (res != EI_IMPULSE_OK) {
      ei_printf("ERR: Failed to run classifier (%d)\n", res);
      return 0;
  }

  float lastInference = result.classification[0].value;
  //char predic[8];
  //sprintf(predic,"%.5f",lastInference);
  //Serial.println ("Predicción: " + String(predic));

  return lastInference;
}

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

void updateBuffer(float TA_n, float HR_n, float PA_n, float HRfoam_n){
  Serial.println(String(TA_n)+" "+String(HR_n)+" "+String(PA_n)+" "+String(HRfoam_n));
  for(int s=0; s < numSensors; s++){
    for(int d=0; d < samplesPerSensor; d++){
      sensorBuffer[s][d] = sensorBuffer[s][d+1];
    }
  }
  sensorBuffer[0][samplesPerSensor-1] = TA_n;
  sensorBuffer[1][samplesPerSensor-1] = HR_n;
  sensorBuffer[2][samplesPerSensor-1] = PA_n;
  sensorBuffer[3][samplesPerSensor-1] = HRfoam_n;
}

void flattenBuffer(){
  int flatIndex = 0;
  for(int d=0; d < samplesPerSensor; d++){
    for(int s=0; s < numSensors; s++){
      flatIndex = (d * numSensors) + s;
      features[flatIndex] = sensorBuffer[s][d];
    }
  }
}

void cleanFeatures(){
  for(int i=0; i < (numSensors * samplesPerSensor); i++){
    features[i]=0;
  }
}

void cleanBuffer(){
  for(int s=0; s < numSensors; s++){
    for(int d=0; d < samplesPerSensor; d++){
      sensorBuffer[s][d] = 0;
    }
  }
}

float reScaleHumS(float nHR){
  float HRfoamPredict = (nHR * (foamHRScalerMax - foamHRScalerMin))+foamHRScalerMin;
  return HRfoamPredict;   
}

float normTemp(float temp){
  return ((temp - tempScalerMin)/(tempScalerMax - tempScalerMin));
}

float normHum(float hum){
  return ((hum - airHRScalerMin)/(airHRScalerMax - airHRScalerMin));
}

float normPres(float pres){
  return ((pres - paScalerMIn)/(paScalerMax - paScalerMIn));
}

float normHumS(float humS){
  return ((humS - foamHRScalerMin)/(foamHRScalerMax - foamHRScalerMin));
}

void printBuffer(){
    Serial.println();
  for(int d=0; d < samplesPerSensor; d++){
    for(int s=0; s < numSensors; s++){
      Serial.print(sensorBuffer[s][d]);
      Serial.print(",");
    }
    Serial.println();
  }
  Serial.println();
}

void getBME280Data(){
  float temp_(NAN), hum_(NAN), pres_(NAN);
  float temp5=0.0, hum5=0.0, pres5=0.0;
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  for (int i=0; i<5;i++){
    bme280.read(pres_, temp_, hum_, tempUnit, presUnit);
    temp5 += temp_;
    hum5 += hum_;
    pres5 += pres_;
    delay(1000);
  }  
  pres = pres5*1.008/500.00;
  hum = hum5/5.0;
  temp = temp5/5.0;
}

void check_HR(){
  int val_foam = 0;
  digitalWrite (ADC_ON, HIGH);
  delay(1000);
  for (int i=0; i<5; i++){
    delay(100);
    val_foam += analogRead(ADC_R);

  }
  digitalWrite (ADC_ON, LOW);
  val_foam = val_foam/5;
  
  //Serial.print("ADC: " + String(val_foam) + "\t");
  double v_foam = 0.047 + double(map(val_foam, 0.00, 4095.00, 0.00, 3300.00))/1000.00;  //ADC_ATTEN_DB_11	150 mV ~ 2450 mV
  char val[6]="";
  sprintf(val,"%.3f",v_foam);
  Serial.println("v_ADC: " + String(val));

  humS = ((v_foam*m) + b);
  
} 

void ledRED(int estado){
	//digitalWrite(LEDBUILIN,estado);
  digitalWrite(LED_R,estado);
}

void ledYELLOW(int estado){
	digitalWrite(LED_Y,estado);
}