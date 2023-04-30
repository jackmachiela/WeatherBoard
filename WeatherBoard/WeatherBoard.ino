// WeatherBoard, code by Jack Machiela, 2023.
//

#include <WiFiManager.h>        // lib "WifiManager by tzapu" - Tested at v2.0.15-rc.1
#include <ezTime.h>             // lib "ezTime by Rop Gongrijp" - Tested at v0.8.3
#include <ESP8266HTTPClient.h>  // std lib. Make sure "Board" is set to your ESP8266 board (NodeMCU, Mini D1, or whatever you're using)
#include <ArduinoJson.h>        // lib "ArduinoJson by Benoit Blanchon" - Tested at v6.21.2
#include <Servo.h>              // lib "Servo by Michael Margolis, Arduino" - Tested at v1.1.8
#include <FastLED.h>            // lib "FastLED by Daniel Garcia" - Tested at v3.5.0

// Define some wibbley wobbley timey wimey stuff
String TimeZoneDB = "Pacific/Auckland";              // Olson format "Pacific/Auckland" - Posix: "NZST-12NZDT,M9.5.0,M4.1.0/3" (as at 2020-06-18)
                                                     // For other Olson codes, go to: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones 
String localNtpServer = "nz.pool.ntp.org";           // [Optional] - See full list of public NTP servers here: http://support.ntp.org/bin/view/Servers/WebHome
Timezone myTimeZone;                                 // Define this as a global so all functions have access to correct timezoned time

// Dannevirke, New Zealand = latitude=-40.21&longitude=176.10
String          LATITUDE = "-40.21";
String         LONGITUDE = "176.10";
String          TIMEZONE = "auto";              // "Pacific/Auckland";    works: "Pacific%2FAuckland"

// Set up the servos
Servo servoThermo;      // create servo object to control a servo
Servo servoForecast;    // create servo object to control a servo
Servo servoRain;        // create servo object to control a servo

#define pinServoThermo   D6   // Define which pin goes to which servo
#define pinServoForecast D7   // Define which pin goes to which servo
#define pinServoRain     D8   // Define which pin goes to which servo

const int dayButtonPin = D1;      // Define the day-selector button pin

int todayNumber;

bool dayButtonState = false;      // Define button state variables
bool dayButtonReleased = true;    
unsigned long dayButtonPressedMillis = 0;

// Define LED strip parameters
#define LED_PIN     D3
#define LED_COUNT   7
CRGB leds[LED_COUNT];

// prepare the array for the weatherforecast data
String json_array;

int thisDay = 0;
int weatherIcon;                       // For second servo (servoForecast); which weather icon to point at
int displayDay = 0;                    // indicates which day to display on the board
const int displayDays = 8;             // Number of days for the forecast. Actually (displayDays-1) : Day 0 = now, today as a whole = day 1, tomorrow = day 2, etc.

float  maxTemperature[displayDays];    // max temperatures for one week forecast
float  maxRainfall[displayDays];       // expected daily rainfall for one week forecast
int    wmoWeatherCode[displayDays];    // wmo weather code for one week forecast
String weatherDesc[displayDays];       // wmo weather descriptions for one week forecast (only used for debugging)

int weatherInterval = 1800000;                  // How often should the time be reset to NTP server (milliseconds).
                                                //Don't make it longer than an hour (3,600,000), as day 0 ("now") is this hour's worth.
                                                //Suggest every 30 mins (1800000)
unsigned long currentMillis =  weatherInterval;
unsigned long previousMillis = 0;


void setup() {
  Serial.begin(74880);                 // This weird looking serial speed is compatible with the native clock speed on NodeMCU ESP8266 units, 
                                       //   which allows me to see the hardware boot messages. If the POST messages are not needed, set it higher.

  WiFiManager wifiManager;             // Start a Wifi link
  wifiManager.autoConnect("WeatherBoard");         //   (on first use: will set up as a Wifi name; set up by selecting new Wifi network on iPhone for instance)

  setInterval(3000);                   // How often should the time be reset to NTP server (seconds)
  setServer(localNtpServer);           // [Optional] - Set the NTP server to the closest pool
  waitForSync();                       // Before you start the main loop, make sure the internal clock has synced at least once.
  myTimeZone.setLocation(TimeZoneDB);  // Set the Timezone
  
  myTimeZone.setDefault();             // Set the local timezone as the default time, so all time commands should now work local.

  // attach the servos on their pins to the servo objects
  servoThermo.attach(pinServoThermo, 500,5200);
  servoForecast.attach(pinServoForecast, 500,5200);
  servoRain.attach(pinServoRain, 500,5200);

  // Set up LED strip
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setBrightness(84);

  hardwarePOST();     // test the hardware and set them at zero point
  // Set pin mode for the button pin
  pinMode(dayButtonPin, INPUT_PULLUP);

}


