// =====================================================================
// GameServer.cpp
// GameServer 클래스의 실제 구현. 파일이 길지만 위에서 아래로 순서대로
// "연결 처리 -> 참가 처리 -> 회전/시작 처리 -> 자동 이동(틱) -> 정리 -> 전송"
// 흐름으로 읽으면 이해하기 쉽다.
// =====================================================================

#include "GameServer.h"
#include <esp_random.h>
#include <string.h>
#include <ctype.h>

// ---------------------------------------------------------------------
// 기본 맵 레이아웃 (12 x 12)
// 0 = 빈 칸, 1 = 벽, 2 = 장애물
// 가장 바깥쪽 테두리는 전부 벽으로 막혀 있어서, 플레이어는 절대 맵 밖으로
// 나갈 수 없다 (좌표가 배열 범위를 벗어날 일도 없다).
// ---------------------------------------------------------------------
static const uint8_t kDefaultMap[Config::MAP_HEIGHT][Config::MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,2,0,0,0,0,0,0,2,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,0,0,2,2,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,0,0,2,2,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,2,0,0,0,0,0,0,2,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
};

GameServer::GameServer()
    : ws_("/ws"), mutex_(nullptr), lastCleanupTime_(0) {
}

// =====================================================================
// 시작
// =====================================================================
void GameServer::begin(AsyncWebServer &server) {
    mutex_ = xSemaphoreCreateRecursiveMutex();

    for (uint8_t i = 0; i < Config::MAX_ROOMS; i++) {
        rooms_[i].active = false;
    }
    for (uint8_t i = 0; i < Config::MAX_CONNECTIONS; i++) {
        connections_[i].active = false;
    }

    // AsyncWebSocket의 이벤트 콜백은 std::function이라 람다로 등록할 수 있다.
    // 람다 안에서 this를 캡처해 실제 처리는 멤버 함수인 onEvent()로 넘긴다.
    ws_.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                        AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onEvent(server, client, type, arg, data, len);
    });

    server.addHandler(&ws_);
    Serial.println("[INFO] WebSocket 핸들러 등록 완료 (경로: /ws)");
}

// =====================================================================
// 메인 loop() 에서 매번 호출됨
// =====================================================================
void GameServer::loop() {
    unsigned long now = millis();

    xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);

    for (uint8_t i = 0; i < Config::MAX_ROOMS; i++) {
        GameRoom &room = rooms_[i];
        if (room.active && room.gameStarted) {
            if (now - room.lastTick >= Config::TICK_INTERVAL_MS) {
                room.lastTick = now;
                updateRoomTick(i, now);
            }
            if (now - room.lastObstacleUpdate >= Config::OBSTACLE_UPDATE_INTERVAL_MS) {
                room.lastObstacleUpdate = now;
                updateObstacles(i, now);
            }
        }
    }

    if (now - lastCleanupTime_ >= Config::CLEANUP_INTERVAL_MS) {
        lastCleanupTime_ = now;
        cleanupRooms(now);
    }

    xSemaphoreGiveRecursive(mutex_);

    // 라이브러리가 권장하는, 끊어진 지 오래된 클라이언트 정리 호출.
    // delay()가 없는 비차단 방식이라 웹서버/WebSocket 처리를 막지 않는다.
    ws_.cleanupClients();
}

// =====================================================================
// WebSocket 이벤트 (연결 / 종료 / 데이터 / 오류)
// =====================================================================
void GameServer::onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                          AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] 클라이언트 연결됨 (id=%u, ip=%s)\n",
                          client->id(), client->remoteIP().toString().c_str());
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] 클라이언트 연결 종료 (id=%u)\n", client->id());
            handleDisconnect(client->id());
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo *info = reinterpret_cast<AwsFrameInfo *>(arg);
            // 프로토타입에서는 짧은 JSON 메시지만 다루므로, 조각나지 않은
            // 단일 텍스트 프레임만 처리한다 (일반 브라우저 WebSocket이 보내는
            // 짧은 메시지는 항상 이 조건을 만족한다).
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                handleMessage(client, data, len);
            }
            break;
        }

        case WS_EVT_ERROR:
            Serial.printf("[ERROR] WebSocket 오류 발생 (id=%u)\n", client->id());
            break;

        case WS_EVT_PONG:
        default:
            break;
    }
}

