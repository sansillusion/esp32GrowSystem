#include <NTPClient.h>
#include <time.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <DS3231.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

Preferences preferences;
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.nrc.ca", -18000, 86400000);// timezone -5h et update aux 24h
#define ONE_WIRE_BUS_PIN 33 // pin 33
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature sensors(&oneWire);

DeviceAddress Probe0, Probe1, Probe2, Probe3, Probe4, Probe5;

int growled[] = {12, 13, 14, 15, 16, 17}; // les pins pour les lumieres (relay)
int alarm[] = {50, 50, 50, 50, 50, 50}; // temp en C max (on eteint la led si la temp est plus chaude)
int passer[] = {0, 0, 0, 0, 0, 0}; // confir pour savoir quel est desactivee
int passerx[] = {0, 0, 0, 0, 0, 0}; // confir pour savoir quel est desactivee par temperature
int etat[] = {0, 0, 0, 0, 0, 0}; // confir pour savoir quel etat est la led
int senseur[] = {0, 1, 2, 3, 4, 5}; // confir pour savoir quel senseur pour quel led
int allumeH[] = {4, 4, 4, 5, 5, 5}; // confir pour savoir quel heur allumer
int allumeM[] = {0, 30, 55, 0, 30, 55}; // confir pour savoir quel minute allumer
int eteintH[] = {21, 21, 21, 4, 4, 4}; // confir pour savoir quel heur eteindre
int eteintM[] = {0, 30, 55, 0, 45, 55}; // confir pour savoir quel minute eteindre
int derntemp[] = {0, 0, 0, 0, 0, 0}; // confir pour savoir la temp de la led
int erreur[] = {0, 0, 0, 0, 0, 0}; // confir pour savoir combien erreur de senseur de suite
int configur = 0; // config pour savoir si first run
unsigned long previousMillis = 0;
unsigned long previousMillisc = 0;
unsigned long previousMillist = 0;

DS3231 Clock;
bool Century = false;
bool h12;
bool PM;
byte ADay, AHour, AMinute, ASecond, ABits;
bool ADy, A12h, Apm;

String liens = "";

// stylesheet for web pages
const String css = "<style>\n"
                   ".heure,.minute,.celcius,.curtemp,.senseur {width:35px;}"
                   "</style>\n";
// end of stylesheet for web pages

void lesliens() {
  liens = "<a href=\"/\">Acceuil</a> - <a href=\"/timer\">Timer</a> - <a href=\"/senseurs\">Senseurs</a>";
}

char* string2char(String command) {
  if (command.length() != 0) {
    char *p = const_cast<char*>(command.c_str());
    return p;
  }
  return 0;
}



int heureEte(int year, int month, int dayOfMonth, int hour) {
  int DST;
  // ********************* Calculate offset for Sunday *********************
  int y = year;                          // DS3231 uses two digit year (required here)
  int x = (y + y / 4 + 2) % 7;    // remainder will identify which day of month
  // is Sunday by subtracting x from the one
  // or two week window.  First two weeks for March
  // and first week for November
  // *********** Test DST: BEGINS on 2nd Sunday of March @ 2:00 AM *********
  if (month == 3 && dayOfMonth == (14 - x) && hour >= 2)
  {
    DST = 1;                           // Daylight Savings Time is TRUE (add one hour)
  }
  if ((month == 3 && dayOfMonth > (14 - x)) || month > 3)
  {
    DST = 1;
  }
  // ************* Test DST: ENDS on 1st Sunday of Nov @ 2:00 AM ************
  if (month == 11 && dayOfMonth == (7 - x) && hour >= 2)
  {
    DST = 0;                            // daylight savings time is FALSE (Standard time)
  }
  if ((month == 11 && dayOfMonth > (7 - x)) || month > 11 || month < 3)
  {
    DST = 0;
  }
  return DST;
}





String printAddress(DeviceAddress deviceAddress)
{
  String retour = "";
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) retour += "0";
    retour += deviceAddress[i];
  }
  return retour;
}

