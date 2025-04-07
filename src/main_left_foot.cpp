#include <Arduino.h>
#include <HX711.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define DOUT 20
#define SCK 21

// BLE 설정
#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "abcdef01-1234-5678-1234-56789abcdef0"
#define WRITE_CHARACTERISTIC_UUID "abcdef02-1234-5678-1234-56789abcdef0" // 쓰기용 UUID

int retryCount = 0; // HX711 연결 재시도 카운트

HX711 scale;
float calibration_factor = 420.0; // 초기 보정값

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pWriteCharacteristic = NULL;
bool deviceConnected = false;
bool measureWeight = false; // 무게 측정 논리

// BLE 서버 콜백 클래스
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("✅ BLE 왼쪽 클라이언트 연결됨!");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("🔄 BLE 왼쪽 클라이언트 연결 끊김. 대기 중...");
        BLEDevice::startAdvertising(); // 재광고 시작
        Serial1.println("🔄 BLE 왼쪽 클라이언트 연결 끊김. 대기 중...");
    }


};

// BLE 쓰기 요청 처리 콜백 클래스
class WriteCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value == "measure") { // "measure" 명령 수신
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

    // BLE 초기화
    BLEDevice::init("ESP32-S3 BLE Scale");

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    // 읽기 및 알림용 특성
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->setValue("0.0");

    // 쓰기용 특성
    pWriteCharacteristic = pService->createCharacteristic(
        WRITE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pWriteCharacteristic->setCallbacks(new WriteCallbacks()); // 쓰기 요청 처리 콜백 설정

    pService->start();

    // BLE 광고 시작
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("📡 BLE 왼쪽 서버 시작 완료!");
}

void loop() {
    static bool lastcoonect = false; // 마지막 연결 상태 저장
    
    /*
    if(deviceConnected != lastcoonect) {
        if (deviceConnected) {
            Serial.println("✅ BLE 왼쪽 클라이언트 연결됨!");
        } else {
            Serial.println("🔄 BLE 왼쪽 클라이언트 연결 끊김. 대기 중...");
        }
        lastcoonect = deviceConnected;
    }
    */

    if (deviceConnected && measureWeight) {
        float weight = scale.get_units(10); // 10회 평균 측정
        Serial.print("무게 (g): ");
        Serial.println(weight, 2);

        pCharacteristic->setValue((uint8_t*)&weight, sizeof(weight));
        pCharacteristic->notify();  // BLE Notify로 데이터 전송

        Serial.println("📤 BLE 왼쪽전송 완료!");
        measureWeight = false; // 측정 완료 후 플래그 초기화
    } 
     delay(100); // 짧은 대기
    }
