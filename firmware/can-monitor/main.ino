/**
 *
 *  This should be a photon or electron running in online mode
 *
 *  The CAN bus should be on D1/D2,
 *  and the ping sensor should be on
 *
 *
 *
 **/

#include "bitset.h"
SYSTEM_THREAD(ENABLED);


CANChannel can(CAN_D1_D2);


unsigned long lastSendTime = 0;
#define SEND_DELAY_MS 1000

bool led_state = false;

void setup() {
    Serial.begin(9600);
    pinMode(D7, OUTPUT);

    //common bauds - 50000, 100000, 125000, 250000, 500000, 1000000
    can.begin(100000);
    //can.addFilter(0x100, 0x7FF);
}

void loop() {
    unsigned int now = millis();


    // once a second, make all the LEDs blink
    if ((now - lastSendTime) > SEND_DELAY_MS) {
        lastSendTime = now;

        CANMessage message;
        message.id = 0x100;
        message.len = 1;
        message.data[0] = 0;
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

    if (msg.id == 0x201) {
        // got a distance update
        float distance = getFloat(msg.data, 0);
        Serial.println("got distance " + String(distance));

        //TODO: average it out?  publish it once a second when its different?
    }
}
