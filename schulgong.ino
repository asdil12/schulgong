/*
 * SyncProvider.ino
 * example code illustrating time synced from a DCF77 receiver
 * Thijs Elenbaas, 2012-2017
 * This example code is in the public domain.

  This example shows how to fetch a DCF77 time and synchronize
  the internal clock. In order for this example to give clear output,
  make sure that you disable logging  from the DCF library. You can 
  do this by commenting out   #define VERBOSE_DEBUG 1   in Utils.cpp. 
  
  NOTE: If you used a package manager to download the DCF77 library, 
  make sure have also fetched these libraries:

 * Time 
  
 */

#include "webpages.h"

#include "DCF77.h"       //https://github.com/thijse/Arduino-Libraries/downloads
#include <TimeLib.h>     //http://www.arduino.cc/playground/Code/Time

#include "Audio.h"

// https://microcontrollerslab.com/microsd-card-esp32-arduino-ide/
// https://randomnerdtutorials.com/esp32-web-server-microsd-card/
// https://github.com/me-no-dev/ESPAsyncWebServer#file-upload-handling
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// audio
// https://github.com/schreibfaul1/ESP32-audioI2S

// for webui
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"


#define DCF_PIN 13	         // Connection pin to DCF 77 device
#define DCF_INTERRUPT 13		 // Interrupt number associated with pin

#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

const char* ssid = "Schulgong";

time_t prevDisplay = 0;          // when the digital clock was displayed
time_t lastSync = 0;
String last_timestamp = "";


DCF77 DCF = DCF77(DCF_PIN, DCF_INTERRUPT);
DNSServer dnsServer;
AsyncWebServer server(80);
Audio audio;


time_t getDCFTime() { 
  time_t DCFtime = DCF.getTime();
  // Indicator that a time check is done
  if (DCFtime!=0) {
    lastSync = DCFtime;
    Serial.println("Time synced from DCF77");
  }
  return DCFtime;
}

void init_dcf77_sync() {
  DCF.Start();
  setSyncInterval(30);
  setSyncProvider(getDCFTime);
  // It is also possible to directly use DCF.getTime, but this function gives a bit of feedback
  //setSyncProvider(DCF.getTime);

  Serial.println("Waiting for DCF77 time ... ");
  Serial.println("It will take at least 2 minutes until a first update can be processed.");
}

void init_sdcard() {
  if(!SD.begin()){
      Serial.println("Card Mount Failed");
      return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
      Serial.println("No SD card attached");
      return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
      Serial.println("MMC");
  } else if(cardType == CARD_SD){
      Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
      Serial.println("SDHC");
  } else {
      Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  // ensure sdcard directories
  SD.mkdir("/schedule");
  SD.mkdir("/scvolume");
  SD.mkdir("/sounds");
}

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return request->host() != WiFi.softAPIP().toString();
  }

  void handleRequest(AsyncWebServerRequest *request) {
    request->redirect("http://" + WiFi.softAPIP().toString() + "/");
  }
};

const char* get_time_status() {
  const char* time_status_str = NULL;
  switch(timeStatus()) {
    case timeNotSet:    time_status_str = "TIME NOT SET"; break;
    case timeNeedsSync: time_status_str = "TIME NEEDS SYNC"; break;
    case timeSet:       time_status_str = "TIME SET"; break;
    default:            time_status_str = "n/a"; break;
  }
  return time_status_str;
}

const char* get_sdcard_status() {
  const char* card_type_str = NULL;
  switch(SD.cardType()) {
    case CARD_NONE: card_type_str = "NONE"; break;
    case CARD_MMC:  card_type_str = "MMC"; break;
    case CARD_SD:   card_type_str = "SD"; break;
    case CARD_SDHC: card_type_str = "SDHC"; break;
    default:        card_type_str = "UNKNOWN"; break;
  }
  return card_type_str;
}

String read_file(String path){
  Serial.printf("Reading file: %s\n", path.c_str());
  File file = SD.open(path.c_str());
  return read_file(file);
}

String read_file(File file){
  String content;
  if(!file){
      Serial.println("Failed to open file for reading");
      return content;
  }

  while(file.available()){
      content += (char) file.read();
  }
  file.close();
  return content;
}

void play_audio(String timestamp) {
  String sound_file = read_file("/schedule/"+timestamp);
  if (sound_file.length() == 0) return;
  String sound_path = "/sounds/" + read_file("/schedule/"+timestamp);
  Serial.println("Soundfile: " + sound_path);
  String volume_str = read_file("/scvolume/"+timestamp);
  Serial.println("Volume: " + volume_str);
  audio.setVolume(atoi(volume_str.c_str()));
  audio.connecttoSD(sound_path.c_str());
}

String get_schedule_web() {
  String sched_tbl;
  File schedule_dir = SD.open("/schedule");
  if (!schedule_dir) return "ERROR: no schedule dir";
  if (!schedule_dir.isDirectory()) return "ERROR: schedule dir is not a dir";

  File file = schedule_dir.openNextFile();
  while(file) {
    String basename = file.name();
    basename = basename.substring(10, basename.length());
    String sound = read_file(file);
    String volume = read_file(String("/scvolume/")+basename);
    sched_tbl += "<tr><td>" + basename + "</td><td>" + sound + "</td><td>" + volume + "</td>";
    sched_tbl += "<td><button type=\"button\" onclick=\"play_audio('"+basename+"');\">Play</button>&nbsp;";
    sched_tbl += "<button type=\"button\" onclick=\"fill_sched_form('"+basename+"', '"+sound+"', '"+volume+"');\">Edit</button>&nbsp;";
    sched_tbl += "<button type=\"button\" onclick=\"del_sched('"+basename+"');\">Delete</button></tr>";
    file = schedule_dir.openNextFile();
  }
  return sched_tbl;
}

