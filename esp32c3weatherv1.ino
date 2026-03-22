#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <Wire.h>

// OLED declarations
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


//---------------------------------------------------------------------------------------------------------------------------------------
// Replace with your wifi credentials
const char* ssid = "replace_with_your_ssid";
const char* password = "replace_with_your_pass";

// change gpio pin for the touch switch if you want to use any other pin
#define TOUCH_PIN 6

// Replace with your coordinates
String lat = "replace_with_your_latitude";
String lon = "replace_with_your_longitude";

// Replace with your openweathermap API key
String apiKey = "replace_with_your_apikey";

// NTP declarations, only change the gmtOffset_sec according to your country
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // +5:30
const int daylightOffset_sec = 0;

bool use24HourFormat = false; // true = 24hr, false = 12hr

unsigned long weatherInterval = 300000;  // How often the Api should be called, here 5min
const long screenInterval = 5000;   // How often should the display change, here 5 sec
//---------------------------------------------------------------------------------------------------------------------------------------


// To keep track of the events
unsigned long lastWeatherFetch = 0;
unsigned long lastScreenChange = 0;
unsigned long lastDisplayUpdate = 0;
const int displayInterval = 500; // display refreshes after 
int screenIndex = 0;

// Variables to store weather info
float temp, feels, tempMin, tempMax;
int humidity, pressure;
float windSpeed, windGust;
int windDeg;
int visibility;
String weatherMain, weatherDesc;
String city, country;
int clouds;
unsigned long sunrise;
unsigned long sunset;
float ifRainorSnow;

// These are few icons I planned to use but did't use as of now, maybe in future, needs impovements(use bitmaps)
void drawSun() { display.fillCircle(100, 20, 8, WHITE); }
void drawCloud() {
  display.fillCircle(95, 20, 6, WHITE);
  display.fillCircle(105, 20, 6, WHITE);
  display.fillRect(95, 20, 16, 8, WHITE);
}
void drawRain() {
  drawCloud();
  display.drawLine(98, 32, 98, 38, WHITE);
  display.drawLine(104, 32, 104, 38, WHITE);
}
void drawWeatherIcon(String cond) {
  cond.toLowerCase();
  if (cond.indexOf("clear") >= 0) drawSun();
  else if (cond.indexOf("rain") >= 0) drawRain();
  else drawCloud();
}

// Function to fetch the weather info..
void fetchWeather() {
  HTTPClient http;

  String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + lat +
               "&lon=" + lon + "&appid=" + apiKey + "&units=metric";

  http.begin(url);
  int code = http.GET();

  if (code > 0) {
    String payload = http.getString();
    Serial.println(payload);

    DynamicJsonDocument doc(4096); // its like allocating 4kb to store the json data
    deserializeJson(doc, payload);
    
    // the following are the values being stored
    temp = doc["main"]["temp"];
    feels = doc["main"]["feels_like"];
    tempMin = doc["main"]["temp_min"]; 
    tempMax = doc["main"]["temp_max"]; 
    humidity = doc["main"]["humidity"];
    pressure = doc["main"]["pressure"];

    windSpeed = doc["wind"]["speed"];
    windDeg = doc["wind"]["deg"];
    windSpeed = doc["wind"]["gust"];

    visibility = doc["visibility"];
    clouds = doc["clouds"]["all"];

    weatherMain = doc["weather"][0]["main"].as<String>();
    weatherDesc = doc["weather"][0]["description"].as<String>();

    city = doc["name"].as<String>();
    country = doc["sys"]["country"].as<String>();

    sunrise = doc["sys"]["sunrise"];
    sunset  = doc["sys"]["sunset"];
    

    // this is cuz always ["rain"]["1h"] or "snow"]["1h"] isn't available in the json if the location doesn't experience it
    if(weatherMain == "Rain"){
      ifRainorSnow = doc["rain"]["1h"];
    }else if(weatherMain == "Snow"){
      ifRainorSnow = doc["snow"]["1h"];
    }else{
      ifRainorSnow = 00.00;
    }


  } else {
    Serial.println("Weather fetch failed");
  }

  http.end();
}

//old fxn i used previously
// Function to get ntp time
// String getTimeString() {
//   struct tm timeinfo;
//   if (!getLocalTime(&timeinfo)) return "Time Err";

//   char buffer[20];

//   if (use24HourFormat) {
//     strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
//   } else {
//     strftime(buffer, sizeof(buffer), "%I:%M:%S%p", &timeinfo);
//   }
//   return String(buffer);
// }
// // Function to format ntp date
// String getDateString() {
//   struct tm timeinfo;
//   if (!getLocalTime(&timeinfo)) return "Date Err";

//   char buffer[15];
//   strftime(buffer, sizeof(buffer), "%d/%m/%y", &timeinfo);

//   return String(buffer);
// }
// // Function to get day of the week
// String getDayString() {
//   struct tm timeinfo;
//   if (!getLocalTime(&timeinfo)) return "Day Err";

//   char buffer[15];
//   strftime(buffer, sizeof(buffer), "%A", &timeinfo);