void GameServer::handleMessage(AsyncWebSocketClient *client, const uint8_t *data, size_t len) {
    if (len == 0 || len > Config::WS_MAX_MESSAGE_LEN) {
        sendError(client, "메시지 길이가 올바르지 않습니다.");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        Serial.printf("[ERROR] JSON 파싱 실패: %s\n", err.c_str());
        sendError(client, "메시지 형식(JSON)이 올바르지 않습니다.");
        return;
    }

    const char *type = doc["type"] | "";
    if (strlen(type) == 0) {
        sendError(client, "메시지에 type 필드가 없습니다.");
        return;
    }

    // 아래 구간은 채널/플레이어 배열을 읽고 쓰므로 뮤텍스로 보호한다.
    xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);

    if (strcmp(type, WsMsgType::JOIN) == 0) {
        handleJoin(client, doc.as<JsonObject>());
    } else if (strcmp(type, WsMsgType::PING) == 0) {
        JsonDocument res;
        res["type"] = WsMsgType::PONG;
        sendJsonTo(client, res);
    } else {
        // join 이후에만 허용되는 명령들: 반드시 등록된 연결 정보가 있어야 한다.
        ConnectionInfo *conn = findConnection(client->id());
        if (conn == nullptr) {
            sendError(client, "먼저 참가(join) 메시지를 보내야 합니다.");
        } else if (strcmp(type, WsMsgType::TURN) == 0) {
            handleTurn(*conn);
        } else if (strcmp(type, WsMsgType::START_GAME) == 0) {
            handleStartGame(*conn);
        } else if (strcmp(type, WsMsgType::REQUEST_STATE) == 0) {
            handleRequestState(client, *conn);
        } else {
            sendError(client, "알 수 없는 메시지 종류입니다.");
        }
    }

    xSemaphoreGiveRecursive(mutex_);
}

void GameServer::handleDisconnect(uint32_t clientId) {
    xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);

    ConnectionInfo *conn = findConnection(clientId);
    if (conn != nullptr) {
        if (!conn->isDisplay && conn->roomIndex >= 0 && conn->playerIndex >= 0) {
            GameRoom &room = rooms_[conn->roomIndex];
            Player &p = room.players[conn->playerIndex];
            // clientId가 일치할 때만 처리한다. (이미 다른 연결로 대체된 플레이어일 수도 있어서)
            if (p.active && p.clientId == clientId) {
                p.connected = false;
                p.clientId = 0;
                p.lastSeen = millis();
                room.lastActivity = millis();
                Serial.printf("[GAME] 플레이어 연결 끊김: 채널=%s, 이름=%s\n", room.channel, p.name);
                broadcastPlayerDisconnected(conn->roomIndex, p.name, conn->playerIndex + 1);
                broadcastState(conn->roomIndex, false);
            }
        } else if (conn->isDisplay && conn->roomIndex >= 0) {
            rooms_[conn->roomIndex].lastActivity = millis();
        }
        freeConnection(clientId);
    }

    xSemaphoreGiveRecursive(mutex_);
}

