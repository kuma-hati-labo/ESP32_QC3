#include <NimBLEDevice.h>

// nRF Blinkyが求めるServiceと、2つのCharacteristic
#define SERVICE_UUID        "00001523-1212-efde-1523-785feabcd123"
#define LED_CHAR_UUID       "00001525-1212-efde-1523-785feabcd123" // 書き込み用
#define BUTTON_CHAR_UUID    "00001524-1212-efde-1523-785feabcd123" // 通知用（ダミー）
#define SLIDER_CHAR_UUID    "00001526-1212-efde-1523-785feabcd123" // スライダー

NimBLECharacteristic* pButtonChar = nullptr;
NimBLECharacteristic* pSliderChar = nullptr; // ★追加: スライダー用のポインタ

class LedCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            uint8_t val = rxValue[0];
            if (val == 0x01) Serial.println("Blinky App says: ON");
            else if (val == 0x00) Serial.println("Blinky App says: OFF");
        }
    }
};

// ★追加：スライダー用のコールバック
class SliderCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            uint8_t val = rxValue[0]; // 0〜255の値を受け取る
            Serial.printf("Slider App says: %d\n", val);
            
            // TODO: ここで val を使ってアナログ出力（ledcWriteなど）やモーター制御を行います
        }
    }
};

// ★追加：サーバーへの接続・切断を検知するコールバック
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.println("Client Connected!");
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("Client Disconnected! Reason: %d\n", reason);
        // 切断されたら、再び見つけてもらえるようにアドバタイズを再開する
        NimBLEDevice::startAdvertising();
        Serial.println("Restarting advertising...");
    }
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting BLE Server...");

    // 1. 初期化
    NimBLEDevice::init("PD-Ctrl");

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    
    // LED特性（書き込み）の作成
    NimBLECharacteristic* pLedChar = pService->createCharacteristic(
        LED_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pLedChar->setCallbacks(new LedCallbacks());

    // 【対策】Button特性（通知・ダミー）の作成（これでアプリの審査をパスする）
    pButtonChar = pService->createCharacteristic(
        BUTTON_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // ★追加：スライダー特性（読み取り / 書き込み）の作成
    pSliderChar = pService->createCharacteristic(
        SLIDER_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pSliderChar->setCallbacks(new SliderCallbacks()); // コールバックをセット

    // アプリが接続した瞬間に Read して初期表示させるため、初期値(0)をセットしておく
    uint8_t initSliderVal[1] = {0};
    pSliderChar->setValue(initSliderVal, 1);

    pService->start();

    // 2. 【対策】アドバタイズデータを手動で厳密に組み立てる
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    
    // メインパケット（31バイト枠）の設定
    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP); // 末尾を _UNSUP に変更
    advData.setCompleteServices(NimBLEUUID(SERVICE_UUID));
    pAdvertising->setAdvertisementData(advData);

    // スキャンレスポンスパケット（名前用枠）の設定
    NimBLEAdvertisementData scanResponseData;
    scanResponseData.setName("PD-Ctrl"); // 明示的に名前を載せる
    pAdvertising->setScanResponseData(scanResponseData);

    pAdvertising->enableScanResponse(true); 
    pAdvertising->start();

    Serial.println("Waiting for client connection...");
}

void loop() {
    // 接続されているときだけ処理する（NimBLEの便利な機能です）
    if (0 && (NimBLEDevice::getServer()->getConnectedCount() > 0)) {
        
        static bool buttonState = false;
        buttonState = !buttonState; // true と false を交互に切り替える

        // 送信する1バイトのデータ配列を用意
        uint8_t txValue[1];
        txValue[0] = buttonState ? 0x01 : 0x00;

        // 1. 窓口に値をセットして
        pButtonChar->setValue(txValue, 1);
        
        // 2. スマホに向かってビーム（通知）を撃つ！
        pButtonChar->notify();

        Serial.printf("Simulating Button: %s\n", buttonState ? "PRESSED" : "RELEASED");
    }
    delay(2000);
}