#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <LiquidCrystal.h>

// Pin Definitions
#define CE_PIN   9
#define CSN_PIN 10
#define FLOWSENSORPIN 8

const int hallPrimary = 2; 
const int hallSecondary = 3; 

// RPM Measurement Variables
volatile unsigned long lastPulseTime1 = 0, pulseInterval1 = 0;
volatile unsigned long lastPulseTime2 = 0, pulseInterval2 = 0;
double wheelSpeed = 0;

// Fuel Sensor Variables
volatile uint16_t pulses = 0;
volatile uint8_t lastflowpinstate;
volatile uint32_t lastflowratetimer = 0;
volatile float flowrate;

// NRF24L01+ Setup
const byte rxAddr[6] = "00001";
RF24 radio(CE_PIN, CSN_PIN);

// LCD Setup
const int rs = 7, en = 6, d4 = 5, d5 = 4;
LiquidCrystal lcd(rs, en, d4, d5, A2, A3);

// Interrupt Service Routines for Hall Effect Sensors
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

// Fuel Sensor Interrupt
SIGNAL(TIMER0_COMPA_vect) {
    uint8_t x = digitalRead(FLOWSENSORPIN);
    if (x == lastflowpinstate) {
        lastflowratetimer++;
        return;
    }
    if (x == HIGH) {
        pulses++;
    }
    lastflowpinstate = x;
    flowrate = 1000.0 / lastflowratetimer;
    lastflowratetimer = 0;
}

void useInterrupt(boolean v) {
    if (v) {
        OCR0A = 0xAF;
        TIMSK0 |= _BV(OCIE0A);
    } else {
        TIMSK0 &= ~_BV(OCIE0A);
    }
}

void setup() {
    Serial.begin(9600);
    pinMode(hallPrimary, INPUT_PULLUP);
    pinMode(hallSecondary, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(hallPrimary), countPulse1, RISING);
    attachInterrupt(digitalPinToInterrupt(hallSecondary), countPulse2, RISING);
    
    lcd.begin(16, 2);
    pinMode(FLOWSENSORPIN, INPUT);
    digitalWrite(FLOWSENSORPIN, HIGH);
    lastflowpinstate = digitalRead(FLOWSENSORPIN);
    useInterrupt(true);
    
    radio.begin();
    radio.setPALevel(RF24_PA_MIN);
    radio.setDataRate(RF24_250KBPS);
    radio.setRetries(15, 15);
    radio.openWritingPipe(rxAddr);
    radio.stopListening();
}

void loop() {
    float rpm1 = (pulseInterval1 > 0) ? (60.0 * 1000000.0) / pulseInterval1 : 0;
    float rpm2 = (pulseInterval2 > 0) ? (60.0 * 1000000.0) / pulseInterval2 : 0;
    
    if (rpm2 > 0) {
        rpm2 /= 2;
        wheelSpeed = ((11.5 / 12) * (((rpm2 / 2.75) / 2.75) * 0.10467)) * 0.68166;
    } else {
        wheelSpeed = 0;
    }
    
    float liters = pulses / (70.0 * 60.0);
    float gallons = liters / 3.785;
    float initialVol = 2;
    int fuelPercentage = ((initialVol - gallons) / initialVol) * 100;
    
    char rpm1Str[8], rpm2Str[8], wsStr[8];
    dtostrf(rpm1, 0, 2, rpm1Str);  // Convert float to string
    dtostrf(rpm2, 0, 2, rpm2Str);
    dtostrf(wheelSpeed, 0, 2, wsStr);

    char dataBuffer[32];
    snprintf(dataBuffer, sizeof(dataBuffer), "F:%d%% P:%s %s WS:%s", fuelPercentage, rpm1Str, rpm2Str, wsStr);

    
    radio.write(&dataBuffer, sizeof(dataBuffer));
    
    Serial.println(dataBuffer);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("F:"); lcd.print(fuelPercentage); lcd.print("% P1:"); lcd.print(rpm1);
    lcd.setCursor(0, 1);
    lcd.print("P2:"); lcd.print(rpm2); lcd.print(" WS:"); lcd.print(wheelSpeed);
    
    delay(1000);
}

// Load in the libraries
// #include <SPI.h>
// #include <nRF24L01.h>
// #include <RF24.h>
// #include <LiquidCrystal.h>

// // Set the CE & CSN pins
// #define CE_PIN   9
// #define CSN_PIN 10
// #define FLOWSENSORPIN 8

// volatile unsigned long lastPulseTime1 = 0;
// volatile unsigned long pulseInterval1 = 0;

// volatile unsigned long lastPulseTime2 = 0;
// volatile unsigned long pulseInterval2 = 0;

// String fuelString;

// double wheelSpeed = 0; 

// void countPulse1() {
//     unsigned long currentTime = micros();
//     if (lastPulseTime1 > 0) {
//         pulseInterval1 = currentTime - lastPulseTime1;
//     }
//     lastPulseTime1 = currentTime;
// }

