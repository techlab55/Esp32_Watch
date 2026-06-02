//====================================================================================
// Begin NTP related libraries
//====================================================================================

// Time library:
// https://github.com/PaulStoffregen/Time
// #include <Time.h>                        // old, replaced by TimeLib.h
#include <TimeLib.h>
// Time zone correction library:
// https://github.com/JChristensen/Timezone
#include <Timezone.h>

// Choose library to load
#ifdef ESP8266
// ESP8266
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_ARCH_RP2040)
// RP2040 Nano Connect
#include <WiFiNINA.h>
#else
// ESP32
#include <WiFi.h>
#endif

#include <WiFiUdp.h>

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

//====================================================================================
//                                  Settings
//====================================================================================

#define TIMEZONE euCET // See below for other "Zone references", UK, usMT etc

#ifdef ESP32 // Temporary fix, ESP8266 fails to communicate with some servers...
// Try to use pool url instead so the server IP address is looked up from those available
// (use a pool server in your own country to improve response time and reliability)
//const char* ntpServerName = "time.nist.gov";
//const char* ntpServerName = "time.google.com";
const char* ntpServerName = "es.pool.ntp.org";                                          // I am in Spain, so the Spanish server pool
#else
// Try to use pool url instead so the server IP address is looked up from those available
// (use a pool server in your own country to improve response time and reliability)
// const char* ntpServerName = "time.nist.gov";
const char* ntpServerName = "es.pool.ntp.org";
//const char* ntpServerName = "time.google.com";
#endif

// Try not to use hard-coded IP addresses which might change, you can if you want though...
//IPAddress timeServerIP(129, 6, 15, 30);   // time-c.nist.gov NTP server
//IPAddress timeServerIP(24, 56, 178, 140); // wwv.nist.gov NTP server
IPAddress timeServerIP;                     // Use server pool

// Example time zone and DST rules, see Timezone library documents to see how
// to add more time zones https://github.com/JChristensen/Timezone
// TimeChangeRule myRule = {abbrev, week, dow, month, hour, offset};
//
// Select one of them
/*
// Zone reference "UK" United Kingdom (London, Belfast)
TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};        //British Summer (Daylight saving) Time
TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};         //Standard Time
Timezone UK(BST, GMT);
*/

// Zone reference "euCET" Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule  CET = {"CET ", Last, Sun, Oct, 3, 60};      // Central European Standard Time
Timezone euCET(CEST, CET);
/*
// Zone reference "ausET" Australia Eastern Time Zone (Sydney, Melbourne)
TimeChangeRule aEDT = {"AEDT", First, Sun, Oct, 2, 660};    //UTC + 11 hours
TimeChangeRule aEST = {"AEST", First, Sun, Apr, 3, 600};    //UTC + 10 hours
Timezone ausET(aEDT, aEST);

// Zone reference "usET US Eastern Time Zone (New York, Detroit)
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  //Eastern Daylight Time = UTC - 4 hours
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   //Eastern Standard Time = UTC - 5 hours
Timezone usET(usEDT, usEST);

// Zone reference "usCT" US Central Time Zone (Chicago, Houston)
TimeChangeRule usCDT = {"CDT", Second, dowSunday, Mar, 2, -300};
TimeChangeRule usCST = {"CST", First, dowSunday, Nov, 2, -360};
Timezone usCT(usCDT, usCST);

// Zone reference "usMT" US Mountain Time Zone (Denver, Salt Lake City)
TimeChangeRule usMDT = {"MDT", Second, dowSunday, Mar, 2, -360};
TimeChangeRule usMST = {"MST", First, dowSunday, Nov, 2, -420};
Timezone usMT(usMDT, usMST);

// Zone reference "usAZ" Arizona is US Mountain Time Zone but does not use DST
Timezone usAZ(usMST, usMST);

// Zone reference "usPT" US Pacific Time Zone (Las Vegas, Los Angeles)
TimeChangeRule usPDT = {"PDT", Second, dowSunday, Mar, 2, -420};
TimeChangeRule usPST = {"PST", First, dowSunday, Nov, 2, -480};
Timezone usPT(usPDT, usPST);
*/

//====================================================================================
// Begin Variables
//====================================================================================
TimeChangeRule *tzCode;                                   // Pointer to the time change rule, use to get the TZ abbrev, e.g. "GMT"

time_t utc                            = 0;
time_t local                          = 0;
time_t boottime                       = 0;

bool timeValid = false;
unsigned int localPort                = 2390;             // local port to listen for UDP packets
const int NTP_PACKET_SIZE             = 48;               // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE ];                     //buffer to hold incoming and outgoing packets

uint8_t lastMinute                    = 0;
uint32_t nextSendTime                 = 0;
uint32_t newRecvTime                  = 0;
uint32_t lastRecvTime                 = 0;
uint32_t newTickTime                  = 0;
uint32_t lastTickTime                 = 0;
bool ntp_start                        = true;
uint32_t no_packet_count              = 0;
//====================================================================================
// End Variables
//====================================================================================