void loop() {

  checkNtpStatus();                                                     // [Optional] temp, remove when done
  events();                        // Check if any scheduled ntp events, including NTP resyncs.
  
  if (currentMillis - previousMillis >= weatherInterval) {

    String MeteoAPI = SetupMeteoApi();    
    json_array = GET_Request(MeteoAPI.c_str());
    //Serial.print("json_array = ");
    //Serial.println(json_array);
    if (json_array == "{}") {
      Serial.println("Error getting new weather, reverting to previous forecast");
    } else {
    // save the last time the weather was checked
      previousMillis = currentMillis;
      JsonCONV();
      ShowForecastOnDisplay();

      SetWeatherIcon();

      SetServos();

    }

  }
  currentMillis = millis();

  // Read button state
  bool newDayButtonState = digitalRead(dayButtonPin) == LOW;

  // If button state has changed, record release event
  if (newDayButtonState != dayButtonState) {
    dayButtonState = newDayButtonState;
    if (dayButtonState) {
      // Button has been pressed, wait for bouncing to stop
      delay(50);
      if (digitalRead(dayButtonPin) == LOW) {
        // Button is still pressed after delay, register release event
        dayButtonReleased = true;
      }
    }
  }

  // Increment displayDay on button release and wrap around at 7
  if (dayButtonState && dayButtonReleased) {
    displayDay++;
    if (displayDay > 7) {
      displayDay = 0;
    }
    Serial.print("Button clicked. displayDay: ");
    Serial.print(displayDay);
    

    SetWeatherIcon();
    SetServos();

    dayButtonPressedMillis = millis();
    dayButtonReleased = false;
  }

  if (((millis() - dayButtonPressedMillis) > 15000) && (displayDay > 0 ))  {            // if it's been 15 seconds since the last button press,
                                                                                      // AND the current view is not day 0,
                                                                                      // reset back to hourly "now" forecast (day 0)

    Serial.println("No clicks for 15 seconds - Resetting to current forecast: ");
    displayDay = 0;
    SetWeatherIcon();
    SetServos();

  }

  // todayNumber = (UTC.dateTime("N").toInt());
  todayNumber = 4;

  for (int i = 0; i < LED_COUNT; i++) {
    int j = i + todayNumber - 1;
    if (j > 7) j = j - 6;

    Serial.print("DEBUG==  displayDay: ");
    Serial.print(displayDay);
    Serial.print("   : todayNumber : ");
    Serial.print(todayNumber);
    Serial.print("   : i : ");
    Serial.print(i);
    Serial.print("   : j : ");
    Serial.println(j);


    if (i == (displayDay-1)) {

    Serial.print("   : i : ");
    Serial.print(i);
    Serial.print("   : j : ");
    Serial.println(j);

      leds[j] = CRGB::Red;
    } else {
      leds[j] = CRGB::Black;
    }
  }

  FastLED.show();

}

void checkNtpStatus() {
  //Serial.println("Time: " + dateTime());                    // <---- prints UTC time          - https://time.is/UTC
  //if (timeStatus() == timeSet)       Serial.println("Status: Time Set.      ");
  if (timeStatus() == timeNotSet)    Serial.println("Status: Time NOT Set.  ");
  if (timeStatus() == timeNeedsSync) Serial.println("Status: Time needs Syncing.      ");
}

String SetupMeteoApi() {

     // create this here: https://api.open-meteo.com/v1/forecast?latitude=-40.21&longitude=176.11&hourly=precipitation&daily=weathercode,temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max&current_weather=true&timezone=auto
     String Api = "https://api.open-meteo.com/v1/forecast?latitude=(LAT)&longitude=(LONG)&hourly=precipitation&daily=weathercode,temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max&current_weather=true&timezone=(TMZ)";

    Api.replace("(LONG)", LONGITUDE);
    Api.replace("(LAT)", LATITUDE);
    Api.replace("(TMZ)", TIMEZONE);
    
    //Serial.println("-------------------------------- String to send to the server: --------------------------------");
    //Serial.println(Api);

    return Api;
    
}

String GET_Request(const char* server) {
     String payload = "{}"; 

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

    client->setInsecure();
    // Or, if you happy to ignore the SSL certificate, then use the following line instead:
    // client->setInsecure();

    HTTPClient https;

    //Serial.print("[HTTPS] begin...\n");
    //Serial.print(server);
    if (https.begin(*client, server)) {  // HTTPS

      //Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      int httpCode = https.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        //Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          payload = https.getString();
          //Serial.println("RECEIPT STRING: ");
          //Serial.println(payload);
        } else {
            Serial.print("NOT FOUND: ");
            Serial.println(httpCode);
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
    }

  return payload;
}


