volatile unsigned long lastPulseTime1 = 0;
volatile unsigned long pulseInterval1 = 0;

volatile unsigned long lastPulseTime2 = 0;
volatile unsigned long pulseInterval2 = 0;

double wheelSpeed = 0; 

void countPulse1() {
    unsigned long currentTime = micros();
    if (lastPulseTime1 > 0) {
        pulseInterval1 = currentTime - lastPulseTime1;
    }
    lastPulseTime1 = currentTime;
}

void countPulse2() {
    unsigned long currentTime = micros();
    if (lastPulseTime2 > 0) {
        pulseInterval2 = currentTime - lastPulseTime2;
    }
    lastPulseTime2 = currentTime;
}

void setup() {
    Serial.begin(9600);
    pinMode(2, INPUT_PULLUP);
    pinMode(3, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(2), countPulse1, RISING);
    attachInterrupt(digitalPinToInterrupt(3), countPulse2, RISING);
}

void loop() {
    // Default values before pulse detection
    float rpm1 = 0;
    float rpm2 = 0;

    // Only update values if we have valid pulse intervals
    if (pulseInterval1 > 0) {
        rpm1 = (60.0 * 1000000.0) / pulseInterval1;
    }
    if (pulseInterval2 > 0) {
        rpm2 = (60.0 * 1000000.0) / pulseInterval2;
    }

    // Compute wheel speed only if rpm2 is valid
    if (rpm2 > 0) { 
        wheelSpeed = ((11.5 / 12) * ((((rpm2) / 2.75)) / 2.75) * (0.10467)) * 0.68166;
    } else {
        wheelSpeed = 0; // Ensure wheel speed reads 0 if no pulses occur
    }

    // Print output
    Serial.print(millis());
    Serial.print(" Primary: "); 
    Serial.print(rpm1 / 2);
    Serial.print(" ");
    Serial.print("Secondary: "); 
    Serial.print(rpm2);
    Serial.print(" Wheel Speed: ");
    Serial.println(wheelSpeed); 

    delay(100);
}