//===================================================================================
// Begin Function prototype (s)
//===================================================================================
void printTime(time_t zone, char *tzCode);
String timeString();
void decodeNTP(void);
void syncTime(void);
void displayTime(void);
void sendNTPpacket(IPAddress& address);
//===================================================================================
// End Function prototype (s)
//===================================================================================




//====================================================================================
// Begin syncTime() Update Time
//====================================================================================
void syncTime(void) {
  if (ntp_start ==  true) {                                       // Run once
        if ( ForceForget == true || WiFiSpecified == false ) {
              WiFi.begin(WIFI_SSID_FOUND);                        // open, found network        
              Serial.print("Connecting to found network: ");
              Serial.println(WIFI_SSID_FOUND);
              }
              else
              {
              WiFi.begin(WIFI_SSID, WIFI_PASSWORD);               // use specified network       
              Serial.print("Connecting to specified network: ");
              Serial.println(WIFI_SSID);                
              }

      uint32_t WaitForConnectionTime = millis() + 10000;
      while (WiFi.status() != WL_CONNECTED) {           
            Serial.print(".");
                  if ( ForceForget == true || WiFiSpecified == false ) {
                        if (WiFi.status() != WL_CONNECTED) WiFi.begin(WIFI_SSID_FOUND);           // connect to the found, open network
                  }
                  else
                  {
                        if (WiFi.status() != WL_CONNECTED) WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // connect to the specified, secure network
                        if ( WaitForConnectionTime < millis() ) break;
                  }
      delay(500);
      }
    
    Serial.println("");
    if ( (WiFi.status() != WL_CONNECTED) && ( ForceForget == false || WiFiSpecified == true ) ) { // if this fails, switch to look for an open network
          Serial.println("Unable to connect to specified network, check your WiFI name and Password");
          Serial.println("Looking for an open network now... (See TFT for info)");
          ForceForget = true;
          ntp_start = true;
          WiFi.disconnect();
          }
                                  
      Serial.println();
      udp.begin(localPort); 
      if (WiFi.status() == WL_CONNECTED) {ntp_start = false;}
  }

  if ( WiFi.status() == WL_CONNECTED ) {
    
    if (nextSendTime < millis()) {                                        // Don't send too often so we don't trigger Denial of Service

      WiFiClient client;                                                // Added to check if the WiFi is still connected
      if (!client.connect("api.ipify.org", 80)) {                       // This is a site that can be used to get your Publlic IP, not used now, just to check the connection..
            Serial.println("Failed to connect with 'api.ipify.org' !");
            WiFi.disconnect();
            Serial.println("Trying to reconnect!");
            WiFi.reconnect();
            WiFiRefresh = WiFiRefresh + 1;                              // variable used to indicate a refresh was (once or more times) necesary, shown on cockface
            while (WiFi.status() != WL_CONNECTED) {
                  Serial.print('.');
                  delay(500);
            }
      nextSendTime = millis();                                          // After this re-connect, make sure to get a new NTP time again quickly
      }
        
      nextSendTime = millis() + ( 60 * 60 * 1000 );                     // Wait 1 hour for next sync
      WiFi.hostByName(ntpServerName, timeServerIP);                     // Get a random server from the pool
      sendNTPpacket(timeServerIP);                                      // send an NTP packet to a time server
      decodeNTP();

      if ( no_packet_count > 0 )  {                                     // Wait 1 minute for next sync
            nextSendTime = millis() + ( 60 * 1000 );
      }
      else 
      {
      nextSendTime = millis() + ( 60 * 60 * 1000 );                     // Wait 1 hour for next sync
      }
    }
  }
}
//====================================================================================
// End syncTime() Update Time
//====================================================================================