// void countPulse2() {
//     unsigned long currentTime = micros();
//     if (lastPulseTime2 > 0) {
//         pulseInterval2 = currentTime - lastPulseTime2;
//     }
//     lastPulseTime2 = currentTime;
// }

// const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
// LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// // This is the address used to send/receive
// const byte rxAddr[6] = "00001";

// // Create a Radio
// RF24 radio(CE_PIN, CSN_PIN); 



// // count how many pulses!
// volatile uint16_t pulses = 0;
// // track the state of the pulse pin
// volatile uint8_t lastflowpinstate;
// // you can try to keep time of how long it is between pulses
// volatile uint32_t lastflowratetimer = 0;
// // and use that to calculate a flow rate
// volatile float flowrate;
// // Interrupt is called once a millisecond, looks for any pulses from the sensor!
// SIGNAL(TIMER0_COMPA_vect) {
//   uint8_t x = digitalRead(FLOWSENSORPIN);
  
//   if (x == lastflowpinstate) {
//     lastflowratetimer++;
//     return; // nothing changed!
//   }
  
//   if (x == HIGH) {
//     //low to high transition!
//     pulses++;
//   }
//   lastflowpinstate = x;
//   flowrate = 1000.0;
//   flowrate /= lastflowratetimer;  // in hertz
//   lastflowratetimer = 0;
// }

// void useInterrupt(boolean v) {
//   if (v) {
//     // Timer0 is already used for millis() - we'll just interrupt somewhere
//     // in the middle and call the "Compare A" function above
//     OCR0A = 0xAF;
//     TIMSK0 |= _BV(OCIE0A);
//   } else {
//     // do not call the interrupt function COMPA anymore
//     TIMSK0 &= ~_BV(OCIE0A);
//   }
// }

// void setup() {
  
//   pinMode(0, INPUT_PULLUP);
//   pinMode(1, INPUT_PULLUP);

//   attachInterrupt(digitalPinToInterrupt(0), countPulse1, RISING);
//   attachInterrupt(digitalPinToInterrupt(1), countPulse2, RISING);

//   lcd.begin(16,2); 
//   // Start up the Serial connection
//   while (!Serial);
//   Serial.begin(9600);
//   Serial.println("NRF24L01P Receiver Starting...");

//   pinMode(FLOWSENSORPIN, INPUT);
//    digitalWrite(FLOWSENSORPIN, HIGH);
//    lastflowpinstate = digitalRead(FLOWSENSORPIN);
//    useInterrupt(true);
  
//   // Start the Radio!
//   radio.begin();
  
//   // Power setting. Due to likelihood of close proximity of the devices, set as RF24_PA_MIN (RF24_PA_MAX is default)
//   radio.setPALevel(RF24_PA_LOW); // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  
//   // Slower data rate for better range
//   radio.setDataRate(RF24_250KBPS); // RF24_250KBPS, RF24_1MBPS, RF24_2MBPS
  
//   // Number of retries and set tx/rx address
//   radio.setRetries(15, 15);
//   radio.openWritingPipe(rxAddr);

//   // Stop listening, so we can send!
//   radio.stopListening();
// }

// void loop() {

//   // Default values before pulse detection
//     float rpm1 = 0;
//     float rpm2 = 0;

//     // Only update values if we have valid pulse intervals
//     if (pulseInterval1 > 0) {
//         rpm1 = (60.0 * 1000000.0) / pulseInterval1;
//     }
//     if (pulseInterval2 > 0) {
//         rpm2 = (60.0 * 1000000.0) / pulseInterval2;
//     }

//     // Compute wheel speed only if rpm2 is valid
//     if (rpm2 > 0) {
//         rpm2 = rpm2 / 2; 
//         wheelSpeed = ((11.5 / 12) * ((((rpm2) / 2.75)) / 2.75) * (0.10467)) * 0.68166;
//     } else {
//         wheelSpeed = 0; // Ensure wheel speed reads 0 if no pulses occur
//     }

  
//   float liters = pulses;
//   liters /= 70;
//   liters /= 60.0;

//   float gallons = liters / 3.785;
//   float initialVol = 2;
//   int percentage = ((initialVol - gallons) / (initialVol)) * 100; 

//   fuelString = percentage; 
//   int str_length = fuelString.length() + 1; 

//   char char_array[str_length]; 

//   fuelString.toCharArray(char_array, str_length); 

//   // Ace, let's now send the message
//   radio.write(&char_array, sizeof(char_array));
  
//   // Let the ourside world know..
//   lcd.setCursor(0,0);
//   lcd.print("Fuel:"); 
//   Serial.print("Fuel: "); 
//   Serial.print(char_array);
//   Serial.println("%");
//   lcd.print(percentage); 
//   lcd.print("%"); 
  
//   // Wait a short while before sending the other one
//   delay(1000);
//   lcd.clear(); 
// }
