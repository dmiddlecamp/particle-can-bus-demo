// This #include statement was automatically added by the Spark IDE.
#include "Adafruit_GPS.h"
#include "Adafruit_LIS3DH.h"
#include "GPS_Math.h"

#include <math.h>
#include "math.h"
#include <ctype.h>

#define STARTING_LATITUDE_LONGITUDE_ALTITUDE "44.9778,-93.2650,200"
uint8_t internalANT[]={0xB5,0x62,0x06,0x13,0x04,0x00,0x00,0x00,0xF0,0x7D,0x8A,0x2A};
uint8_t externalANT[]={0xB5,0x62,0x06,0x13,0x04,0x00,0x01,0x00,0xF0,0x7D,0x8B,0x2E};


#define mySerial Serial1
Adafruit_GPS GPS(&mySerial);
Adafruit_LIS3DH accel = Adafruit_LIS3DH(A2, A5, A4, A3);
FuelGauge fuel;

#define MY_NAME "AssetTrackerPro"

#define CLICKTHRESHHOLD 100


int lastSecond = 0;
bool ledState = false;
float lastLevel = 0;

// lets keep the radio off until we get a fix, or 2 minutes go by.
//SYSTEM_MODE(SEMI_AUTOMATIC);


#include "bitset.h"
CANChannel can(CAN_D1_D2);

float canDistanceValue = 0;
unsigned long lastSendTime = 0;
bool led_state = false;

STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));


//
//  PRODUCT SETTINGS
//

//PRODUCT_ID(4211);
//PRODUCT_VERSION(1);

//
//  How often do we use the CAN-BUS to synchronize LEDs?
//

#define SEND_DELAY_MS 100




unsigned long lastMotion = 0;
unsigned long lastPublish = 0;
unsigned long lastReading = 0;
time_t lastIdleCheckin = 0;

#define PUBLISH_DELAY (60 * 1000)

// if no motion for 3 minutes, sleep! (milliseconds)
#define NO_MOTION_IDLE_SLEEP_DELAY (3 * 60 * 1000)

// lets wakeup every 6 hours and check in (seconds)
#define HOW_LONG_SHOULD_WE_SLEEP (6 * 60 * 60)

// when we wakeup from deep-sleep not as a result of motion,
// how long should we wait before we publish our location?
// lets set this to less than our sleep time, so we always idle check in.
// (seconds)
#define MAX_IDLE_CHECKIN_DELAY (HOW_LONG_SHOULD_WE_SLEEP - 60)


//
//
//loop
//set wakeOnMotion with accelerometer
//<wake>
//send GPS every 1? minute while in motion (or just publish start and end of trip)
//when no motion for 3 minutes - check battery, publish if low
//if after 6:30 PM - deep sleep till morning
//wake on morning - send position and check battery


void setup() {
    // mirror RGB PINS
    RGB.mirrorTo(B3,B2,B1,true, true);

    // water sensor
    pinMode(A1, OUTPUT);
    pinMode(A0, INPUT);

    digitalWrite(A1, HIGH);         // power pin for water sensor

    lastMotion = 0;
    lastPublish = 0;

    // electron asset tracker shield needs this to enable the power to the gps module.
    pinMode(D6, OUTPUT);
    digitalWrite(D6, LOW);

    // for blinking.
    pinMode(D7, OUTPUT);
    digitalWrite(D7, LOW);

    // wait a little for the GPS to wakeup
    delay(250);

    GPS.begin(9600);
    mySerial.begin(9600);
    Serial.begin(9600);


    //# request a HOT RESTART, in case we were in standby mode before.
    GPS.sendCommand("$PMTK101*32");
    delay(250);

    // request everything!
    //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_ALLDATA);
    //delay(250);

    // turn off antenna updates
    GPS.sendCommand(PGCMD_NOANTENNA);
    delay(250);

    delay(2000);    // give the module a long time to warm up just in case?

    // select internal antenna
    antennaSelect(internalANT);
    //antennaSelect(externalANT);


    //suggest_time_and_location();

    initAccel();


    can.begin(100000);

}