//   return String(buffer);
// }


void displayDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Time Err");
    return;
  }

  char timeBuffer[20];
  char dateBuffer[15];
  char dayBuffer[15];

  // Time
  if (use24HourFormat) {
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  } else {
    strftime(timeBuffer, sizeof(timeBuffer), "%I:%M:%S%p", &timeinfo);
  }

  // Date and Day
  strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%y", &timeinfo);
  strftime(dayBuffer, sizeof(dayBuffer), "%A", &timeinfo);

  // Display
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(timeBuffer);

  display.setTextSize(1);
  display.print(dayBuffer);
  display.print("    ");
  display.println(dateBuffer);
}

// function to convert unix time into readable format
String formatUnixTime(unsigned long unixTime) {
  const long TIME_OFFSET = gmtOffset_sec; // +5:30 

  time_t rawTime = unixTime + TIME_OFFSET;
  struct tm *timeinfo = gmtime(&rawTime);

  char buffer[20];

  if (use24HourFormat) {
    strftime(buffer, sizeof(buffer), "%H:%M", timeinfo);
  } else {
    strftime(buffer, sizeof(buffer), "%I:%M %p", timeinfo);
  }

  return String(buffer);
}

// basically maps the degree to corresponding direction
String getWindDirection(int deg) {
  const char* directions[] = {
    "North", "NE", "East", "SE",
    "South", "SW", "West", "NW"
  };

  int index = (deg + 22) / 45;  
  index = index % 8;

  return String(directions[index]);
}


// These are the screens which are being cycled upon
void drawScreen0() {
  // time, day, date, location
  displayDateTime();

  display.setTextSize(2);
  display.println("");
  display.print(city);

  //display.println(weatherMain);
  //drawWeatherIcon(weatherMain);
}

void drawScreen1() {
  displayDateTime();
  

  display.setTextSize(2);
  display.println("");
  display.print(temp, 1);
  display.println("C");

  display.setTextSize(1);
  display.print("Feels like: ");
  display.print(feels);
  display.println("C");

  // display.print("Min: "); display.print(tempMin);
  // display.print(" Max: "); display.println(tempMax);
}

void drawScreen2() {
  displayDateTime();


  display.setTextSize(1);
  display.println("");

  display.print("Humidity  : "); 
  display.print(humidity);
  display.println(" %");
  display.print("Pressure  : "); 
  display.print(pressure);
  display.println(" hPa");
  display.print("Cloudiness: "); 
  display.print(clouds);
  display.println(" %");
  display.print("Visibility: "); 
  display.print(visibility / 1000.0);
  display.println(" km");
}

void drawScreen3() {
  displayDateTime();


  display.setTextSize(1);
  display.println("");

  display.print("Wind speed: "); 
  display.print(windSpeed);
  display.println(" m/s");
  display.print("Wind Dirn : "); 
  display.println(getWindDirection(windDeg));
  display.print("Precip    : "); 
  display.print(ifRainorSnow);
  display.println(" mm/h");
  display.println(weatherDesc);
}

void drawScreen4() {
  displayDateTime();


  display.setTextSize(1);
  display.println("");
  display.println("");
  display.print("Sunrise: ");
  display.println(formatUnixTime(sunrise));
  display.print("Sunset : ");
  display.println(formatUnixTime(sunset));
}

void setup() {
  Serial.begin(115200);
  delay(500); // sometimes the serial monitor fails to load
  pinMode(TOUCH_PIN, INPUT);


  //Wire.begin(8, 9);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("could't begin the oled display..");
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);


  WiFi.begin(ssid, password);
  display.setCursor(0, 0);
  display.println("Connecting to WiFi...");
  display.display();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // display in serial monitor
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // display in oled
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi connected");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);  // intentionally
  display.clearDisplay();

  // NTP initialization
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  fetchWeather(); // fetch the weather at startup
  lastWeatherFetch = millis();
}

void loop() {
  // Weather refreshing condition
  if (millis() - lastWeatherFetch > weatherInterval) {
    fetchWeather();
    lastWeatherFetch = millis();
  }

  // Screen cycling every predefined sec
  if (millis() - lastScreenChange > screenInterval) {
    screenIndex = (screenIndex + 1) % 5;
    lastScreenChange = millis();
  }
 
  // to check touch button press/tap
  static bool lastTouchState = LOW;
  bool currentTouchState = digitalRead(TOUCH_PIN);

  if (lastTouchState == LOW && currentTouchState == HIGH) {
    screenIndex = (screenIndex + 1) % 5;
    lastScreenChange = millis(); // reset auto-cycle timer
  }

    lastTouchState = currentTouchState;

  // To prevent the display from refreshing continuously
  if (millis() - lastDisplayUpdate > displayInterval) {
    lastDisplayUpdate = millis();

    display.clearDisplay();

    switch (screenIndex) {
      case 0: drawScreen0(); break;
      case 1: drawScreen1(); break;
      case 2: drawScreen2(); break;
      case 3: drawScreen3(); break;
      case 4: drawScreen4(); break;
    }

    display.display();
  }
}