// =====================================================================
// join 처리
// =====================================================================
void GameServer::handleJoin(AsyncWebSocketClient *client, JsonObject obj) {
    const char *role = obj["role"] | "";
    const char *channelRaw = obj["channel"] | "";

    if (strcmp(role, "controller") != 0 && strcmp(role, "display") != 0) {
        sendError(client, "역할(role)은 controller 또는 display여야 합니다.");
        return;
    }

    char channel[Config::MAX_CHANNEL_NAME_LEN + 1];
    if (!normalizeChannelName(channelRaw, channel, sizeof(channel))) {
        sendError(client, "채널 이름은 영문/숫자 1~16자로 입력해주세요.");
        return;
    }

    // 같은 연결이 join을 다시 보낸 경우(중복 참가)를 대비해 기존 연결 정보를 정리한다.
    freeConnection(client->id());

    int8_t roomIndex = findRoomByChannel(channel);
    if (roomIndex < 0) {
        roomIndex = allocRoom(channel);
        if (roomIndex < 0) {
            sendError(client, "채널을 더 만들 수 없습니다 (최대 4개까지 가능).");
            return;
        }
        Serial.printf("[GAME] 새 채널 생성: %s\n", channel);
    }
    GameRoom &room = rooms_[roomIndex];
    room.lastActivity = millis();

    // ---------------- 컴퓨터 화면(display) 참가 ----------------
    if (strcmp(role, "display") == 0) {
        uint8_t displayCount = 0;
        for (uint8_t i = 0; i < Config::MAX_CONNECTIONS; i++) {
            if (connections_[i].active && connections_[i].roomIndex == roomIndex && connections_[i].isDisplay) {
                displayCount++;
            }
        }
        if (displayCount >= Config::MAX_DISPLAYS_PER_ROOM) {
            sendError(client, "이 채널에 연결 가능한 화면 수를 초과했습니다.");
            return;
        }

        ConnectionInfo *conn = allocConnection(client->id());
        if (conn == nullptr) {
            sendError(client, "서버 연결 자원이 부족합니다. 잠시 후 다시 시도해주세요.");
            return;
        }
        conn->isDisplay = true;
        conn->roomIndex = roomIndex;
        conn->playerIndex = -1;

        Serial.printf("[GAME] 컴퓨터 화면 참가: 채널=%s\n", channel);

        JsonDocument res;
        res["type"] = WsMsgType::JOINED;
        res["role"] = "display";
        res["channel"] = room.channel;
        sendJsonTo(client, res);

        // 참가 직후에는 맵 전체를 함께 보낸다 (아래 broadcastState 설명 참고).
        sendStateTo(client, roomIndex, true);
        return;
    }

    // ---------------- 휴대폰 조작기(controller) 참가 ----------------
    const char *nameRaw = obj["name"] | "";
    const char *tokenRaw = obj["token"] | "";

    // 토큰이 유효하면(=같은 브라우저의 재접속/새로고침) 기존 플레이어로 복귀시킨다.
    int8_t existingIndex = -1;
    if (strlen(tokenRaw) > 0) {
        for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
            Player &p = room.players[i];
            if (p.active && strncmp(p.token, tokenRaw, Config::TOKEN_LEN) == 0) {
                existingIndex = i;
                break;
            }
        }
    }

    int8_t playerIndex;
    if (existingIndex >= 0) {
        playerIndex = existingIndex;
        Player &p = room.players[playerIndex];

        // 같은 플레이어가 다른 연결(예: 다른 탭)로 이미 접속해 있다면 이전 연결은 끊는다.
        if (p.connected && p.clientId != 0 && p.clientId != client->id()) {
            AsyncWebSocketClient *oldClient = ws_.client(p.clientId);
            freeConnection(p.clientId);
            if (oldClient != nullptr) {
                oldClient->close(1000, "다른 기기에서 재접속함");
            }
        }

        p.clientId = client->id();
        p.connected = true;
        p.lastSeen = millis();
        Serial.printf("[GAME] 플레이어 재연결: 채널=%s, 이름=%s\n", channel, p.name);
    } else {
        if (room.playerCount >= Config::MAX_PLAYERS_PER_ROOM) {
            sendError(client, "채널의 플레이어 정원(8명)이 가득 찼습니다.");
            return;
        }

        playerIndex = -1;
        for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
            if (!room.players[i].active) {
                playerIndex = i;
                break;
            }
        }
        if (playerIndex < 0) {
            sendError(client, "빈 플레이어 슬롯을 찾을 수 없습니다.");
            return;
        }

        Player &p = room.players[playerIndex];
        memset(&p, 0, sizeof(Player));
        p.active = true;
        p.connected = true;
        p.clientId = client->id();
        generateToken(p.token, sizeof(p.token));

        char safeName[Config::MAX_PLAYER_NAME_LEN + 1];
        sanitizeUtf8Copy(safeName, sizeof(safeName), nameRaw);
        if (strlen(safeName) == 0) {
            // 이름이 비어 있으면 자동으로 이름을 만들어준다.
            snprintf(safeName, sizeof(safeName), "\355\224\214\353\240\210\354\235\264\354\226\264%d", playerIndex + 1); // "플레이어N"
        }
        strncpy(p.name, safeName, sizeof(p.name) - 1);
        p.name[sizeof(p.name) - 1] = '\0';

        bool found = false;
        int8_t sx = 1, sy = 1;
        findFreeStartPosition(room, sx, sy, found);
        p.x = found ? sx : 1;
        p.y = found ? sy : 1;
        p.direction = Direction::Up;
        p.isTagger = false;
        p.lastSeen = millis();
        p.lastTurnTime = 0;

        room.playerCount++;
        Serial.printf("[GAME] 새 플레이어 생성: 채널=%s, 이름=%s, 위치=(%d,%d)\n",
                      channel, p.name, p.x, p.y);
    }

    ConnectionInfo *conn = allocConnection(client->id());
    if (conn == nullptr) {
        sendError(client, "서버 연결 자원이 부족합니다. 잠시 후 다시 시도해주세요.");
        return;
    }
    conn->isDisplay = false;
    conn->roomIndex = roomIndex;
    conn->playerIndex = playerIndex;

    Player &p = room.players[playerIndex];

    JsonDocument res;
    res["type"] = WsMsgType::JOINED;
    res["role"] = "controller";
    res["channel"] = room.channel;
    res["token"] = p.token; // 클라이언트가 localStorage에 저장해야 함
    res["playerId"] = playerIndex + 1;
    res["name"] = p.name;
    sendJsonTo(client, res);

    // 참가 직후에는 맵 전체를 이 클라이언트에게만 보낸다.
    sendStateTo(client, roomIndex, true);
    // 나머지 클라이언트에게는 맵 없이 플레이어 정보만 갱신해서 보낸다 (대역폭 절약).
    broadcastState(roomIndex, false, client->id());
}

// =====================================================================
// turn 처리 (회전 명령)
// =====================================================================
void GameServer::handleTurn(ConnectionInfo &conn) {
    if (conn.isDisplay || conn.roomIndex < 0 || conn.playerIndex < 0) {
        AsyncWebSocketClient *client = ws_.client(conn.clientId);
        sendError(client, "컨트롤러로 참가한 후에만 회전할 수 있습니다.");
        return;
    }

    GameRoom &room = rooms_[conn.roomIndex];
    Player &p = room.players[conn.playerIndex];
    if (!p.active || !p.connected) {
        return;
    }

    unsigned long now = millis();
    if (now - p.lastTurnTime < Config::MIN_TURN_INTERVAL_MS) {
        // 너무 빠른 입력은 조용히 무시한다 (에러를 남발하지 않기 위함).
        return;
    }
    p.lastTurnTime = now;
    p.lastSeen = now;
    p.direction = rotateClockwise(p.direction);
    room.lastActivity = now;

    Serial.printf("[GAME] 회전 명령: 채널=%s, 이름=%s, 새 방향=%d\n",
                  room.channel, p.name, static_cast<int>(p.direction));

    // 회전은 다음 틱(최대 400ms 후)까지 기다리지 않고 바로 반영해서 보여준다.
    broadcastState(conn.roomIndex, false);
}

