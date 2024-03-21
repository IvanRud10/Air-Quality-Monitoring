#include <Wire.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_BME680.h"
#include "SparkFunCCS811.h" 

#define CCS811_ADDR 0x5B //Default I2C Address for ccs811
#define SEALEVELPRESSURE_HPA (1020) //sea level pressure
#define WINDOW_SIZE 10
const int buzzer = 9;

Adafruit_BME680 bme; // I2C
CCS811 ccs(CCS811_ADDR); // I2C

int   temp, hum, co2, gas, tvoc, pres, iaq, alt;
int   humidity_score, gas_score;
float gas_reference = 2500;
float hum_reference = 40;
int   getgasreference_count = 0;
int   gas_lower_limit = 10000;  // Bad air quality limit
int   gas_upper_limit = 300000; // Good air quality limit
int   avgTemperature, avgHumidity, avgCO2, avgTVOC, avgIAQ;
int   temperatureData[WINDOW_SIZE];
int   humidityData[WINDOW_SIZE];
int   co2Data[WINDOW_SIZE];
int   tvocData[WINDOW_SIZE];
int   iaqData[WINDOW_SIZE];
int   dataIndex = 0;

void setup() {
  Serial.begin(9600);
  pinMode(buzzer, OUTPUT);

  Wire.begin(); //Inialize I2C Hardware
  if (!bme.begin()) {
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
    while (1);
  }
  if (!ccs.begin()) {
    Serial.println("Could not find a valid CCS811 sensor, check wiring!");
    while(1);
  } 
  GetGasReference();

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
}

void loop() {
  digitalWrite(buzzer,HIGH);
  // Read data from sensors
  humidity_score = GetHumidityScore();
  gas_score      = GetGasScore();
  float air_quality_score = humidity_score + gas_score;
  iaq = CalculateIAQ(air_quality_score);
  co2 = ccs.getCO2();
  tvoc = ccs.getTVOC();
  gas = bme.gas_resistance / 1000.0;
  temp = bme.temperature;
  hum = bme.humidity;
  pres = bme.pressure / 100.0;
  alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
  
  // Insert data into arrays
  temperatureData[dataIndex] = temp;
  humidityData[dataIndex] = hum;
  co2Data[dataIndex] = co2;
  tvocData[dataIndex] = tvoc;
  iaqData[dataIndex] = iaq;
  dataIndex++;
   
  if (dataIndex >= WINDOW_SIZE) {
    dataIndex = 0;
  // Calculate moving averages
    avgTemperature = calculateAverage(temperatureData, WINDOW_SIZE);
    avgHumidity = calculateAverage(humidityData, WINDOW_SIZE);
    avgCO2 = calculateAverage(co2Data, WINDOW_SIZE);
    avgTVOC = calculateAverage(tvocData, WINDOW_SIZE);
    avgIAQ = calculateAverage(iaqData, WINDOW_SIZE);
  
  if ((getgasreference_count++) % 5 == 0) GetGasReference();
  ccs.readAlgorithmResults();
  
  Serial.print("iaqV.val=");
  Serial.print(avgIAQ);
  // each command ends with these 3 0xff to sent data to the Nextion display
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  Serial.print("co2V.val=");
  Serial.print(avgCO2);
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);
       
  Serial.print("tvocV.val=");
  Serial.print(avgTVOC);
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  Serial.print("tempV.val=");
  Serial.print(avgTemperature);
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);
  
  Serial.print("presV.val=");
  Serial.print(pres);
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  Serial.print("humV.val=");
  Serial.print(avgHumidity);
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);

  Serial.print("altV.val=");
  Serial.print(alt);
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);
  }

// buzzer
  if (iaq > 300 | co2 > 2500 | tvoc > 1000) // hazardous values
  {
  delay(300);
  digitalWrite(buzzer,HIGH);
  noTone(buzzer);
  tone(buzzer,250,300);
  delay(300);
  digitalWrite(buzzer,LOW);
  noTone(buzzer);
  tone(buzzer,350,300);
  delay(200);
  digitalWrite(buzzer,HIGH);
  delay(50);
  digitalWrite(buzzer,HIGH);
  delay(50);
  noTone(buzzer);
  digitalWrite(buzzer,HIGH);
  }
}

void GetGasReference() {
  int readings = 10;
  for (int i = 1; i <= readings; i++) { // read gas for 10 x 0.150mS = 1.5secs
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
}

int GetHumidityScore() {  
  //Calculate humidity contribution to IAQ index
  float current_humidity = bme.readHumidity();
  if (current_humidity >= 38 && current_humidity <= 42) // Humidity +/-5% around optimum
    humidity_score = 0.25 * 100;
  else
  { // Humidity is sub-optimal
    if (current_humidity < 38)
      humidity_score = 0.25 / hum_reference * current_humidity * 100;
    else
    {
      humidity_score = ((-0.25 / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
    }
  }
  return humidity_score;
}

int GetGasScore() {
  //Calculate gas contribution to IAQ index
  gas_score = (0.75 / (gas_upper_limit - gas_lower_limit) * gas_reference - (gas_lower_limit * (0.75 / (gas_upper_limit - gas_lower_limit)))) * 100.00;
  if (gas_score > 75) gas_score = 75; // Sometimes gas readings can go outside of expected scale maximum
  if (gas_score <  0) gas_score = 0;  // Sometimes gas readings can go outside of expected scale minimum
  return gas_score;
}
int CalculateIAQ(int score) {
  score = (100 - score) * 5;
  /*if      (score >= 301)                IAQ_text += "Hazardous";
  else if (score >= 201 && score <= 300 ) IAQ_text += "Unhealthy";
  else if (score >= 176 && score <= 200 ) IAQ_text += "Sensitive";
  else if (score >=  51 && score <= 175 ) IAQ_text += "Moderate";
  else if (score >=  00 && score <=  50 ) IAQ_text += "Good";*/
  return score;
}

int calculateAverage(int arr[], int len) {
  long sum = 0; // Use long to avoid overflow before dividing
  for (int i = 0; i < len; i++) {
    sum += arr[i];
  }
  return (int) (sum / len); // Cast the result to int
}