void loop() {
    if (lastIdleCheckin == 0) {
        lastIdleCheckin = Time.now();
    }

    unsigned long now = millis();

    if (lastMotion > now) { lastMotion = now; }
    //if (lastPublish > now) { lastPublish = now; }

    checkGPS();

    // we'll be woken by motion, lets keep listening for more motion.
    // if we get two in a row, then we'll connect to the internet and start reporting in.
    bool hasMotion = digitalRead(WKP);
    digitalWrite(D7, (hasMotion) ? HIGH : LOW);
    if (hasMotion) {
        Serial.println("BUMP!");
        lastMotion = now;

        if (Particle.connected() == false) {
            Serial.println("CONNECTING DUE TO MOTION!");
            Particle.connect();
        }
    }

    // use the real-time-clock here, instead of millis.
    if ((Time.now() - lastIdleCheckin) >= MAX_IDLE_CHECKIN_DELAY) {

        // it's been too long!  Lets say hey!
        if (Particle.connected() == false) {
            Serial.println("CONNECTING DUE TO IDLE!");
            Particle.connect();
        }

        Particle.publish(MY_NAME + String("_status"), "miss you <3");
        lastIdleCheckin = Time.now();
    }


    // have we published recently?
    //Serial.println("lastPublish is " + String(lastPublish));
    if (((millis() - lastPublish) > PUBLISH_DELAY) || (lastPublish == 0)) {
        lastPublish = millis();

        publishGPS();
    }


    publishLevel();

    //
    //  How often do we use the CAN-BUS to synchronize LEDs?
    //
    if ((now - lastSendTime) > SEND_DELAY_MS) {
        lastSendTime = now;

        CANMessage message;
        message.id = 0x100;
        message.len = 1;
        message.data[0] = 0;
        can.transmit(message);
    }

    receiveMessages();

    delay(10);
}


void checkGPS() {
    // process and dump everything from the module through the library.
    while (mySerial.available()) {
        char c = GPS.read();

        if (GPS.newNMEAreceived()) {
            Serial.println(GPS.lastNMEA());
            GPS.parse(GPS.lastNMEA());

            //Serial.println("my location is " + String::format(" %f, %f, ", GPS.latitude, GPS.longitude));
        }
    }


    //+ "\"c_lat\":" + String(convertDegMinToDecDeg(GPS.latitude))
    //+ ",\"c_lng\":" + String(convertDegMinToDecDeg(GPS.longitude))
}

void initAccel() {
    accel.begin(LIS3DH_DEFAULT_ADDRESS);

    // Default to 5kHz low-power sampling
    accel.setDataRate(LIS3DH_DATARATE_LOWPOWER_5KHZ);

    // Default to 4 gravities range
    accel.setRange(LIS3DH_RANGE_4_G);

    // listen for single-tap events at the threshold
    // keep the pin high for 1s, wait 1s between clicks

    //uint8_t c, uint8_t clickthresh, uint8_t timelimit, uint8_t timelatency, uint8_t timewindow
    accel.setClick(1, CLICKTHRESHHOLD);//, 0, 100, 50);
}

void publishGPS() {
    unsigned int msSinceLastMotion = (millis() - lastMotion);
    int motionInTheLastMinute = (msSinceLastMotion < 60000);

    /*
    String gps_line = String::format("%f,%f,%f,%f,
        convertDegMinToDecDeg(GPS.latitude), convertDegMinToDecDeg(GPS.longitude), GPS.altitude, GPS.speed);
    */

//    String gps_line =
//          "{\"lat\":"    + String(convertDegMinToDecDeg(GPS.latitude))
//        + ",\"lon\":-"   + String(convertDegMinToDecDeg(GPS.longitude))
//        + ",\"a\":"     + String(GPS.altitude)
//        + ",\"q\":"     + String(GPS.fixquality)
//        + ",\"spd\":"   + String(GPS.speed)
//        + ",\"mot\":"   + String(motionInTheLastMinute)
//        + ",\"s\": "  + String(GPS.satellites)
//        + ",\"vcc\":"   + String(fuel.getVCell())
//        + ",\"soc\":"   + String(fuel.getSoC())
//        + "}";
//
//    Particle.publish(MY_NAME + String("_location"), gps_line, 60, PRIVATE);



    float latitude = convertDegMinToDecDeg(GPS.latitude);
    float longitude = convertDegMinToDecDeg(GPS.longitude);

    if ((latitude != 0) && (longitude != 0)) {
        String trkJsonLoc = String("{")
            + "\"c_lat\":" + String(convertDegMinToDecDeg(GPS.latitude))
            + ",\"c_lng\":" + String(convertDegMinToDecDeg(GPS.longitude))
            + ",\"c_unc\":" + String(GPS.fixquality)
            + ",\"c_alt\":" + String(GPS.altitude)
            + "}";
         Particle.publish("trk/loc", trkJsonLoc, 60, PRIVATE);
     }


    float batteryVoltage = fuel.getVCell();
    CellularSignal signalInfo = Cellular.RSSI();
    String devJson = String("{")
            + "\"vcell\":" + String::format("%.4f", batteryVoltage)
            + ",\"cell_rssi\":" + String(signalInfo.rssi)
            + ",\"cell_qual\":" + String(signalInfo.qual)
            + "}";
     Particle.publish("trk/dev", devJson, 60, PRIVATE);


    publishLevel();

//          int value = rand() * 100;
//     String sensorJson = String("{")
//            + "\"level\":" + String::format("%d", value)
//            + "}";
//     Particle.publish("trk/level", sensorJson, 60, PRIVATE);
}