// =====================================================================
// startGame 처리
// =====================================================================
void GameServer::handleStartGame(ConnectionInfo &conn) {
    AsyncWebSocketClient *client = ws_.client(conn.clientId);

    if (!conn.isDisplay || conn.roomIndex < 0) {
        sendError(client, "게임 시작은 컴퓨터 화면에서만 요청할 수 있습니다.");
        return;
    }

    GameRoom &room = rooms_[conn.roomIndex];

    uint8_t connectedIndexes[Config::MAX_PLAYERS_PER_ROOM];
    uint8_t connectedCount = 0;
    for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
        Player &p = room.players[i];
        if (p.active && p.connected) {
            connectedIndexes[connectedCount++] = i;
        }
    }

    if (connectedCount < 2) {
        sendError(client, "게임을 시작하려면 플레이어가 2명 이상 필요합니다.");
        return;
    }

    // 모든 플레이어 위치를 임시로 "미배치(-1,-1)" 상태로 만든 뒤 하나씩 빈 칸에 배치한다.
    // 이렇게 해야 새로 배치되는 플레이어가 이미 배치된 다른 플레이어와 겹치지 않는다.
    for (uint8_t i = 0; i < connectedCount; i++) {
        Player &p = room.players[connectedIndexes[i]];
        p.x = -1;
        p.y = -1;
        p.direction = Direction::Up;
        p.isTagger = false;
    }
    for (uint8_t i = 0; i < connectedCount; i++) {
        Player &p = room.players[connectedIndexes[i]];
        bool found = false;
        int8_t sx = 1, sy = 1;
        findFreeStartPosition(room, sx, sy, found);
        p.x = found ? sx : 1;
        p.y = found ? sy : 1;
    }

    uint8_t taggerPick = connectedIndexes[esp_random() % connectedCount];
    room.players[taggerPick].isTagger = true;
    room.taggerIndex = taggerPick;
    room.gameStarted = true;
    room.lastTick = millis();
    room.lastActivity = millis();

    Serial.printf("[GAME] 게임 시작: 채널=%s, 술래=%s\n", room.channel, room.players[taggerPick].name);

    JsonDocument res;
    res["type"] = WsMsgType::GAME_STARTED;
    res["channel"] = room.channel;
    res["taggerPlayerId"] = taggerPick + 1;
    res["taggerName"] = room.players[taggerPick].name;
    broadcastJson(conn.roomIndex, res);

    broadcastState(conn.roomIndex, false);
}

void GameServer::handleRequestState(AsyncWebSocketClient *client, ConnectionInfo &conn) {
    if (conn.roomIndex < 0) {
        sendError(client, "먼저 참가(join)해야 합니다.");
        return;
    }
    sendStateTo(client, conn.roomIndex, true);
}

