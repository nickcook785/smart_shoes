#include <Arduino.h>
#include "global.h"
#include "HX711.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "esp_sleep.h"  // 💡 Deep Sleep용 라이브러리 추가

#define DOUT 20
#define SCK 21
#define THRESHOLD 5.0  // 💡 무게 임계값
#define uS_TO_S_FACTOR 1000000ULL
#define SLEEP_DURATION 30  // 💡 Deep Sleep 지속 시간 (초)

HX711 scale;

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

// 💡 Deep Sleep 함수 정의
void enterDeepSleep() {
    Serial.println("💤 무게 없음! Deep Sleep 모드 진입...");
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION * uS_TO_S_FACTOR);
    Serial.flush(); // 시리얼 데이터 전송 마무리
    esp_deep_sleep_start();
}

// BLE 연결 콜백
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

// BLE 쓰기 콜백
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
    scale.begin(DOUT, SCK);

    Serial.println("🔵 HX711 로드셀 시작 중...");
    while (!scale.is_ready() && retryCount < 10) {
        Serial.println("❌ HX711 연결 실패. 배선 확인 필요!");
        delay(1000);
        retryCount++;
    }

    scale.set_scale(calibration_factor);
    scale.tare();
    Serial.println("✅ 초기화 완료!");

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
    if (deviceConnected && measureWeight) {
        float weight = scale.get_units(10);
        Serial.print("무게 (g): ");
        Serial.println(weight, 2);

        pCharacteristic->setValue((uint8_t*)&weight, sizeof(weight));
        pCharacteristic->notify();
        Serial.println("📤 BLE 왼쪽 전송 완료!");

        // 💡 무게가 임계값보다 작으면 슬립
        if (abs(weight) < THRESHOLD) {
            enterDeepSleep();
        }

        measureWeight = false;
    }

    delay(100);
}