void publishLevel() {
    float levelValue = canDistanceValue;//getLevelReading();

    accel.read();
    float aX = accel.x;
    float aY = accel.y;
    float aZ = accel.z;

    //float temperatureC = ((3300*analogRead(A0)/4096.0)-500)/10.0;
    //float temperatureF = (temperatureC * (9/5)) + 32;
    int temperatureF = 70;

     unsigned int now = millis();
     if ((levelValue == lastLevel) || ((now - lastReading) < 2500)) {
        return;
     }
     lastReading = now;
     lastLevel = levelValue;

     String sensorJson = String("{")
            + "\"level\":" + String::format("%f", levelValue)
            + ",\"tempF\":" + String::format("%d", temperatureF)
            + ",\"x\":" + String::format("%.2f", aX)
            + ",\"y\":" + String::format("%.2f", aY)
            + ",\"z\":" + String::format("%.2f", aZ)

            + "}";
     Particle.publish("trk/env", sensorJson, 60, PRIVATE);
}


//int getLevelReading() {
//
//    //
//    int emptyLevelValue = 3500;
//    int fullLevelValue = 2460;
//    // about 2 inches of water ->
//    //int levelValue = analogRead(D0) - 2434;
//
//    //delay(50);
//    int levelReading = analogRead(A0);
//    int levelValue = map(levelReading, fullLevelValue, emptyLevelValue, 0, 100);
//    levelValue = 100 - levelValue;  // flip it
//
//    Serial.println("water level is " + String(levelReading) + " percentage full is " + String(levelValue));
//
//    return levelValue;
//}

int crc8(String str) {
  int len = str.length();
  const char * buffer = str.c_str();

  int crc = 0;
  for(int i=0;i<len;i++) {
    crc ^= (buffer[i] & 0xff);
  }
  return crc;
}


void suggest_time_and_location() {
    String locationString = STARTING_LATITUDE_LONGITUDE_ALTITUDE;


    //TODO: guarantee that the clock has actually been set by the cloud, and we're not early.
    //Particle.syncTime();

    time_t timeValue = Time.now();
    String timeString = Time.format(timeValue, "%Y,%m,%d,%H,%M,%S");

    Particle.publish("GPS", "Time was " + String(timeString));


    //  PMTK740,YYYY,MM,DD,hh,mm,ss*CS<CR><LF>
    //  PMTK741,Lat,Long,Alt,YYYY,MM,DD,hh,mm,ss *CS<CR><LF>
    //The packet contains reference location for the GPS receiver. To have faster TTFF, the accuracy of the location shall be better than 30km.
    String gpsTimeCmd = "PMTK740," + timeString;
    String locationTimeCmd = "PMTK741,"+locationString + "," + timeString;


    String cmd = String::format("$%s*%02x", gpsTimeCmd.c_str(), crc8(gpsTimeCmd));
    mySerial.println(cmd);
    //GPS.sendCommand(cmd.c_str());     // why doesn't this support const char *...
    delay(250);


    cmd = String::format("$%s*%02x", locationTimeCmd.c_str(), crc8(locationTimeCmd));
    mySerial.println(cmd);
    //GPS.sendCommand(cmd.c_str());     // why doesn't this support const char *...
    delay(250);

}



void antennaSelect(uint8_t *buf){

    for(uint8_t i=0;i<12;i++)
    {
        Serial1.write(buf[i]); //send the command to gps module
        Serial.print(buf[i],HEX);
        Serial.print(",");
    }
    Serial.println("");
}

void receiveMessages() {
    if(can.available() <= 0) {
        return;
    }

    // we have messages

    CANMessage msg;
    if(!can.receive(msg)) {
        // there was a problem getting the message!
        return;
    }

    Serial.println(String::format("Msg: %d, Length: %d ", msg.id, msg.len));


    // every time we get message id 100, lets blink our LED.
    if (msg.id == 0x100) {
        led_state = !led_state;
        digitalWrite(D7, (led_state) ? HIGH:LOW);
    }

    if (msg.id == 0x201) {
        // got a distance update
        float distance = getFloat(msg.data, 0);
        canDistanceValue = distance;
        Serial.println("got distance " + String(distance));

        //TODO: average it out?  publish it once a second when its different?
    }
}