// index page
void handleRoot() {
  String addy = server.client().remoteIP().toString();
  String header;
  String userAgent;
  if (server.hasHeader("User-Agent")) {
    userAgent = server.header("User-Agent");
  } else {
    userAgent = "Pas de userAgent ?";
  }
  String contenu = "<!DOCTYPE html>\n<html lang=\"en\" dir=\"ltr\" class=\"client-nojs\">\n<head>\n";
  contenu += "<meta charset=\"UTF-8\" />\n<title>420</title>\n"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  contenu += css;
  contenu += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js\"></script>\n";
  contenu += "</head>\n<body>\n"
             "<div style=\"text-align:center;width:100%;\">\n"
             "<h1>Acceuil</h1>";
  contenu += "<br>\n";
  contenu += liens;
  contenu += "</div></body></html>\n";
  server.send(200, "text/html", contenu);
  Serial.println("");
  Serial.println(addy);
  Serial.println(userAgent);
  Serial.println("Page acceuil");
}

// timer page
void handleTimer() {
  String addy = server.client().remoteIP().toString();
  String header;
  String userAgent;
  if (server.hasHeader("User-Agent")) {
    userAgent = server.header("User-Agent");
  } else {
    userAgent = "Pas de userAgent ?";
  }
  int heureux = Clock.getHour(h12, PM);
  if (h12 == 1) { //check si le rtc est en mode 12h
    if (PM == 1) { //si oui et que c'est pm
      heureux = heureux  + 12; // ajoute 12h
    }
  }
  int lamin = Clock.getMinute();
  String contenu = "<!DOCTYPE html>\n<html lang=\"en\" dir=\"ltr\" class=\"client-nojs\">\n<head>\n";
  contenu += "<meta charset=\"UTF-8\" />\n<title>Timer 420</title>\n"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  contenu += css;
  contenu += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js\"></script>\n";
  contenu += "</head>\n<body>\n"
             "<div style=\"text-align:center;width:100%;\">\n"
             "<h1>";
  contenu += heureux;
  contenu += ":";
  if (lamin <= 9) {
    contenu += "0";
  }
  contenu += lamin;
  contenu += "</h1>\n"
             "<form>\n"
             "<h2>Allumage :</h2>\n"
             "H:";
  for (int i = 0; i < 6; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"heure\" type=\"number\" min=\"0\" max=\"23\" value=\"";
      contenu += allumeH[i];
      contenu += "\" name=\"ah";
      contenu += i;
      contenu += "\">\n";
      if (i != 5) {
        contenu += " - ";
      }
    }
  }
  contenu += "<br>\n"
             "M:";
  for (int i = 0; i < 6; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"minute\" type=\"number\" min=\"0\" max=\"59\" value=\"";
      contenu += allumeM[i];
      contenu += "\" name=\"am";
      contenu += i;
      contenu += "\">\n";
      if (i != 5) {
        contenu += " - ";
      }
    }
  }
  contenu += "<br>\n"
             "<h2>Eteignage :</h2>\n"
             "H:";
  for (int i = 0; i < 6; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"heure\" type=\"number\" min=\"0\" max=\"23\" value=\"";
      contenu += eteintH[i];
      contenu += "\" name=\"eh";
      contenu += i;
      contenu += "\">\n";
      if (i != 5) {
        contenu += " - ";
      }
    }
  }
  contenu += "<br>\n"
             "M:";
  for (int i = 0; i < 6; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"minute\" type=\"number\" min=\"0\" max=\"59\" value=\"";
      contenu += eteintM[i];
      contenu += "\" name=\"em";
      contenu += i;
      contenu += "\">\n";
      if (i != 5) {
        contenu += " - ";
      }
    }
  }
  contenu += "</form><br>\n";
  contenu += liens;
  contenu += "</div>\n"
             "<script>"
             "var timerh;"
             "var timerm;"
             "$(\".heure\").change(function(event){\n"
             "event.preventDefault();\n"
             "clearTimeout(timerh);"
             "var lenom = $(this).attr('name');\n"
             "var laval = $(this).val();\n"
             "if (laval >= 24){\n"
             "laval = 23;\n"
             "$(this).val(laval);\n"
             "alert(\"max 23\");\n"
             "}\n"
             "timerh = setTimeout(function(){"
             "$.get( \"/changer?\"+lenom+\"=\"+laval);"
             "},500);"
             "});"
             "$(\".minute\").change(function(event){\n"
             "event.preventDefault();\n"
             "clearTimeout(timerm);"
             "if ($(this).val() >> 59){\n"
             "$(this).val(59);\n"
             "}\n"
             "var lenom = $(this).attr('name');\n"
             "var laval = $(this).val();\n"
             "if (laval >= 60){\n"
             "laval = 59;\n"
             "$(this).val(laval);\n"
             "alert(\"max 59\");\n"
             "}\n"
             "timerm = setTimeout(function(){"
             "$.get( \"/changer?\"+lenom+\"=\"+laval);"
             "},500);"
             "});\n"
             "</script>\n"
             "</body></html>\n";
  server.send(200, "text/html", contenu);
  Serial.println("");
  Serial.println(addy);
  Serial.println(userAgent);
  Serial.println("Page Timer");
}

