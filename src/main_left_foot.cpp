#include <Arduino.h>
#include "global.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "esp_sleep.h"
#include "HX711.h"  // 로드셀 사용 중이므로 다시 추가

#define DOUT 20
#define SCK 21
#define THRESHOLD 5.0
#define uS_TO_S_FACTOR 1000000ULL
#define SLEEP_DURATION 300

HX711 scale;
Adafruit_MPU6050 mpu;

float calibration_factor = 420.0;
int retryCount = 0;
bool deviceConnected = false;
bool measureWeight = false;

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
BLECharacteristic* pWriteCharacteristic = nullptr;

#define SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "abcdef01-1234-5678-1234-56789abcdef0"
#define WRITE_CHARACTERISTIC_UUID "abcdef02-1234-5678-1234-56789abcdef0"

void enterLightSleep(uint64_t sleepTimeMs) {
    Serial.println("😴 Light Sleep 모드 진입...");
    esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000);
    esp_light_sleep_start();
    Serial.println("🌞 깨어났습니다!");
}

// ✅ 자세 평가 함수 (예: pitch가 너무 기울어졌는지)
bool evaluatePosture(float weight, float pitch) {
    if (abs(weight) < 5.0) return false;
    return pitch > -10 && pitch < 10;
}

// ✅ pitch 각도 계산 함수
float getPitchAngle() {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float ax = a.acceleration.x;
    float ay = a.acceleration.y;
    float az = a.acceleration.z;

    float pitch = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
    return pitch;
}


class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("✅ BLE 왼쪽 클라이언트 연결됨!");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("🔄 BLE 왼쪽 클라이언트 연결 끊김. 대기 중...");
        BLEDevice::startAdvertising();
    }
};

class WriteCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value == "measure") {
            measureWeight = true;
            Serial.println("📥 '왼쪽 무게 측정' 명령 수신!");
        }
    }
};


void setup() {
    Serial.begin(115200);
    delay(1000);  // 센서 초기화 기다림

    // ✅ 로드셀 초기화
    scale.begin(DOUT, SCK);
    scale.set_scale(calibration_factor);
    scale.tare();
    Serial.println("✅ HX711 초기화 완료!");

    // ✅ MPU6050 초기화
    if (!mpu.begin()) {
        Serial.println("❌ MPU6050 연결 실패!");
        delay(10);
    }
    Serial.println("✅ MPU6050 초기화 완료!");

    // ✅ BLE 초기화
    BLEDevice::init("ESP32-S3 BLE Scale");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->setValue("0.0");

    pWriteCharacteristic = pService->createCharacteristic(
        WRITE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pWriteCharacteristic->setCallbacks(new WriteCallbacks());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("📡 BLE 왼쪽 서버 시작 완료!");
}

void loop() {
    if (deviceConnected) {
        // 무게와 자세 데이터를 지속적으로 측정
        float weight = scale.get_units(10); // 로드셀 데이터 (10회 평균)
        float pitch = getPitchAngle();      // MPU6050에서 pitch 각도 계산
        bool posture = evaluatePosture(weight, pitch); // 자세 평가

        // 데이터 출력
        Serial.print("무게 (g): ");
        Serial.print(weight, 2);
        Serial.print(" | Pitch: ");
        Serial.print(pitch, 2);
        Serial.print(" | 자세: ");
        Serial.println(posture ? "GOOD" : "BAD");
        delay(100); // 데이터 출력 주기 (100ms)

        // BLE 특성에 데이터 설정 및 알림 전송
        String sendData = String(weight, 2) + "," + String(pitch, 2) + "," + (posture ? "GOOD" : "BAD");
        pCharacteristic->setValue(sendData.c_str());
        pCharacteristic->notify();

        delay(100); // 데이터 전송 주기 (100ms)
    } 
    }

