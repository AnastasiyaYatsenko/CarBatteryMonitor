#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include <melody_player.h>
#include <melody_factory.h>

#define Pin_i2c_scl 4
#define Pin_i2c_sda 5
#define Pin_buzzer 19
#define Pin_ds18b20 18
#define Pin_LedGreen 1
#define Pin_LedRed 2
#define Pin_LedYellow 3
#define Pin_FreeButton 10

Adafruit_INA219 VoltageSensor;
OneWire oneWire(Pin_ds18b20);
DallasTemperature ds18(&oneWire); //no sensor
Adafruit_SSD1306 display;
MelodyPlayer player(Pin_buzzer, HIGH);

RTC_DATA_ATTR float shuntvoltage = 0;
RTC_DATA_ATTR float busvoltage = 0;
RTC_DATA_ATTR float current_mA = 0;
RTC_DATA_ATTR float loadvoltage = 0;
RTC_DATA_ATTR float power_mW = 0;
RTC_DATA_ATTR int measurments_stored = 0;

RTC_DATA_ATTR float min_voltage = 30;
RTC_DATA_ATTR float max_voltage = 0;


//       low voltage       average voltage     high voltage
// (crit_low low low_avg) (low_avg avg high) (high upper_high)

//voltage thresholds
float crit_low; // 0%
float low; // 10%

float low_avg; // 20%
float avg; // 30%

float high; // 40%
float upper_high; // 100%

////voltage thresholds 12V
//crit_low   = 10.50; // 0%
//low        = 11.31; // 10%
//
//low_avg    = 11.58; // 20%
//avg        = 11.75; // 30%
//
//high       = 11.90; // 40%
//upper_high = 12.60; // 100%

//voltage thresholds 24V
//crit_low   = 22.00; // 0%
//low        = 22.6; // 10%
//
//low_avg    = 23.10; // 20%
//avg        = 23.50; // 30%
//
//high       = 23.80; // 40%
//upper_high = 25.20; // 100%

Preferences preferences;

const char* ssid       = "OpenWRT";
const char* password   = "1357924680";

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 3600;
const char* time_zone = "";

time_t now;                         // this is the epoch
tm myTimeInfo;

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return(0);
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  time(&now);
  return now;
}

void synchroniseWith_NTP_Time() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("configTime uses ntpServer ");
  Serial.println(ntpServer1);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1);
  Serial.print("synchronising time");
  
  while (myTimeInfo.tm_year + 1900 < 2000 ) {
    time(&now);                       // read the current time
    localtime_r(&now, &myTimeInfo);
    delay(100);
    Serial.print(".");
  }
  Serial.print("\n time synchronsized \n");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

int playMelody(int melody[], int noteDuration){
  for (int i = 0; i < sizeof(melody) / sizeof(melody[0]); i++)
  {
    tone(Pin_buzzer, melody[i], noteDuration);
    delay(noteDuration);
  }
  noTone(Pin_buzzer);
  return 0;
}
int playSuccessMelody(){ 
  int successMelody[] = {262, 330, 392, 523, 392, 330, 262, 392, 523, 659, 523, 392, 262, 659, 784, 880};
  int noteDuration = 400; 
  playMelody(successMelody, noteDuration);    
  return 0; 
}
int playErrorMelody(){
  int errorMelody[] = {262, 196, 196, 262, 330, 247, 247, 330};
  int noteDuration = 400;
  playMelody(errorMelody, noteDuration);
  return 0;
}

int playMelody_(){
  const int nNotes = 8;
  String notes[nNotes] = { "C4", "SILENCE", "C4", "SILENCE", "C4", "SILENCE", "C4", "SILENCE"};
  const int timeUnit = 175;
  // create a melody
  Melody melody = MelodyFactory.load("Nice Melody", timeUnit, notes, nNotes);
  player.playAsync(melody);
  return 0;
}

void displayInfo(){
  drawVoltage(String(loadvoltage)+" V");
  delay(5000);
}

