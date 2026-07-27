#pragma once

// =====================================================================
// Config.h
// 게임 전체에서 사용하는 "매직 넘버"를 한 곳에 모아둔 설정 파일이다.
// 나중에 게임 속도, 맵 크기, 최대 인원, Wi-Fi 정보 등을 바꾸고 싶다면
// 코드 여러 곳을 찾아다닐 필요 없이 이 파일만 수정하면 된다.
// =====================================================================

#include <Arduino.h>

namespace Config {

// ---------------------------------------------------------------
// Wi-Fi 설정 (여기를 바꾸면 Wi-Fi 이름/비밀번호가 바뀐다)
// ---------------------------------------------------------------
constexpr const char *WIFI_SSID = "ESP32-TAG-GAME";
constexpr const char *WIFI_PASSWORD = "12345678";
constexpr uint8_t WIFI_CHANNEL = 1;
// ESP32 Wi-Fi AP(공유기 역할)는 하드웨어 특성상 동시 접속 기기 수가
// 실질적으로 8~10대 정도로 제한된다. 소프트웨어상 채널/플레이어 정원과는
// 별개의 "무선 연결" 제한이므로 8로 설정한다.
constexpr uint8_t WIFI_MAX_CONNECTIONS = 8;

// ---------------------------------------------------------------
// 웹 서버
// ---------------------------------------------------------------
constexpr uint16_t HTTP_PORT = 80;

// ---------------------------------------------------------------
// 채널(방) / 플레이어 정원 (여기를 바꾸면 최대 인원이 바뀐다)
// ---------------------------------------------------------------
constexpr uint8_t MAX_ROOMS = 4;                 // 동시에 만들 수 있는 채널 수
constexpr uint8_t MAX_PLAYERS_PER_ROOM = 8;      // 채널 하나당 최대 플레이어(휴대폰) 수
constexpr uint8_t MAX_DISPLAYS_PER_ROOM = 4;     // 채널 하나당 최대 컴퓨터 화면 수
// 서버가 동시에 관리할 수 있는 "WebSocket 연결 정보" 슬롯 수.
// (채널 4개) x (플레이어 8명 + 화면 4개) = 48
constexpr uint8_t MAX_CONNECTIONS = MAX_ROOMS * (MAX_PLAYERS_PER_ROOM + MAX_DISPLAYS_PER_ROOM);

constexpr uint8_t MAX_CHANNEL_NAME_LEN = 16; // 채널 이름 최대 길이 (문자 수, NUL 제외)
constexpr uint8_t MAX_PLAYER_NAME_LEN = 16;  // 플레이어 이름 최대 길이 (바이트 수, NUL 제외)
constexpr uint8_t TOKEN_LEN = 32;            // 재접속용 토큰 길이 (16진수 문자 수)

// ---------------------------------------------------------------
// 맵 크기 (여기를 바꾸면 맵 크기가 바뀐다. GameServer.cpp의 기본 맵 배열도
// 같이 수정해야 한다)
// ---------------------------------------------------------------
constexpr uint8_t MAP_WIDTH = 12;
constexpr uint8_t MAP_HEIGHT = 12;

// ---------------------------------------------------------------
// 게임 타이밍 (여기를 바꾸면 게임 속도, 회전 제한 속도가 바뀐다)
// ---------------------------------------------------------------
constexpr unsigned long TICK_INTERVAL_MS = 400;      // 자동 이동 주기 (한 칸 이동 간격)
constexpr unsigned long MIN_TURN_INTERVAL_MS = 100;  // 회전 명령 최소 간격 (서버 기준)
constexpr unsigned long PLAYER_TIMEOUT_MS = 30000;   // 연결 끊긴 플레이어를 제거하기까지 대기 시간
constexpr unsigned long ROOM_IDLE_TIMEOUT_MS = 300000; // 채널이 완전히 비었을 때 정리하기까지 대기 시간(5분)
constexpr unsigned long CLEANUP_INTERVAL_MS = 5000;  // 정리 작업을 확인하는 주기

// ---------------------------------------------------------------
// 네트워크 메시지 크기
// ---------------------------------------------------------------
constexpr size_t WS_MAX_MESSAGE_LEN = 512; // 클라이언트가 보낼 수 있는 메시지 최대 길이(바이트)

} // namespace Config
