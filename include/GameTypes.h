#pragma once

// =====================================================================
// GameTypes.h
// 게임 상태를 표현하는 자료구조들을 모아둔 파일이다.
// 모두 "고정 크기 배열 + 고정 크기 구조체"로 만들어서 힙 메모리를 거의
// 사용하지 않는다 (String도 사용하지 않는다). ESP32-S3 메모리를 아끼기 위함이다.
// =====================================================================

#include <Arduino.h>
#include "Config.h"

// 플레이어가 바라보는 방향. 시계 방향 순서: 위 -> 오른쪽 -> 아래 -> 왼쪽 -> 위...
enum class Direction : uint8_t {
    Up = 0,
    Right = 1,
    Down = 2,
    Left = 3
};

// 맵 칸의 종류
enum class TileType : uint8_t {
    Empty = 0,
    Wall = 1,
    Obstacle = 2
};

// 플레이어(휴대폰 조작기) 한 명의 정보
struct Player {
    bool active;      // 이 슬롯이 실제로 사용 중인지 여부
    bool connected;    // 현재 WebSocket 연결이 살아있는지 여부 (끊겨도 active는 유지될 수 있음)
    uint32_t clientId; // 현재 연결된 AsyncWebSocketClient의 id() 값. 연결 없으면 0.
    char token[Config::TOKEN_LEN + 1]; // 새로고침/재접속 시 같은 플레이어로 복귀하기 위한 토큰
    char name[Config::MAX_PLAYER_NAME_LEN + 1];
    int8_t x;
    int8_t y;
    Direction direction;
    bool isTagger;
    unsigned long lastSeen;     // 마지막으로 활동(연결/이동/회전)이 있었던 시각(millis)
    unsigned long lastTurnTime; // 마지막 회전 명령을 처리한 시각(millis) - 너무 빠른 입력 방지용
};

// 채널(방) 하나의 정보
struct GameRoom {
    bool active;
    char channel[Config::MAX_CHANNEL_NAME_LEN + 1];
    bool gameStarted;
    Player players[Config::MAX_PLAYERS_PER_ROOM];
    uint8_t playerCount;  // active인 플레이어 수 (배열을 매번 세지 않기 위한 캐시 값)
    uint8_t taggerIndex;  // 현재 술래의 players[] 인덱스. 0xFF면 술래 없음.
    uint8_t map[Config::MAP_HEIGHT][Config::MAP_WIDTH]; // TileType 값을 uint8_t로 저장
    unsigned long lastTick;     // 마지막 자동 이동 처리 시각(millis)
    unsigned long lastActivity; // 마지막으로 누군가 있었던 시각(millis) - 빈 방 정리용
    unsigned long lastObstacleUpdate; // 마지막 장애물 동적 변경 시각(millis)
};

// WebSocket 연결 하나(휴대폰 1대 또는 컴퓨터 화면 1개)에 대한 정보.
// AsyncWebSocketClient 자체에는 "이 연결이 몇 번 채널의 몇 번째 플레이어인지"를
// 저장할 안전한 공간이 없으므로, 서버가 별도 배열로 관리한다.
struct ConnectionInfo {
    bool active;
    uint32_t clientId;
    bool isDisplay;     // true면 컴퓨터 화면, false면 휴대폰 조작기
    int8_t roomIndex;   // 참가한 방의 rooms_[] 인덱스. -1이면 아직 참가 전.
    int8_t playerIndex; // 컨트롤러일 때 players[] 인덱스. -1이면 없음(화면이거나 참가 전).
};