int getIna219Data(){
  shuntvoltage = 0;
  busvoltage = 0;
  current_mA = 0;
  loadvoltage = 0;
  power_mW = 0;

  shuntvoltage = VoltageSensor.getShuntVoltage_mV();
  busvoltage = VoltageSensor.getBusVoltage_V();
  current_mA = VoltageSensor.getCurrent_mA();
  power_mW = VoltageSensor.getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);

  if (loadvoltage<low_avg){
    playMelody_();
    digitalWrite(Pin_LedRed, HIGH);
    delay(1000);
    digitalWrite(Pin_LedRed, LOW);
    displayInfo();
  }
  else if ((loadvoltage>=low_avg)&&(loadvoltage<high)){
    digitalWrite(Pin_LedYellow, HIGH);
    delay(1000);
    digitalWrite(Pin_LedYellow, LOW);
//    displayInfo();
  }
  else if (loadvoltage>=high){
    digitalWrite(Pin_LedGreen, HIGH);
    delay(1000);
    digitalWrite(Pin_LedGreen, LOW);
//    displayInfo();
  }
  if (loadvoltage>max_voltage){ max_voltage = loadvoltage; }
  if (loadvoltage<min_voltage){ min_voltage = loadvoltage; }

  Serial.print("Bus Voltage:   ");
  Serial.print(busvoltage);
  Serial.println(" V");
  Serial.print("Shunt Voltage: ");
  Serial.print(shuntvoltage);
  Serial.println(" mV");
  Serial.print("Load Voltage:  ");
  Serial.print(loadvoltage);
  Serial.println(" V");
  Serial.print("Current:       ");
  Serial.print(current_mA);
  Serial.println(" mA");
  Serial.print("Power:         ");
  Serial.print(power_mW);
  Serial.println(" mW");
  Serial.println("");

  return 0;
}
int setLedRainbow(){

  int ledPins[] = {Pin_LedGreen, Pin_LedRed, Pin_LedYellow};

  for (int i = 0; i < 3; i++)
  {
    digitalWrite(ledPins[i], HIGH);
    delay(200);
    digitalWrite(ledPins[i], LOW);
    delay(200);
  }

  for (int i = 2; i >= 0; i--)
  {
    digitalWrite(ledPins[i], HIGH);
    delay(200);
    digitalWrite(ledPins[i], LOW);
    delay(200);
  }
}

int drawLoadingBar(int percentage){
  display.clearDisplay(); // Очистка дисплея

  int x = 2;
  int y = 12;
  int width = 124;
  int height = 8;
  int fillWidth = map(percentage, 0, 100, 0, width);
  display.drawRect(x, y, width, height, WHITE);
  display.fillRect(x, y, fillWidth, height, WHITE);

  display.display();

  return 0;
}

int drawVoltage(String voltage){     
  display.clearDisplay();
  display.setCursor(10, 5);
  display.println(("Current: "+voltage).c_str());

  display.setCursor(10, 15);
  display.println(("Min: "+String(min_voltage)+" V").c_str());

  display.setCursor(10, 25);
  display.println(("Max: "+String(max_voltage)+" V").c_str());
  
  display.display();
  return 0;
}

//скорее всего минимальным будет первые в keys ключ, но на всякий лучше пока проверка
void write_data(){
  preferences.begin("battery", false);
  unsigned long time_now = getTime();
  char cstr[11];
  itoa(time_now, cstr, 10);
  preferences.putFloat(cstr, loadvoltage);
//  float a = preferences.getFloat(cstr);
  measurments_stored++;

  if (preferences.isKey("keys")) {
    size_t schLen = preferences.getBytes("keys", NULL, NULL);
    char buffer[schLen];
    preferences.getBytes("keys", buffer, schLen);
    unsigned long *key_array = (unsigned long *) buffer;
    int key_len = sizeof(buffer) / sizeof(unsigned long);
    if (measurments_stored != key_len) {measurments_stored = key_len;}
    unsigned long new_key_array[key_len+1];
    for (int i=0; i<key_len; i++){
      new_key_array[i] = key_array[i];
    }
    new_key_array[key_len] = time_now;
    preferences.putBytes("keys", &new_key_array, sizeof(new_key_array));
  }
  else {
    measurments_stored=1;
    unsigned long key_array[] = {time_now};
    preferences.putBytes("keys", &key_array, sizeof(key_array));
  }


  while (measurments_stored>124){
//  while (measurments_stored>10){
    unsigned long min_key = 4294967295;
    int min_i = 0;
    size_t schLen = preferences.getBytes("keys", NULL, NULL);
    char buffer[schLen];
    preferences.getBytes("keys", buffer, schLen);
    unsigned long *key_array = (unsigned long *) buffer;
    int key_len = sizeof(buffer) / sizeof(unsigned long);
    for (int i=0; i<key_len; i++){
      unsigned long current_key = key_array[i];
      if (current_key<min_key){
        min_key = current_key;
        min_i = i;
      }
    }
    char remove_cstr[11];
    itoa(min_key, remove_cstr, 10);
    preferences.remove(remove_cstr);

    unsigned long new_key_array[key_len-1];
    for (int i=0; i<min_i; i++){
      new_key_array[i] = key_array[i];
    }
    for (int i=min_i; i<key_len-1; i++){
      new_key_array[i] = key_array[i+1];
    }
    preferences.putBytes("keys", &new_key_array, sizeof(new_key_array));
    measurments_stored--;    
  }  
  preferences.end();
}

void printMemory(){
  preferences.begin("battery", false);
  if (preferences.isKey("keys")) {
    size_t schLen = preferences.getBytes("keys", NULL, NULL);
    char buffer[schLen];
    preferences.getBytes("keys", buffer, schLen);
    unsigned long *key_array = (unsigned long *) buffer;
    int key_len = sizeof(buffer) / sizeof(unsigned long);
    Serial.printf("key_len=%d : ",key_len);
    for (int i=0; i<key_len; i++){
      char cstr[11];
      itoa(key_array[i], cstr, 10);
      Serial.print(preferences.getFloat(cstr));
      Serial.print(" ");
    }
  }
  Serial.println("");
  preferences.end();
}

