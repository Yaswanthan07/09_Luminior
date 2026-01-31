#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <BluetoothSerial.h>

// ---------------- OBJECTS ----------------
Adafruit_VL53L0X lox;
MPU6050 mpu;
TinyGPSPlus gps;
BluetoothSerial SerialBT;

// ---------------- PINS ----------------
#define TRIG_LEFT   32
#define ECHO_LEFT   33
#define TRIG_RIGHT  12
#define ECHO_RIGHT  13

#define MOTOR_LEFT  14
#define MOTOR_RIGHT 27

#define BUZZER 18
#define LED    19

#define GPS_RX 16
#define GPS_TX 17

// ---------------- VARIABLES ----------------
long leftDist, rightDist;
int centerDist;
float accMag, tilt;

// Emergency contact (phone handles SMS)
String emergencyContact = "+91XXXXXXXXXX";

// ---------------- FUNCTIONS ----------------
long readUltrasonic(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 25000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void vibrateLeft(int onT, int offT) {
  digitalWrite(MOTOR_LEFT, HIGH);
  delay(onT);
  digitalWrite(MOTOR_LEFT, LOW);
  delay(offT);
}

void vibrateRight(int onT, int offT) {
  digitalWrite(MOTOR_RIGHT, HIGH);
  delay(onT);
  digitalWrite(MOTOR_RIGHT, LOW);
  delay(offT);
}

void vibrateBoth(int onT, int offT) {
  digitalWrite(MOTOR_LEFT, HIGH);
  digitalWrite(MOTOR_RIGHT, HIGH);
  delay(onT);
  digitalWrite(MOTOR_LEFT, LOW);
  digitalWrite(MOTOR_RIGHT, LOW);
  delay(offT);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  SerialBT.begin("Belt_Emergency");

  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);
  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);

  pinMode(MOTOR_LEFT, OUTPUT);
  pinMode(MOTOR_RIGHT, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);

  digitalWrite(LED, HIGH);
  digitalWrite(BUZZER, HIGH); delay(150); digitalWrite(BUZZER, LOW);

  Wire.begin();

  if (!lox.begin()) {
    Serial.println("❌ ToF not detected");
  }

  mpu.initialize();

  Serial2.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  Serial.println("✅ BELT MODULE CENTRAL BRAIN READY");
}

// ---------------- LOOP ----------------
void loop() {

  // -------- ULTRASONIC --------
  leftDist = readUltrasonic(TRIG_LEFT, ECHO_LEFT);
  rightDist = readUltrasonic(TRIG_RIGHT, ECHO_RIGHT);

  // -------- TOF --------
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);
  centerDist = (measure.RangeStatus == 0) ? measure.RangeMilliMeter / 10 : 999;

  Serial.print("LEFT: "); Serial.print(leftDist);
  Serial.print(" | CENTER: "); Serial.print(centerDist);
  Serial.print(" | RIGHT: "); Serial.println(rightDist);

  // -------- HAPTIC LOGIC --------
  if (leftDist < 40) vibrateLeft(200, 0);
  else if (leftDist < 80) vibrateLeft(150, 150);
  else if (leftDist < 150) vibrateLeft(300, 300);

  if (rightDist < 40) vibrateRight(200, 0);
  else if (rightDist < 80) vibrateRight(150, 150);
  else if (rightDist < 150) vibrateRight(300, 300);

  if (centerDist < 40) vibrateBoth(200, 0);
  else if (centerDist < 80) vibrateBoth(150, 150);

  // -------- FALL DETECTION --------
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  accMag = sqrt(ax*ax + ay*ay + az*az) / 16384.0;
  tilt = abs(atan2(ay, az) * 57.3);

  if (accMag > 2.7 && tilt > 50) {
    Serial.println("🚨 EXTREME FALL DETECTED");

    digitalWrite(BUZZER, HIGH);
    vibrateBoth(500, 100);

    while (Serial2.available()) gps.encode(Serial2.read());

    if (gps.location.isValid()) {
      String msg = "EXTREME FALL!\nLAT: " + String(gps.location.lat(), 6) +
                   "\nLON: " + String(gps.location.lng(), 6) +
                   "\nCONTACT: " + emergencyContact;

      SerialBT.println(msg);
      Serial.println("📲 Emergency sent via Bluetooth");
    }
  }

  digitalWrite(BUZZER, LOW);
  delay(100);
}