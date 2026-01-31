#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ================= PIN DEFINITIONS =================
#define TRIG_PIN 18
#define ECHO_PIN 19
#define VIB_PIN  25

// ================= OBJECTS =================
Adafruit_VL53L0X tof;
Adafruit_MPU6050 mpu;

// ================= OBSTACLE CODES =================
#define SAFE                0
#define APPROACHING_OBJECT  1
#define SHIN_OBSTACLE       2
#define UNEVEN_GROUND       3
#define DROP_POTHOLE        4
#define IMMEDIATE_DANGER    5

// ================= GAIT THRESHOLDS =================
// (Human walking tuned values)
#define STANCE_THRESHOLD 10.5   // below → stance
#define SWING_THRESHOLD  12.5   // above → swing

// ================= FUNCTIONS =================

// ---- Ultrasonic Distance ----
long readUltrasonicCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

// ---- Gait Phase Detection (Hysteresis) ----
bool isStancePhase() {
  static bool stance = true;   // remember last state

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  float accelMag = sqrt(
    a.acceleration.x * a.acceleration.x +
    a.acceleration.y * a.acceleration.y +
    a.acceleration.z * a.acceleration.z
  );

  if (accelMag > SWING_THRESHOLD) {
    stance = false;
  } 
  else if (accelMag < STANCE_THRESHOLD) {
    stance = true;
  }

  return stance;
}

// ---- Local Vibration (temporary) ----
void vibrate(int onMs, int offMs) {
  digitalWrite(VIB_PIN, HIGH);
  delay(onMs);
  digitalWrite(VIB_PIN, LOW);
  delay(offMs);
}

// ---- UART Send to Belt ----
void sendToBelt(int obstacle, int tofCm, int ultraCm, bool stance) {
  Serial.print("$SHIN,");
  Serial.print(obstacle);
  Serial.print(",");
  Serial.print(tofCm);
  Serial.print(",");
  Serial.print(ultraCm);
  Serial.print(",");
  Serial.println(stance ? "STANCE" : "SWING");
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(VIB_PIN, OUTPUT);

  // ---- I2C INIT ----
  Wire.begin(21, 22);
  delay(200);

  // ---- MPU6050 INIT FIRST ----
  if (!mpu.begin()) {
    Serial.println("MPU6050 INIT FAILED");
  } else {
    Serial.println("MPU6050 OK");
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  delay(200);

  // ---- ToF INIT AFTER MPU ----
  if (!tof.begin()) {
    Serial.println("VL53L0X INIT FAILED");
  } else {
    Serial.println("VL53L0X OK");
  }

  Serial.println("Shin Module READY");
}

// ================= LOOP =================
void loop() {

  bool stance = isStancePhase();

  // ---- SWING PHASE ----
  if (!stance) {
    digitalWrite(VIB_PIN, LOW);
    sendToBelt(SAFE, 999, 999, false);
    delay(50);
    return;
  }

  // ---- SENSOR READINGS ----
  VL53L0X_RangingMeasurementData_t measure;
  tof.rangingTest(&measure, false);
  int tofCm = (measure.RangeStatus == 0) ? measure.RangeMilliMeter / 10 : 999;

  int ultraCm = readUltrasonicCM();

  int obstacle = SAFE;

  // ---- CORRELATED DECISION LOGIC ----
  if (tofCm < 40 && ultraCm < 20) {
    obstacle = IMMEDIATE_DANGER;
  }
  else if (ultraCm < 15) {
    obstacle = DROP_POTHOLE;
  }
  else if (ultraCm >= 15 && ultraCm <= 30) {
    obstacle = UNEVEN_GROUND;
  }
  else if (tofCm >= 40 && tofCm <= 80) {
    obstacle = SHIN_OBSTACLE;
  }
  else if (tofCm > 80 && tofCm <= 120) {
    obstacle = APPROACHING_OBJECT;
  }

  // ---- SEND TO BELT MODULE ----
  sendToBelt(obstacle, tofCm, ultraCm, true);

  // ---- TEMP LOCAL HAPTICS (REMOVE LATER) ----
  switch (obstacle) {

    case IMMEDIATE_DANGER:
      digitalWrite(VIB_PIN, HIGH);
      break;

    case DROP_POTHOLE:
      vibrate(300, 100);
      break;

    case SHIN_OBSTACLE:
      vibrate(200, 120);
      break;

    case UNEVEN_GROUND:
      vibrate(120, 120);
      break;

    case APPROACHING_OBJECT:
      vibrate(80, 200);
      break;

    default:
      digitalWrite(VIB_PIN, LOW);
  }

  delay(50);
}