void printChart(){
  preferences.begin("battery", false);
  if (preferences.isKey("keys")) {
    size_t schLen = preferences.getBytes("keys", NULL, NULL);
    char buffer[schLen];
    preferences.getBytes("keys", buffer, schLen);
    unsigned long *key_array = (unsigned long *) buffer;
    int key_len = sizeof(buffer) / sizeof(unsigned long);
    
    display.clearDisplay();
    int i=0;
    char key_i[11];
    do{
      itoa(key_array[i], key_i, 10);

      int voltage = preferences.getFloat(key_i);
      int vol_time = key_array[i]-key_array[0];
      
      int x = 2;
//      int y = 2;
      int width = 124;
      int height = 28;
      int volHeight;
      if (voltage<11){ volHeight = 1; }
      else { volHeight = map(voltage, min_voltage, max_voltage, 0, height); }
//      Serial.println("---");
//      Serial.println(volHeight);
//      Serial.println(height+1-volHeight);
//      Serial.println("---");
      int pointWidth = i+1;
      display.drawLine(x+pointWidth, 28, x+pointWidth, height+1-volHeight, WHITE);
        
      i++;
    } while (i<key_len && preferences.getFloat(key_i)>0);
    display.display();
  }
  preferences.end();
}

void checkWork(){
  for (int percentage = 0; percentage <= 100; percentage++)
  {
    drawLoadingBar(percentage);
    delay(10);
  }

  playSuccessMelody();
  delay(2000);  
  playErrorMelody();
  digitalWrite(Pin_LedGreen, HIGH);
  digitalWrite(Pin_LedRed, HIGH);
  digitalWrite(Pin_LedYellow, HIGH);
  delay(1000);
  digitalWrite(Pin_LedGreen, LOW);
  digitalWrite(Pin_LedRed, LOW);
  digitalWrite(Pin_LedYellow, LOW);
//  playMelody_();
//  delay(1000);
}

void setup(){
  Serial.begin(115200);
  pinMode(Pin_buzzer, OUTPUT);
  pinMode(Pin_ds18b20, INPUT);
  pinMode(Pin_LedGreen, OUTPUT);
  pinMode(Pin_LedRed, OUTPUT);
  pinMode(Pin_LedYellow, OUTPUT);
  pinMode(Pin_FreeButton, INPUT);

  synchroniseWith_NTP_Time();

  ds18.begin();
  Wire.begin(Pin_i2c_sda, Pin_i2c_scl);

  display = Adafruit_SSD1306(128, 32, &Wire);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  checkWork();

  if (!VoltageSensor.begin(&Wire))
  {
    Serial.print("INA219 - Failed to find INA219 chip");
    while (1)
    {
      delay(10);
    }
  }

  getIna219Data();
  if (loadvoltage>20){
    //voltage thresholds 24V
    crit_low   = 22.00; // 0%
    low        = 22.6; // 10%
    
    low_avg    = 23.10; // 20%
    avg        = 23.50; // 30%
    
    high       = 23.80; // 40%
    upper_high = 25.20; // 100%
  }
  else {
    //voltage thresholds 12V
    crit_low   = 10.50; // 0%
    low        = 11.31; // 10%
    
    low_avg    = 11.58; // 20%
    avg        = 11.75; // 30%
    
    high       = 11.90; // 40%
    upper_high = 12.60; // 100%
  }
}

void loop(){
  gpio_wakeup_enable(GPIO_NUM_10, GPIO_INTR_HIGH_LEVEL);    // Enable wakeup on high-level on GPIO 4
  esp_sleep_enable_gpio_wakeup();
//  esp_sleep_enable_timer_wakeup(3000000); //3sec
  esp_sleep_enable_timer_wakeup(300000000); //5min = 300000000
    
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == 7){
    displayInfo();
    delay(1000);
    printChart();
    delay(2000);
    display.clearDisplay();
    display.display();
  }
  else {
    getIna219Data();
    write_data();
    printMemory();
    preferences.begin("battery", false);
    if (preferences.isKey("keys")) {
      size_t schLen = preferences.getBytes("keys", NULL, NULL);
      char buffer[schLen];
      preferences.getBytes("keys", buffer, schLen);
      unsigned long *key_array = (unsigned long *) buffer;
      int key_len = sizeof(buffer) / sizeof(unsigned long);
      for (int i=0; i<key_len; i++){
        Serial.print(key_array[i]);
        Serial.print(" ");
      }
      Serial.println("");
    }
    preferences.end();
//    for (byte i = 0; i < measurmentStorage.size() - 1; i++) {
//      Serial.print(measurmentStorage[i]);
//      Serial.print(" ");
//    }
    delay(1000);
  }
//  playErrorMelody();
//  while(1){
//   getIna219Data();
//   handleButtonPress();
//   delay(1000);
//  }

  display.clearDisplay();
  display.display();
  esp_light_sleep_start();
}