// =====================================================================
// 게임 틱: 자동 이동 처리 (400ms마다 한 번씩 호출됨)
// 처리 순서: 1.다음 위치 계산 -> 2.벽/장애물 검사 -> 3.도망자끼리 충돌 검사
//           -> 4.이동 적용 -> 5.잡기 판정 -> 6.상태 전송
// =====================================================================
void GameServer::updateRoomTick(uint8_t roomIndex, unsigned long now) {
    GameRoom &room = rooms_[roomIndex];

    int8_t targetX[Config::MAX_PLAYERS_PER_ROOM];
    int8_t targetY[Config::MAX_PLAYERS_PER_ROOM];
    bool willMove[Config::MAX_PLAYERS_PER_ROOM];

    // 1~2단계: 각 플레이어가 가려는 다음 칸을 계산하고, 벽/장애물 규칙을 적용한다.
    for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
        Player &p = room.players[i];
        targetX[i] = p.x;
        targetY[i] = p.y;
        willMove[i] = false;
        if (!p.active || !p.connected) continue;

        int8_t dx = 0, dy = 0;
        directionDelta(p.direction, dx, dy);
        int nx = static_cast<int>(p.x) + dx;
        int ny = static_cast<int>(p.y) + dy;

        // 배열 범위 검사 (테두리가 전부 벽이라 이론상 여기 걸릴 일은 없지만,
        // 방어적으로 검사해 배열 밖 접근을 원천 차단한다).
        if (nx < 0 || nx >= Config::MAP_WIDTH || ny < 0 || ny >= Config::MAP_HEIGHT) continue;

        TileType tile = static_cast<TileType>(room.map[ny][nx]);
        if (tile == TileType::Wall) continue;              // 벽: 아무도 통과 불가
        if (tile == TileType::Obstacle) continue;          // 장애물: 아무도 통과 불가

        targetX[i] = static_cast<int8_t>(nx);
        targetY[i] = static_cast<int8_t>(ny);
        willMove[i] = true;
    }

    // 3단계: 도망자끼리의 충돌 검사. 배열 인덱스가 빠른 플레이어가 우선권을 가진다.
    // (술래의 칸은 예약하지 않는다 - 도망자가 술래 칸으로 들어가야 "잡기"가 일어나기 때문)
    static bool cellReserved[Config::MAP_HEIGHT][Config::MAP_WIDTH];
    memset(cellReserved, 0, sizeof(cellReserved));

    // 제자리에 머무르는 도망자의 칸을 먼저 예약해서, 다른 도망자가 그 칸으로 못 들어오게 한다.
    for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
        Player &p = room.players[i];
        if (!p.active || !p.connected || p.isTagger) continue;
        if (!willMove[i]) {
            cellReserved[p.y][p.x] = true;
        }
    }

    for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
        Player &p = room.players[i];
        if (!p.active || !p.connected || !willMove[i] || p.isTagger) continue;

        if (cellReserved[targetY[i]][targetX[i]]) {
            // 이미 다른 도망자가 그 칸을 차지하기로 했다면, 이번 틱은 제자리에 머무른다.
            targetX[i] = p.x;
            targetY[i] = p.y;
            willMove[i] = false;
        }
        cellReserved[targetY[i]][targetX[i]] = true;
    }

    // 4단계: 계산된 위치를 실제로 적용한다.
    for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
        Player &p = room.players[i];
        if (!p.active || !p.connected) continue;
        p.x = targetX[i];
        p.y = targetY[i];
    }

    // 5단계: 술래-도망자 잡기 판정. 여러 명이 동시에 겹쳐도 배열상 첫 번째 대상만 처리한다.
    if (room.taggerIndex != 0xFF) {
        Player &tagger = room.players[room.taggerIndex];
        if (tagger.active && tagger.connected) {
            for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
                if (i == room.taggerIndex) continue;
                Player &p = room.players[i];
                if (!p.active || !p.connected) continue;
                if (p.x == tagger.x && p.y == tagger.y) {
                    uint8_t oldTaggerIndex = room.taggerIndex;
                    tagger.isTagger = false;
                    p.isTagger = true;
                    room.taggerIndex = i;

                    Serial.printf("[GAME] 잡기 발생: 채널=%s, %s 님이 %s 님을 잡았습니다 -> 새 술래: %s\n",
                                  room.channel, tagger.name, p.name, p.name);

                    broadcastPlayerCaught(roomIndex, i, oldTaggerIndex);
                    break; // 한 틱에 한 번만 처리 (반복적인 역할 변경 방지)
                }
            }
        }
    }

    // 6단계: 최신 상태 전송. 맵은 참가 시 이미 전달했고 게임 중에는 바뀌지 않으므로
    // 매 틱마다 다시 보내지 않는다 (대역폭 절약).
    broadcastState(roomIndex, false);
}

// =====================================================================
// 장애물 동적 변경: 기존 장애물 일부 제거 + 빈 곳에 새로 배치
// 플레이어가 있는 칸에는 장애물을 놓지도 제거하지도 않는다.
// =====================================================================
void GameServer::updateObstacles(uint8_t roomIndex, unsigned long now) {
    GameRoom &room = rooms_[roomIndex];

    // 1. 현재 장애물 위치와 개수를 수집한다.
    struct Pos { uint8_t x; uint8_t y; };
    Pos obstacles[Config::MAP_WIDTH * Config::MAP_HEIGHT];
    uint8_t obstacleCount = 0;

    for (uint8_t y = 1; y < Config::MAP_HEIGHT - 1; y++) {
        for (uint8_t x = 1; x < Config::MAP_WIDTH - 1; x++) {
            if (room.map[y][x] == static_cast<uint8_t>(TileType::Obstacle)) {
                obstacles[obstacleCount++] = {x, y};
            }
        }
    }

    // 2. 장애물 일부 제거 (플레이어가 없는 곳에서만, 최소 개수 유지)
    uint8_t removed = 0;
    for (uint8_t attempt = 0; attempt < 20 && removed < Config::OBSTACLE_CHANGE_COUNT; attempt++) {
        if (obstacleCount <= Config::OBSTACLE_MIN_COUNT) break;
        uint8_t pick = esp_random() % obstacleCount;
        uint8_t ox = obstacles[pick].x;
        uint8_t oy = obstacles[pick].y;

        // 플레이어가 인접해있으면 제거하지 않는다 (갑자기 길이 뚫리면 안 되므로)
        if (!isPositionOccupied(room, static_cast<int8_t>(ox), static_cast<int8_t>(oy))) {
            room.map[oy][ox] = static_cast<uint8_t>(TileType::Empty);
            // 배열에서 제거 (마지막 요소와 교체)
            obstacles[pick] = obstacles[obstacleCount - 1];
            obstacleCount--;
            removed++;
        }
    }

    // 3. 새로운 장애물 배치 (빈 칸이면서 플레이어가 없는 곳)
    uint8_t added = 0;
    for (uint8_t attempt = 0; attempt < 40 && added < Config::OBSTACLE_CHANGE_COUNT; attempt++) {
        if (obstacleCount >= Config::OBSTACLE_MAX_COUNT) break;
        // 테두리(벽)를 제외한 내부 영역에서 랜덤 좌표 선택
        uint8_t rx = 1 + (esp_random() % (Config::MAP_WIDTH - 2));
        uint8_t ry = 1 + (esp_random() % (Config::MAP_HEIGHT - 2));

        if (room.map[ry][rx] != static_cast<uint8_t>(TileType::Empty)) continue;
        if (isPositionOccupied(room, static_cast<int8_t>(rx), static_cast<int8_t>(ry))) continue;

        room.map[ry][rx] = static_cast<uint8_t>(TileType::Obstacle);
        obstacles[obstacleCount++] = {rx, ry};
        added++;
    }

    // 4. 변경이 있었다면 맵 포함 상태를 모든 클라이언트에게 전송
    if (removed > 0 || added > 0) {
        Serial.printf("[GAME] 장애물 변경: 채널=%s, 제거=%d, 추가=%d, 현재=%d\n",
                      room.channel, removed, added, obstacleCount);
        broadcastState(roomIndex, true); // 맵 포함 전송
    }
}

