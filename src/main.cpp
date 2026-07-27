// =====================================================================
// main.cpp
// ESP32-S3 격자 술래잡기 게임 서버의 진입점.
// 1) LittleFS 마운트 (웹페이지 파일들이 들어있음)
// 2) Wi-Fi AP(핫스팟) 생성
// 3) 정적 파일 웹서버 + GameServer(WebSocket) 시작
// 세부 게임 로직은 전부 GameServer 클래스(lib/GameServer)에 들어있다.
// =====================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

#include "Config.h"
#include "GameServer.h"

static AsyncWebServer httpServer(Config::HTTP_PORT);
static GameServer gameServer;

void setup() {
    Serial.begin(115200);

    // ESP32-S3는 USB-CDC로 시리얼을 출력하기 때문에, PC가 USB 연결을
    // 인식하는 데 약간 시간이 걸릴 수 있다. 최대 3초까지만 기다린다.
    unsigned long serialWaitStart = millis();
    while (!Serial && (millis() - serialWaitStart) < 3000) {
        delay(10);
    }

    Serial.println();
    Serial.println("[INFO] ===== ESP32-S3 격자 술래잡기 서버 부팅 시작 =====");

    if (!LittleFS.begin(true)) {
        Serial.println("[ERROR] LittleFS 마운트 실패! data 폴더를 업로드했는지 확인하세요.");
        Serial.println("[ERROR] VS Code에서 PlatformIO: Upload Filesystem Image 를 실행해주세요.");
    } else {
        Serial.println("[INFO] LittleFS 마운트 성공. 저장된 파일 목록:");
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        if (!file) Serial.println("  (파일이 하나도 없습니다! 비어있음)");
        while (file) {
            Serial.print("  - ");
            Serial.print(file.name());
            Serial.print(" (");
            Serial.print(file.size());
            Serial.println(" bytes)");
            file = root.openNextFile();
        }
    }

    WiFi.mode(WIFI_AP);
    bool apOk = WiFi.softAP(Config::WIFI_SSID, Config::WIFI_PASSWORD,
                             Config::WIFI_CHANNEL, 0, Config::WIFI_MAX_CONNECTIONS);

    if (apOk) {
        Serial.println("[INFO] Wi-Fi AP 생성 성공");
        Serial.print("[INFO] AP 이름(SSID): ");
        Serial.println(Config::WIFI_SSID);
        Serial.print("[INFO] AP IP 주소: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("[ERROR] Wi-Fi AP 생성 실패! 보드를 재시작해보세요.");
    }

    gameServer.begin(httpServer);

    // data 폴더(LittleFS)의 파일들을 그대로 웹에 제공한다.
    // 예) data/index.html -> http://192.168.4.1/index.html (혹은 /)
    //     data/js/controller.js -> http://192.168.4.1/js/controller.js
    httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    httpServer.onNotFound([](AsyncWebServerRequest *request) {
        Serial.printf("[WARN] 404 Not Found: %s\n", request->url().c_str());
        request->send(404, "text/plain", "404: \355\216\230\354\235\264\354\247\200\353\245\274 \354\260\276\354\235\204 \354\210\230 \354\227\206\354\212\265\353\213\210\353\213\244.");
    });

    httpServer.begin();
    Serial.println("[INFO] 웹서버 시작 완료 (포트 80)");
    Serial.print("[INFO] 브라우저에서 다음 주소로 접속하세요: http://");
    Serial.println(WiFi.softAPIP());
    Serial.println("[INFO] ===== 부팅 완료, 접속을 기다립니다 =====");
}

void loop() {
    // delay()를 사용하지 않는 비차단(non-blocking) 방식.
    // 게임 틱 처리, 연결 정리 등은 모두 GameServer::loop() 내부에서
    // millis() 기반으로 처리되어 웹서버/WebSocket 응답을 막지 않는다.
    gameServer.loop();
}
