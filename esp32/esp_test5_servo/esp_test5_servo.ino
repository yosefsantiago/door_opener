#include <ESP32Servo.h>

Servo engagingServo;
int angle = 0;

void setup() {
    Serial.begin(115200);
    pinMode(13,OUTPUT);
    engagingServo.setPeriodHertz(50);    // standard servo frequency
    engagingServo.attach(13, 500, 2500); // pulse range in microseconds
}

void loop() {
    if (angle >= 180) {
        angle = 0;
    }
    engagingServo.write(angle);
    angle += 10;
    delay(500);
}