// sensor page
void handleSenseur() {
  String addy = server.client().remoteIP().toString();
  String header;
  String userAgent;
  if (server.hasHeader("User-Agent")) {
    userAgent = server.header("User-Agent");
  } else {
    userAgent = "Pas de userAgent ?";
  }
  String contenu = "<!DOCTYPE html>\n<html lang=\"en\" dir=\"ltr\" class=\"client-nojs\">\n<head>\n";
  contenu += "<meta charset=\"UTF-8\" />\n<title>Senseurs 420</title>\n"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  contenu += css;
  contenu += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js\"></script>\n";
  contenu += "</head>\n<body>\n"
             "<div style=\"text-align:center;width:100%;\">\n"
             "<h1>Senseurs :</h1>\n"
             "<form>\n";
  contenu +=  "<h2>temp :</h2>\n"
              "S:";
  for (int i = 0; i < 6; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"senseur\" type=\"number\" min=\"0\" max=\"5\" value=\"";
      contenu += senseur[i];
      contenu += "\" name=\"sen";
      contenu += i;
      contenu += "\">\n";
      if (i != 5) {
        contenu += " - ";
      }
    }
  }
  contenu +=  "<br>\n"
              "C:";
  for (int i = 0; i < 6; i = i + 1) {
    if (passer[i] == 0) {
      int tempC = derntemp[i];
      contenu += "<input class=\"curtemp\" type=\"number\" min=\"0\" max=\"100\" value=\"";
      contenu += tempC;
      contenu += "\" name=\"tmp";
      contenu += i;
      contenu += "\" disabled>\n";
      if (i != 5) {
        contenu += " - ";
      }
    }
  }
  contenu +=  "<br>\n"
              "C:";
  for (int i = 0; i < 6; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"celcius\" type=\"number\" min=\"0\" max=\"100\" value=\"";
      contenu += alarm[i];
      contenu += "\" name=\"ala";
      contenu += i;
      contenu += "\">\n";
      if (i != 5) {
        contenu += " - ";
      }
    }
  }
  contenu += "<br>\n"
             "</form><br>\n";
  contenu += liens;
  contenu += "</div>\n"
             "<script>"
             "var timerc;"
             "var timers;"
             "$(\".celcius\").change(function(event){\n"
             "event.preventDefault();\n"
             "clearTimeout(timerc);"
             "var lenom = $(this).attr('name');\n"
             "var laval = $(this).val();\n"
             "if (laval >= 101){\n"
             "laval = 100;\n"
             "$(this).val(laval);\n"
             "alert(\"max 100\");\n"
             "}\n"
             "timerc = setTimeout(function(){"
             "$.get( \"/changer?\"+lenom+\"=\"+laval);"
             "},500);"
             "});"
             "$(\".senseur\").change(function(event){\n"
             "event.preventDefault();\n"
             "clearTimeout(timers);"
             "var lenom = $(this).attr('name');\n"
             "var laval = $(this).val();\n"
             "if (laval >= 6){\n"
             "laval = 5;\n"
             "$(this).val(laval);\n"
             "alert(\"max 5\");\n"
             "}\n"
             "timers = setTimeout(function(){"
             "$.get( \"/changer?\"+lenom+\"=\"+laval);"
             "},500);"
             "});"
             "</script>\n"
             "</body></html>\n";
  server.send(200, "text/html", contenu);
  Serial.println("");
  Serial.println(addy);
  Serial.println(userAgent);
  Serial.println("Page Senseurs");
}