void JsonCONV() {

   DynamicJsonDocument doc(10000);
   DeserializationError error = deserializeJson(doc, json_array);

  //    time_t Today_t =  now();
  //    time_t Hour = hour(Today_t);
    
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    //} else {
    //    Serial.println("Deserialization OK");
    }

  
    // TEMPERATURE UNIT
  //  JsonObject daily_units = doc["daily_units"];
  //  const String TempUnit = daily_units["temperature_2m_max"]; // "°C"

   // WEATHERCODE
    JsonObject daily = doc["daily"];
    
    // First, set the current conditions (array item [0])
    wmoWeatherCode[0] =   int(doc["current_weather"]["weathercode"]);
    maxTemperature[0] = float(doc["current_weather"]["temperature"]);
    maxRainfall[0]    = float(doc["hourly"]["precipitation"][0]);

    // next, set array items [1-x] to the forecast 
    for (int i = 1; i < (displayDays); i++) {
      wmoWeatherCode[i] = int(daily["weathercode"][i-1]);
      maxTemperature[i] = float(daily["temperature_2m_max"][i-1]);
      maxRainfall[i] = float(daily["precipitation_sum"][i]);
    }
    
}

String translateMeteoCode(int i){           // convert meteo code to track number           // see also: https://www.jodc.go.jp/data_format/weather-code.html
  String weatherDescription = "none";
  if(i==0)  { weatherDescription = "Clear sky"; }
  if(i==1)  { weatherDescription = "Mainly Clear"; }
  if(i==2)  { weatherDescription = "Partly Cloudy"; }
  if(i==3)  { weatherDescription = "Overcast"; }
  if(i==45) { weatherDescription = "Fog"; }
  if(i==48) { weatherDescription = "Depositing Rime Fog"; }
  if(i==51) { weatherDescription = "Drizzle: Light"; }
  if(i==53) { weatherDescription = "Drizzle: Moderate"; }
  if(i==55) { weatherDescription = "Drizzle: Dense"; }
  if(i==56) { weatherDescription = "Freezing Drizzle Light"; }
  if(i==57) { weatherDescription = "Freezing Drizzle Dense"; }
  if(i==61) { weatherDescription = "Rain: Slight"; }
  if(i==63) { weatherDescription = "Rain: Moderate"; }
  if(i==65) { weatherDescription = "Rain: Heavy"; }
  if(i==71) { weatherDescription = "Snow Fall: Slight, "; }
  if(i==73) { weatherDescription = "Snow Fall: Moderate"; }
  if(i==75) { weatherDescription = "Snow Fall: Heavy"; }
  if(i==77) { weatherDescription = "Snow Grains"; }
  if(i==80) { weatherDescription = "Rain Showers: Slight"; }
  if(i==81) { weatherDescription = "Rain Showers: Moderate"; }
  if(i==82) { weatherDescription = "Rain Showers: Violent"; }
  if(i==85) { weatherDescription = "Snow Showers: Slight"; }
  if(i==86) { weatherDescription = "Snow Showers: heavy"; }
  if(i==95) { weatherDescription = "Thunderstorm: Slight or moderate"; }
  if(i==96) { weatherDescription = "Thunderstorm with slight hail"; }
  if(i==99) { weatherDescription = "Thunderstorm with heavy hail"; }
  
  return weatherDescription;

}


void ShowForecastOnDisplay() {

  Serial.println("");
  Serial.println("===================== The Forecast =====");

  for (int thisDay = 0; thisDay < (displayDays); thisDay++) {
    weatherDesc[thisDay] = translateMeteoCode(wmoWeatherCode[thisDay]);

    Serial.print("[Day ");
    Serial.print(thisDay);
    Serial.print("]");

    Serial.print(" - ");

    Serial.print("[Temperature: ");
    Serial.print(maxTemperature[thisDay]);
    Serial.print("°C]");

    Serial.print(" - ");
    
    Serial.print("[Precipitation: ");
    Serial.print(maxRainfall[thisDay]);
    Serial.print(" mm]");

    Serial.print(" - ");
    
    Serial.print("[Weather Code ");
    if (wmoWeatherCode[thisDay]<10) Serial.print("0");
    Serial.print(wmoWeatherCode[thisDay]);
    Serial.print(" <");
    Serial.print(weatherDesc[thisDay]);
    Serial.println(">");

  }

  Serial.println("===================== Forecast Ends ====");
  Serial.println("");

}