// =====================================================================
// 정리 작업: 연결 끊긴 플레이어 제거, 빈 채널 정리
// =====================================================================
void GameServer::cleanupRooms(unsigned long now) {
    for (uint8_t r = 0; r < Config::MAX_ROOMS; r++) {
        GameRoom &room = rooms_[r];
        if (!room.active) continue;

        bool stateChanged = false;

        for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
            Player &p = room.players[i];
            if (!p.active) continue;
            if (!p.connected && (now - p.lastSeen > Config::PLAYER_TIMEOUT_MS)) {
                Serial.printf("[GAME] 플레이어 시간 초과로 제거: 채널=%s, 이름=%s\n", room.channel, p.name);
                bool wasTagger = p.isTagger;
                p.active = false;
                if (room.playerCount > 0) room.playerCount--;
                stateChanged = true;

                if (wasTagger) {
                    room.taggerIndex = 0xFF;
                    if (room.gameStarted) {
                        // 남은 접속자 중 무작위로 새 술래를 선정해서 게임이 멈추지 않게 한다.
                        uint8_t candidates[Config::MAX_PLAYERS_PER_ROOM];
                        uint8_t count = 0;
                        for (uint8_t j = 0; j < Config::MAX_PLAYERS_PER_ROOM; j++) {
                            if (room.players[j].active && room.players[j].connected) {
                                candidates[count++] = j;
                            }
                        }
                        if (count > 0) {
                            uint8_t pick = candidates[esp_random() % count];
                            room.players[pick].isTagger = true;
                            room.taggerIndex = pick;
                            Serial.printf("[GAME] 술래 연결 끊김 -> 새 술래 선정: 채널=%s, 이름=%s\n",
                                          room.channel, room.players[pick].name);
                        } else {
                            room.gameStarted = false;
                        }
                    }
                }
            }
        }

        if (stateChanged) {
            broadcastState(r, false);
        }

        // 방에 플레이어도, 화면도 하나도 없는지 확인한다.
        bool anyoneHere = false;
        for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
            if (room.players[i].active) { anyoneHere = true; break; }
        }
        if (!anyoneHere) {
            for (uint8_t i = 0; i < Config::MAX_CONNECTIONS; i++) {
                if (connections_[i].active && connections_[i].roomIndex == r) { anyoneHere = true; break; }
            }
        }

        if (!anyoneHere && (now - room.lastActivity > Config::ROOM_IDLE_TIMEOUT_MS)) {
            Serial.printf("[GAME] 오래 비어있던 채널 정리: %s\n", room.channel);
            room.active = false;
        }
    }
}

// =====================================================================
// 연결 정보(connections_) 관리
// =====================================================================
ConnectionInfo *GameServer::findConnection(uint32_t clientId) {
    for (uint8_t i = 0; i < Config::MAX_CONNECTIONS; i++) {
        if (connections_[i].active && connections_[i].clientId == clientId) {
            return &connections_[i];
        }
    }
    return nullptr;
}

ConnectionInfo *GameServer::allocConnection(uint32_t clientId) {
    for (uint8_t i = 0; i < Config::MAX_CONNECTIONS; i++) {
        if (!connections_[i].active) {
            connections_[i].active = true;
            connections_[i].clientId = clientId;
            connections_[i].isDisplay = false;
            connections_[i].roomIndex = -1;
            connections_[i].playerIndex = -1;
            return &connections_[i];
        }
    }
    return nullptr;
}

void GameServer::freeConnection(uint32_t clientId) {
    for (uint8_t i = 0; i < Config::MAX_CONNECTIONS; i++) {
        if (connections_[i].active && connections_[i].clientId == clientId) {
            connections_[i].active = false;
            connections_[i].clientId = 0;
            connections_[i].roomIndex = -1;
            connections_[i].playerIndex = -1;
            return;
        }
    }
}