void handleChange() {
  String addy = server.client().remoteIP().toString();
  Serial.println("");
  Serial.println(addy);
  Serial.println("Change");
  for (int i = 0; i < 6; i = i + 1) {
    String alal = "ala";
    alal += i;
    String llah = "ah";
    llah += i;
    String llam = "am";
    llam += i;
    String lleh = "eh";
    lleh += i;
    String llem = "em";
    llem += i;
    String sens = "sen";
    sens += i;
    String alrm = server.arg(alal);
    String lah = server.arg(llah);
    String lam = server.arg(llam);
    String leh = server.arg(lleh);
    String lem = server.arg(llem);
    String sed = server.arg(sens);
    if (sed != "") {
      senseur[i] = sed.toInt();
      Serial.print("sen");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(sed);
      String sea = "se";
      sea += i;
      int isen = senseur[i];
      preferences.putInt(string2char(sea), isen);
    }
    if (alrm != "") {
      alarm[i] = alrm.toInt();
      Serial.print("ala");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(alrm);
      String alm = "ala";
      alm += i;
      int iala = alarm[i];
      preferences.putInt(string2char(alm), iala);
    }
    if (lah != "") {
      allumeH[i] = lah.toInt();
      Serial.print("ah");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(lah);
      String ahs = "ah";
      ahs += i;
      int iah = allumeH[i];
      preferences.putInt(string2char(ahs), iah);
    }
    if (lam != "") {
      allumeM[i] = lam.toInt();
      Serial.print("am");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(lam);
      String ams = "am";
      ams += i;
      int iam = allumeM[i];
      preferences.putInt(string2char(ams), iam);
    }
    if (leh != "") {
      eteintH[i] = leh.toInt();
      Serial.print("eh");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(leh);
      int ieh = eteintH[i];
      String ehs = "eh";
      ehs += i;
      preferences.putInt(string2char(ehs), ieh);
    }
    if (lem != "") {
      eteintM[i] = lem.toInt();
      Serial.print("em");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(lem);
      String ems = "em";
      ems += i;
      int iem = eteintM[i];
      preferences.putInt(string2char(ems), iem);
    }
  }
  String contenu = "<!DOCTYPE html>\n<html lang=\"en\" dir=\"ltr\" class=\"client-nojs\">\n<head>\n";
  contenu += "<meta http-equiv=\"refresh\" content=\"0;url=/\" />\n";
  contenu += "<meta charset=\"UTF-8\" />\n<title>Que la lumiere soit</title>\n"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  contenu += css;
  contenu += "</head>\n<body>\n"
             "Merci</body></html>\n";
  server.send(200, "text/html", contenu);
}

void handleNotFound() {
  String header;
  String addy = server.client().remoteIP().toString();
  Serial.println("");
  String userAgent;
  Serial.println(addy);
  if (server.hasHeader("User-Agent")) {
    userAgent = server.header("User-Agent");
  } else {
    userAgent = "Pas de userAgent ?";
  }
  Serial.println(userAgent);
  String htmlmessage = "<!DOCTYPE html>\n<html lang=\"en\" dir=\"ltr\" class=\"client-nojs\">\n<head>\n";
  htmlmessage += "<meta charset=\"UTF-8\" />\n<title>404 Not found</title>\n"
                 "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  htmlmessage += css;
  htmlmessage += "</head>\n<body>\n"
                 "<div style=\"text-align:center;width:100%;\">\n";
  htmlmessage += "<h1>404 File Not Found</h1><br>\n";
  htmlmessage += "URI: ";
  htmlmessage += server.uri();
  htmlmessage += "<br>\nMethod: ";
  htmlmessage += (server.method() == HTTP_GET) ? "GET" : "POST";
  htmlmessage += "<br>\nArguments: ";
  htmlmessage += server.args();
  htmlmessage += "<br>\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    htmlmessage += " " + server.argName(i) + ": " + server.arg(i) + "<br>\n";
  }
  htmlmessage += "<br>\n";
  htmlmessage += liens;
  htmlmessage += "<br>\n</div></body></html>\n";
  server.send(404, "text/html", htmlmessage);
  String message = "404 File Not Found\n";
  message += "URI: ";
  String uriL = server.uri();
  message += uriL;
  message += "\nMethod: ";
  String methodeL = (server.method() == HTTP_GET) ? "GET" : "POST";
  message += methodeL;
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  String argsL;
  for (uint8_t i = 0; i < server.args(); i++) {
    argsL += ": " + server.argName(i) + "=" + server.arg(i) + " ";
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  Serial.print(message);
}

