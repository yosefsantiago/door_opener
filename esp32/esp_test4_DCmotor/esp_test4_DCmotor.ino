void setup() {
    Serial.begin(115200);
    pinMode(33,OUTPUT);
    pinMode(25,OUTPUT);
}

void loop() {
    Serial.println("go left");
    digitalWrite(33,HIGH);
    digitalWrite(25,LOW);
    delay(10000); // 10s
    Serial.println("go right");
    digitalWrite(25,HIGH);
    digitalWrite(33,LOW);
    delay(10000);
}