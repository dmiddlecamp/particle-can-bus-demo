/**
 *
 *  This should be a photon or electron running in offline mode, connected to an ultrasonic ping sensor.
 *
 *  The CAN bus should be on D1/D2,
 *  and the ping sensor should be on
 *
 *
 *
 **/

#include "HC_SR04.h"
#include "bitset.h"


// run offline
SYSTEM_MODE(MANUAL);

int triggerPin = D4;
int echoPin = D5;


CANChannel can(CAN_D1_D2);
HC_SR04 ping(triggerPin, echoPin);

unsigned long lastSendTime = 0;
#define SEND_DELAY_MS 250

bool led_state = false;;

void setup() {
    Serial.begin(9600);
    pinMode(D7, OUTPUT);

    //common bauds - 50000, 100000, 125000, 250000, 500000, 1000000
    can.begin(100000);
    //can.addFilter(0x100, 0x7FF);
}

void loop() {
    unsigned int now = millis();


    // read from the ping sensor

    if ((now - lastSendTime) > SEND_DELAY_MS) {
        lastSendTime = now;

        double distanceCM = ping.getDistanceCM();

        //double distanceCM = -1;
        int tries = 0;
        //noInterrupts();
        //SINGLE_THREADED_BLOCK() {
        //    while ((distanceCM == -1) && (tries < 10)) {
        //        distanceCM = ping.getDistanceCM();
        //        //distanceCM = ping.getDistanceInch();
        //        //tries++;
        //    }
        //}
        //interrupts();

        Serial.println(String::format("Distance is %f, tries was %d ", distanceCM, tries));

        CANMessage message;
        message.id = 0x201;
        message.len = 8;
        setFloat(message.data, (float)distanceCM, 0);
        can.transmit(message);
    }


    receiveMessages();
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
}
