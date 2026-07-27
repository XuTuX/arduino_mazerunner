#pragma once

// =====================================================================
// WebSocketProtocol.h
// 클라이언트<->서버가 주고받는 JSON 메시지의 "type" 문자열 값들을 모아둔 파일이다.
// 문자열을 여기저기 직접 타이핑하면 오타가 나기 쉬우므로 상수로 관리한다.
// =====================================================================

namespace WsMsgType {

// ---- 클라이언트 -> 서버 ----
constexpr const char *JOIN = "join";
constexpr const char *TURN = "turn";
constexpr const char *START_GAME = "startGame";
constexpr const char *REQUEST_STATE = "requestState";
constexpr const char *PING = "ping";

// ---- 서버 -> 클라이언트 ----
constexpr const char *JOINED = "joined";
constexpr const char *STATE = "state";
constexpr const char *ERROR_MSG = "error";
constexpr const char *GAME_STARTED = "gameStarted";
constexpr const char *PLAYER_CAUGHT = "playerCaught";
constexpr const char *PLAYER_DISCONNECTED = "playerDisconnected";
constexpr const char *PONG = "pong";

} // namespace WsMsgType