String get_sounds_web() {
  String sounds_tbl;
  File sounds_dir = SD.open("/sounds");
  if (!sounds_dir) return "ERROR: no sounds dir";
  if (!sounds_dir.isDirectory()) return "ERROR: sounds dir is not a dir";

  File file = sounds_dir.openNextFile();
  while(file) {
    String basename = file.name();
    basename = basename.substring(8, basename.length());
    sounds_tbl += "<tr><td><a href='/sdcard/sounds/"+basename+"'>" + basename + "</a></td><td>" + file.size() + "</td>";
    sounds_tbl += "<td><button type=\"button\" onclick=\"del_sound('"+basename+"');\">Delete</button></td></tr>";
    file = sounds_dir.openNextFile();
  }
  return sounds_tbl;
}

void handle_upload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    // open file for writing
    request->_tempFile = SD.open("/sounds/" + filename, FILE_WRITE);
  }
  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
  }
  if (final) {
    // close the file handle as the upload is now done
    request->_tempFile.close();
    request->redirect("/");
  }
}

void init_wifi() {
  Serial.print("\r\n\r\nSetting AP (Access Point)…");

  WiFi.softAP(ssid);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/time", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("set", true)) {
      AsyncWebParameter* p = request->getParam("set", true);
      time_t newtime = atoi(p->value().c_str());
      setTime(newtime);
      request->send(200, "text/plain", format_timestamp(newtime));
      return;
    }
    request->send(200, "text/plain", "nothing done");
  });

  server.on("/schedule", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("delete", true)) {
      AsyncWebParameter* p = request->getParam("delete", true);
      SD.remove("/schedule/" + p->value());
      request->send(200, "text/plain", "deleted");
      return;
    }
    else if (request->hasParam("timestamp", true) && request->hasParam("sound", true) && request->hasParam("volume", true)) {
      AsyncWebParameter* ts = request->getParam("timestamp", true);
      AsyncWebParameter* so = request->getParam("sound", true);
      AsyncWebParameter* vo = request->getParam("volume", true);
      File sched_file = SD.open("/schedule/"+ts->value(), FILE_WRITE);
      sched_file.print(so->value());
      sched_file.close();
      File vol_file = SD.open("/scvolume/"+ts->value(), FILE_WRITE);
      vol_file.print(vo->value());
      vol_file.close();
      request->redirect("/");
      return;
    }
  });

  server.on("/sounds", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("delete", true)) {
      AsyncWebParameter* p = request->getParam("delete", true);
      SD.remove("/sounds/" + p->value());
      request->send(200, "text/plain", "deleted");
      return;
    }
  });

  server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("timestamp")) {
      AsyncWebParameter* p = request->getParam("timestamp");
      play_audio(p->value());
      request->redirect("/");
      return;
    }
    request->send(200, "text/plain", "error");
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200);
  }, handle_upload);

  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP

  // server files from sdcard
  server.serveStatic("/sdcard/", SD, "/");
  
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");

    //File root = SD.open(dirname);

    response->printf(index_html, get_time_status(), format_timestamp(now()), format_timestamp(DCF.getTime()), get_sdcard_status(), get_schedule_web().c_str(), get_sounds_web().c_str());
    request->send(response);
  });
  

  server.begin();
}

String format_timestamp(time_t ts) {
  char* dow_str = NULL;
  switch(weekday(ts)) {
    case dowMonday:    dow_str = "Mo"; break;
    case dowTuesday:   dow_str = "Di"; break;
    case dowWednesday: dow_str = "Mi"; break;
    case dowThursday:  dow_str = "Do"; break;
    case dowFriday:    dow_str = "Fr"; break;
    case dowSaturday:  dow_str = "Sa"; break;
    case dowSunday:    dow_str = "So"; break;
    case dowInvalid:   dow_str = "IN"; break;
    default:           dow_str = "NA"; break;
  }
  char timestr[16];
  sprintf(timestr, "%s%02i%02i", dow_str, hour(ts), minute(ts));
  return String(timestr);
}



void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);

  init_wifi();
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  init_dcf77_sync();
  init_sdcard();
}


void loop() {
  dnsServer.processNextRequest(); // this is hopefully non blocking
  audio.loop();

  if (timeStatus() != timeNotSet) {
    /*
    if(now() != prevDisplay) //update the display only if the time has changed
    {
      prevDisplay = now();
     digitalClockDisplay();  
    }
    */

    time_t t_now = now();
    String ts_now = format_timestamp(t_now);
    if (last_timestamp != ts_now) {
      last_timestamp = ts_now;
      Serial.println(ts_now);
      play_audio(ts_now);
    }
    
  }
}




/*
void digitalClockDisplay() {
  // digital clock display of the time
  Serial.println("");
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

void printDigits(int digits) {
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
*/