//====================================================================================
// Begin sendNTPpacket() Send an NTP request to the time server at the given address
//====================================================================================
void sendNTPpacket(IPAddress& address) {                // Serial.println("sending NTP packet...");
    memset(packetBuffer, 0, NTP_PACKET_SIZE);           // set all bytes in the buffer to 0
                                                        // Initialize values needed to form NTP request
                                                        // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;                       // LI, Version, Mode
    packetBuffer[1] = 0;                                // Stratum, or type of clock
    packetBuffer[2] = 6;                                // Polling Interval
    packetBuffer[3] = 0xEC;                             // Peer Clock Precision
                                                        // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;
                                                        // all NTP fields have been given values, now
                                                        // you can send a packet requesting a timestamp:
    udp.beginPacket(address, 123);                      // NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}
//====================================================================================
// End sendNTPpacket() Send an NTP request to the time server at the given address
//====================================================================================




//====================================================================================
// Begin decodeNTP() Decode the NTP message and print status to serial port
//====================================================================================
void decodeNTP(void) {
    timeValid = false;
    uint32_t waitTime = millis() + 500;
    while (millis() < waitTime && !timeValid){
        yield();
        if (udp.parsePacket()) {
              newRecvTime = millis();                   // We've received a packet, read the data from it
              udp.read(packetBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer
              Serial.print("\nNTP response time was : ");
              Serial.print(500 - (waitTime - newRecvTime));
              Serial.println(" ms");
              Serial.print("Time since last sync is: ");
              Serial.print((newRecvTime - lastRecvTime) / 1000.0);
              Serial.println(" s");
              lastRecvTime = newRecvTime;
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);  // The timestamp starts at byte 40 of the received packet and is four bytes,
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);   // or two words, long. First, extract the two words:
                                                                          // Combine the four bytes (two words) into a long integer
      unsigned long secsSince1900 = highWord << 16 | lowWord;             // this is NTP time (seconds since Jan 1 1900):

      utc = secsSince1900 - 2208988800UL;                                 // Now convert NTP Unix time (Seconds since Jan 1 1900) into everyday time:
                                                                          // UTC time starts on Jan 1 1970. In seconds the difference is 2208988800:
        
      setTime(utc);                                                       // Set system clock to utc time (not time zone compensated)
      local = euCET.toLocal(utc, &tzCode);                                // convert utc into local timezone compensated
      setTime(local);                                                     // Set system clock to local time (this IS time zone compensated)
           
      timeValid = true;

      // Print the hour, minute and second:
      Serial.print("Received NTP UTC time plus local correction is: ");

      uint8_t hh = hour(local);                                           // uint8_t hh = hour(utc);
      Serial.print(hh);                                                   // print the hour (86400 equals secs per day)

      Serial.print(':');
      uint8_t mm = minute(local);                                         // uint8_t mm = minute(utc);
      if (mm < 10 ) Serial.print('0');
      Serial.print(mm);                                                   // print the minute (3600 equals secs per minute)
      Serial.print(':');
      uint8_t ss = second(local);                                         // uint8_t ss = second(utc); 
      if ( ss < 10 ) Serial.print('0');
      Serial.println(ss);                                                 // print the second
    }
  }

      if ( timeValid ) {                                                  // Keep a count of missing or bad NTP replies
          no_packet_count = 0;
      }
      else
      {
      Serial.println("\nNo NTP reply, trying again in 1 minute...");
      no_packet_count++;
      }

      if  (no_packet_count >= 10) {                                       // 10 minutes with no NTP update, force network change
            if ( WiFiSpecified == true ) ForceForget = true;              // force to no longer use specified network, look for open network from now on
            no_packet_count = 0;                                          // Reset no_packet_count
            nextSendTime = millis();                                      // After the next re-connect, make sure to get a new NTP time again quickly
            WiFiToUse = 99;                                               // Added to force look for new free WiFi network
            memset(WIFI_SSID_FOUND, 0, MAX_SSID_LEN);                     // clear the WIFI_SSID
            esp_task_wdt_deinit();                                        // no NTP, so probably a network issue, therefor temporarily disable the watchdog, so there is time to find a new network
            Serial.println("");
            Serial.println("No NTP packet in last 10 minutes, watchdog disabled, forced network change");
      }
}
//====================================================================================
// Begin decodeNTP() Decode the NTP message and print status to serial port
//====================================================================================




//====================================================================================
// Begin timeString()                                 Time string: 00:00:00
//====================================================================================
String timeString(uint32_t t_secs) {
    String timeNow = "";

    uint8_t h = t_secs / 3600;
    if ( h < 10) timeNow += "0";
    timeNow += h;

    timeNow += ":";
    uint8_t m = (t_secs - ( h * 3600 )) / 60;
    if (m < 10) timeNow += "0";
    timeNow += m;

    timeNow += ":";
    uint8_t s = t_secs - ( h * 3600 ) - ( m * 60 );
    if (s < 10) timeNow += "0";
    timeNow += s;

    return timeNow;
}
//====================================================================================
// End timeString()                                 Time string: 00:00:00
//====================================================================================




//====================================================================================
// Begin printTime()                                 Debug use only
//====================================================================================
void printTime(time_t t, char *tzCode)
{
    String dateString = dayStr(weekday(t));
    dateString += " ";
    dateString += day(t);
    if (day(t) == 1 || day(t) == 21 || day(t) == 31) dateString += "st";
    else if (day(t) == 2 || day(t) == 22) dateString += "nd";
    else if (day(t) == 3 || day(t) == 23) dateString += "rd";
    else dateString += "th";

    dateString += " ";
    dateString += monthStr(month(t));
    dateString += " ";
    dateString += year(t);

    // Print time to serial port
    Serial.print(hour(t));
    Serial.print(":");
    Serial.print(minute(t));
    Serial.print(":");
    Serial.print(second(t));
    Serial.print(" ");
    // Print time t
    Serial.print(tzCode);
    Serial.print(" ");

    // Print date
    Serial.print(day(t));
    Serial.print("/");
    Serial.print(month(t));
    Serial.print("/");
    Serial.print(year(t));
    Serial.print("  ");

    // Now test some other functions that might be useful one day!
    Serial.print(dayStr(weekday(t)));
    Serial.print(" ");
    Serial.print(monthStr(month(t)));
    Serial.print(" ");
    Serial.print(dayShortStr(weekday(t)));
    Serial.print(" ");
    Serial.print(monthShortStr(month(t)));
    Serial.println();
}
//====================================================================================
// End printTime()                                 Debug use only
//====================================================================================