// =====================================================================
// 방(rooms_) 관리
// =====================================================================
int8_t GameServer::findRoomByChannel(const char *channel) const {
    for (uint8_t i = 0; i < Config::MAX_ROOMS; i++) {
        if (rooms_[i].active && strcmp(rooms_[i].channel, channel) == 0) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

int8_t GameServer::allocRoom(const char *channel) {
    for (uint8_t i = 0; i < Config::MAX_ROOMS; i++) {
        if (!rooms_[i].active) {
            GameRoom &room = rooms_[i];
            memset(&room, 0, sizeof(GameRoom));
            room.active = true;
            strncpy(room.channel, channel, sizeof(room.channel) - 1);
            room.channel[sizeof(room.channel) - 1] = '\0';
            room.gameStarted = false;
            room.playerCount = 0;
            room.taggerIndex = 0xFF;
            room.lastTick = millis();
            room.lastActivity = millis();
            room.lastObstacleUpdate = millis();
            initRoomMap(room);
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

void GameServer::initRoomMap(GameRoom &room) const {
    for (uint8_t y = 0; y < Config::MAP_HEIGHT; y++) {
        for (uint8_t x = 0; x < Config::MAP_WIDTH; x++) {
            room.map[y][x] = kDefaultMap[y][x];
        }
    }
}

void GameServer::findFreeStartPosition(GameRoom &room, int8_t &outX, int8_t &outY, bool &found) const {
    found = false;
    for (uint8_t y = 1; y < Config::MAP_HEIGHT - 1 && !found; y++) {
        for (uint8_t x = 1; x < Config::MAP_WIDTH - 1 && !found; x++) {
            if (room.map[y][x] != static_cast<uint8_t>(TileType::Empty)) continue;
            if (isPositionOccupied(room, static_cast<int8_t>(x), static_cast<int8_t>(y))) continue;
            outX = static_cast<int8_t>(x);
            outY = static_cast<int8_t>(y);
            found = true;
        }
    }
}

bool GameServer::isPositionOccupied(const GameRoom &room, int8_t x, int8_t y) const {
    for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
        const Player &p = room.players[i];
        if (p.active && p.connected && p.x == x && p.y == y) {
            return true;
        }
    }
    return false;
}

// =====================================================================
// 메시지 전송
// =====================================================================
void GameServer::sendJsonTo(AsyncWebSocketClient *client, JsonDocument &doc) {
    if (client == nullptr) return;
    String out;
    serializeJson(doc, out);
    client->text(out);
}

void GameServer::broadcastJson(uint8_t roomIndex, JsonDocument &doc, uint32_t excludeClientId) {
    String out;
    serializeJson(doc, out);

    for (uint8_t i = 0; i < Config::MAX_CONNECTIONS; i++) {
        ConnectionInfo &conn = connections_[i];
        if (!conn.active || conn.roomIndex != roomIndex) continue;
        if (excludeClientId != 0 && conn.clientId == excludeClientId) continue;

        AsyncWebSocketClient *client = ws_.client(conn.clientId);
        if (client != nullptr && client->status() == WS_CONNECTED) {
            client->text(out);
        }
    }
}

void GameServer::sendError(AsyncWebSocketClient *client, const char *message) {
    if (client == nullptr) return;
    JsonDocument doc;
    doc["type"] = WsMsgType::ERROR_MSG;
    doc["message"] = message;
    sendJsonTo(client, doc);
    Serial.printf("[WARN] 클라이언트(id=%u)에 오류 전송: %s\n", client->id(), message);
}

void GameServer::sendStateTo(AsyncWebSocketClient *client, uint8_t roomIndex, bool includeMap) {
    if (client == nullptr) return;
    JsonDocument doc;
    buildStateJson(doc, rooms_[roomIndex], includeMap);
    sendJsonTo(client, doc);
}

void GameServer::broadcastState(uint8_t roomIndex, bool includeMap, uint32_t excludeClientId) {
    JsonDocument doc;
    buildStateJson(doc, rooms_[roomIndex], includeMap);
    broadcastJson(roomIndex, doc, excludeClientId);
}

// state 메시지를 만든다.
// [맵 전송 정책]
// 맵은 참가 직후 1회, 그리고 장애물 동적 변경 시에만 맵 데이터를 포함해 보낸다.
// 일반 틱(이동/회전)에서는 플레이어 정보만 담아 대역폭을 절약한다.
void GameServer::buildStateJson(JsonDocument &doc, GameRoom &room, bool includeMap) {
    doc["type"] = WsMsgType::STATE;
    doc["channel"] = room.channel;
    doc["gameStarted"] = room.gameStarted;
    doc["tickInterval"] = Config::TICK_INTERVAL_MS;
    doc["taggerPlayerId"] = (room.taggerIndex == 0xFF) ? 0 : (room.taggerIndex + 1);

    JsonArray players = doc["players"].to<JsonArray>();
    uint8_t connectedCount = 0;
    for (uint8_t i = 0; i < Config::MAX_PLAYERS_PER_ROOM; i++) {
        Player &p = room.players[i];
        if (!p.active) continue;
        JsonObject po = players.add<JsonObject>();
        po["id"] = i + 1;
        po["name"] = p.name;
        po["x"] = p.x;
        po["y"] = p.y;
        po["direction"] = static_cast<uint8_t>(p.direction);
        po["isTagger"] = p.isTagger;
        po["connected"] = p.connected;
        if (p.connected) connectedCount++;
    }
    doc["playerCount"] = connectedCount;

    if (includeMap) {
        JsonObject map = doc["map"].to<JsonObject>();
        map["width"] = Config::MAP_WIDTH;
        map["height"] = Config::MAP_HEIGHT;
        JsonArray tiles = map["tiles"].to<JsonArray>();
        for (uint8_t y = 0; y < Config::MAP_HEIGHT; y++) {
            for (uint8_t x = 0; x < Config::MAP_WIDTH; x++) {
                tiles.add(room.map[y][x]);
            }
        }
    }
}

void GameServer::broadcastPlayerCaught(uint8_t roomIndex, uint8_t caughtIndex, uint8_t oldTaggerIndex) {
    GameRoom &room = rooms_[roomIndex];
    JsonDocument doc;
    doc["type"] = WsMsgType::PLAYER_CAUGHT;
    doc["channel"] = room.channel;
    doc["caughtPlayerId"] = caughtIndex + 1;
    doc["caughtPlayerName"] = room.players[caughtIndex].name;
    doc["newTaggerPlayerId"] = caughtIndex + 1;
    doc["previousTaggerPlayerId"] = oldTaggerIndex + 1;
    broadcastJson(roomIndex, doc);
}

void GameServer::broadcastPlayerDisconnected(uint8_t roomIndex, const char *name, int8_t playerId) {
    JsonDocument doc;
    doc["type"] = WsMsgType::PLAYER_DISCONNECTED;
    doc["channel"] = rooms_[roomIndex].channel;
    doc["playerId"] = playerId;
    doc["name"] = name;
    broadcastJson(roomIndex, doc);
}

// =====================================================================
// 검증 / 유틸리티
// =====================================================================
bool GameServer::normalizeChannelName(const char *raw, char *dest, size_t destSize) {
    size_t len = strlen(raw);
    if (len == 0 || len > destSize - 1) return false;
    for (size_t i = 0; i < len; i++) {
        char c = raw[i];
        if (!isalnum(static_cast<unsigned char>(c))) return false; // 영문/숫자만 허용
        dest[i] = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
    dest[len] = '\0';
    return true;
}

// 플레이어 이름을 안전하게 복사한다. 제어 문자는 제거하고, 한글처럼 UTF-8로
// 인코딩된 멀티바이트 문자가 버퍼 끝에서 잘리지 않도록 문자 단위로 자른다.
void GameServer::sanitizeUtf8Copy(char *dest, size_t destSize, const char *src) {
    size_t di = 0;
    size_t si = 0;
    size_t srcLen = strlen(src);

    while (si < srcLen && di < destSize - 1) {
        unsigned char c = static_cast<unsigned char>(src[si]);
        uint8_t charLen;

        if (c < 0x20) { si++; continue; } // 제어 문자는 건너뜀
        if ((c & 0x80) == 0x00) charLen = 1;
        else if ((c & 0xE0) == 0xC0) charLen = 2;
        else if ((c & 0xF0) == 0xE0) charLen = 3;
        else if ((c & 0xF8) == 0xF0) charLen = 4;
        else { si++; continue; } // 잘못된 UTF-8 시작 바이트는 건너뜀

        if (si + charLen > srcLen) break;        // 잘린 멀티바이트 문자는 포함하지 않음
        if (di + charLen > destSize - 1) break;   // 문자 중간에서 자르지 않고 여기서 멈춤

        memcpy(dest + di, src + si, charLen);
        di += charLen;
        si += charLen;
    }
    dest[di] = '\0';
}

void GameServer::generateToken(char *dest, size_t destSize) {
    static const char hexChars[] = "0123456789abcdef";
    size_t n = destSize - 1;
    for (size_t i = 0; i < n; i++) {
        dest[i] = hexChars[esp_random() % 16];
    }
    dest[n] = '\0';
}

void GameServer::directionDelta(Direction dir, int8_t &dx, int8_t &dy) {
    switch (dir) {
        case Direction::Up:    dx = 0;  dy = -1; break;
        case Direction::Right: dx = 1;  dy = 0;  break;
        case Direction::Down:  dx = 0;  dy = 1;  break;
        case Direction::Left:  dx = -1; dy = 0;  break;
        default:               dx = 0;  dy = 0;  break;
    }
}

Direction GameServer::rotateClockwise(Direction dir) {
    return static_cast<Direction>((static_cast<uint8_t>(dir) + 1) % 4);
}