void SetWeatherIcon(){
  // The WeatherIcon determines what the second servo is pointing at. There's 11 settings; 6 icons and 5 in-between points.
  // The nwo weather code determines which icon to point at.
  // The six icons
  if (wmoWeatherCode[displayDay] >= 00 && wmoWeatherCode[displayDay] <=  00 ) weatherIcon =  1 ;    // (01) Sunshine
  if (wmoWeatherCode[displayDay] >= 01 && wmoWeatherCode[displayDay] <=  01 ) weatherIcon =  2 ;    // (02) (no icon - in between)
  if (wmoWeatherCode[displayDay] >= 02 && wmoWeatherCode[displayDay] <=  02 ) weatherIcon =  3 ;    // (03) Sunshine with cloudy periods
  if (wmoWeatherCode[displayDay] >= 03 && wmoWeatherCode[displayDay] <=  23 ) weatherIcon =  4 ;    // (04) (no icon - in between)
  if (wmoWeatherCode[displayDay] >= 04 && wmoWeatherCode[displayDay] <=  39 ) weatherIcon =  5 ;    // (05) Overcast
  if (wmoWeatherCode[displayDay] >= 40 && wmoWeatherCode[displayDay] <=  49 ) weatherIcon =  6 ;    // (06) (no icon - in between)
  if (wmoWeatherCode[displayDay] >= 50 && wmoWeatherCode[displayDay] <=  58 ) weatherIcon =  7 ;    // (07) Mild showers
  if (wmoWeatherCode[displayDay] >= 62 && wmoWeatherCode[displayDay] <=  62 ) weatherIcon =  7 ;    // (07) Mild showers
  if (wmoWeatherCode[displayDay] >= 59 && wmoWeatherCode[displayDay] <=  61 ) weatherIcon =  8 ;    // (08) (no icon - in between)
  if (wmoWeatherCode[displayDay] >= 66 && wmoWeatherCode[displayDay] <=  67 ) weatherIcon =  8 ;    // (08) (no icon - in between)
  if (wmoWeatherCode[displayDay] >= 63 && wmoWeatherCode[displayDay] <=  65 ) weatherIcon =  9 ;    // (09) Hard Rains
  if (wmoWeatherCode[displayDay] >= 68 && wmoWeatherCode[displayDay] <=  69 ) weatherIcon =  9 ;    // (09) Hard Rains
  if (wmoWeatherCode[displayDay] >= 70 && wmoWeatherCode[displayDay] <=  94 ) weatherIcon = 10 ;    // (10) (no icon - in between)
  if (wmoWeatherCode[displayDay] >= 95 && wmoWeatherCode[displayDay] <=  99 ) weatherIcon = 11 ;    // (11) Thunderstorms
 
 Serial.print("Icon = ");
 Serial.println(weatherIcon);
}

void SetServos(){
    Serial.println("");
    Serial.print("SERVOS: [ ");
    Serial.print(map(maxTemperature[displayDay], 0, 30, 150, 3));
    Serial.print(" ] - [ ");
    Serial.print(map(weatherIcon, 1, 11, 150, 3));
    Serial.print(" ] - [ ");
    Serial.print(map(maxRainfall[displayDay], 0, 10, 150, 3));
    Serial.println(" ] ");

  servoThermo.write(map(maxTemperature[displayDay], 0, 30, 150, 3));              // tell servo to go to position (0C to 30C)
  servoForecast.write(map(weatherIcon, 1, 11, 150, 3));                           // tell servo to go to position (11 weather icons)
  servoRain.write(map(maxRainfall[displayDay], 0, 10, 150, 3));                   // tell servo to go to position (0mm - 10mm)


}

void hardwarePOST(){                    //basic Power On Self Test for the hardware (LED strip and servos)

  //Sweep servos to get them started
  servoThermo.write(3);              // tell servo to go to position
  servoForecast.write(3);                           // tell servo to go to position
  servoRain.write(3);                   // tell servo to go to position

  LEDsweep();

  //delay(1000);
  servoThermo.write(150);              // tell servo to go to position
  servoForecast.write(150);                           // tell servo to go to position
  servoRain.write(150);                   // tell servo to go to position
  LEDsweep();
  //delay(1000);


}

void LEDsweep() { 
	for(int i = 0; i < LED_COUNT; i++) {                 	// First slide the led in one direction
    FastLED.clear();               // Set all LEDs to off
    leds[i] = CRGB::Red;           // Set the current LED to red
    FastLED.show();                // Show the LED colors
    delay(50);                     // Wait 50 milliseconds
	}
	for(int i = (LED_COUNT)-1; i >= 0; i--) {           	// Now go in the other direction.  
    FastLED.clear();               // Set all LEDs to off
    leds[i] = CRGB::Blue;           // Set the current LED to red
    FastLED.show();                // Show the LED colors
    delay(50);                     // Wait 50 milliseconds
	}
  FastLED.clear();                 // and finally, clear them all again
}
