
#include <Arduino.h>
#include <HX711.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define DOUT 20
#define SCK 21



HX711 scale;
const float calibration_factor = 420.0; // 보정값, 테스트를 통해 교정

// BLE 설정
#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "abcdef01-1234-5678-1234-56789abcdef0"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

// 서버 콜백 클래스
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("✅ BLE 클라이언트 연결됨!");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("🔄 BLE 클라이언트 연결 끊김. 대기 중...");
    }
};



void setup() {
    Serial.begin(115200);
    scale.begin(DOUT, SCK);

    Serial.println("🔵 HX711 로드셀 시작 중...");

    while (!scale.is_ready()) {
        Serial.println("❌ HX711 연결 실패. 배선 확인 필요!");
        delay(1000);
    }

    scale.set_scale(calibration_factor);
    scale.tare();
    Serial.println("✅ 초기화 완료!");

    // BLE 초기화
    BLEDevice::init("ESP32-S3 BLE Scale");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ   | 
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    pCharacteristic->setValue("0.0");
    pService->start();

    // BLE 광고 시작
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("📡 BLE 서버 시작 완료!");
}

void loop() {
    if (deviceConnected) {
        float weight = scale.get_units(10); // 10회 평균 측정
        Serial.print("무게 (g): ");
        Serial.println(weight, 2);

        String weightStr = String(weight, 2);
        pCharacteristic->setValue(weightStr.c_str());
        pCharacteristic->notify();  // BLE Notify로 데이터 전송
        Serial.println("📤 BLE 전송: " + weightStr);
    } else {
        Serial.println("⏳ BLE 클라이언트 연결 대기 중...");
    }

    delay(1000); // 데이터 측정 주기
}

