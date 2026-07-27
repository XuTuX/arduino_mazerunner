#pragma once

// =====================================================================
// GameServer.h
// 게임의 모든 상태(채널, 플레이어, 맵, 타이머)와 WebSocket 처리 로직을
// 담고 있는 핵심 클래스이다. main.cpp는 이 클래스를 생성하고
// begin()/loop()만 호출하면 된다.
// =====================================================================

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "Config.h"
#include "GameTypes.h"
#include "WebSocketProtocol.h"

class GameServer {
public:
    GameServer();

    // AsyncWebServer에 WebSocket 핸들러를 등록한다. setup()에서 한 번 호출.
    void begin(AsyncWebServer &server);

    // loop()에서 매번 호출해야 한다. 자동 이동(틱) 처리와 정리 작업을 수행한다.
    void loop();

private:
    AsyncWebSocket ws_;

    // ESP32가 게임 상태의 "유일한 권한"을 갖는다. 클라이언트는 이 배열을
    // 절대 직접 수정할 수 없고, 오직 서버만 수정한다.
    GameRoom rooms_[Config::MAX_ROOMS];
    ConnectionInfo connections_[Config::MAX_CONNECTIONS];

    // WebSocket 콜백(비동기 태스크에서 실행됨)과 loop()(메인 태스크에서 실행됨)가
    // 동시에 rooms_/connections_를 건드릴 수 있어 경쟁 상태(race condition)가
    // 생길 수 있다. 이를 막기 위해 재귀 뮤텍스로 두 곳 모두를 보호한다.
    // (재귀 뮤텍스를 쓰는 이유: 내부 함수들이 서로를 호출할 때 같은 태스크가
    //  두 번 잠그더라도 데드락이 나지 않도록 하기 위함이다)
    SemaphoreHandle_t mutex_;

    unsigned long lastCleanupTime_;

    // ---- WebSocket 이벤트 처리 ----
    void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                 AwsEventType type, void *arg, uint8_t *data, size_t len);
    void handleMessage(AsyncWebSocketClient *client, const uint8_t *data, size_t len);
    void handleDisconnect(uint32_t clientId);

    // ---- 메시지 종류별 처리 ----
    void handleJoin(AsyncWebSocketClient *client, JsonObject obj);
    void handleTurn(ConnectionInfo &conn);
    void handleStartGame(ConnectionInfo &conn);
    void handleRequestState(AsyncWebSocketClient *client, ConnectionInfo &conn);

    // ---- 연결 정보(connections_) 관리 ----
    ConnectionInfo *findConnection(uint32_t clientId);
    ConnectionInfo *allocConnection(uint32_t clientId);
    void freeConnection(uint32_t clientId);

    // ---- 방(rooms_) 관리 ----
    int8_t findRoomByChannel(const char *channel) const;
    int8_t allocRoom(const char *channel);
    void initRoomMap(GameRoom &room) const;
    void findFreeStartPosition(GameRoom &room, int8_t &outX, int8_t &outY, bool &found) const;
    bool isPositionOccupied(const GameRoom &room, int8_t x, int8_t y) const;

    // ---- 게임 틱(자동 이동) ----
    void updateRoomTick(uint8_t roomIndex, unsigned long now);
    void updateObstacles(uint8_t roomIndex, unsigned long now);

    // ---- 방/플레이어 정리 ----
    void cleanupRooms(unsigned long now);

    // ---- 메시지 전송 ----
    void sendJsonTo(AsyncWebSocketClient *client, JsonDocument &doc);
    void broadcastJson(uint8_t roomIndex, JsonDocument &doc, uint32_t excludeClientId = 0);
    void sendError(AsyncWebSocketClient *client, const char *message);
    void sendStateTo(AsyncWebSocketClient *client, uint8_t roomIndex, bool includeMap);
    void broadcastState(uint8_t roomIndex, bool includeMap, uint32_t excludeClientId = 0);
    void buildStateJson(JsonDocument &doc, GameRoom &room, bool includeMap);
    void broadcastPlayerCaught(uint8_t roomIndex, uint8_t caughtIndex, uint8_t oldTaggerIndex);
    void broadcastPlayerDisconnected(uint8_t roomIndex, const char *name, int8_t playerId);

    // ---- 검증 / 유틸리티 (정적 함수) ----
    static bool normalizeChannelName(const char *raw, char *dest, size_t destSize);
    static void sanitizeUtf8Copy(char *dest, size_t destSize, const char *src);
    static void generateToken(char *dest, size_t destSize);
    static void directionDelta(Direction dir, int8_t &dx, int8_t &dy);
    static Direction rotateClockwise(Direction dir);
};