void lumiereloop() {
  sensors.requestTemperatures();
  int heureux = Clock.getHour(h12, PM);
  if (h12 == 1) { //check si le rtc est en mode 12h
    if (PM == 1) { //si oui et que c'est pm
      heureux = heureux  + 12; // ajoute 12h
    }
  }
  int lamin = Clock.getMinute();
  float tempC;
  for (int i = 0; i < 6; i = i + 1) {
    switch (senseur[i]) {
      case 0:
        tempC = sensors.getTempC(Probe0);
        break;
      case 1:
        tempC = sensors.getTempC(Probe1);
        break;
      case 2:
        tempC = sensors.getTempC(Probe2);
        break;
      case 3:
        tempC = sensors.getTempC(Probe3);
        break;
      case 4:
        tempC = sensors.getTempC(Probe4);
        break;
      case 5:
        tempC = sensors.getTempC(Probe5);
        break;
      default:
        tempC = sensors.getTempC(Probe0);
        break;
    }
    if (tempC != -127.00) {
      derntemp[i] = tempC;
      if (tempC >= alarm[i]) {
        Serial.print("!!! ALERTE !!! - Led #");
        Serial.print(i);
        Serial.print(" Temp : ");
        Serial.print(tempC);
        Serial.println("°C - !!! ALERTE !!!");
        passerx[i] = 1;
      } else {
        int alarmeux = alarm[i] - 4;// attend 5C de moin que lalarme avant de reactiver une led desactiver par temperature
        if (tempC <= alarmeux) {
          passerx[i] = 0;
        }
      }
      erreur[i] = 0;
    } else {
      erreur[i]++;
      if (erreur[i] >= 10) {
        Serial.print("10 erreures consecutives desactivation de la del #");
        Serial.println(i);
        passer[i] = 1;
        passerx[i] = 1;
      }
    }
    if (passer[i] == 0) {
      if (passerx[i] == 0) {
        if (allumeH[i] < eteintH[i]) {
          if (heureux >= allumeH[i] && heureux < eteintH[i] ) { //Allume H ok
            if (heureux == allumeH[i]) {
              if (lamin >= allumeM[i]) {
                if (passer[i] == 0) {
                  digitalWrite(growled[i], HIGH); //Allume la lumiere
                  etat[i] = 1;
                }
              } else {
                digitalWrite(growled[i], LOW); //Eteint la lumiere
                etat[i] = 0;
              }
            } else {
              if (passer[i] == 0) {
                digitalWrite(growled[i], HIGH); //Allume la lumiere
                etat[i] = 1;
              }
            }
          } else if (heureux >= eteintH[i]) {
            if (heureux == eteintH[i]) {
              if (lamin >= eteintM[i]) {
                digitalWrite(growled[i], LOW); // Eteint la lumiere a reviser
                etat[i] = 0;
              } else {
                if (passer[i] == 0) {
                  digitalWrite(growled[i], HIGH); //Allume la lumiere
                  etat[i] = 1;
                }
              }
            } else {
              digitalWrite(growled[i], LOW); // Eteint la lumiere a reviser
              etat[i] = 0;
            }
          } else {
            digitalWrite(growled[i], LOW); // Eteint la lumiere
            etat[i] = 0;
          }
        }
        if (allumeH[i] > eteintH[i]) {
          if (heureux >= allumeH[i] && heureux <= 23) {                //Start
            if (heureux == allumeH[i]) {
              if (lamin >= allumeM[i]) {
                if (passer[i] == 0) {
                  digitalWrite(growled[i], HIGH); //Allume la lumiere
                  etat[i] = 1;
                }
              } else {
                digitalWrite(growled[i], LOW); //Eteint la lumiere
                etat[i] = 0;
              }
            } else {
              if (heureux > eteintH[i]) {
                if (passer[i] == 0) {
                  digitalWrite(growled[i], HIGH); //Eteint la lumiere
                  etat[i] = 1;
                }
              } else {
                if (passer[i] == 0) {
                  digitalWrite(growled[i], HIGH); //Allume la lumiere
                  etat[i] = 1;
                }
              }
            }
          }
          else if (heureux < eteintH[i]) {
            if (passer[i] == 0) {
              digitalWrite(growled[i], HIGH); //Eteint la lumiere
              etat[i] = 1;
            }
          }
          else if (heureux >= eteintH[i] && heureux < allumeH[i]) {
            if (heureux == eteintH[i]) {
              if (lamin >= eteintM[i]) {
                digitalWrite(growled[i], LOW); //Eteint la lumiere
                etat[i] = 0;
              } else {
                if (passer[i] == 0) {
                  digitalWrite(growled[i], HIGH); //Allume la lumiere
                  etat[i] = 1;
                }
              }
            } else {
              digitalWrite(growled[i], LOW); //Eteint la lumiere
              etat[i] = 0;
            }
          }
        }
        if (allumeH[i] == eteintH[i]) {
          if (allumeM[i] < eteintM[i]) {
            if (lamin < eteintM[i]) {
              if (lamin >= allumeM[i]) {
                if (passer[i] == 0) {
                  digitalWrite(growled[i], HIGH); //Allume la lumiere
                  etat[i] = 1;
                }
              }
            } else {
              digitalWrite(growled[i], LOW); //Eteint la lumiere
              etat[i] = 0;
            }
          }
          if (allumeM[i] > eteintM[i]) {
            if (lamin < eteintM[i]) {
              if (passer[i] == 0) {
                digitalWrite(growled[i], HIGH); //Allume la lumiere
                etat[i] = 1;
              }
            } else {
              if (lamin >= allumeM[i]) {
                if (passer[i] == 0) {
                  digitalWrite(growled[i], HIGH); //Allume la lumiere
                  etat[i] = 1;
                }
              } else {
                digitalWrite(growled[i], LOW); //Eteint la lumiere
                etat[i] = 0;
              }
            }
          }
          if (allumeM[i] == eteintM[i]) { // le meme heure d'allumage et d'eteignage == eteint permanent
            digitalWrite(growled[i], LOW); //Eteint la lumiere
            etat[i] = 0;
          }
        }
      } else {
        digitalWrite(growled[i], LOW); //Eteint la lumiere car elle chauffe
        etat[i] = 0;
      }
    } else {
      digitalWrite(growled[i], LOW); //Eteint la lumiere car elle est desactivee
      etat[i] = 0;
    }
  }
}

