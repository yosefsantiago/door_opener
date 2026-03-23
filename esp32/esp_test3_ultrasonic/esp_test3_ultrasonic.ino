int echoPin = 14; // GPIO14
int trigPin = 12; // GPIO12

void setup() {
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    Serial.begin(115200);
}

void loop() {
    digitalWrite(trigPin,LOW);
    delay(500);
    digitalWrite(trigPin,HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin,LOW);
    float duration = pulseIn(echoPin,HIGH); // time (us)
    float distance = duration * 0.034 / 2;// converts echo time to distance (cm)
    Serial.println(distance);
}