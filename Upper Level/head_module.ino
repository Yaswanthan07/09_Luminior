#include <Wire.h>
#include <Adafruit_VL53L0X.h>

/* ===== PIN DEFINITIONS ===== */
#define SDA_PIN 21
#define SCL_PIN 22

#define XSHUT_FRONT 16
#define XSHUT_UP    17

#define LEFT_MOTOR  25
#define RIGHT_MOTOR 26

#define STATUS_LED  2

/* ===== DISTANCE THRESHOLDS (cm) ===== */
#define IGNORE_DISTANCE   30     // Ignore noise below this
#define SAFE_DISTANCE    80
#define DANGER_DISTANCE  50

Adafruit_VL53L0X tofFront;
Adafruit_VL53L0X tofUp;

/* ===== SETUP ===== */
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(XSHUT_FRONT, OUTPUT);
  pinMode(XSHUT_UP, OUTPUT);

  pinMode(LEFT_MOTOR, OUTPUT);
  pinMode(RIGHT_MOTOR, OUTPUT);

  pinMode(STATUS_LED, OUTPUT);

  // Turn everything OFF initially
  digitalWrite(LEFT_MOTOR, LOW);
  digitalWrite(RIGHT_MOTOR, LOW);
  digitalWrite(STATUS_LED, LOW);

  /* ===== ToF ADDRESS SETUP ===== */
  digitalWrite(XSHUT_FRONT, LOW);
  digitalWrite(XSHUT_UP, LOW);
  delay(10);

  // Front sensor
  digitalWrite(XSHUT_FRONT, HIGH);
  delay(10);
  if (!tofFront.begin(0x30)) {
    Serial.println("❌ Front ToF not detected");
    while (1);
  }

  // Upward sensor
  digitalWrite(XSHUT_UP, HIGH);
  delay(10);
  if (!tofUp.begin(0x31)) {
    Serial.println("❌ Upward ToF not detected");
    while (1);
  }

  // Ready indication
  digitalWrite(STATUS_LED, HIGH);
  delay(300);
  digitalWrite(STATUS_LED, LOW);

  Serial.println("✅ HEAD MODULE READY");
}

/* ===== VIBRATION FUNCTIONS ===== */
void vibrateLeft(int duration) {
  digitalWrite(LEFT_MOTOR, HIGH);
  delay(duration);
  digitalWrite(LEFT_MOTOR, LOW);
}

void vibrateRight(int duration) {
  digitalWrite(RIGHT_MOTOR, HIGH);
  delay(duration);
  digitalWrite(RIGHT_MOTOR, LOW);
}

void vibrateBoth(int duration) {
  digitalWrite(LEFT_MOTOR, HIGH);
  digitalWrite(RIGHT_MOTOR, HIGH);
  delay(duration);
  digitalWrite(LEFT_MOTOR, LOW);
  digitalWrite(RIGHT_MOTOR, LOW);
}

/* ===== MAIN LOOP ===== */
void loop() {

  VL53L0X_RangingMeasurementData_t frontData, upData;

  tofFront.rangingTest(&frontData, false);
  tofUp.rangingTest(&upData, false);

  int frontDist = frontData.RangeMilliMeter / 10; // cm
  int upDist    = upData.RangeMilliMeter / 10;    // cm

  Serial.print("Front: ");
  Serial.print(frontDist);
  Serial.print(" cm | Up: ");
  Serial.print(upDist);
  Serial.println(" cm");

  bool obstacleDetected = false;

  /* ===== FRONT (HEAD-LEVEL) OBSTACLE ===== */
  if (frontDist > IGNORE_DISTANCE && frontDist < SAFE_DISTANCE) {
    obstacleDetected = true;

    if (frontDist < DANGER_DISTANCE) {
      Serial.println("🚨 DANGER: HEAD-LEVEL OBSTACLE AHEAD");
      vibrateBoth(300);   // continuous danger feel
    } else {
      Serial.println("⚠ WARNING: HEAD-LEVEL OBSTACLE AHEAD");
      vibrateBoth(120);   // slow pulse
    }
  }

  /* ===== UPWARD (HANGING) OBSTACLE ===== */
  else if (upDist > IGNORE_DISTANCE && upDist < SAFE_DISTANCE) {
    obstacleDetected = true;

    if (upDist < DANGER_DISTANCE) {
      Serial.println("🚨 DANGER: HANGING OBSTACLE");
      vibrateRight(300);  // strong warning
    } else {
      Serial.println("⚠ WARNING: HANGING OBSTACLE");
      vibrateRight(120);
    }
  }

  /* ===== STATUS LED ===== */
  if (obstacleDetected) {
    digitalWrite(STATUS_LED, HIGH);
  } else {
    digitalWrite(STATUS_LED, LOW);
  }

  delay(150); // stability delay
}