void loop1(void *pvParameters) {
  while (1) {
    if (configur == 1) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= 1000) {
        previousMillis = currentMillis;
        lumiereloop();
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  sensors.begin();
  for (int i = 0; i < 6; i = i + 1) {
    pinMode(growled[i], OUTPUT);
    digitalWrite(growled[i], LOW); //Eteint la lumiere
  }
  preferences.begin("420", false);
  String tst0 = preferences.getString("add0", "0");
  if (tst0 == "0") {
    if (!sensors.getAddress(Probe0, 0)) {
      Serial.println("Erreure pas de senseurs 0");
      passer[0] = 1;
    } else {
      String tsts0 = printAddress(Probe0);
      Serial.print("Addresse du senseur 0 : ");
      Serial.println(tsts0);
      preferences.putString("add0", tsts0);
    }
  } else {
    if (!sensors.getAddress(Probe0, 0)) {
      Serial.println("Erreure senseurs 0 non fonctionnel !");
      passer[0] = 1;
    } else {
      String tsts0 = printAddress(Probe0);
      if (tst0 == tsts0) {
        Serial.println("Senseur 0 OK !");
      } else {
        Serial.println("Mismatch de senseurs veuillez reinitialiser !");
        for (int i = 0; i < 6; i = i + 1) {
          passer[i] = 1;
        }
      }
    }
  }
  String tst1 = preferences.getString("add1", "0");
  if (tst1 == "0") {
    if (!sensors.getAddress(Probe1, 1)) {
      Serial.println("Erreure pas de senseurs 1");
      passer[1] = 1;
    } else {
      String tsts1 = printAddress(Probe1);
      Serial.print("Addresse du senseur 1 : ");
      Serial.println(tsts1);
      preferences.putString("add1", tsts1);
    }
  } else {
    if (!sensors.getAddress(Probe1, 1)) {
      Serial.println("Erreure senseurs 1 non fonctionnel !");
      passer[1] = 1;
    } else {
      String tsts1 = printAddress(Probe1);
      if (tst1 == tsts1) {
        Serial.println("Senseur 1 OK !");
      } else {
        Serial.println("Mismatch de senseurs veuillez reinitialiser !");
        for (int i = 0; i < 6; i = i + 1) {
          passer[i] = 1;
        }
      }
    }
  }
  String tst2 = preferences.getString("add2", "0");
  if (tst2 == "0") {
    if (!sensors.getAddress(Probe2, 2)) {
      Serial.println("Erreure pas de senseurs 2");
      passer[2] = 1;
    } else {
      String tsts2 = printAddress(Probe2);
      Serial.print("Addresse du senseur 2 : ");
      Serial.println(tsts2);
      preferences.putString("add2", tsts2);
    }
  } else {
    if (!sensors.getAddress(Probe2, 2)) {
      Serial.println("Erreure senseurs 2 non fonctionnel !");
      passer[2] = 1;
    } else {
      String tsts2 = printAddress(Probe2);
      if (tst2 == tsts2) {
        Serial.println("Senseur 2 OK !");
      } else {
        Serial.println("Mismatch de senseurs veuillez reinitialiser !");
        for (int i = 0; i < 6; i = i + 1) {
          passer[i] = 1;
        }
      }
    }
  }
  String tst3 = preferences.getString("add3", "0");
  if (tst3 == "0") {
    if (!sensors.getAddress(Probe3, 3)) {
      Serial.println("Erreure pas de senseurs 3");
      passer[3] = 1;
    } else {
      String tsts3 = printAddress(Probe3);
      Serial.print("Addresse du senseur 3 : ");
      Serial.println(tsts3);
      preferences.putString("add3", tsts3);
    }
  } else {
    if (!sensors.getAddress(Probe3, 3)) {
      Serial.println("Erreure senseurs 3 non fonctionnel !");
      passer[3] = 1;
    } else {
      String tsts3 = printAddress(Probe3);
      if (tst3 == tsts3) {
        Serial.println("Senseur 3 OK !");
      } else {
        Serial.println("Mismatch de senseurs veuillez reinitialiser !");
        for (int i = 0; i < 6; i = i + 1) {
          passer[i] = 1;
        }
      }
    }
  }
  String tst4 = preferences.getString("add4", "0");
  if (tst4 == "0") {
    if (!sensors.getAddress(Probe4, 4)) {
      Serial.println("Erreure pas de senseurs 4");
      passer[4] = 1;
    } else {
      String tsts4 = printAddress(Probe4);
      Serial.print("Addresse du senseur 4 : ");
      Serial.println(tsts4);
      preferences.putString("add4", tsts4);
    }
  } else {
    if (!sensors.getAddress(Probe4, 4)) {
      Serial.println("Erreure senseurs 4 non fonctionnel !");
      passer[4] = 1;
    } else {
      String tsts4 = printAddress(Probe4);
      if (tst4 == tsts4) {
        Serial.println("Senseur 4 OK !");
      } else {
        Serial.println("Mismatch de senseurs veuillez reinitialiser !");
        for (int i = 0; i < 6; i = i + 1) {
          passer[i] = 1;
        }
      }
    }
  }
  String tst5 = preferences.getString("add5", "0");
  if (tst5 == "0") {
    if (!sensors.getAddress(Probe5, 5)) {
      Serial.println("Erreure pas de senseurs 5");
      passer[5] = 1;
    } else {
      String tsts5 = printAddress(Probe5);
      Serial.print("Addresse du senseur 5 : ");
      Serial.println(tsts5);
      preferences.putString("add5", tsts5);
    }
  } else {
    if (!sensors.getAddress(Probe5, 5)) {
      Serial.println("Erreure senseurs 5 non fonctionnel !");
      passer[5] = 1;
    } else {
      String tsts5 = printAddress(Probe5);
      if (tst5 == tsts5) {
        Serial.println("Senseur 5 OK !");
      } else {
        Serial.println("Mismatch de senseurs veuillez reinitialiser !");
        for (int i = 0; i < 6; i = i + 1) {
          passer[i] = 1;
        }
      }
    }
  }
  sensors.setResolution(Probe0, 10);
  sensors.setResolution(Probe1, 10);
  sensors.setResolution(Probe2, 10);
  sensors.setResolution(Probe3, 10);
  sensors.setResolution(Probe4, 10);
  sensors.setResolution(Probe5, 10);
  for (int i = 0; i < 6; i = i + 1) {
    String ala = "ala";
    ala += i;
    String ahs = "ah";
    ahs += i;
    String ams = "am";
    ams += i;
    String ehs = "eh";
    ehs += i;
    String ems = "em";
    ems += i;
    String sen = "se";
    sen += i;
    int alar = alarm[i];
    int iah = allumeH[i];
    int iam = allumeM[i];
    int ieh = eteintH[i];
    int iem = eteintM[i];
    int sea = senseur[i];
    int alam = preferences.getInt(string2char(ala), alar);
    int ahi = preferences.getInt(string2char(ahs), iah);
    int ami = preferences.getInt(string2char(ams), iam);
    int ehi = preferences.getInt(string2char(ehs), ieh);
    int emi = preferences.getInt(string2char(ems), iem);
    int sed = preferences.getInt(string2char(sen), sea);
    alarm[i] = alam;
    allumeH[i] = ahi;
    allumeM[i] = ami;
    eteintH[i] = ehi;
    eteintM[i] = emi;
    senseur[i] = sed;
  }
  configur = preferences.getInt("configur", 0);
  xTaskCreatePinnedToCore(loop1, "loop1", 4096, NULL, 2, NULL, ARDUINO_RUNNING_CORE); // on dirait que les sensors aiment pas avoir une priorite de 1 alors 2 semble ok
  WiFiManager wifiManager;
  wifiManager.setTimeout(240);
  if (!wifiManager.autoConnect("Setup420")) {
    delay(3000);
    ESP.restart();
    delay(5000);
  }
  MDNS.begin("config420");// use preferance pour le nom
  lesliens();
  if (configur == 0) {
    preferences.putInt("configur", 1);
    configur = 1;
  }
  server.on("/", handleRoot);
  server.on("/timer", handleTimer);
  server.on("/senseurs", handleSenseur);
  server.on("/changer", handleChange);
  server.onNotFound(handleNotFound);
  const char * headerkeys[] = {"User-Agent"} ;//peut ajouter autres si besoin eg {"User-Agent", "Cookie"}
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  server.collectHeaders(headerkeys, headerkeyssize );
  server.begin();
  Serial.println("Serveur web démaré");
  MDNS.addService("_http", "_tcp", 80);
  timeClient.begin();
  delay(1000);
  Clock.setClockMode(false);
  if (timeClient.update()) {
    int ntpheure = timeClient.getHours();
    int ntpmins = timeClient.getMinutes();
    int heureux = Clock.getHour(h12, PM);
    if (h12 == 1) { //check si le rtc est en mode 12h
      if (PM == 1) { //si oui et que c'est pm
        heureux = heureux  + 12; // ajoute 12h
      }
    }
    int lamin = Clock.getMinute();
    int lanee = Clock.getYear();
    int lemois = Clock.getMonth(Century);
    int lejour = Clock.getDate();
    if (heureEte(lanee, lemois, lejour, heureux)) {
      Serial.println("heure d'ete");
      ntpheure = ntpheure + 1;
    }
    if (heureux != ntpheure) {
      Clock.setHour(ntpheure);
      Serial.print("Heures : ");
      Serial.println(ntpheure);
    }
    if (lamin != ntpmins) {
      Clock.setMinute(ntpmins);
      Serial.print("Minutes : ");
      Serial.println(ntpmins);
    }
  }
}

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisc >= 10000) {
    previousMillisc = currentMillis;
    int status =  WiFi.status();
    if (status == 6) {
      WiFi.reconnect();
      delay(6000);
    }
  }
  if (currentMillis - previousMillist >= 3600000) { // met le RTC a jour avec le NTP chaques 1h si changements
    previousMillist = currentMillis;
    if (timeClient.update()) {
      int ntpheure = timeClient.getHours();
      int ntpmins = timeClient.getMinutes();
      int heureux = Clock.getHour(h12, PM);
      if (h12 == 1) { //check si le rtc est en mode 12h
        if (PM == 1) { //si oui et que c'est pm
          heureux = heureux + 12; // ajoute 12h
        }
      }
      int lamin = Clock.getMinute();
      int lanee = Clock.getYear();
      int lemois = Clock.getMonth(Century);
      int lejour = Clock.getDate();
      if (heureEte(lanee, lemois, lejour, heureux)) {
        Serial.println("heure d'ete");
        ntpheure = ntpheure + 1;
      }
      if (heureux != ntpheure) {
        Clock.setHour(ntpheure);
        Serial.print("Heures : ");
        Serial.println(ntpheure);
      }
      if (lamin != ntpmins) {
        Clock.setMinute(ntpmins);
        Serial.print("Minutes : ");
        Serial.println(ntpmins);
      }
    }
  }
}
