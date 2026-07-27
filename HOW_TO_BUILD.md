# 이 게임을 처음부터 어떻게 만드는가

이 문서는 완성된 코드를 "설명"하는 문서가 아니라, **여러분이 아무 것도 없는 빈 폴더에서 시작해서 이 게임을 직접 다시 만들 수 있도록**, 만든 순서 그대로 따라가며 "왜 이 순서로, 왜 이렇게" 만들었는지를 설명하는 문서입니다.

실제로 소프트웨어를 만들 때는 규칙부터 다 정해놓고 코드를 한 번에 쭉 쓰지 않습니다. **작게 만들고, 눈으로 확인하고, 그 위에 한 층씩 쌓는 것**이 핵심입니다. 아래 순서가 바로 그 방식입니다.

## 이 문서를 읽는 방법

- 각 단계는 **① 무엇을 만드는가 → ② 왜 이 순서인가 → ③ 어떻게 확인하는가 → ④ 이 프로젝트에서는 어디에 있는가** 순서로 되어 있습니다.
- 중간중간 나오는 상자들은 이런 뜻입니다.
  - 🔍 **왜 이렇게?** — 다른 선택지도 있었는데 왜 이걸 골랐는지
  - 💡 **실전 팁** — 실제로 개발할 때 이렇게 하면 편하다
  - ⚠️ **함정** — 여기서 대부분 몇 시간씩 날린다
  - ✍️ **연습** — 직접 고쳐보면 실력이 느는 지점
- 처음 읽을 때는 0~4단계를 손으로 직접 따라 해보세요. 5단계부터는 읽으면서 실제 코드를 같이 열어놓는 것이 좋습니다.

## 목차

| 단계 | 내용 | 이 단계의 "완료" 기준 |
|---|---|---|
| 0 | 전체 그림과 핵심 원칙 정하기 | 종이에 그림 한 장 |
| 0.5 | 빌드 파이프라인 이해하기 | `pio run`이 성공한다 |
| 1 | 시리얼 출력 살리기 | 모니터에 글자가 보인다 |
| 2 | Wi-Fi AP 켜기 | 휴대폰에 SSID가 보인다 |
| 3 | 정적 파일 서버 | 브라우저에 HTML이 보인다 |
| 3.5 | 코드를 어디에 둘 것인가 | 파일이 3덩이로 나뉜다 |
| 4 | WebSocket 연결만 | "연결됨" 로그가 뜬다 |
| 5 | 메시지 규격 설계 | 표 한 장 |
| 6 | 자료구조 설계 | 구조체 3개 |
| 7 | join 구현 | 새 플레이어 생성 로그 |
| 8 | turn 구현 | 방향이 바뀐다 |
| 9 | 게임 틱(자동 이동) | 가만히 둬도 움직인다 |
| 10 | 상태 전송 정책 | 화면이 갱신된다 |
| 11 | 잡기 판정 | 술래가 바뀐다 |
| 12 | 장애물 동적 변경 | 맵이 5초마다 변한다 |
| 13 | 프론트엔드 3종 | 실제로 플레이된다 |
| 14 | 예외 상황 대비 | 새로고침해도 안 죽는다 |
| 15 | 동시성(뮤텍스) | 오래 켜둬도 안 죽는다 |
| 16 | 입력 검증 | 이상한 값을 보내도 안 죽는다 |
| 17 | 통합 테스트 | 시나리오 8개 통과 |

부록: [A. 트러블슈팅](#부록-a-자주-겪는-문제와-해결법) · [B. 알려진 한계와 개선 과제](#부록-b-이-코드의-알려진-한계와-개선-과제) · [C. 명령어 치트시트](#부록-c-명령어-치트시트) · [D. 새 기능 추가 레시피](#부록-d-새-기능을-추가할-때의-5단계-레시피)

---

## 0단계. 시작하기 전에 — 전체 그림 먼저 그리기

코드를 한 줄도 쓰기 전에, 이 시스템에 등장하는 것이 몇 개이고 서로 어떻게 연결되는지부터 종이에 그려봅니다.

```
                    ┌──────────────────────────────┐
                    │        ESP32-S3 (보드)        │
                    │                              │
   Wi-Fi AP  ◀──────│  ① Wi-Fi 공유기 역할          │
   (SSID:            │  ② 웹서버 (HTML/CSS/JS 배달)  │
    ESP32-TAG-GAME)  │  ③ WebSocket 서버 (실시간)    │
                    │  ④ 게임 상태 (유일한 원본)     │
                    │     - 위치, 방향, 술래, 맵     │
                    └──────────────────────────────┘
                          ▲                  ▲
              ws://.../ws │                  │ ws://.../ws
                          │                  │
              ┌───────────┴────┐      ┌──────┴──────────┐
              │  휴대폰 브라우저  │      │  컴퓨터 브라우저   │
              │  controller.html│      │  display.html   │
              │  (조작기)        │      │  (게임 화면)      │
              │  회전 버튼만 있음  │      │  캔버스에 그리기만  │
              └────────────────┘      └─────────────────┘
```

이 그림에서 가장 중요한 결정 하나가 나옵니다.

> **"위치와 방향 계산은 ESP32만 한다. 브라우저는 절대 계산하지 않는다."**

🔍 **왜 이렇게?**
멀티플레이 게임에서 "누가 진짜 상태를 계산하느냐"는 세 가지 선택지가 있습니다.

| 방식 | 설명 | 이 프로젝트에 맞나? |
|---|---|---|
| 클라이언트 권위 | 각 브라우저가 계산하고 결과를 서버에 보고 | ❌ 두 폰의 계산이 어긋나면 중재 불가, 조작(치팅)도 쉬움 |
| 락스텝(lockstep) | 모두가 같은 입력을 받아 각자 똑같이 계산 | ❌ 한 명만 느려도 전원이 멈춤, 구현 난이도 높음 |
| **서버 권위(server-authoritative)** | **서버 한 곳만 계산, 나머지는 결과를 그리기만** | ✅ **가장 단순하고, 치팅이 원천 차단됨** |

세 번째를 고르면 이후 모든 설계가 자동으로 단순해집니다. "이 값을 누가 바꿀 수 있지?"라는 질문의 답이 항상 "서버만"이기 때문입니다. 앞으로 나올 결정 대부분이 이 원칙 하나에서 파생됩니다.

이 원칙이 실제 코드에서 어떻게 나타나는지 미리 봐두면 좋습니다.

- 회전 메시지에 플레이어 ID가 **없다** (`{"type":"turn","direction":"clockwise"}`) → 8단계
- 브라우저는 `x`, `y`를 **계산하지 않고 받기만** 한다 → 13단계
- 게임 시작 시 술래 뽑기도 서버가 `esp_random()`으로 → 11단계

💡 **실전 팁**: 이 그림을 종이에 그려서 모니터 옆에 붙여두세요. 개발 중에 "이 기능은 어느 쪽에 넣어야 하지?"라는 질문이 나오면, 답은 항상 "상태를 바꾸는 것 = 서버, 보여주는 것 = 브라우저"입니다.

---

## 0.5단계. 빌드 파이프라인 이해하기 — 코드가 보드까지 가는 길

많은 초보자가 여기서 막힙니다. **"내가 고친 코드가 실제로 보드에 올라갔는가"를 확신할 수 없으면 그 뒤의 모든 디버깅이 헛수고**가 되기 때문입니다.

### 이 프로젝트에는 "업로드"가 두 종류 있다

```
  ┌─ src/, lib/, include/  ──[컴파일]──▶  firmware.bin ──[업로드]──▶ 보드의 app0 영역
  │                                        (pio run -t upload)
  │
  └─ data/                 ──[이미지 생성]▶ littlefs.bin ──[업로드]──▶ 보드의 spiffs 영역
                                            (pio run -t uploadfs)
```

⚠️ **함정 (가장 많이 겪는 실수)**
**HTML/CSS/JS만 고쳤는데 `pio run -t upload`만 하면 아무 것도 안 바뀝니다.** `data/` 폴더는 펌웨어에 포함되지 않습니다. 반대로 C++ 코드만 고쳤는데 `uploadfs`만 하면 역시 안 바뀝니다.

외우는 방법:
- **`.cpp` / `.h` 고쳤다 → `upload`**
- **`data/` 안의 무엇이든 고쳤다 → `uploadfs`**
- **둘 다 고쳤다 → 둘 다**

### platformio.ini 한 줄씩 읽기

이 파일이 "빌드 설명서"입니다. [platformio.ini](platformio.ini)를 열어서 같이 보세요.

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32          ; 어떤 칩 계열인가 (툴체인/SDK를 여기서 받아옴)
board = esp32-s3-devkitc-1      ; 정확히 어떤 보드인가 (플래시 크기, 핀맵 등)
framework = arduino             ; Arduino API로 코딩한다 (ESP-IDF 대신)

monitor_speed = 115200          ; 시리얼 모니터 속도. Serial.begin()과 반드시 같아야 함
monitor_filters = esp32_exception_decoder   ; ★ 크래시 났을 때 주소를 함수 이름으로 번역
```

💡 **실전 팁 — `esp32_exception_decoder`는 반드시 켜세요.**
이게 없으면 보드가 죽었을 때 이런 걸 봅니다.
```
Backtrace: 0x400d1234:0x3ffb1f30 0x400d5678:0x3ffb1f50
```
켜져 있으면 이렇게 나옵니다.
```
Backtrace: GameServer::handleTurn(ConnectionInfo&) at GameServer.cpp:396
```
**"어디서 죽었는지 모르는 상태"와 "몇 번째 줄에서 죽었는지 아는 상태"의 차이**는 디버깅 시간으로 몇 시간 차이가 납니다.

```ini
build_flags =
    ; -D ARDUINO_USB_CDC_ON_BOOT=1
    ; -D ARDUINO_USB_MODE=1
    -I include
```

여기 두 줄이 **주석 처리되어 있다는 점**이 중요합니다. ESP32-S3 보드에는 보통 USB 단자가 **두 개**입니다.

| 단자 | 이름 | 필요한 설정 |
|---|---|---|
| `UART` 라고 쓰인 쪽 | USB-UART 변환칩(CP2102 등)을 거침 | 위 두 플래그 **불필요** (주석 처리 상태) |
| `USB` 라고 쓰인 쪽 | ESP32-S3의 네이티브 USB를 직접 사용 | 위 두 플래그 **필수** (주석 해제해야 함) |

⚠️ **함정**: 시리얼 모니터에 아무 글자도 안 나오면, 대부분 **케이블을 꽂은 단자와 이 설정이 안 맞는 것**입니다. 보드가 죽은 게 아닙니다. 단자를 바꿔 꽂아보거나, 두 줄의 주석을 풀고 다시 올려보세요.

`-I include` 는 컴파일러에게 "헤더 파일을 찾을 때 `include/` 폴더도 뒤져라"라고 알려주는 것입니다. 왜 필요한지는 3.5단계에서 설명합니다.

```ini
lib_deps =
    bblanchon/ArduinoJson @ ^7.2.0      ; JSON 만들기/읽기
    ESP32Async/AsyncTCP                 ; 비동기 TCP (아래 라이브러리의 토대)
    ESP32Async/ESPAsyncWebServer        ; 비동기 웹서버 + WebSocket
```

🔍 **왜 `ESP32Async/` 인가?**
원래 이 라이브러리들은 `me-no-dev`라는 개발자가 만들었는데, 관리가 중단되어 ESP32 Arduino 코어 3.x에서 컴파일 오류가 납니다. `ESP32Async` 조직이 이어받은 것이 현재 유지보수되는 공식 포크입니다. 인터넷의 오래된 예제를 따라 하다 보면 옛날 저장소를 쓰게 되어 빌드가 깨지는 일이 흔하니 주의하세요.

🔍 **왜 "비동기(Async)" 웹서버인가?**
Arduino 기본 `WebServer`는 요청 하나를 처리하는 동안 다른 일을 못 합니다. 그러면 게임 틱(9단계)이 밀립니다. `ESPAsyncWebServer`는 별도 태스크에서 콜백 방식으로 동작해서 `loop()`를 막지 않습니다. **대신 "콜백이 다른 태스크에서 실행된다"는 사실이 15단계의 동시성 문제를 낳습니다.** 공짜 점심은 없습니다.

```ini
board_build.filesystem = littlefs       ; data 폴더를 LittleFS 형식으로 만든다
board_build.partitions = partitions.csv ; 플래시 영역을 직접 나눈다
```

### partitions.csv — 플래시 메모리를 어떻게 나눌 것인가

[partitions.csv](partitions.csv)는 보드의 플래시(저장 공간)를 구역별로 자르는 표입니다.

```csv
# Name,      Type, SubType, Offset,   Size,     Flags
nvs,         data, nvs,     0x9000,   0x5000,     # 설정값 저장용 (Wi-Fi 자격증명 등)
otadata,     data, ota,     0xe000,   0x2000,     # 어느 app을 부팅할지 기록
app0,        app,  ota_0,   0x10000,  0x140000,   # 펌웨어 A (1.25MB)
app1,        app,  ota_1,   0x150000, 0x140000,   # 펌웨어 B (1.25MB)
spiffs,      data, spiffs,  0x290000, 0x160000,   # ★ data 폴더가 들어가는 곳 (1.375MB)
```

여기서 배울 점이 세 가지 있습니다.

**① 이름은 `spiffs`인데 실제 내용은 LittleFS다.**
헷갈리지만 이게 정상입니다. Arduino의 LittleFS 라이브러리는 기본적으로 `spiffs`라는 **이름표**가 붙은 파티션을 찾도록 되어 있습니다. 이름을 `littlefs`로 바꾸면 `LittleFS.begin()`이 실패합니다.

⚠️ **함정**: "이름이 이상한데?" 하고 고치면 마운트 실패합니다. **이름은 `spiffs` 그대로 두세요.**

**② 왜 4MB 기준으로 계산했나.**
`0x290000 + 0x160000 = 0x3F0000` ≈ 3.94MB로, 4MB 플래시에 딱 들어갑니다. 8MB/16MB 보드에서는 남는 공간을 안 쓸 뿐 문제없이 동작합니다. **가장 작은 보드 기준으로 맞춰두면 어느 보드에 꽂아도 동작한다**는 것이 이 선택의 이유입니다.

**③ `app1`은 지금 순전히 낭비다.**
`app0`/`app1`이 두 개인 이유는 OTA(무선 펌웨어 업데이트) 때문입니다. 이 프로젝트는 OTA를 안 쓰므로 1.25MB가 그냥 놀고 있습니다.

✍️ **연습**: `app1` 줄을 지우고 그 공간을 `spiffs`에 합쳐보세요. LittleFS 공간이 1.375MB → 2.6MB로 늘어납니다. 나중에 이미지나 사운드 파일을 넣고 싶을 때 필요한 작업입니다. (단, `otadata`와 `ota_0` 서브타입 대신 `factory`를 써야 합니다.)

### 빌드 → 업로드 → 확인 사이클

실제 개발할 때 반복하게 되는 명령들입니다.

```bash
pio run
```
컴파일만 합니다. **문법 오류를 잡을 때는 보드를 연결할 필요도 없습니다.** 가장 빠른 확인 방법이니 자주 돌리세요.

```bash
pio run --target upload
```
컴파일 + 펌웨어 업로드.

```bash
pio run --target uploadfs
```
`data/` 폴더를 LittleFS 이미지로 만들어서 업로드.

```bash
pio device monitor
```
시리얼 모니터 열기. (`Ctrl+C`로 나옴)

💡 **실전 팁 — 개발 중 가장 자주 쓰는 조합**
```bash
pio run --target upload && pio device monitor
```
업로드가 성공하면 바로 모니터를 열어서 부팅 로그를 놓치지 않고 봅니다. `&&`는 "앞이 성공했을 때만 뒤를 실행"이라는 뜻이라, 업로드 실패 시 엉뚱한 모니터가 열리지 않습니다.

---

## 1단계. 뼈대부터: "켜졌다"는 것만 확인하기

가장 먼저 할 일은 게임 로직이 아니라 **"보드에 코드가 올라가고 살아있다"**는 것을 눈으로 보는 것입니다.

1. PlatformIO로 빈 프로젝트를 만듭니다 (`platformio.ini`, `src/main.cpp`만 있는 상태).
2. `platformio.ini`에 보드와 `monitor_speed`를 넣습니다.
3. `setup()`에서 `Serial.begin(115200)` 하고 `Serial.println("살아있음")` 정도만 찍고, `loop()`는 비워둡니다.
4. 업로드 → 시리얼 모니터 → 글자가 보이면 1단계 완료.

🔍 **왜 이걸 제일 먼저 하나?**
**눈이 없으면 그 뒤로는 전부 깜깜이로 개발하는 것과 같습니다.** Wi-Fi가 안 되는지, 웹서버가 안 되는지, 코드가 아예 안 올라간 건지 구분할 방법이 시리얼 출력밖에 없습니다. 여기서 30분 쓰는 게 나중에 5시간을 아낍니다.

### 실제 코드에서 배울 점: 시리얼을 "기다리는" 코드

[src/main.cpp:26](src/main.cpp:26)를 보세요.

```cpp
unsigned long serialWaitStart = millis();
while (!Serial && (millis() - serialWaitStart) < 3000) {
    delay(10);
}
```

네이티브 USB(USB-CDC)를 쓰면 PC가 USB 장치를 인식하는 데 시간이 걸려서, `Serial.begin()` 직후에 찍은 로그가 **통째로 사라집니다.** 그래서 "PC가 준비될 때까지 최대 3초 기다린다"는 코드가 필요합니다.

⚠️ **`while (!Serial);` 만 쓰면 안 되는 이유**: PC에 연결하지 않고 보드만 전원에 꽂으면 `Serial`이 영원히 준비되지 않아서 **부팅이 여기서 멈춰버립니다.** 반드시 위처럼 "최대 몇 초까지만"이라는 탈출 조건을 붙이세요. 이런 패턴을 **타임아웃(timeout)** 이라고 하며, 임베디드에서 "기다리는 코드"에는 거의 항상 붙여야 합니다.

### 로그를 남기는 습관: 접두어 붙이기

```
[INFO] ===== ESP32-S3 격자 술래잡기 서버 부팅 시작 =====
[ERROR] LittleFS 마운트 실패!
[WS] 클라이언트 연결됨 (id=1, ip=192.168.4.2)
[GAME] 새 플레이어 생성: 채널=A, 이름=철수, 위치=(1,1)
[WARN] 클라이언트(id=3)에 오류 전송: ...
```

💡 **실전 팁**: 처음부터 `[INFO]`, `[ERROR]`, `[WS]`, `[GAME]` 같은 접두어를 붙이는 습관을 들이세요. 로그가 100줄씩 쏟아질 때, 눈으로 `[GAME]`만 훑으면 게임 흐름만 볼 수 있습니다. 나중에 붙이려면 수십 군데를 고쳐야 합니다.

---

## 2단계. Wi-Fi AP 켜기 — 휴대폰으로 신호가 잡히는지 확인

다음 층은 "ESP32가 자기만의 Wi-Fi를 만든다"입니다. 아직 웹서버도, 게임도 없이 **Wi-Fi AP만** 켭니다.

```cpp
WiFi.mode(WIFI_AP);
WiFi.softAP("ESP32-TAG-GAME", "12345678");
Serial.println(WiFi.softAPIP()); // 보통 192.168.4.1
```

업로드하고 휴대폰 Wi-Fi 목록에 `ESP32-TAG-GAME`이 뜨는지, 비밀번호를 넣고 연결이 되는지만 확인합니다. 아직 브라우저로 접속은 안 해도 됩니다.

🔍 **왜 AP 모드(공유기 역할)인가?**
선택지가 두 가지입니다.

| 모드 | 동작 | 장단점 |
|---|---|---|
| STA (station) | ESP32가 집 공유기에 접속 | 인터넷은 되지만, 공유기가 없는 곳(교실, 야외)에서는 못 씀. IP도 매번 바뀜 |
| **AP (access point)** | **ESP32가 스스로 공유기가 됨** | **공유기 없이 어디서나 동작. IP가 항상 192.168.4.1로 고정** |

교실이나 행사장에서 여러 대의 휴대폰을 모아 플레이하는 게 목적이므로 AP가 맞습니다. **"항상 192.168.4.1"이라는 고정 주소**를 얻는 것도 큰 장점입니다 — 접속 주소를 안내하기 쉽습니다.

### 이 프로젝트의 실제 호출

[src/main.cpp:53](src/main.cpp:53)에서는 인자를 5개 넘깁니다.

```cpp
WiFi.softAP(Config::WIFI_SSID,          // "ESP32-TAG-GAME"
            Config::WIFI_PASSWORD,      // "12345678"
            Config::WIFI_CHANNEL,       // 1
            0,                          // ssid_hidden: 0 = 이름을 숨기지 않음
            Config::WIFI_MAX_CONNECTIONS); // 8
```

`WIFI_MAX_CONNECTIONS = 8`에 주목하세요. [include/Config.h:23](include/Config.h:23)에 이유가 적혀 있습니다.

> ESP32 Wi-Fi AP는 **하드웨어 특성상 동시 접속 기기 수가 8~10대 정도**로 제한된다.

⚠️ **함정 — 두 종류의 "정원"이 있다**
이 프로젝트에는 인원 제한이 **두 겹**입니다.

| 제한 | 값 | 의미 |
|---|---|---|
| `WIFI_MAX_CONNECTIONS` | 8 | **무선으로 연결 가능한 기기 수** (하드웨어 한계) |
| `MAX_ROOMS × (플레이어 8 + 화면 4)` | 48 | **소프트웨어가 관리할 수 있는 연결 정보 슬롯 수** |

소프트웨어상으로는 48개까지 준비되어 있지만, **실제로는 Wi-Fi 단계에서 8대를 넘으면 접속 자체가 안 됩니다.** "4채널 × 8명 = 32명이 되겠지?"라고 생각하면 안 됩니다. 이런 **"보이지 않는 진짜 병목"**을 미리 파악해두는 것이 임베디드 설계의 핵심입니다.

💡 **실전 팁**: Wi-Fi 비밀번호는 8자 이상이어야 합니다 (WPA2 규격). `1234`처럼 짧게 하면 `softAP()`가 조용히 실패하거나 암호 없는 AP가 됩니다.

> **이 프로젝트에서는**: [include/Config.h](include/Config.h)에 SSID/비밀번호를 상수로 빼두고, [src/main.cpp:52](src/main.cpp:52)의 `WiFi.softAP(...)` 호출 부분이 이 단계입니다. 반환값 `apOk`를 검사해서 실패 시 `[ERROR]` 로그를 남기는 것도 눈여겨보세요 — **"성공했겠지"라고 가정하지 않는 습관**입니다.

---

## 3단계. 정적 파일 서버 — 브라우저에 HTML 한 장 띄우기

이제 "브라우저로 뭔가 보이게" 만듭니다. 게임 로직 없이, `<h1>안녕</h1>` 한 줄짜리 HTML 파일 하나만 준비합니다.

1. `data/index.html`에 아주 간단한 HTML을 하나 넣습니다.
2. `platformio.ini`에 `board_build.filesystem = littlefs`를 추가합니다.
3. `main.cpp`에서 `LittleFS.begin(true)`로 파일시스템을 마운트하고, `AsyncWebServer`를 만들어 `serveStatic()`으로 `data` 폴더를 통째로 웹에 연결합니다.
4. **펌웨어 업로드와는 별개로** `pio run --target uploadfs`로 `data` 폴더 내용만 따로 올립니다.
5. 브라우저에서 `http://192.168.4.1` 접속 → 글자가 보이면 완료.

### `serveStatic` 한 줄이 하는 일

[src/main.cpp:71](src/main.cpp:71):

```cpp
httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
```

이 한 줄이 이런 매핑을 전부 자동으로 만들어 줍니다.

| 브라우저가 요청한 주소 | 실제로 읽는 파일 |
|---|---|
| `http://192.168.4.1/` | `data/index.html` ← `setDefaultFile` 덕분 |
| `http://192.168.4.1/controller.html` | `data/controller.html` |
| `http://192.168.4.1/js/common.js` | `data/js/common.js` |
| `http://192.168.4.1/css/style.css` | `data/css/style.css` |

**파일을 하나 추가할 때마다 C++ 코드를 고칠 필요가 없습니다.** `data/` 폴더에 넣고 `uploadfs`만 하면 됩니다. 이것이 HTML을 C++ 문자열에 박아 넣는 방식(`PROGMEM`에 통째로 넣기)보다 훨씬 나은 이유입니다.

🔍 **왜 HTML을 코드에 박지 않고 파일시스템에 두나?**

| 방식 | 장점 | 단점 |
|---|---|---|
| C++ 문자열에 박기 | 업로드가 한 번으로 끝남 | 따옴표 이스케이프 지옥, 문법 강조 안 됨, HTML 한 글자 고쳐도 전체 재컴파일(1분+) |
| **LittleFS 파일** | **일반 HTML 파일로 편집 가능, 재컴파일 불필요** | **업로드가 두 종류로 나뉨** |

프론트엔드를 자주 고치는 프로젝트에서는 두 번째가 압도적으로 유리합니다. `uploadfs`는 컴파일을 거치지 않아서 몇 초면 끝납니다.

### 부팅 시 파일 목록을 찍어보는 이유

[src/main.cpp:38-49](src/main.cpp:38)를 보세요. 마운트 성공 후 저장된 파일을 전부 출력합니다.

```cpp
File root = LittleFS.open("/");
File file = root.openNextFile();
if (!file) Serial.println("  (파일이 하나도 없습니다! 비어있음)");
while (file) {
    Serial.print("  - "); Serial.print(file.name());
    Serial.print(" ("); Serial.print(file.size()); Serial.println(" bytes)");
    file = root.openNextFile();
}
```

💡 **실전 팁 — 이 코드는 "디버깅 도구"입니다.**
"브라우저에 404가 뜬다"는 문제가 생겼을 때, 원인은 대개 셋 중 하나입니다.
1. `uploadfs`를 안 했다 → 목록이 **비어 있음**
2. 파일 이름 오타 (`comon.js`) → 목록에 **틀린 이름이 보임**
3. 코드의 경로가 틀림 → 목록은 **정상인데 404**

**부팅 로그만 보고 세 가지를 구분할 수 있게 만들어 둔 것**입니다. 이렇게 "문제가 생겼을 때 원인을 좁혀주는 출력"을 미리 심어두는 것을 **관측 가능성(observability)을 확보한다**고 합니다.

### `LittleFS.begin(true)`의 `true`

인자 `true`는 "마운트 실패하면 포맷해라"는 뜻입니다. 처음 쓰는 보드에서 파일시스템이 없을 때 자동으로 만들어 주므로 편합니다.

⚠️ **함정**: 실제 제품이라면 이건 위험합니다. 파일시스템이 잠깐 손상됐을 뿐인데 **사용자 데이터를 전부 날려버릴 수 있습니다.** 프로토타입이라 편의를 택한 것이며, 이런 선택은 코드 옆에 이유를 적어두는 게 좋습니다.

### 404 핸들러

[src/main.cpp:73](src/main.cpp:73):

```cpp
httpServer.onNotFound([](AsyncWebServerRequest *request) {
    Serial.printf("[WARN] 404 Not Found: %s\n", request->url().c_str());
    request->send(404, "text/plain", "404: \355\216\230...");
});
```

**어떤 주소를 요청했다가 실패했는지 시리얼에 찍는 것**이 핵심입니다. 브라우저 개발자 도구를 켜지 않고도 "아, `js/common.js`를 못 찾는구나"를 알 수 있습니다.

응답 문자열의 `\355\216\230...`는 한글 "페이지를 찾을 수 없습니다"를 **UTF-8 바이트의 8진수 표기로 직접 쓴 것**입니다.

🔍 **왜 이렇게 썼나?** 소스 파일의 인코딩이나 컴파일러 설정에 따라 한글 문자열이 깨지는 경우가 있어서, **어떤 환경에서도 똑같은 바이트가 나가도록** 못 박은 것입니다. 다만 읽기가 매우 어려우므로, 이런 코드에는 반드시 옆에 주석으로 원문을 적어두어야 합니다 (이 프로젝트도 `// "플레이어N"` 처럼 적어두고 있습니다 — [GameServer.cpp:339](lib/GameServer/GameServer.cpp:339)).

> **이 프로젝트에서는**: [data/index.html](data/index.html)이 이 단계에서 시작해서 점점 채널/이름 입력 폼이 붙은 지금 모습으로 자랐습니다.

---

## 3.5단계. 코드를 어디에 둘 것인가 — src / lib / include

`main.cpp` 하나에 전부 넣으면 처음엔 편하지만, 1000줄이 넘어가면 원하는 함수를 찾는 데만 한참 걸립니다. **어느 시점에 파일을 나눌 것인가, 그리고 어떤 기준으로 나눌 것인가**를 여기서 정합니다.

PlatformIO 프로젝트에는 폴더 세 개가 미리 준비되어 있고, 각각 역할이 다릅니다.

```
jump/
├── platformio.ini      빌드 설명서
├── partitions.csv      플래시 영역 나누기
├── src/
│   └── main.cpp        ★ 진입점(setup/loop)만. 얇게 유지한다.
├── include/            ★ 여러 곳에서 공유하는 헤더
│   ├── Config.h            - 설정값(매직 넘버) 모음
│   ├── GameTypes.h         - 자료구조 정의
│   └── WebSocketProtocol.h - 메시지 이름 상수
├── lib/
│   └── GameServer/     ★ 독립적인 "모듈". 알아서 컴파일된다.
│       ├── GameServer.h
│       └── GameServer.cpp
└── data/               ★ 브라우저로 전송할 파일들 (컴파일 대상 아님)
    ├── index.html / controller.html / display.html
    ├── css/style.css
    └── js/common.js / controller.js / display.js
```

### 나누는 기준

| 폴더 | 여기에 넣는 것 | 이 프로젝트의 예 |
|---|---|---|
| `src/` | 프로그램의 시작점, 부품들을 조립하는 코드 | `main.cpp` (90줄. **의도적으로 짧게 유지**) |
| `lib/<이름>/` | 그 자체로 하나의 기능 덩어리. 다른 프로젝트에 복사해 갈 수도 있는 것 | `GameServer/` (게임 로직 전체) |
| `include/` | 여러 파일이 **공유**하는 선언 (설정, 타입, 상수) | `Config.h`, `GameTypes.h`, `WebSocketProtocol.h` |

🔍 **왜 `main.cpp`를 90줄로 유지했나?**
`main.cpp`를 읽으면 **"이 시스템이 무엇으로 이루어져 있는지"가 5초 안에 파악**되어야 합니다. 지금 `setup()`을 읽으면 이렇게 읽힙니다.

> 시리얼 켜기 → 파일시스템 마운트 → Wi-Fi AP 켜기 → 게임서버 시작 → 정적 파일 연결 → 웹서버 시작

세부 로직이 여기 섞여 있으면 이 흐름이 안 보입니다. 이런 구조를 **"조립은 위에서, 세부는 아래에서"**라고 부릅니다.

### `lib/` 폴더의 마법과 함정

PlatformIO는 `lib/` 안의 각 폴더를 **자동으로 별도 라이브러리로 컴파일**합니다. `platformio.ini`에 아무 것도 안 적어도 `lib/GameServer/GameServer.cpp`가 빌드에 포함됩니다.

⚠️ **함정 — 여기가 `-I include`가 필요한 이유입니다.**
PlatformIO는 각 라이브러리를 **독립적인 것**으로 취급합니다. 그래서 `lib/GameServer/GameServer.h`에서

```cpp
#include "Config.h"   // ← include/Config.h 를 쓰고 싶다
```

라고 쓰면 **"Config.h: No such file or directory"** 오류가 납니다. 프로젝트의 `include/`는 `src/`에는 자동으로 열려 있지만 `lib/`에는 열려 있지 않기 때문입니다.

해결책이 바로 이것입니다.

```ini
build_flags =
    -I include        ; 모든 컴파일 단위에서 include/ 폴더를 뒤지게 한다
```

💡 **실전 팁**: `lib/` 안의 코드에서 `include/`의 헤더를 못 찾는다는 오류를 만나면, 십중팔구 이 플래그가 없는 것입니다. 이건 PlatformIO 초보자가 반드시 한 번은 겪는 문제입니다.

### 헤더 파일을 세 개로 나눈 기준

한 파일에 다 넣어도 동작은 합니다. 그런데도 셋으로 나눈 이유는 **"고치는 이유가 서로 다르기 때문"**입니다.

| 파일 | 고치게 되는 상황 |
|---|---|
| [Config.h](include/Config.h) | "게임 속도를 바꾸고 싶다", "최대 인원을 늘리고 싶다" |
| [GameTypes.h](include/GameTypes.h) | "플레이어에게 점수 항목을 추가하고 싶다" |
| [WebSocketProtocol.h](include/WebSocketProtocol.h) | "새로운 메시지 종류를 추가하고 싶다" |

**"함께 바뀌는 것끼리 모으고, 다른 이유로 바뀌는 것은 떼어놓는다"** — 이것이 파일을 나누는 가장 실용적인 기준입니다.

`#pragma once`가 모든 헤더 맨 위에 있는 것도 확인하세요. 같은 헤더가 두 번 포함되어 "재정의 오류"가 나는 것을 막아줍니다. **헤더 파일을 만들면 반사적으로 첫 줄에 쓰는 습관**을 들이세요.

---

## 4단계. WebSocket 연결만 — 아직 게임 없이 "핑퐁"만

HTTP(정적 파일)와 WebSocket(실시간 통신)은 다른 것입니다. 게임을 만들기 전에 **WebSocket 연결 자체가 되는지**부터 확인합니다.

### 왜 HTTP로는 안 되는가

| | HTTP | WebSocket |
|---|---|---|
| 방향 | 클라이언트가 물어봐야 서버가 답함 | **양쪽 모두 아무 때나 보낼 수 있음** |
| 연결 | 요청마다 새로 맺고 끊음 | **한 번 맺으면 계속 유지** |
| 이 게임에 쓰면 | 브라우저가 0.4초마다 "바뀐 거 있어?"라고 물어봐야 함(폴링) → 낭비 + 느림 | **서버가 움직인 순간 바로 밀어줌** |

게임처럼 **서버가 먼저 말을 걸어야 하는** 상황에서는 WebSocket이 정답입니다.

### 최소한의 코드로 연결만 확인

서버 쪽:
```cpp
AsyncWebSocket ws("/ws");
ws.onEvent([](AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type,
              void*, uint8_t*, size_t){
  if (type == WS_EVT_CONNECT) Serial.println("연결됨");
});
server.addHandler(&ws);
```

브라우저 콘솔 쪽 (F12 → Console에 직접 붙여넣기):
```js
const ws = new WebSocket("ws://192.168.4.1/ws");
ws.onopen = () => console.log("연결됨");
```

이 둘만 맞춰보고 시리얼/콘솔에 "연결됨"이 뜨는지 확인합니다. **아직 JSON도, join도, 게임 로직도 없습니다.**

💡 **실전 팁 — 브라우저 콘솔이 최고의 테스트 도구입니다.**
HTML 파일을 만들기 전에도 F12 콘솔에서 위 두 줄만 쳐보면 서버가 정상인지 확인할 수 있습니다. **"프론트엔드를 만들지 않고 백엔드만 테스트하는 방법"**을 아는 것은 큰 무기입니다.

### 실제 코드에서 배울 점 ①: 람다로 멤버 함수 연결하기

[GameServer.cpp:53](lib/GameServer/GameServer.cpp:53):

```cpp
ws_.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                    AwsEventType type, void *arg, uint8_t *data, size_t len) {
    this->onEvent(server, client, type, arg, data, len);
});
```

`onEvent()`는 일반 함수 포인터가 아니라 `std::function`을 받으므로 **람다를 넘길 수 있고, 람다는 `this`를 캡처할 수 있습니다.** 이 덕분에 콜백 안에서 `rooms_` 같은 멤버 변수에 접근할 수 있습니다.

🔍 **왜 굳이 한 단계 거치나?** 람다 본문에 로직을 다 쓰면 `begin()` 함수가 수백 줄이 됩니다. **"콜백은 등록만 하고, 실제 처리는 이름 있는 멤버 함수로 넘긴다"**는 패턴을 쓰면 코드가 얕게 유지됩니다.

### 실제 코드에서 배울 점 ②: 데이터 프레임 검사

[GameServer.cpp:112-121](lib/GameServer/GameServer.cpp:112):

```cpp
case WS_EVT_DATA: {
    AwsFrameInfo *info = reinterpret_cast<AwsFrameInfo *>(arg);
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        handleMessage(client, data, len);
    }
    break;
}
```

WebSocket 메시지는 **여러 조각(fragment)으로 쪼개져서 도착할 수 있습니다.** 큰 파일을 보낼 때 그렇습니다. 이 네 가지 조건은 "쪼개지지 않은, 한 번에 다 온, 텍스트 메시지"만 처리하겠다는 뜻입니다.

| 조건 | 의미 |
|---|---|
| `info->final` | 이게 마지막 조각인가 |
| `info->index == 0` | 첫 조각부터 시작하는가 |
| `info->len == len` | 전체 길이와 이번에 받은 길이가 같은가 (= 한 방에 다 왔는가) |
| `info->opcode == WS_TEXT` | 바이너리가 아니라 텍스트인가 |

🔍 **왜 조각난 메시지를 처리하지 않아도 되나?** 이 게임의 메시지는 가장 길어야 200바이트 정도입니다. 브라우저 WebSocket은 이렇게 짧은 메시지를 절대 쪼개지 않습니다. **"일어나지 않을 상황을 처리하는 코드를 미리 쓰지 않는다"**는 판단이며, 대신 조건에 안 맞는 메시지는 **조용히 버려지므로 안전**합니다.

⚠️ 만약 나중에 큰 데이터(예: 맵 편집기에서 만든 맵 업로드)를 받아야 한다면 이 부분에 조각 모으기 로직을 추가해야 합니다. **지금은 안 만들지만 어디를 고쳐야 하는지는 알고 있는 상태** — 이게 좋은 프로토타입입니다.

> **이 프로젝트에서는**: [GameServer.cpp:99](lib/GameServer/GameServer.cpp:99)의 `onEvent()` 함수 중 `WS_EVT_CONNECT`/`WS_EVT_DISCONNECT` 로그 부분이 이 단계의 흔적입니다. 연결 시 IP까지 찍어두면(`client->remoteIP()`) "몇 번 폰이 몇 번 id인지" 추적하기 쉬워집니다.

---

## 5단계. 메시지 규격을 코드보다 먼저 "종이"에 정하기

여기서부터가 진짜 설계입니다. **코드를 쓰기 전에 클라이언트↔서버가 주고받을 JSON 메시지의 모양을 먼저 문서로 정합니다.**

🔍 **왜 코드보다 문서가 먼저인가?**
프론트엔드(브라우저)와 백엔드(ESP32)를 **동시에** 만들 것이기 때문입니다. 규격이 없으면 브라우저는 `{"cmd":"rotate"}`를 보내는데 서버는 `{"type":"turn"}`을 기다리는 상황이 벌어지고, **양쪽 다 코드는 멀쩡한데 동작만 안 되는** 최악의 디버깅 상황에 빠집니다. 혼자 만들더라도 마찬가지입니다 — 3일 뒤의 나는 남입니다.

### 이 프로젝트가 정한 규칙 (이 순서로 하나씩 결정했습니다)

**규칙 1. 모든 메시지는 `{"type": "..."}` 필드로 종류를 구분한다.**

```json
{"type": "turn", "direction": "clockwise"}
```

서버 코드가 `strcmp(type, "turn")` 한 번으로 분기할 수 있어 가장 단순합니다.

**규칙 2. 회전 메시지에 "누구를" 정보를 아예 넣지 않는다.**

```json
{"type":"turn","direction":"clockwise"}   ← playerId가 없다!
```

플레이어 ID를 클라이언트가 보내게 하면, 브라우저 콘솔에서 `ws.send('{"type":"turn","playerId":3}')`만 쳐도 **남의 캐릭터를 조종할 수 있습니다.** 그래서 "이 WebSocket 연결이 누구인지"는 **서버가 연결 시점에 기억해뒀다가** 그 정보만 사용합니다. (이게 6단계의 `ConnectionInfo`로 이어집니다.)

💡 **일반 원칙**: **클라이언트가 보낸 값으로 "누구인지"를 판단하면 안 됩니다.** 웹 개발에서 세션/토큰을 쓰는 이유와 완전히 같습니다.

**규칙 3. 서버 → 클라이언트 메시지는 "전체 상태"와 "사건 알림"으로 나눈다.**

| 종류 | 예 | 성격 |
|---|---|---|
| 전체 상태 | `state` | **지금 이 순간의 모든 것**. 이것만 받아도 화면을 완전히 그릴 수 있음 |
| 사건 알림 | `playerCaught`, `playerDisconnected`, `gameStarted` | **방금 무슨 일이 있었는지**. 효과음/토스트 메시지용 |

🔍 **왜 나누나?** 사건 알림만 보내면 메시지 하나를 놓쳤을 때 화면이 영원히 틀어집니다. 전체 상태를 주기적으로 보내면 **한 번 놓쳐도 다음 상태 메시지에서 저절로 복구**됩니다. 이런 성질을 **자기 치유(self-healing)**라고 합니다. 반대로 전체 상태만 보내면 "누가 잡혔다"는 순간을 놓쳐서 연출을 못 합니다. 그래서 둘 다 씁니다.

### 전체 메시지 규격표

**클라이언트 → 서버**

| type | 필드 | 누가 보내나 | 설명 |
|---|---|---|---|
| `join` | `role`, `channel`, `name`, `token` | 양쪽 | 참가. `role`은 `controller` 또는 `display` |
| `turn` | `direction` | 컨트롤러 | 시계 방향 90도 회전 |
| `startGame` | (없음) | **디스플레이만** | 게임 시작 |
| `requestState` | (없음) | 양쪽 | 상태 재요청 (맵 포함) |
| `ping` | (없음) | 양쪽 | 살아있는지 확인 |

**서버 → 클라이언트**

| type | 주요 필드 | 언제 보내나 |
|---|---|---|
| `joined` | `role`, `channel`, `token`, `playerId`, `name` | join 성공 직후 (그 클라이언트에게만) |
| `state` | `players[]`, `gameStarted`, `taggerPlayerId`, `map?` | 참가 시 / 매 틱 / 회전 시 / 장애물 변경 시 |
| `error` | `message` | 뭔가 잘못됐을 때 (그 클라이언트에게만) |
| `gameStarted` | `taggerPlayerId`, `taggerName` | 게임 시작 시 (방 전체) |
| `playerCaught` | `caughtPlayerId`, `caughtPlayerName`, `previousTaggerPlayerId` | 잡혔을 때 (방 전체) |
| `playerDisconnected` | `playerId`, `name` | 연결이 끊겼을 때 (방 전체) |
| `pong` | (없음) | `ping`에 대한 응답 |

### `state` 메시지의 실제 모양

```json
{
  "type": "state",
  "channel": "A",
  "gameStarted": true,
  "tickInterval": 400,
  "taggerPlayerId": 2,
  "playerCount": 3,
  "players": [
    {"id":1, "name":"철수", "x":3, "y":5, "direction":1, "isTagger":false, "connected":true},
    {"id":2, "name":"영희", "x":7, "y":2, "direction":0, "isTagger":true,  "connected":true}
  ],
  "map": {                          ← 이 필드는 있을 때도 있고 없을 때도 있다 (10단계)
    "width": 12, "height": 12,
    "tiles": [1,1,1,...]            ← 144개. 0=빈칸, 1=벽, 2=장애물
  }
}
```

여기서 설계 결정 두 개를 더 볼 수 있습니다.

**① `id`는 배열 인덱스 + 1이다.**
서버 내부에서는 `players[0]`인데 클라이언트에는 `id: 1`로 보냅니다. 사용자에게 "0번 플레이어"라고 하면 어색하기 때문입니다. 코드 곳곳에 `playerIndex + 1`과 `id - 1`이 보이는 이유입니다.

⚠️ **함정**: 이런 "+1 / -1 변환"은 실수하기 딱 좋은 지점입니다. **"서버 내부는 인덱스(0부터), 통신은 ID(1부터)"**라는 규칙을 정하고, 변환은 오직 JSON을 만들 때와 읽을 때만 하도록 위치를 한정하세요.

**② `tiles`는 2차원이 아니라 1차원 배열이다.**
`[[1,1,1],[1,0,1]]`이 아니라 `[1,1,1,1,0,1]`로 보냅니다. JSON 크기가 작아지고, 브라우저에서는 `tiles[y * width + x]`로 읽으면 됩니다 ([display.js:151](data/js/display.js:151)의 `tileAt()`).

### 만들어뒀지만 아직 안 쓰는 것들

정직하게 짚고 넘어갑시다. 코드에는 있지만 **현재 프론트엔드가 쓰지 않는** 메시지가 둘 있습니다.

- `ping` / `pong` — 서버는 응답할 준비가 되어 있지만 브라우저가 보내지 않습니다.
- `requestState` — 서버는 처리하지만 브라우저가 보내지 않습니다.

🔍 **이건 나쁜 코드인가?** 상황에 따라 다릅니다. 지금은 **재연결 시 `join`을 다시 보내면 `state`가 따라오므로** `requestState`가 필요 없습니다. 다만 **"쓰지 않는 코드가 있다"는 사실을 문서에 적어두지 않으면**, 나중에 읽는 사람이 "이게 어디서 호출되지?" 하고 한참 찾게 됩니다.

✍️ **연습**: `common.js`의 `createGameSocket`에 30초마다 `ping`을 보내는 하트비트를 추가해보세요. Wi-Fi가 조용히 끊겼는데 브라우저가 눈치채지 못하는 상황(**좀비 연결**)을 감지할 수 있습니다.

> **이 프로젝트에서는**: [include/WebSocketProtocol.h](include/WebSocketProtocol.h)가 이 "합의된 규격"을 코드 상수로 옮겨놓은 것입니다. `"join"`이라고 여기저기 직접 타이핑하다가 `"jion"`으로 오타를 내면, **컴파일은 성공하는데 동작만 안 됩니다.** 상수로 만들어 두면 `WsMsgType::JION`은 컴파일 오류가 나서 즉시 잡힙니다. **"실수를 런타임 버그가 아니라 컴파일 오류로 만드는 것"**이 좋은 설계의 핵심 기법입니다.

---

## 6단계. 데이터 구조 설계 — "무엇을 기억해야 하는가"부터 나열하기

메시지 규격이 정해졌으면, 이제 **서버가 무엇을 기억하고 있어야 이 메시지들을 처리할 수 있는지**를 거꾸로 추론합니다.

| 처리해야 할 메시지 | 그러려면 무엇을 알아야 하나 | 필요한 것 |
|---|---|---|
| `join` | 이 채널이 이미 있나? | **채널(방) 목록** |
| `join` | 이 토큰을 가진 사람이 이미 있나? | **방 안의 플레이어 목록** |
| `turn` | 이 WebSocket이 어느 방 몇 번 플레이어인가? | **연결별 정보** |
| 게임 틱 | 이 방의 맵은 어떻게 생겼나? | **방마다 맵 사본** |
| 잡기 판정 | 지금 술래가 누구인가? | **술래 인덱스** |

이렇게 거꾸로 따라가면 자연스럽게 구조체 세 개가 나옵니다. ([include/GameTypes.h](include/GameTypes.h))

```cpp
struct Player     { active, connected, clientId, token[33], name[17], x, y,
                    direction, isTagger, lastSeen, lastTurnTime };
struct GameRoom   { active, channel[17], gameStarted, players[8], playerCount,
                    taggerIndex, map[12][12], lastTick, lastActivity, lastObstacleUpdate };
struct ConnectionInfo { active, clientId, isDisplay, roomIndex, playerIndex };
```

### 결정 ①: 왜 `String` 대신 고정 크기 배열(`char name[17]`)인가

ESP32는 RAM이 약 320KB(+PSRAM)뿐입니다. `String`을 계속 만들었다 지웠다 하면 **메모리 조각남(fragmentation)** 이 생깁니다.

```
처음:   [■■■■■■■■■■■■■■■■■■■■]  ← 20칸 연속으로 비어있음
사용 중: [■□■□■□■□■■□■□■□■■□■□]  ← 총 10칸이 비었지만
                                    "연속된 5칸"이 없어서 할당 실패!
```

**총 여유 메모리는 충분한데 할당이 실패**하는 이 현상이, 며칠 켜둔 기기가 갑자기 죽는 대표적 원인입니다.

고정 배열을 쓰면 **컴파일 시점에 전체 메모리 사용량이 결정**되고, 실행 중에는 할당이 아예 일어나지 않으므로 이 문제가 원천적으로 없습니다.

> **작은 임베디드 기기에서는 "얼마나 유연한가"보다 "얼마나 예측 가능한가"가 더 중요합니다.**

💡 **직접 확인해보기**: `setup()`에 이 줄을 넣고 부팅해보세요.
```cpp
Serial.printf("Player=%u, GameRoom=%u, 전체 rooms_=%u bytes\n",
              sizeof(Player), sizeof(GameRoom), sizeof(GameRoom) * Config::MAX_ROOMS);
Serial.printf("남은 힙: %u bytes\n", ESP.getFreeHeap());
```
**자기가 만든 구조체가 실제로 몇 바이트인지 확인하는 습관**은 임베디드 개발자의 기본기입니다. `MAX_ROOMS`를 4에서 20으로 올렸을 때 RAM이 얼마나 늘어나는지도 바로 알 수 있습니다.

### 결정 ②: `active`와 `connected`가 왜 둘 다 필요한가

`Player`에는 비슷해 보이는 불리언이 둘 있습니다. 이 구분이 이 프로젝트에서 가장 중요한 설계 중 하나입니다.

| 필드 | 의미 | false가 되는 때 |
|---|---|---|
| `active` | **이 슬롯이 사용 중인가** (플레이어가 존재하는가) | 30초 넘게 안 돌아와서 완전히 제거될 때 |
| `connected` | **지금 WebSocket이 살아있는가** | 브라우저를 새로고침하거나 Wi-Fi가 끊긴 즉시 |

즉 **`active == true && connected == false`** 라는 상태가 존재합니다. "잠깐 끊겼지만 아직 자리는 지켜주고 있는 플레이어"입니다.

이 상태 덕분에 **새로고침해도 같은 플레이어로 돌아올 수 있습니다** (14단계). 만약 불리언 하나만 뒀다면 새로고침할 때마다 새 캐릭터가 생겼을 것입니다.

💡 **일반화**: "지워졌다"와 "잠깐 자리 비웠다"를 구분해야 하는 상황은 매우 흔합니다. 상태를 불리언 하나로 표현하려다 막히면, **"내가 두 가지 다른 개념을 하나로 뭉뚱그린 건 아닐까?"**를 의심해보세요.

### 결정 ③: 왜 `ConnectionInfo`를 따로 두나

WebSocket 연결 객체(`AsyncWebSocketClient`)에는 "이건 A채널 3번 플레이어다" 같은 정보를 매달아 둘 자리가 없습니다. 그래서 서버가 직접 배열을 만들어 **"연결 ID → (방, 플레이어)"** 대응표를 관리합니다.

```
clientId 17  →  roomIndex 0, playerIndex 2, isDisplay false
clientId 18  →  roomIndex 0, playerIndex -1, isDisplay true   ← 화면은 playerIndex 없음
```

이 표가 있어야 5단계의 "클라이언트가 자기 ID를 속일 수 없다"는 원칙이 실제로 지켜집니다. `turn` 메시지가 오면 **메시지 내용은 무시하고 이 표만 보고** 누구인지 판단합니다.

⚠️ **`roomIndex`, `playerIndex`가 `int8_t`인 이유**: `-1`을 "없음"으로 쓰기 위해서입니다. `uint8_t`였다면 음수를 못 써서 `0xFF` 같은 값을 "없음"으로 정해야 하는데, 그러면 실수로 배열에 `0xFF`를 넣어 접근하는 사고가 납니다. **`taggerIndex`는 `uint8_t`라서 실제로 `0xFF`를 "술래 없음"으로 쓰고 있습니다** — 코드에서 `if (room.taggerIndex != 0xFF)` 검사를 절대 빼먹으면 안 되는 이유입니다.

### 결정 ④: 방마다 맵 사본을 따로 갖는다

`GameRoom` 안에 `map[12][12]`가 들어 있습니다. 방이 4개면 맵도 4벌입니다(144바이트 × 4 = 576바이트).

🔍 **왜 하나를 공유하지 않나?** 12단계에서 **장애물이 방마다 다르게 변하기 때문**입니다. A채널의 장애물이 사라졌다고 B채널도 같이 바뀌면 안 됩니다. 처음 설계할 때 "맵은 안 변하니까 공유하자"고 했다면, 12단계에서 구조를 갈아엎어야 했을 것입니다.

💡 **교훈**: "지금은 안 변하는 값"도 **"논리적으로 누구의 소유인가"**를 기준으로 배치하세요. 맵은 게임 전체가 아니라 **방의 소유물**입니다.

> **이 프로젝트에서는**: [include/GameTypes.h](include/GameTypes.h) 전체가 이 단계의 결과물이고, [include/Config.h](include/Config.h)의 상수들이 배열 크기를 결정합니다. `Direction`과 `TileType`을 `enum class`로 만든 것도 눈여겨보세요 — 그냥 `int`로 뒀다면 방향에 `7`을 넣는 실수를 컴파일러가 못 잡아줍니다.

---

## 7단계. join 로직 먼저 완성하기 — 다른 기능보다 먼저

메시지 종류 중에 `join`을 제일 먼저 완성해야 합니다. **나머지 모든 메시지가 "이미 참가한 사람"을 전제로 하기 때문**입니다.

### 실제 구현 순서

[GameServer.cpp:211](lib/GameServer/GameServer.cpp:211)의 `handleJoin()`을 열어놓고 따라가세요.

```
1. role이 controller인지 display인지 확인          ← 이상하면 즉시 거절
2. channel 문자열 검증 + 정규화 (대문자로 통일)      ← "a"와 "A"는 같은 방
3. 중복 join 대비: 기존 연결 정보를 먼저 정리
4. 방 찾기 → 없으면 새로 만들기 (최대 4개)
5-A. display면 → 화면 수 검사 → 연결 등록 → 끝
5-B. controller면 → 토큰 확인
     ├ 토큰이 유효 → 기존 플레이어로 복귀 (재접속)
     └ 토큰이 없음 → 새 플레이어 생성 (빈 슬롯 + 시작 위치 + 토큰 발급)
6. joined 응답 + state(맵 포함) 전송
7. 나머지 사람들에게는 state(맵 없이) 전송
```

### 배울 점 ①: 채널 이름 정규화

[GameServer.cpp:936](lib/GameServer/GameServer.cpp:936):

```cpp
bool GameServer::normalizeChannelName(const char *raw, char *dest, size_t destSize) {
    size_t len = strlen(raw);
    if (len == 0 || len > destSize - 1) return false;      // 길이 검사
    for (size_t i = 0; i < len; i++) {
        char c = raw[i];
        if (!isalnum((unsigned char)c)) return false;      // 영문/숫자만 허용
        dest[i] = (char)toupper((unsigned char)c);         // 대문자로 통일
    }
    dest[len] = '\0';
    return true;
}
```

이 짧은 함수가 세 가지 일을 합니다.

1. **검증** — 길이와 문자 종류를 확인해서 이상하면 `false`
2. **정규화** — `a1`과 `A1`을 같은 방으로 취급하도록 대문자로 통일
3. **안전한 복사** — 목적지 크기(`destSize`)를 넘지 않도록 보장

🔍 **왜 영문/숫자만 허용하나?** 한글이나 이모지를 허용하면 UTF-8 멀티바이트 처리, 대소문자 개념 없음, URL 인코딩 등 문제가 줄줄이 딸려옵니다. **"채널 이름은 A, B, TEST1 정도면 충분하다"**는 판단으로 문제를 통째로 없앤 것입니다.

💡 **일반 원칙 — 정규화는 입구에서 딱 한 번.**
채널 이름이 들어오는 순간 대문자로 바꿔 저장하면, 그 이후 코드는 **"채널 이름은 항상 대문자"**라고 가정해도 됩니다. 여기저기서 `strcasecmp`를 쓰는 것보다 훨씬 안전합니다. 이것을 **경계에서 정제한다(sanitize at the boundary)** 고 합니다.

⚠️ 프론트엔드도 같은 검사를 합니다 ([common.js:12](data/js/common.js:12)의 정규식 `/^[A-Za-z0-9]{1,16}$/`). **둘 다 필요합니다.** 프론트엔드 검사는 사용자 편의(즉시 오류 표시), 서버 검사는 보안(브라우저를 우회한 공격 차단)입니다. **프론트엔드 검사는 절대 보안이 아닙니다.**

### 배울 점 ②: 재접속(토큰) 처리

```cpp
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
```

토큰은 32자리 16진수 난수입니다. 서버가 발급해서 `joined` 메시지로 보내주고, 브라우저는 `localStorage`에 저장합니다 ([controller.js:96](data/js/controller.js:96)). 새로고침하면 그 토큰을 다시 보내서 같은 자리로 돌아옵니다.

**토큰이 하는 일은 "이름표"가 아니라 "열쇠"입니다.** 그래서 예측 불가능해야 합니다.

```cpp
void GameServer::generateToken(char *dest, size_t destSize) {
    static const char hexChars[] = "0123456789abcdef";
    for (size_t i = 0; i < destSize - 1; i++) {
        dest[i] = hexChars[esp_random() % 16];   // ★ rand()가 아니라 esp_random()
    }
    dest[destSize - 1] = '\0';
}
```

⚠️ **`rand()`를 쓰면 안 되는 이유**: Arduino의 `rand()`는 시드가 같으면 항상 같은 수열을 냅니다. 보드를 껐다 켤 때마다 **똑같은 토큰이 발급**되어, 남의 자리를 뺏을 수 있게 됩니다. `esp_random()`은 ESP32의 **하드웨어 난수 생성기**를 씁니다 (Wi-Fi/RF가 켜져 있을 때 진짜 무작위성이 보장됩니다 — 이 프로젝트는 AP가 항상 켜져 있으니 조건 충족).

### 배울 점 ③: 중복 접속 처리 (같은 사람이 두 탭을 열었을 때)

[GameServer.cpp:298](lib/GameServer/GameServer.cpp:298):

```cpp
if (p.connected && p.clientId != 0 && p.clientId != client->id()) {
    AsyncWebSocketClient *oldClient = ws_.client(p.clientId);
    freeConnection(p.clientId);
    if (oldClient != nullptr) {
        oldClient->close(1000, "다른 기기에서 재접속함");
    }
}
p.clientId = client->id();
```

같은 토큰으로 두 번째 연결이 오면 **먼저 있던 연결을 끊습니다.** 한 캐릭터를 두 화면에서 조종하는 혼란을 막기 위함입니다.

그런데 여기에 미묘한 함정이 숨어 있습니다. `oldClient->close()`를 하면 잠시 후 **`WS_EVT_DISCONNECT`가 날아와서 `handleDisconnect()`가 호출됩니다.** 그때 "이 플레이어는 연결이 끊겼다"고 처리해버리면, 방금 접속한 새 연결까지 끊긴 것으로 잘못 표시됩니다.

이 사고를 막는 코드가 [GameServer.cpp:190](lib/GameServer/GameServer.cpp:190)입니다.

```cpp
if (p.active && p.clientId == clientId) {   // ★ clientId가 일치할 때만 처리
    p.connected = false;
    ...
}
```

이미 `p.clientId`는 **새 연결의 id로 바뀌어 있으므로**, 뒤늦게 도착한 옛 연결의 종료 이벤트는 이 검사에서 걸러집니다.

💡 **이것이 비동기 프로그래밍의 전형적인 사고 패턴입니다.** "A를 정리하라고 지시했지만, 실제 정리 완료 알림은 나중에 온다. 그 사이에 상태가 이미 바뀌었을 수 있다." → **알림을 받았을 때 "이게 아직 유효한 알림인가"를 반드시 다시 확인**해야 합니다.

### 배울 점 ④: 실패 경로를 먼저 처리하기

`handleJoin()`을 읽어보면 이런 형태가 반복됩니다.

```cpp
if (조건이 안 맞으면) {
    sendError(client, "...");
    return;            // ★ 여기서 빠져나감
}
// 정상 처리는 여기서부터
```

들여쓰기가 깊어지지 않고, **"정상 흐름"이 항상 함수의 가장 바깥 레벨에 있습니다.** 이것을 **조기 반환(early return)** 또는 **가드 절(guard clause)** 이라고 하며, 조건이 5개쯤 되면 `if-else` 중첩보다 압도적으로 읽기 쉽습니다.

💡 **실전 팁**: 오류 메시지는 **사용자가 무엇을 해야 하는지** 알려주는 문장으로 쓰세요.
- ❌ `"invalid channel"`
- ✅ `"채널 이름은 영문/숫자 1~16자로 입력해주세요."`

> **이 프로젝트에서는**: [GameServer.cpp:211](lib/GameServer/GameServer.cpp:211)의 `handleJoin()`이 이 단계 전체입니다. 코드가 "컴퓨터 화면 참가"와 "휴대폰 조작기 참가" 블록으로 나뉜 것을 확인하면 위 순서와 그대로 대응됩니다.
>
> 이 시점에서 프론트엔드도 **최소한으로** 같이 만들어서, 참가 버튼을 눌렀을 때 시리얼에 `[GAME] 새 플레이어 생성: 채널=A, 이름=철수, 위치=(1,1)` 이 찍히는 것을 눈으로 확인하세요. 캔버스도, 회전 버튼 동작도 아직 필요 없습니다.

---

## 8단계. turn 로직 — "서버 권위" 원칙을 실제로 지키는 첫 코드

`join`이 되면, 다음으로 제일 쉬운 `turn`을 구현합니다. 여기서 0단계의 원칙이 코드로 처음 나타납니다.

[GameServer.cpp:388](lib/GameServer/GameServer.cpp:388):

```cpp
void GameServer::handleTurn(ConnectionInfo &conn) {
    // conn = "이 WebSocket이 누구인지" 서버가 join 때 저장해둔 정보.
    // 메시지 안의 값이 아니라 이 값만 사용한다.  ← 서버 권위의 실체
    if (conn.isDisplay || conn.roomIndex < 0 || conn.playerIndex < 0) { ... 거절 ... }

    Player &p = rooms_[conn.roomIndex].players[conn.playerIndex];
    if (!p.active || !p.connected) return;

    unsigned long now = millis();
    if (now - p.lastTurnTime < Config::MIN_TURN_INTERVAL_MS) return;  // 연타 방지

    p.lastTurnTime = now;
    p.lastSeen = now;
    p.direction = rotateClockwise(p.direction);   // 방향만 바꾼다. 위치는 안 바꾼다!
    broadcastState(conn.roomIndex, false);        // 즉시 반영해서 보여준다
}
```

### 결정 ①: 회전과 이동을 분리한다

**회전 버튼 = "다음 이동 방향을 바꾸는 것"이지, "한 칸 이동시키는 것"이 아닙니다.** 실제 이동은 9단계의 게임 틱이 담당합니다.

```
[입력 계층]  회전 버튼 → direction만 변경          (아무 때나, 사용자 속도로)
                            ↓
[시뮬레이션]  0.4초마다 direction 방향으로 한 칸 이동  (일정한 속도로)
```

🔍 **왜 나누나?** 나중에 이런 요구가 들어와도 회전 로직은 **전혀 안 건드려도 됩니다.**
- "이동 속도를 0.4초 → 0.3초로 바꿔주세요" → `TICK_INTERVAL_MS`만 수정
- "술래만 더 빠르게 해주세요" → 틱 로직만 수정
- "아이템 먹으면 2칸씩 이동" → 틱 로직만 수정

**입력을 받는 곳과 상태를 바꾸는 곳을 분리**하면 변경의 영향 범위가 좁아집니다. 게임 엔진에서 흔히 쓰는 구조입니다.

### 결정 ②: 속도 제한을 "지금" 넣는다

```cpp
if (now - p.lastTurnTime < Config::MIN_TURN_INTERVAL_MS) return;
```

버튼을 연타하면 초당 수십 번 메시지가 날아와서 ESP32가 그것만 처리하다 게임 틱이 밀립니다. 이 제한을 **기능을 만드는 바로 그 시점에** 넣는 것이, 나중에 "연타하면 이상해져요"라는 제보를 받고 다시 여기로 돌아오는 것보다 훨씬 쌉니다.

💡 **주목 — 조용히 무시한다.**
제한에 걸렸을 때 `sendError()`를 보내지 **않고** 그냥 `return`합니다. 연타하면 오류 메시지가 초당 수십 개 날아가서 상황이 더 나빠지기 때문입니다. **"거절하되 시끄럽게 굴지 않는다"**는 판단이며, 이런 종류의 결정은 반드시 주석으로 이유를 남겨야 합니다 (안 그러면 다음 사람이 "왜 에러를 안 보내지?" 하고 추가해버립니다).

### 결정 ③: 방어는 두 겹으로

프론트엔드에도 같은 제한이 있습니다 ([controller.js:36](data/js/controller.js:36)).

```js
var MIN_CLIENT_TURN_GAP_MS = 150;   // 서버(100ms)보다 넉넉하게
```

| 계층 | 값 | 목적 |
|---|---|---|
| 브라우저 | 150ms | **불필요한 메시지를 아예 안 보냄** (Wi-Fi 대역폭 절약) |
| 서버 | 100ms | **브라우저를 우회한 공격 차단** (진짜 방어선) |

클라이언트를 더 엄격하게(150 > 100) 잡아둔 것이 포인트입니다. 그러면 정상적인 브라우저는 서버 제한에 절대 걸리지 않습니다.

### 결정 ④: 회전은 틱을 기다리지 않고 즉시 방송한다

```cpp
broadcastState(conn.roomIndex, false);
```

방향이 바뀐 것을 다음 틱(최대 400ms 후)까지 기다리지 않고 바로 보냅니다. **버튼을 눌렀는데 0.4초 동안 아무 반응이 없으면 사용자는 "안 눌렸나?" 하고 또 누릅니다.**

💡 **일반 원칙 — 즉각적인 피드백은 체감 성능의 전부입니다.**
[controller.js:120](data/js/controller.js:120)에도 같은 사상이 보입니다.
```js
rotateBtn.classList.add('pressed');                              // 누르자마자 시각 효과
setTimeout(function () { rotateBtn.classList.remove('pressed'); }, 120);
```
서버 응답을 기다리지 않고 **버튼을 누른 순간 눌린 티를 냅니다.** 실제 결과는 서버에서 오지만, 사용자는 "즉시 반응했다"고 느낍니다.

---

## 9단계. 게임 틱 — 이 프로젝트에서 가장 어려운 부분

`join`, `turn`이 되면 이제 "가만히 둬도 자동으로 움직이는" 부분을 만듭니다. 가장 까다로운 이유는 **여러 플레이어가 동시에 움직이기 때문**입니다.

### 9-1. 왜 `delay()`를 쓰면 안 되는가

```cpp
// ❌ 절대 이렇게 하면 안 된다
void loop() {
    delay(400);
    모든_플레이어_이동();
}
```

`delay(400)` 동안 ESP32는 **다른 어떤 일도 못 합니다.** WebSocket 메시지가 도착해도 0.4초 뒤에나 처리되고, 회전 버튼을 누르면 최악의 경우 400ms 늦게 반응합니다.

```cpp
// ✅ 비차단(non-blocking) 방식
void loop() {
    unsigned long now = millis();
    if (now - room.lastTick >= Config::TICK_INTERVAL_MS) {
        room.lastTick = now;
        updateRoomTick(i, now);     // 0.4초가 지났을 때만 이동 처리
    }
    // 나머지 시간에는 그냥 지나간다 → 다른 일이 즉시 처리됨
}
```

`loop()`는 초당 수만 번 돌면서 **"시간 됐나?"만 확인하고 지나갑니다.** 이 패턴을 익혀두면 임베디드에서 하는 거의 모든 주기 작업에 쓸 수 있습니다.

### ⚠️ 반드시 알아야 할 것: `millis()` 오버플로우

`millis()`는 `unsigned long`(32비트)이라 **약 49.7일이 지나면 0으로 되돌아갑니다.**

그런데 위 코드는 **오버플로우가 나도 정확히 동작합니다.** 이유가 중요합니다.

```
lastTick = 4,294,967,000   (거의 최대값)
now      =           500   (한 바퀴 돌아서 작아짐)

now - lastTick = 500 - 4,294,967,000
```

음수처럼 보이지만, **unsigned 끼리의 뺄셈은 2^32으로 나눈 나머지로 계산**되므로 결과는 `796`이 됩니다. 실제로 흐른 시간과 정확히 같습니다.

```cpp
if (now - room.lastTick >= INTERVAL)     // ✅ 안전 — 항상 이렇게 쓴다
if (now >= room.lastTick + INTERVAL)     // ❌ 위험 — 오른쪽이 넘치면 깨진다
```

💡 **외울 것: 시간 비교는 반드시 "뺄셈"으로 하고, "덧셈"으로 하지 마세요.** 이 한 줄 차이로 "49일마다 한 번 멈추는 기기"가 만들어집니다. 이런 버그는 테스트로 절대 못 잡습니다.

### 9-2. 이동을 "한 명씩 바로 적용"하면 안 되는 이유

가장 순진한 방법은 이렇습니다.

```cpp
// ❌ 계산하면서 동시에 반영
for (플레이어 p : 전원) {
    if (다음_칸이_비어있으면) p.위치 = 다음_칸;   // 바로 적용
}
```

이러면 **플레이어 0을 옮긴 결과를 플레이어 1이 보고 판단**하게 됩니다. 배열 순서가 빠른 사람이 항상 유리해지고, 무엇보다 **결과를 예측할 수 없어서 디버깅이 불가능**해집니다.

그래서 순서를 딱 정해서 지킵니다. [GameServer.cpp:495](lib/GameServer/GameServer.cpp:495)의 `updateRoomTick()`이 그대로 이 순서입니다.

```
1단계  전원의 "가고 싶은 칸"을 계산만 해둔다     (아직 아무도 안 움직임)
2단계  벽/장애물 규칙으로 이동 가능 여부 정리
3단계  도망자끼리 같은 칸을 원하면 우선순위로 정리
4단계  그제서야 전원의 위치를 한꺼번에 반영
5단계  술래-도망자가 겹쳤는지 검사 (잡기 판정)
6단계  결과를 전송
```

**"계산 따로, 반영 따로"**가 핵심 패턴입니다.

### 1~2단계: 목적지 계산

```cpp
int8_t targetX[8], targetY[8];
bool willMove[8];

for (uint8_t i = 0; i < MAX_PLAYERS_PER_ROOM; i++) {
    Player &p = room.players[i];
    targetX[i] = p.x;  targetY[i] = p.y;  willMove[i] = false;  // ★ 기본값 = 제자리
    if (!p.active || !p.connected) continue;

    int8_t dx, dy;  directionDelta(p.direction, dx, dy);
    int nx = p.x + dx,  ny = p.y + dy;

    if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) continue;  // 방어적 검사
    TileType tile = (TileType)room.map[ny][nx];
    if (tile == TileType::Wall)     continue;
    if (tile == TileType::Obstacle) continue;

    targetX[i] = nx;  targetY[i] = ny;  willMove[i] = true;
}
```

💡 **주목 ① — 배열을 "제자리"로 먼저 채운다.**
`targetX[i] = p.x`를 먼저 해두면, 그 뒤 어느 `continue`로 빠져나가도 **목적지는 항상 유효한 값**입니다. "값을 안 채우고 빠져나가는 경로"가 존재하지 않게 만드는 것이 배열 다루기의 기본기입니다.

💡 **주목 ② — 일어나지 않을 일도 검사한다.**
맵 테두리가 전부 벽이므로 좌표가 배열을 벗어날 일은 이론상 없습니다. 그래도 검사합니다.

> **왜?** 나중에 누군가 `kDefaultMap`의 테두리를 `0`으로 고치는 순간, 이 검사가 없으면 **배열 밖 메모리를 건드려서 보드가 재부팅**됩니다. 그것도 원인을 알 수 없는 형태로. **"내 가정이 깨졌을 때 조용히 실패할 것인가, 요란하게 죽을 것인가"**를 선택할 수 있다면, 임베디드에서는 조용히 실패하는 쪽이 훨씬 낫습니다.

### 3단계: 같은 칸 다툼 해결 — 예약 시스템

```cpp
static bool cellReserved[MAP_HEIGHT][MAP_WIDTH];
memset(cellReserved, 0, sizeof(cellReserved));

// (1) 제자리에 머무는 도망자의 칸을 먼저 예약
for (...) {
    if (!p.active || !p.connected || p.isTagger) continue;
    if (!willMove[i]) cellReserved[p.y][p.x] = true;
}

// (2) 움직이려는 도망자들을 순서대로 처리
for (...) {
    if (!p.active || !p.connected || !willMove[i] || p.isTagger) continue;
    if (cellReserved[targetY[i]][targetX[i]]) {     // 이미 찜한 사람이 있으면
        targetX[i] = p.x;  targetY[i] = p.y;        // 제자리에 머문다
        willMove[i] = false;
    }
    cellReserved[targetY[i]][targetX[i]] = true;    // 내 칸을 찜한다
}
```

🔍 **왜 술래는 예약에서 빠지나?** 술래의 칸을 예약해버리면 **도망자가 술래 칸으로 들어갈 수 없어서 "잡기"가 영원히 일어나지 않습니다.** 술래는 자유롭게 겹칠 수 있어야 합니다. 규칙을 코드로 옮길 때 이런 "예외 하나"를 놓치면 게임이 통째로 성립하지 않습니다.

🔍 **왜 `static` 배열인가?** `bool [12][12]` = 144바이트를 매 틱마다 스택에 잡는 대신, 프로그램 전체에서 딱 하나만 두고 재사용합니다. ESP32의 태스크 스택은 작아서(기본 8KB 정도) 큰 지역 배열은 스택 넘침의 원인이 됩니다.

⚠️ **단, `static`은 모든 방이 공유합니다.** A방을 처리하다 말고 B방을 처리하면 값이 섞입니다. 지금은 **뮤텍스(15단계)로 보호되고 한 번의 호출 안에서만 쓰이므로** 안전합니다. 이런 "안전한 이유"가 사라지는 순간(예: 방마다 별도 태스크를 만든다면) 즉시 버그가 되므로, **조건부로 안전한 코드에는 반드시 그 조건을 주석으로 적어야 합니다.**

### 4~5단계: 반영과 잡기 판정

```cpp
// 4단계: 한꺼번에 반영
for (...) { p.x = targetX[i]; p.y = targetY[i]; }

// 5단계: 술래와 겹친 사람 찾기
if (room.taggerIndex != 0xFF) {
    Player &tagger = room.players[room.taggerIndex];
    for (...) {
        if (p.x == tagger.x && p.y == tagger.y) {
            tagger.isTagger = false;
            p.isTagger = true;
            room.taggerIndex = i;
            broadcastPlayerCaught(roomIndex, i, oldTaggerIndex);
            break;                  // ★ 한 틱에 한 번만
        }
    }
}
```

**`break`가 핵심입니다.** 술래가 두 명과 동시에 겹쳤을 때 반복문을 계속 돌면, 첫 번째 사람을 술래로 만들고 → 그 다음 사람도 술래로 만들어 **술래가 2명이 되는** 이상한 상태가 됩니다. "동시에 여러 명이 잡히면 배열 순서상 첫 번째만 처리하고 나머지는 다음 틱으로 미룬다"는 규칙을 정하고 `break`로 강제합니다.

💡 **일반 원칙**: **"동시에 여러 개가 성립할 수 있는 조건"을 만나면, 반드시 "그러면 무엇을 우선하는가"를 명시적으로 정하세요.** 규칙이 없으면 코드의 우연한 순서가 규칙이 되어버립니다.

> **이 프로젝트에서는**: `updateRoomTick()`에 1~6단계 주석이 코드 순서 그대로 달려 있습니다. 처음 이 함수를 짤 때도 **6줄짜리 순서를 먼저 주석으로 써놓고, 그 아래에 한 블록씩 채워나갔습니다.**
>
> 💡 **복잡한 함수를 짜는 방법**: 먼저 "단계 이름만" 주석으로 나열하고, 그다음 한 단계씩 코드를 채우세요. 머릿속에서 전체를 한 번에 풀려고 하면 반드시 어딘가를 빠뜨립니다.

✍️ **연습 (실제로 있는 빈틈)**: 도망자 A가 오른쪽으로, 바로 오른쪽 칸의 도망자 B가 왼쪽으로 이동하면 어떻게 될까요? A의 목적지는 B가 **떠날** 칸이라 예약되어 있지 않고, B의 목적지도 마찬가지입니다. **둘이 서로를 통과해 지나갑니다.** 이걸 막으려면 어떤 검사를 추가해야 할까요? (힌트: "내 목적지에 있던 사람의 목적지가 내 현재 위치인가?")

---

## 10단계. 상태를 클라이언트에 어떻게 보여줄지 정하기

서버 안의 상태가 바뀌어도 브라우저에 전송하지 않으면 아무도 모릅니다. 여기서 정할 질문: **"얼마나 자주, 얼마나 많이 보낼 것인가?"**

이것이 실시간 멀티플레이에서 **가장 중요한 성능 결정**입니다. Wi-Fi 대역폭과 ESP32의 CPU가 여기서 다 소모되기 때문입니다.

### 결정 ①: 맵은 필요할 때만 보낸다

맵은 144개의 숫자입니다. JSON으로 만들면 300바이트가 넘습니다. 이걸 8명에게 0.4초마다 보내면 **초당 6KB**가 낭비됩니다.

그래서 `buildStateJson()`에 `includeMap` 플래그를 두고, **맵을 포함하는 경우를 딱 세 가지로 한정**했습니다.

| 언제 | 맵 포함? | 호출 위치 |
|---|---|---|
| 방에 처음 참가했을 때 | ✅ 포함 | `handleJoin()` → `sendStateTo(client, roomIndex, **true**)` |
| **장애물이 동적으로 변했을 때** | ✅ 포함 | `updateObstacles()` → `broadcastState(roomIndex, **true**)` |
| `requestState` 요청을 받았을 때 | ✅ 포함 | `handleRequestState()` |
| 매 틱(자동 이동) | ❌ 제외 | `updateRoomTick()` → `broadcastState(roomIndex, **false**)` |
| 회전했을 때 | ❌ 제외 | `handleTurn()` |
| 누가 나갔을 때 | ❌ 제외 | `handleDisconnect()`, `cleanupRooms()` |

⚠️ **여기는 문서가 자주 낡는 지점입니다.**
이 프로젝트의 초기 설계는 **"맵은 게임 중에 절대 안 바뀌므로 참가 시 딱 1번만 보낸다"**였습니다. 그런데 12단계에서 **장애물 동적 변경**을 추가하면서 그 전제가 깨졌고, 맵을 다시 보내야 하는 두 번째 경우가 생겼습니다.

> 💡 **교훈**: 코드를 고칠 때는 **그 코드가 근거로 삼던 문서/주석도 같이 고쳐야 합니다.** "맵은 안 바뀐다"는 문장을 그대로 두면, 다음 사람은 존재하지 않는 전제를 믿고 최적화를 시도하다 버그를 만듭니다. **낡은 주석은 없는 주석보다 나쁩니다.**

### 결정 ②: "바뀔 때만"이 아니라 "매 틱마다" 보낸다

더 아끼려면 "위치가 실제로 변한 플레이어만 골라서 보내기"도 가능합니다. 하지만 하지 않았습니다.

| 방식 | 절약량 | 코드 복잡도 |
|---|---|---|
| 매 틱 전체 전송 (선택함) | - | 매우 단순. 상태가 항상 완전함 |
| 변경분만 전송 | 8명이면 거의 매 틱마다 누군가는 움직이므로 **실익이 작음** | 이전 상태 보관 + 비교 + 부분 갱신 로직 필요. **메시지를 놓치면 화면이 영구히 틀어짐** |

> **최적화는 필요한 만큼만.** 측정해보지도 않고 미리 최적화하면, 얻는 것보다 잃는 것(복잡도, 버그)이 큽니다. 이걸 **성급한 최적화(premature optimization)** 라고 합니다.

💡 **언제 최적화해야 하나?** "8명이 들어오니 화면이 끊긴다"는 **증상을 관측한 뒤에** 하세요. 그때는 이런 순서로 접근합니다.
1. 정말 대역폭이 문제인지 측정 (`state` JSON의 실제 바이트 수를 찍어본다)
2. 더 싼 방법부터 시도 (`tickInterval`을 500ms로? 이름을 매번 안 보내고 처음만?)
3. 그래도 부족하면 변경분 전송 구현

### 결정 ③: 전체 상태를 보내면 "자기 치유"가 공짜로 따라온다

`state` 메시지 하나만 받으면 화면을 완전히 그릴 수 있으므로, 브라우저가 메시지를 몇 개 놓쳐도 **다음 메시지에서 저절로 정상으로 돌아옵니다.** 재연결 후에도 `join` → `state` 한 번이면 완전히 복구됩니다.

이 성질이 있기 때문에 [common.js:49](data/js/common.js:49)의 자동 재연결이 이렇게 단순할 수 있습니다. "끊기면 다시 연결하고 다시 join한다" 끝입니다. **놓친 메시지를 되짚어 받는 복잡한 로직이 전혀 필요 없습니다.**

> **이 프로젝트에서는**: [GameServer.cpp:871](lib/GameServer/GameServer.cpp:871)의 `buildStateJson()` 위에 이 결정과 이유가 주석으로 남아 있습니다. **"왜 이렇게 만들었더라?"를 스스로 잊지 않기 위해서라도, 결정한 이유는 코드 옆에 적어두세요.** 코드는 "무엇을 하는가"를 말해주지만 "왜 그렇게 하는가"는 절대 말해주지 않습니다.

---

## 11단계. 잡기 판정 — 규칙을 코드로 옮기기

"술래와 도망자가 같은 칸에 있으면 잡힌다"는 규칙 **자체**는 쉽습니다. 어려운 건 그 주변의 질문들입니다.

| 질문 | 이 프로젝트의 답 | 근거 |
|---|---|---|
| 동시에 두 명이 겹치면? | 배열 순서상 첫 번째만, `break`로 강제 | 술래가 2명이 되는 상태를 원천 차단 |
| 잡힌 사람은 어떻게 되나? | **새 술래가 된다** (탈락 아님) | 인원이 줄지 않아 게임이 계속됨 |
| 잡은 술래는? | 도망자로 바뀐다 | 역할 교대 |
| 연결이 끊긴 사람도 잡히나? | ❌ `!p.connected`면 건너뜀 | 조작 못 하는 사람이 술래가 되면 게임이 멈춤 |
| 잡은 직후 또 잡히면? | 다음 틱에 판정하므로 가능 | 400ms 무적 시간이 사실상 존재 |

### 왜 "탈락"이 아니라 "역할 교대"인가

탈락 방식으로 만들면 이런 문제가 생깁니다.
- 먼저 잡힌 사람은 남은 시간 내내 **할 게 없습니다.** (교실에서 제일 나쁜 상황)
- 인원이 줄어들어 게임이 점점 싱거워집니다.
- "게임 끝" 조건과 재시작 로직을 추가로 만들어야 합니다.

역할 교대 방식은 **누구도 심심하지 않고, 끝나지 않으며, 추가 코드가 거의 없습니다.** 규칙을 정할 때 "구현이 얼마나 단순해지는가"도 정당한 판단 기준입니다.

### 잡기 알림 메시지

[GameServer.cpp:912](lib/GameServer/GameServer.cpp:912):

```cpp
doc["caughtPlayerId"]          = caughtIndex + 1;      // 잡힌 사람
doc["caughtPlayerName"]        = room.players[caughtIndex].name;
doc["newTaggerPlayerId"]       = caughtIndex + 1;      // = 잡힌 사람 (새 술래)
doc["previousTaggerPlayerId"]  = oldTaggerIndex + 1;   // 직전 술래
```

`caughtPlayerId`와 `newTaggerPlayerId`가 **같은 값**인 게 이 게임의 규칙입니다. 굳이 둘 다 보내는 이유는 **클라이언트가 규칙을 몰라도 되게 하기 위함**입니다. 나중에 "잡히면 탈락하고 술래는 그대로" 같은 규칙으로 바꿔도 클라이언트 코드는 그대로 동작합니다.

💡 **일반 원칙 — 메시지는 "사실"을 담고, 클라이언트가 "추론"하게 하지 마세요.** 클라이언트가 `caughtPlayerId`로부터 "그러니까 얘가 새 술래겠지"를 추론하기 시작하면, 서버 규칙이 바뀌는 순간 조용히 틀린 화면이 나옵니다.

⚠️ **현재 프론트엔드의 작은 문제**: [display.js:75](data/js/display.js:75)는 이렇게 표시합니다.
```js
showError(msg.caughtPlayerName + ' 님이 잡혔습니다! 새 술래: ' + msg.caughtPlayerName);
```
같은 이름이 두 번 나와서 문장이 어색합니다. 부록 B의 개선 과제로 정리해두었습니다.

---

## 12단계. 장애물 동적 변경 — 정적인 게임에 변화 주기

여기까지 만들면 게임이 동작하지만 **금방 지루해집니다.** 맵이 고정되어 있으면 몇 판만에 최적 경로가 정해지기 때문입니다. 그래서 5초마다 장애물 일부를 바꿉니다.

🔍 **왜 이 기능을 12단계에서 하나?** 이건 **"핵심 기능이 다 된 뒤에 붙이는 재미 요소"**입니다. 9단계 틱이 안정적으로 돌지 않는 상태에서 맵까지 변하면, 버그가 생겼을 때 이동 로직 탓인지 맵 변경 탓인지 구분할 수 없습니다. **재미 요소는 항상 마지막에.**

### 구현 ([GameServer.cpp:596](lib/GameServer/GameServer.cpp:596))

```
1. 현재 장애물 위치를 전부 수집한다
2. 일부를 제거한다   (최소 개수 OBSTACLE_MIN_COUNT=4 는 유지)
3. 일부를 추가한다   (최대 개수 OBSTACLE_MAX_COUNT=10 을 넘지 않게)
4. 변경이 있었으면 맵을 포함한 state를 방송한다
```

### 배울 점 ①: "시도 횟수 제한"으로 무한 루프 막기

```cpp
for (uint8_t attempt = 0; attempt < 40 && added < Config::OBSTACLE_CHANGE_COUNT; attempt++) {
    uint8_t rx = 1 + (esp_random() % (Config::MAP_WIDTH - 2));
    uint8_t ry = 1 + (esp_random() % (Config::MAP_HEIGHT - 2));
    if (room.map[ry][rx] != (uint8_t)TileType::Empty) continue;   // 이미 뭔가 있음
    if (isPositionOccupied(room, rx, ry)) continue;                // 플레이어가 서 있음
    ...
    added++;
}
```

**"빈 칸을 찾을 때까지 무한 반복"으로 짜면 안 됩니다.** 맵이 꽉 찬 상황에서 영원히 못 빠져나와 보드가 멈춥니다(워치독 리셋). 대신 **"40번 시도해보고 안 되면 포기"**합니다.

이번 5초에 장애물이 안 늘어나도 게임에는 아무 문제가 없습니다.

💡 **일반 원칙 — 무작위로 자리를 찾는 알고리즘에는 반드시 시도 횟수 상한을 두세요.** 임베디드에서 "언젠간 되겠지"는 곧 "언젠간 멈춘다"입니다.

### 배울 점 ②: 랜덤 좌표의 범위

```cpp
uint8_t rx = 1 + (esp_random() % (MAP_WIDTH - 2));
```

`MAP_WIDTH = 12`일 때 `esp_random() % 10` → 0~9, 거기에 +1 → **1~10**. 테두리(0과 11)를 정확히 피합니다. 이 `1 +` 와 `- 2` 조합은 **"테두리를 제외한 내부만"** 을 뜻하는 흔한 관용구입니다. 처음 볼 때 헷갈리니 눈에 익혀두세요.

### 배울 점 ③: 배열에서 원소를 O(1)로 제거하기

```cpp
obstacles[pick] = obstacles[obstacleCount - 1];   // 마지막 원소를 그 자리에 덮어씀
obstacleCount--;                                   // 개수만 줄임
```

중간 원소를 지울 때 뒤의 것들을 앞으로 당기면 O(n)이지만, **순서가 상관없는 목록**이라면 마지막 원소를 끌어와 덮으면 O(1)입니다. 게임 개발에서 자주 쓰는 기법입니다 (**swap-and-pop**).

### ⚠️ 이 코드에 실제로 있는 문제 세 가지

정직하게 짚습니다. 이런 걸 스스로 발견하는 것이 실력입니다.

**① 주석과 코드가 다르다.**
```cpp
// 플레이어가 인접해있으면 제거하지 않는다 (갑자기 길이 뚫리면 안 되므로)
if (!isPositionOccupied(room, ox, oy)) { ...제거... }
```
`isPositionOccupied()`는 **인접**이 아니라 **정확히 그 칸**을 검사합니다. 게다가 플레이어는 장애물 칸에 있을 수 없으므로 **이 검사는 항상 참이라 아무 일도 하지 않습니다.** (제거 경로에서는 사실상 죽은 코드)

**② 연결이 끊긴 플레이어 위에 장애물이 생길 수 있다.**
`isPositionOccupied()`는 `p.connected`인 플레이어만 셉니다. 잠깐 끊긴 플레이어(6단계의 `active && !connected` 상태)의 칸은 비어 있다고 판단되어, 그 위에 장애물이 놓입니다. 재접속하면 **장애물 안에 갇힌 채로 시작**합니다.

**③ 플레이어가 완전히 갇힐 수 있다.**
플레이어는 앞으로만 이동하고 후진이 없으므로, 사방이 막히면 회전만 하다 영영 못 움직입니다.

✍️ **연습**: ②는 `isPositionOccupied()`에서 `p.connected` 조건을 빼면 바로 고쳐집니다. 다만 그러면 다른 곳(시작 위치 찾기)의 동작도 바뀝니다. **한 함수를 여러 목적으로 쓰다가 요구가 갈라지는 전형적인 상황**이니, 함수를 둘로 나누는 게 옳은지 생각해보세요.

---

## 13단계. 프론트엔드 3종 — 브라우저 쪽 구조

서버가 완성되면 이제 브라우저 쪽을 제대로 만듭니다. 파일이 셋으로 나뉜 이유부터 봅니다.

```
data/
├── index.html      채널/이름 입력 → 역할 선택 → 페이지 이동
├── controller.html 휴대폰: 회전 버튼 하나
├── display.html    컴퓨터: 캔버스 + 사이드바
├── css/style.css   세 페이지 공용 스타일
└── js/
    ├── common.js     ★ 셋이 공유 (WebSocket 래퍼, localStorage, 검증)
    ├── controller.js 휴대폰 전용
    └── display.js    화면 전용
```

🔍 **왜 SPA(한 페이지에서 화면만 전환)로 안 만들었나?**
[index.html:47](data/index.html:47)에 이유가 적혀 있습니다.

> 역할별로 파일을 분리해 **각 화면의 JS 상태가 완전히 초기화**되게 하는 쪽이 초보자가 이해하고 디버깅하기 훨씬 쉬워서 이 방식을 선택했다.

SPA로 만들면 "조작기 화면에서 쓰던 변수가 게임 화면으로 넘어가서 이상하게 동작"하는 종류의 버그가 생깁니다. **페이지를 이동하면 브라우저가 모든 것을 리셋해주므로, 이 종류의 버그가 존재할 수 없습니다.** 프로토타입에서는 이런 "무식하지만 확실한" 선택이 옳을 때가 많습니다.

### common.js: 자동 재연결 WebSocket 래퍼

[common.js:49](data/js/common.js:49)의 `createGameSocket()`이 이 프로젝트 프론트엔드의 핵심입니다.

```js
socket.onclose = function () {
  if (handlers.onClose) handlers.onClose();
  if (!manuallyClosed) {
    if (handlers.onReconnecting) handlers.onReconnecting();
    reconnectTimer = setTimeout(connect, reconnectDelay);
    reconnectDelay = Math.min(reconnectDelay * 1.5, 8000);   // ★ 지수 백오프
  }
};
```

**지수 백오프(exponential backoff)**: 재시도 간격을 1초 → 1.5초 → 2.25초 → ... → 최대 8초로 점점 늘립니다.

🔍 **왜 그냥 1초마다 재시도하지 않나?** 보드가 재부팅 중일 때 8대의 폰이 각각 1초마다 접속을 시도하면 **초당 8번의 요청**이 부팅 중인 보드에 쏟아집니다. 이걸 **재접속 폭풍(thundering herd)** 이라고 하며, 서버가 영영 못 일어나는 원인이 됩니다. 간격을 늘리면 자연스럽게 부하가 분산됩니다.

`Math.min(..., 8000)`으로 상한을 두는 것도 중요합니다. 없으면 몇 분 뒤엔 간격이 몇 시간이 되어 **보드가 살아나도 아무도 안 돌아옵니다.**

💡 **`manuallyClosed` 플래그**: 사용자가 의도적으로 닫은 경우(페이지 이동)에는 재연결하지 않아야 합니다. **"의도한 종료"와 "사고로 인한 종료"를 구분**하는 이 플래그가 없으면, 페이지를 떠난 뒤에도 좀비 재연결이 계속 돕니다.

### common.js: 프로토콜 자동 감지

```js
function buildWsUrl() {
  var protocol = (window.location.protocol === 'https:') ? 'wss:' : 'ws:';
  return protocol + '//' + window.location.host + '/ws';
}
```

주소를 `ws://192.168.4.1/ws`로 **하드코딩하지 않았습니다.** 현재 페이지의 호스트를 그대로 씁니다.

🔍 **왜?** IP가 바뀌거나(다른 Wi-Fi 설정), 나중에 STA 모드로 공유기에 붙거나, PC에서 로컬 서버로 테스트할 때 **코드를 한 글자도 안 고쳐도 동작**합니다. **"내가 어디서 로딩됐는지"를 기준으로 삼는 것**이 웹 프론트엔드의 기본 습관입니다.

### controller.js: 모바일 터치의 함정

[controller.js:129](data/js/controller.js:129)가 이 프로젝트에서 가장 "모바일스러운" 코드입니다.

```js
var lastTouchTime = 0;
rotateBtn.addEventListener('touchend', function (e) {
  e.preventDefault();          // 기본 동작(확대, 유령 클릭) 차단
  lastTouchTime = Date.now();
  sendTurn();
}, { passive: false });

rotateBtn.addEventListener('click', function () {
  if (Date.now() - lastTouchTime < 500) return;   // 방금 터치가 만든 click이면 무시
  sendTurn();
});
```

⚠️ **모바일 브라우저는 터치 한 번에 이벤트를 두 번 줍니다.**
```
손가락 터치 → touchstart → touchend → (약 300ms 후) → click   ← 유령 클릭(ghost click)
```
`touchend`와 `click` 양쪽에 핸들러를 달면 **한 번 눌렀는데 회전이 두 번** 됩니다.

그렇다고 `click`만 쓰면 300ms 지연이 생겨 반응이 굼뜨고, `touchend`만 쓰면 **PC 브라우저(마우스)에서 동작하지 않습니다.**

그래서 위 코드는 **둘 다 등록하되, 터치 직후 500ms 안에 온 `click`은 무시**합니다. 마우스로 누르면 `lastTouchTime`이 갱신되지 않았으므로 그냥 동작합니다. **두 입력 방식을 모두 지원하면서 중복은 막는** 표준적인 해법입니다.

```js
document.addEventListener('gesturestart', function (e) { e.preventDefault(); });
```
두 손가락 핀치 줌도 막습니다. 게임 중에 화면이 확대되면 버튼을 못 누릅니다. `controller.html`의 `<meta name="viewport" ... user-scalable=no>` 와 한 쌍입니다.

### display.js: 캔버스 렌더링

**① devicePixelRatio 처리** ([display.js:133](data/js/display.js:133))

```js
var dpr = window.devicePixelRatio || 1;
canvas.style.width  = pixelSize + 'px';        // CSS상의 크기 (레이아웃)
canvas.style.height = pixelSize + 'px';
canvas.width  = Math.floor(pixelSize * dpr);   // 실제 픽셀 수 (해상도)
canvas.height = Math.floor(pixelSize * dpr);
ctx.setTransform(dpr, 0, 0, dpr, 0, 0);        // 좌표계는 CSS 기준으로 유지
```

⚠️ 캔버스에는 크기가 **두 개**입니다. `style.width`(화면에서 차지하는 크기)와 `width`(내부 픽셀 수)입니다. 레티나 화면에서는 dpr이 2~3이라, 이 처리를 안 하면 **그림이 흐릿하게** 보입니다. `setTransform`으로 좌표계를 되돌려주면, 그리는 코드는 dpr을 전혀 신경 쓰지 않아도 됩니다.

**② 렌더링과 데이터 갱신의 분리**

```js
var animInterval = setInterval(function () { animFrame++; draw(); }, 80);
```

서버 상태는 400ms마다 오지만, 화면은 80ms마다 다시 그립니다. 술래의 붉은 맥동, 장애물의 반짝임 같은 **서버와 무관한 연출**을 위해서입니다.

```js
var pulse = Math.sin(animFrame * 0.25) * 0.15 + 0.35;   // 술래 글로우 맥동
var sparkle = Math.sin(animFrame * 0.3 + px * 0.1) * 0.3 + 0.5;  // 장애물 반짝임
```

`+ px * 0.1`로 **x좌표를 위상에 섞은 것**이 요령입니다. 이게 없으면 모든 장애물이 똑같은 박자로 깜빡여서 기계적으로 보입니다.

💡 **구조상 배울 점**: `draw()`는 `players`와 `mapData` **전역 상태만 보고 그립니다.** 데이터를 받는 곳(`applyState`)과 그리는 곳(`draw`)이 완전히 분리되어 있어서, 어느 쪽을 고쳐도 다른 쪽이 안 깨집니다. 게임 개발에서 **업데이트와 렌더의 분리**라고 부르는 기본 구조입니다.

⚠️ **개선 여지**: 아무도 안 움직여도 초당 12.5번 계속 그립니다. 노트북 배터리에는 좋지 않습니다. `requestAnimationFrame`을 쓰면 브라우저가 탭이 숨겨졌을 때 자동으로 멈춰줍니다.

**③ 방향을 눈으로 표현하기**

```js
if (direction === 0) dy = -lookDist;       // 위
else if (direction === 1) dx = lookDist;   // 오른쪽
...
ctx.arc(cx - eyeSpacing + dx * 2, cy - radius*0.1 + dy * 2, pupilR, ...);  // 동공은 2배
```

화살표를 그리는 대신 **눈동자가 진행 방향을 보게** 했습니다. 흰자는 `dx`만큼, 동공은 `dx * 2`만큼 움직여서 시선이 더 강조됩니다. 서버가 보내는 것은 그냥 숫자 `0~3`뿐이고, **그것을 어떻게 표현할지는 온전히 클라이언트의 자유**입니다 — 0단계의 역할 분리가 여기서 결실을 맺습니다.

---

## 14단계. 연결이 끊기는 상황들 대비하기

여기까지 하면 "정상적으로 잘 되는 경우"는 다 됩니다. 이제 **비정상적인 상황**을 하나씩 나열하고 대비합니다.

💡 **먼저 목록부터 종이에 적으세요.** 코드를 보면서 생각하면 반드시 빠뜨립니다. "이 시스템에서 사라질 수 있는 것"을 전부 적는 것이 시작입니다.

| 상황 | 그냥 두면 | 대책 | 구현 위치 |
|---|---|---|---|
| Wi-Fi가 잠깐 끊김 | 브라우저가 영원히 멈춤 | 자동 재연결 (지수 백오프) | [common.js:73](data/js/common.js:73) |
| 브라우저 새로고침 | 새 플레이어가 계속 생겨 정원이 참 | **토큰**을 `localStorage`에 저장 후 재사용 | [controller.js:81](data/js/controller.js:81) |
| 술래가 나가서 안 돌아옴 | 게임이 영원히 멈춤 | 30초 후 제거 + **새 술래 재선정** | [GameServer.cpp:674](lib/GameServer/GameServer.cpp:674) |
| 아무도 없는 채널이 남음 | 채널 4개가 유령으로 가득 참 | 5분 후 채널 정리 | [GameServer.cpp:714](lib/GameServer/GameServer.cpp:714) |
| 같은 사람이 두 탭을 염 | 한 캐릭터를 두 곳에서 조종 | 옛 연결을 끊음 | [GameServer.cpp:298](lib/GameServer/GameServer.cpp:298) |

### 두 단계로 나뉜 "사라짐"

6단계에서 본 `active`/`connected` 구분이 여기서 진가를 발휘합니다.

```
연결 끊김 즉시 (handleDisconnect)          30초 후 (cleanupRooms)
────────────────────────────────────      ─────────────────────────
connected = false                          active = false
clientId  = 0                              playerCount--
lastSeen  = 현재 시각                       술래였다면 → 새 술래 뽑기
화면에 "회색 반투명"으로 표시                 목록에서 완전히 사라짐
자리는 그대로 보존 ← 30초 안에 오면 복귀 가능
```

🔍 **왜 즉시 지우지 않나?** 지하철에서 잠깐 신호가 끊기거나, 실수로 새로고침을 눌렀을 때 **캐릭터가 사라지면 게임이 망가집니다.** 30초의 유예를 두면 대부분의 사고는 사용자가 눈치채지 못하고 넘어갑니다.

🔍 **왜 영원히 기다리지 않나?** 슬롯이 8개뿐이라, 나간 사람이 자리를 계속 차지하면 새 사람이 못 들어옵니다. `PLAYER_TIMEOUT_MS = 30000`은 **"복구 가능성"과 "자원 회수" 사이의 타협점**입니다. 이런 값에 정답은 없고, 어떤 트레이드오프인지 아는 것이 중요합니다.

### 술래가 나갔을 때 — 게임이 멈추지 않게

[GameServer.cpp:674](lib/GameServer/GameServer.cpp:674):

```cpp
if (wasTagger) {
    room.taggerIndex = 0xFF;
    if (room.gameStarted) {
        // 남은 접속자 중 무작위로 새 술래를 뽑는다
        uint8_t candidates[MAX_PLAYERS_PER_ROOM];
        uint8_t count = 0;
        for (...) if (players[j].active && players[j].connected) candidates[count++] = j;

        if (count > 0) {
            uint8_t pick = candidates[esp_random() % count];
            room.players[pick].isTagger = true;
            room.taggerIndex = pick;
        } else {
            room.gameStarted = false;    // ★ 아무도 없으면 게임 자체를 중지
        }
    }
}
```

**`else` 절이 중요합니다.** 마지막 한 명이 나갔을 때 `gameStarted = true`인 채로 두면, 아무도 없는 방에서 틱이 영원히 돕니다. **"모든 반복문에는 '후보가 0개일 때'라는 경우가 있다"**는 것을 항상 기억하세요.

💡 **"후보 목록을 먼저 모으고 그중에서 뽑기" 패턴**은 이 프로젝트에 세 번 나옵니다 (`handleStartGame`, `cleanupRooms`, `updateObstacles`). 조건에 맞는 것을 세면서 동시에 뽑으려 하면 코드가 꼬입니다. **모으기 → 세기 → 뽑기**로 나누세요.

### 언제 정리 작업을 하나

```cpp
if (now - lastCleanupTime_ >= Config::CLEANUP_INTERVAL_MS) {   // 5초마다
    lastCleanupTime_ = now;
    cleanupRooms(now);
}
```

매 `loop()`마다 4개 방 × 8명을 전부 검사하는 것은 낭비이므로 **5초에 한 번만** 확인합니다. 그래서 실제 제거 시각은 정확히 30초가 아니라 30~35초 사이입니다. 이 정도 오차는 문제가 되지 않습니다.

💡 **일반 원칙 — "정확히 언제"가 중요하지 않은 작업은 주기적으로 몰아서 하세요.** 이런 것을 **일괄 정리(sweep)** 라고 하며, 이벤트마다 즉시 처리하는 것보다 훨씬 단순하고 예측 가능합니다.

---

## 15단계. 마지막 퍼즐 조각: 동시성(race condition)

9단계까지는 "`loop()` 안에서 순서대로 실행된다"고 가정하고 만들었습니다. 그런데 실제로는 **두 개의 태스크가 동시에 돌고 있습니다.**

```
┌─────────────────────────┐        ┌──────────────────────────────┐
│  메인 태스크 (loop)       │        │  AsyncTCP 태스크              │
│                         │        │                              │
│  GameServer::loop()     │        │  onEvent()                   │
│   ├ updateRoomTick()    │        │   ├ handleMessage()          │
│   ├ updateObstacles()   │        │   │   ├ handleJoin()         │
│   └ cleanupRooms()      │        │   │   ├ handleTurn()         │
│                         │        │   │   └ handleStartGame()    │
│                         │        │   └ handleDisconnect()       │
└───────────┬─────────────┘        └──────────────┬───────────────┘
            │                                     │
            └──────────► rooms_ , connections_ ◄──┘
                         (두 태스크가 동시에 건드림!)
```

ESP32는 듀얼코어이므로 **진짜로 물리적으로 동시에** 실행될 수 있습니다.

### 무슨 일이 일어날 수 있나

게임 틱이 4단계(위치 반영)를 실행하는 도중에, 하필 그 순간 `turn` 메시지가 도착해서 같은 플레이어의 `direction`을 바꾼다고 해봅시다.

```
[메인 태스크]                        [AsyncTCP 태스크]
p.x = targetX[3];                  
                                    p.direction = rotateClockwise(...);
p.y = targetY[3];                  
```

이 정도면 큰 문제가 아닐 수도 있습니다. 하지만 이런 것은 심각합니다.

```
[메인 태스크] cleanupRooms          [AsyncTCP 태스크] handleTurn
p.active = false;    ← 제거 중
                                    Player &p = room.players[idx];  ← 이미 지워진 자리
                                    p.direction = ...;              ← 유령 플레이어 조작
```

또는 `playerCount--`가 두 태스크에서 동시에 일어나면 **개수가 어긋나** 정원 계산이 영원히 틀어집니다. (`count--`는 "읽기 → 빼기 → 쓰기" 3단계라 중간에 끼어들 수 있습니다.)

⚠️ **이런 버그의 무서운 점**: 100번 중 99번은 정상 동작합니다. 재현이 안 되고, 로그를 찍으면 타이밍이 바뀌어서 사라지기도 합니다. **"가끔 이상해요"라는 제보의 상당수가 이것**입니다.

### 해결: 뮤텍스로 문 잠그기

```cpp
xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);   // 문 잠그기 (열릴 때까지 기다림)
    // ... rooms_, connections_ 를 읽거나 쓰는 코드 ...
xSemaphoreGiveRecursive(mutex_);                  // 문 열기
```

**뮤텍스(mutex, mutual exclusion)** = 한 번에 한 태스크만 통과할 수 있는 문입니다. 한쪽이 들어가 있으면 다른 쪽은 문 앞에서 기다립니다.

잠그는 곳은 딱 세 군데입니다.

| 함수 | 실행 태스크 |
|---|---|
| [`GameServer::loop()`](lib/GameServer/GameServer.cpp:65) | 메인 |
| [`handleMessage()`](lib/GameServer/GameServer.cpp:133) | AsyncTCP |
| [`handleDisconnect()`](lib/GameServer/GameServer.cpp:181) | AsyncTCP |

💡 **왜 이 세 곳인가?** **"바깥에서 안으로 들어오는 입구"**이기 때문입니다. 안쪽 함수(`handleTurn`, `updateRoomTick` 등)에 일일이 락을 걸면 실수로 빠뜨리기 쉽고, 성능도 나빠집니다. **입구에서 한 번만 잠그는 것**이 정석입니다.

### 왜 "재귀(Recursive)" 뮤텍스인가

일반 뮤텍스는 **같은 태스크가 두 번 잠그면 자기 자신을 기다리다 영원히 멈춥니다(데드락)**.

```
handleMessage()      → 잠금 획득 ✅
  handleJoin()
    broadcastState() → 또 잠그려 함 → 이미 잠겨있음 → 자기가 열어줄 때까지 대기 → 💀
```

재귀 뮤텍스는 **"같은 태스크가 다시 요청하면 그냥 통과시키고, 잠금 횟수만 센다"**로 동작합니다. 내부 함수들이 서로를 자유롭게 호출할 수 있게 되어 훨씬 안전합니다.

⚠️ 대신 `Take`와 `Give`의 **개수가 정확히 맞아야** 합니다. 중간에 `return`으로 빠져나가면서 `Give`를 빠뜨리면 문이 영원히 잠긴 채로 남습니다.

✍️ **연습 (C++다운 해법)**: 생성자에서 `Take`, 소멸자에서 `Give`를 하는 작은 클래스를 만들어보세요.
```cpp
struct LockGuard {
    SemaphoreHandle_t m;
    LockGuard(SemaphoreHandle_t mu) : m(mu) { xSemaphoreTakeRecursive(m, portMAX_DELAY); }
    ~LockGuard() { xSemaphoreGiveRecursive(m); }
};
// 사용: 함수 맨 위에 LockGuard lock(mutex_); 한 줄이면 끝. return을 아무 데서나 해도 안전.
```
이 기법을 **RAII**(자원 획득이 곧 초기화)라고 하며, C++에서 자원 누수를 막는 가장 중요한 관용구입니다.

### 락 안에서 하면 안 되는 일

[GameServer.cpp:93](lib/GameServer/GameServer.cpp:93)를 보세요.

```cpp
xSemaphoreGiveRecursive(mutex_);   // 먼저 문을 연 다음
ws_.cleanupClients();               // ★ 락 밖에서 호출
```

**락을 잡은 채로 오래 걸리는 일을 하면 안 됩니다.** 그동안 다른 태스크가 전부 멈추기 때문입니다. 특히 이런 것들은 피해야 합니다.

- 오래 걸리는 라이브러리 호출 (`cleanupClients`)
- `delay()`
- 다른 락을 또 잡기 (데드락의 주원인)

⚠️ **이 코드에 남아 있는 고민거리**: `broadcastState()`는 락 안에서 `client->text()`를 호출합니다. 전송이 느리면 그동안 메시지 처리가 밀립니다. 지금 규모(8명, 짧은 JSON)에서는 문제가 되지 않지만, **"JSON을 락 안에서 만들고, 실제 전송은 락 밖에서"**로 나누는 것이 더 견고한 구조입니다.

### 🔍 왜 이 단계를 맨 마지막에 하나

동시성 문제는 **눈에 잘 안 보이고, 어쩌다 한 번 이상하게 동작**하는 식으로만 드러납니다. 처음부터 신경 쓰면서 코드를 짜면 로직 자체가 헷갈립니다.

> 먼저 **"혼자 실행된다"고 가정하고 로직을 다 맞춘 다음**, "이 코드가 동시에 두 곳에서 실행될 수 있는 지점이 어디인가"를 **나중에 찾아서 잠그는 것**이 실수를 줄이는 순서입니다.

단, **"나중에 할 일"이라는 걸 처음부터 알고 있어야** 합니다. 모르고 있다가 나중에 발견하면, 이미 코드가 얽혀서 어디를 잠가야 할지 알 수 없게 됩니다.

---

## 16단계. 입력 검증 — "클라이언트를 믿지 않는다"를 끝까지 적용

0단계의 원칙을 통신 입력값에도 끝까지 적용합니다. 클라이언트가 보내는 모든 값을 다음 순서로 의심합니다.

### 검증 5계층

**① JSON 자체가 깨져 있을 수 있다**
```cpp
DeserializationError err = deserializeJson(doc, data, len);
if (err) { sendError(client, "메시지 형식(JSON)이 올바르지 않습니다."); return; }
```
⚠️ **반환값을 무시하는 것이 가장 흔한 실수입니다.** 파싱이 실패했는데 계속 진행하면 쓰레기 값으로 동작합니다.

**② 길이가 비정상일 수 있다**
```cpp
if (len == 0 || len > Config::WS_MAX_MESSAGE_LEN) { ... }   // 512바이트 상한
```
악의적인 클라이언트가 10MB짜리 메시지를 보내면 ESP32의 메모리가 바닥납니다. **"우리 메시지는 아무리 길어야 200바이트"**라는 사실을 알고 있으니 상한을 두는 것이 맞습니다.

**③ 있어야 할 필드가 없을 수 있다**
```cpp
const char *role       = obj["role"]    | "";     // ArduinoJson의 기본값 문법
const char *channelRaw = obj["channel"] | "";
```
`|` 연산자는 "그 필드가 없거나 타입이 다르면 기본값을 쓴다"는 뜻입니다. **`nullptr`이 절대 나오지 않으므로** 그 뒤의 `strlen()`, `strcmp()`가 안전합니다.

**④ 값이 허용 범위를 벗어날 수 있다**
```cpp
if (strcmp(role, "controller") != 0 && strcmp(role, "display") != 0) { 거절 }
if (!normalizeChannelName(channelRaw, channel, sizeof(channel)))     { 거절 }
```
**허용 목록(allowlist) 방식**입니다. "이상한 것을 걸러내기"가 아니라 **"허용된 것만 통과시키기"** — 전자는 빠뜨린 것이 통과하지만, 후자는 빠뜨려도 안전한 쪽으로 실패합니다.

**⑤ 순서가 이상할 수 있다**
```cpp
ConnectionInfo *conn = findConnection(client->id());
if (conn == nullptr) { sendError(client, "먼저 참가(join) 메시지를 보내야 합니다."); }
else if (strcmp(type, WsMsgType::TURN) == 0) { handleTurn(*conn); }
```
`join` 없이 `turn`을 보내면 등록된 연결 정보가 없으므로 거부됩니다. **`join`만 예외로 두고 나머지는 전부 이 관문을 통과해야 하는 구조**라, 새 메시지 종류를 추가해도 자동으로 보호됩니다.

### 문자열 복사의 함정: UTF-8 절단

[GameServer.cpp:950](lib/GameServer/GameServer.cpp:950)의 `sanitizeUtf8Copy()`가 이 프로젝트에서 가장 섬세한 함수입니다.

한글 "가"는 UTF-8로 **3바이트**(`EA B0 80`)입니다. `name[17]` 버퍼에 `strncpy`로 자르면 이런 일이 생깁니다.

```
"안녕하세요반갑습니다" → 앞 16바이트만 복사
결과: EA B0 80 ... EC 95   ← 마지막 글자가 2바이트만 남음
브라우저 표시: "안녕하세요반�"   ← 깨진 문자
```

더 나쁜 경우, 잘린 바이트가 **JSON을 통째로 무효화**해서 브라우저가 메시지 전체를 파싱하지 못합니다.

```cpp
while (si < srcLen && di < destSize - 1) {
    unsigned char c = src[si];
    uint8_t charLen;

    if (c < 0x20) { si++; continue; }            // 제어 문자 제거

    if      ((c & 0x80) == 0x00) charLen = 1;    // 0xxxxxxx  → ASCII
    else if ((c & 0xE0) == 0xC0) charLen = 2;    // 110xxxxx  → 2바이트
    else if ((c & 0xF0) == 0xE0) charLen = 3;    // 1110xxxx  → 3바이트 (한글!)
    else if ((c & 0xF8) == 0xF0) charLen = 4;    // 11110xxx  → 4바이트 (이모지)
    else { si++; continue; }                      // 잘못된 시작 바이트

    if (si + charLen > srcLen)      break;        // 원본이 이미 잘려있음
    if (di + charLen > destSize - 1) break;       // ★ 글자 중간에서 자르지 않고 여기서 멈춤

    memcpy(dest + di, src + si, charLen);
    di += charLen;  si += charLen;
}
dest[di] = '\0';
```

**핵심은 `if (di + charLen > destSize - 1) break;` 한 줄입니다.** "이 글자를 넣으면 넘친다 → 넣지 말고 여기서 끝낸다". 그 결과 이름은 짧아질지언정 **절대 깨지지 않습니다.**

💡 **배울 점**: UTF-8은 **첫 바이트의 상위 비트만 보면 그 글자가 몇 바이트인지 알 수 있게** 설계되어 있습니다. 이 성질 덕분에 위 코드처럼 앞에서부터 한 글자씩 안전하게 셀 수 있습니다. 다국어를 다루는 프로그램에서 반복해서 쓰게 되는 지식입니다.

⚠️ **그래서 이름은 "16자"가 아니라 "16바이트"입니다.** `Config.h`의 주석도 이 점을 정확히 구분해서 적어두고 있습니다 — 한글은 5글자밖에 못 들어갑니다.

### 🔍 왜 검증을 마지막 단계로 정리하나

실제로는 이 프로젝트도 **각 기능을 만들면서 검증을 같이 넣었습니다.** 여기서 마지막에 모아 설명하는 이유는, **"검증이라는 사고방식"을 하나의 체계로 보여주기 위해서**입니다.

기능을 만들 때마다 스스로에게 이 질문 5개를 던지세요.
1. 이 값이 아예 안 오면?
2. 이 값이 이상한 타입이면?
3. 이 값이 너무 길면?
4. 이 값이 허용 범위를 벗어나면?
5. 이 요청이 잘못된 순서로 오면?

---

## 17단계. 통합 테스트 — 실제 시나리오를 순서대로

기능이 다 만들어지면, 마지막으로 실제 시나리오를 밟아봅니다. **순서가 중요합니다** — 앞이 안 되면 뒤는 볼 필요도 없습니다.

| # | 시나리오 | 확인할 것 | 실패하면 의심할 곳 |
|---|---|---|---|
| 1 | 혼자 컨트롤러로 참가 | 시리얼에 `새 플레이어 생성`, 화면에 캐릭터 | 7단계 join |
| 2 | 컴퓨터로 같은 채널 참가 | 캔버스에 맵과 캐릭터가 그려짐 | 3·10단계 맵 전송 |
| 3 | 두 번째 폰으로 참가 | 둘 다 보임, 위치가 겹치지 않음 | `findFreeStartPosition` |
| 4 | 게임 시작 | 술래가 랜덤 지정, 자동으로 움직임 | 9단계 틱 |
| 5 | 회전 버튼 | 즉시 방향 표시가 바뀌고, 다음 이동부터 방향이 바뀜 | 8단계 turn |
| 6 | 벽으로 돌진 | 벽 앞에서 멈춤 (뚫고 나가지 않음) | 9단계 1~2단계 |
| 7 | 술래가 도망자를 잡음 | 역할이 즉시 바뀌고 알림이 뜸 | 11단계 |
| 8 | 5초 기다리기 | 장애물 모양이 변함 | 12단계 |
| 9 | 다른 채널 만들기 | 두 채널이 서로 안 섞임 | 6단계 방 분리 |
| 10 | 컨트롤러 새로고침 | **같은 번호/이름으로 복귀** | 14단계 토큰 |
| 11 | 폰 Wi-Fi 껐다 켜기 | 자동 재연결, 30초 안이면 복귀 | 13단계 백오프 |
| 12 | 술래가 나가고 30초 대기 | 새 술래가 자동 지정 | 14단계 |

### 시리얼 로그로 흐름 읽기

정상적인 한 판의 로그는 이렇게 보입니다. **이 순서가 익숙해지면, 어디서 끊겼는지 한눈에 보입니다.**

```
[INFO] ===== ESP32-S3 격자 술래잡기 서버 부팅 시작 =====
[INFO] LittleFS 마운트 성공. 저장된 파일 목록:
  - index.html (2314 bytes)
  ...
[INFO] Wi-Fi AP 생성 성공
[INFO] AP IP 주소: 192.168.4.1
[INFO] WebSocket 핸들러 등록 완료 (경로: /ws)
[INFO] ===== 부팅 완료, 접속을 기다립니다 =====
[WS]   클라이언트 연결됨 (id=1, ip=192.168.4.2)
[GAME] 새 채널 생성: A
[GAME] 새 플레이어 생성: 채널=A, 이름=철수, 위치=(1,1)
[WS]   클라이언트 연결됨 (id=2, ip=192.168.4.3)
[GAME] 컴퓨터 화면 참가: 채널=A
[GAME] 게임 시작: 채널=A, 술래=영희
[GAME] 회전 명령: 채널=A, 이름=철수, 새 방향=1
[GAME] 잡기 발생: 채널=A, 영희 님이 철수 님을 잡았습니다 -> 새 술래: 철수
[GAME] 장애물 변경: 채널=A, 제거=2, 추가=2, 현재=10
```

💡 **디버깅 요령 — 로그가 "어디까지" 나왔는지를 보세요.**
- `[WS] 클라이언트 연결됨`이 안 나옴 → Wi-Fi/웹서버 문제 (2·3단계)
- 연결은 됐는데 `[GAME] 새 플레이어 생성`이 없음 → join 메시지 문제 (5·7단계)
- 참가는 됐는데 `[GAME] 회전 명령`이 없음 → 버튼/터치 이벤트 문제 (13단계)

**"마지막으로 성공한 지점"과 "처음 실패한 지점" 사이만 조사하면 됩니다.** 이것이 로그를 단계별로 남겨둔 진짜 이유입니다.

### 브라우저 개발자 도구 활용

F12 → Network 탭 → **WS** 필터를 누르면 주고받은 WebSocket 메시지를 전부 볼 수 있습니다.

- 서버가 보낸 `state`의 실제 JSON 확인 → **"서버가 잘못 보낸 건가, 브라우저가 잘못 그린 건가"**를 즉시 구분
- Console 탭에서 직접 메시지 전송 → 프론트엔드 없이 서버만 테스트

💡 **문제가 생기면 항상 이 질문부터**: **"서버까지는 제대로 갔는가?"** 시리얼 로그와 Network 탭을 나란히 보면 답이 나옵니다. 이 습관 하나가 디버깅 시간을 절반으로 줄입니다.

---

## 정리: 처음부터 다시 만든다면 이 순서로

```
[기반]      0. 전체 그림 + 서버 권위 원칙 정하기
           0.5 빌드 파이프라인 이해 (upload vs uploadfs)
            1. 시리얼 출력 확인          ← 눈을 뜬다
            2. Wi-Fi AP 확인            ← 연결이 된다
            3. 정적 파일 서빙 확인        ← 화면이 뜬다
          3.5 코드 배치 정하기 (src/lib/include)
            4. WebSocket 연결만 확인      ← 실시간 통로가 뚫린다

[설계]      5. 메시지 규격을 문서로 먼저
            6. 자료구조 설계 (거꾸로 추론)

[핵심]      7. join 구현 + 프론트엔드 최소 연결
            8. turn 구현 (서버 권위 적용)
            9. 게임 틱 (계산 따로 → 반영 따로)
           10. 상태 전송 정책 결정
           11. 잡기 판정 + 역할 교체

[살 붙이기] 12. 장애물 동적 변경
           13. 프론트엔드 제대로 만들기

[견고하게]  14. 예외 상황 목록화 후 처리
           15. 동시성 문제 찾아 뮤텍스로 보호
           16. 입력 검증 전체 점검
           17. 시나리오 기반 통합 테스트
```

이 순서의 공통점은 **"항상 눈으로 확인할 수 있는 가장 작은 조각부터 만들고, 그 위에 한 층씩 쌓는다"**는 것입니다.

특히 다음 세 가지 원칙은 이 프로젝트 전체를 관통합니다.

1. **한 번에 하나만 확인한다.** Wi-Fi도 안 되는데 웹서버까지 만들면, 문제가 생겼을 때 어느 쪽인지 구분할 수 없습니다.
2. **결정한 이유를 코드 옆에 적는다.** 코드는 "무엇을"만 말하고 "왜"는 절대 말해주지 않습니다.
3. **정상 흐름을 먼저 완성하고, 예외는 나중에 몰아서 한다.** 동시에 하려 하면 "정상이 뭔지"조차 헷갈립니다.

---

## 부록 A. 자주 겪는 문제와 해결법

| 증상 | 가장 흔한 원인 | 확인/해결 |
|---|---|---|
| 시리얼에 아무것도 안 나옴 | 케이블을 꽂은 USB 단자와 `ARDUINO_USB_CDC_ON_BOOT` 설정이 안 맞음 | 반대쪽 단자에 꽂아보거나, `platformio.ini`의 두 플래그 주석을 해제하고 재업로드 |
| 시리얼에 깨진 글자 | `monitor_speed`와 `Serial.begin()` 값이 다름 | 둘 다 `115200`인지 확인 |
| 부팅 로그의 앞부분이 잘림 | USB 인식 전에 출력됨 | `while (!Serial && ...)` 대기 코드 확인 ([main.cpp:26](src/main.cpp:26)) |
| `LittleFS 마운트 실패` | `uploadfs`를 안 했거나 파티션 이름이 `spiffs`가 아님 | `pio run -t uploadfs` 실행 / [partitions.csv](partitions.csv)의 이름 확인 |
| 파일 목록이 "비어있음" | `uploadfs`를 안 함 | `pio run -t uploadfs` |
| 브라우저에 흰 화면, 404 | 파일 이름 오타 또는 `uploadfs` 누락 | 시리얼의 `[WARN] 404 Not Found: ...` 로그에서 **어떤 파일**을 못 찾는지 확인 |
| **HTML을 고쳤는데 안 바뀜** | `upload`만 하고 `uploadfs`를 안 함 | ⭐ 가장 흔한 실수. `pio run -t uploadfs` |
| **C++를 고쳤는데 안 바뀜** | `uploadfs`만 하고 `upload`를 안 함 | `pio run -t upload` |
| Wi-Fi 목록에 SSID가 없음 | AP 생성 실패 또는 비밀번호가 8자 미만 | 시리얼에서 `[ERROR] Wi-Fi AP 생성 실패` 확인 |
| Wi-Fi는 붙는데 페이지가 안 열림 | 폰이 "인터넷 없음"이라고 판단해 모바일 데이터로 전환 | Wi-Fi 설정에서 "모바일 데이터 자동 전환" 해제, 주소는 `http://192.168.4.1` 직접 입력 |
| `https://`로 열려서 접속 실패 | 브라우저가 자동으로 https를 붙임 | 주소창에 `http://192.168.4.1`을 명시적으로 입력 |
| 9번째 기기부터 접속 안 됨 | `WIFI_MAX_CONNECTIONS = 8` (하드웨어 한계) | 정상 동작입니다 (2단계 참고) |
| `Config.h: No such file` | `lib/` 안에서 `include/`가 안 보임 | `platformio.ini`의 `-I include` 확인 (3.5단계) |
| `AsyncWebServer` 컴파일 오류 | 관리 중단된 옛 저장소(`me-no-dev`)를 씀 | `lib_deps`를 `ESP32Async/...`로 |
| 보드가 계속 재부팅 | 스택 넘침 / 배열 밖 접근 / 널 포인터 | `monitor_filters = esp32_exception_decoder`를 켜고 Backtrace의 파일:줄 확인 |
| 8명일 때 화면이 끊김 | 대역폭 또는 CPU 한계 | `TICK_INTERVAL_MS`를 500~600으로 올려보기 (10단계) |
| 가끔 이상한 동작, 재현 안 됨 | 경쟁 상태(race condition) | 15단계. 뮤텍스로 보호되지 않은 접근이 있는지 확인 |

💡 **문제 좁히는 순서 (이 순서대로만 하세요)**
1. **시리얼 로그**가 어디까지 나왔나? → 서버 문제인지 아닌지 판별
2. **브라우저 F12 → Console**에 오류가 있나? → JS 문법/로딩 문제
3. **F12 → Network → WS**에 메시지가 오가나? → 통신 문제인지 렌더링 문제인지 판별

---

## 부록 B. 이 코드의 알려진 한계와 개선 과제

**동작하는 코드에도 빈틈은 있습니다.** 이걸 숨기지 않고 목록으로 관리하는 것이 좋은 개발 습관입니다. 난이도 순으로 정리했으니, **연습 문제로 하나씩 고쳐보세요.**

### 🟢 쉬움 — 표현/문구 문제

**B-1. 잡기 알림 문구가 어색함**
[display.js:75](data/js/display.js:75)가 `"철수 님이 잡혔습니다! 새 술래: 철수"`처럼 같은 이름을 두 번 표시합니다.
→ `previousTaggerPlayerId`로 이전 술래 이름을 찾아 `"영희 님이 철수 님을 잡았습니다! 새 술래: 철수"`로 고치기.

**B-2. 주석과 코드가 불일치**
[GameServer.cpp:620](lib/GameServer/GameServer.cpp:620)의 "플레이어가 인접해있으면"은 실제로 "정확히 그 칸에 있으면"입니다. 게다가 플레이어는 장애물 칸에 설 수 없으므로 **이 검사는 항상 참**입니다(죽은 코드).
→ 주석을 사실대로 고치거나, 진짜로 "인접 검사"를 구현하거나, 검사를 제거하기.

**B-3. 쓰지 않는 기능**
`ping`/`pong`, `requestState`가 서버에만 있고 클라이언트가 안 씁니다.
→ `common.js`에 30초 하트비트를 추가해서 좀비 연결을 감지하게 만들기.

### 🟡 보통 — 게임성 문제

**B-4. 시작 위치가 한 줄로 뭉친다** ⭐ 가장 체감이 큰 문제
`findFreeStartPosition()`은 항상 좌상단부터 스캔해서 첫 빈 칸을 줍니다. 그래서 게임을 시작하면 전원이 `(1,1), (2,1), (3,1)...`로 **한 줄에 늘어섭니다.** 술래 바로 옆에서 시작하는 사람이 생겨 불공평합니다.
→ 빈 칸 목록을 전부 모은 뒤 `esp_random()`으로 섞어서 배정하기. (14단계의 "모으기 → 세기 → 뽑기" 패턴)

**B-5. 도망자끼리 서로를 통과함**
9단계 연습 문제 참고. A와 B가 마주 보고 이동하면 서로를 지나칩니다.
→ "내 목적지에 있던 사람의 목적지가 내 현재 위치인가"를 검사해서 둘 다 제자리에 두기.

**B-6. 재시작해도 맵이 초기화되지 않음**
`initRoomMap()`은 방을 처음 만들 때만 호출됩니다. 여러 판을 하면 장애물이 계속 변형된 상태로 누적됩니다.
→ `handleStartGame()`에서 `initRoomMap(room)`을 호출하기. (그게 원하는 동작인지 먼저 결정할 것)

**B-7. 게임 중에도 `startGame`이 먹힘**
진행 중에 "게임 시작"을 누르면 위치가 리셋되고 술래가 다시 뽑힙니다. 의도한 것이라면 버튼 이름을 "다시 시작"으로 바꾸고, 아니라면 `if (room.gameStarted) { 거절 }`을 추가.

**B-8. 끊긴 플레이어가 "유령"이 됨**
`!connected`인 플레이어는 칸 예약에서 빠지고 잡기 판정에서도 제외됩니다. 살아있는 사람이 그 위를 지나갑니다. 화면에는 회색으로 보이므로 **보이는데 통과되는** 상태입니다.
→ 의도라면 문서화, 아니라면 예약에 포함시키기.

### 🔴 어려움 — 구조 문제

**B-9. 끊긴 플레이어 위에 장애물이 생김**
`isPositionOccupied()`가 `connected`만 세기 때문입니다. 재접속하면 장애물 안에 갇힙니다.
→ 용도별로 함수를 나누기: `isOccupiedByAnyPlayer()`(장애물 배치용) / `isOccupiedByActivePlayer()`(이동 판정용).

**B-10. 플레이어가 완전히 갇힐 수 있음**
후진이 없으므로 사방이 막히면 영영 못 움직입니다.
→ 장애물 배치 전에 "이 칸을 막아도 플레이어가 이동 가능한 칸이 남는가"를 검사(BFS), 또는 갇힌 플레이어를 빈 칸으로 순간이동.

**B-11. 락을 잡은 채로 네트워크 전송**
15단계 참고. `broadcastState()`가 뮤텍스 안에서 `client->text()`를 호출합니다.
→ 락 안에서는 JSON 문자열만 만들고, 락을 푼 뒤 전송하도록 분리.

**B-12. `Take`/`Give` 짝 맞추기가 수동**
→ RAII `LockGuard` 도입 (15단계 연습 문제).

**B-13. 움직임이 뚝뚝 끊김**
캐릭터가 400ms마다 한 칸씩 순간이동합니다.
→ `display.js`에서 이전 위치와 현재 위치를 저장하고, `tickInterval`(서버가 이미 보내주고 있습니다!) 동안 보간(interpolation)해서 부드럽게 그리기. `state`에 `tickInterval`을 넣어둔 것이 바로 이걸 위한 준비입니다.

### 📌 기능 확장 아이디어

- 라운드 제한 시간 + 점수판 (부록 D의 레시피 그대로 적용해보기)
- 아이템 (속도 증가, 순간이동, 벽 통과)
- 맵 여러 개 중 선택
- 후진 버튼 또는 반시계 회전 버튼
- `app1` 파티션을 없애고 확보한 공간에 효과음(mp3) 넣기 (0.5단계)

---

## 부록 C. 명령어 치트시트

```bash
pio run
```
컴파일만 (보드 없어도 됨). **문법 오류 확인용으로 가장 자주 씁니다.**

```bash
pio run --target upload
```
컴파일 + 펌웨어 업로드. **`.cpp`/`.h`를 고쳤을 때.**

```bash
pio run --target uploadfs
```
`data/` 폴더를 LittleFS로 업로드. **HTML/CSS/JS를 고쳤을 때.**

```bash
pio device monitor
```
시리얼 모니터 (`Ctrl+C`로 종료).

```bash
pio run --target upload && pio device monitor
```
업로드 성공 시에만 모니터 열기. **개발 중 가장 많이 쓰는 조합.**

```bash
pio run --target clean
```
빌드 결과물 삭제. 이상한 링크 오류가 날 때 시도.

```bash
pio device list
```
연결된 포트 확인. 업로드가 실패할 때 보드가 인식되는지 점검.

```bash
pio pkg list
```
설치된 라이브러리와 실제 버전 확인.

**VS Code에서 하려면**: 좌측 PlatformIO 아이콘 → PROJECT TASKS → `Build` / `Upload` / `Upload Filesystem Image` / `Monitor`.

---

## 부록 D. 새 기능을 추가할 때의 5단계 레시피

이 문서의 15개 단계를 압축하면, **어떤 기능을 추가하든 이 순서**로 하면 됩니다. 예시로 **"라운드 제한 시간(60초)"** 기능을 넣어봅니다.

### ① 무엇을 기억해야 하는가 → 자료구조

> "제한 시간이 언제 끝나는가"를 방마다 알아야 한다.

```cpp
// GameTypes.h 의 GameRoom 에 추가
unsigned long roundEndTime;   // 라운드가 끝나는 시각(millis). 0이면 무제한.
```
```cpp
// Config.h 에 추가
constexpr unsigned long ROUND_DURATION_MS = 60000;
```

💡 **여기서 `Config.h`에 상수를 넣는 습관**을 지키세요. 나중에 "90초로 바꿔주세요"라는 요청이 오면 한 줄만 고치면 됩니다.

### ② 어떤 메시지가 오가는가 → 프로토콜

> 남은 시간을 보여줘야 하니 `state`에 필드를 하나 추가한다.
> 시간이 다 되면 알림이 필요하니 새 메시지 종류를 만든다.

```cpp
// WebSocketProtocol.h
constexpr const char *ROUND_ENDED = "roundEnded";
```

**클라이언트에서 계산하게 하면 안 됩니다.** "60초 전에 시작했으니까 지금 몇 초 남았겠지"를 브라우저가 계산하면 기기마다 시계가 달라서 어긋납니다. **서버가 `remainingMs`를 그대로 알려주세요.**

### ③ 서버 로직 → 상태를 바꾸는 코드

> 시작할 때 종료 시각을 정하고, 틱마다 확인한다.

```cpp
// handleStartGame() 안
room.roundEndTime = millis() + Config::ROUND_DURATION_MS;

// GameServer::loop() 의 방 순회 안
if (room.gameStarted && room.roundEndTime != 0 && (long)(now - room.roundEndTime) >= 0) {
    room.gameStarted = false;
    room.roundEndTime = 0;
    broadcastRoundEnded(i);
    broadcastState(i, false);
}
```

⚠️ **9단계의 오버플로우 규칙을 잊지 마세요.** "종료 시각"처럼 미래 시점을 저장하는 방식은 오버플로우에 취약합니다. 더 안전한 방법은 **"시작 시각을 저장하고 `now - roundStartTime >= DURATION`으로 비교"**하는 것입니다.

```cpp
unsigned long roundStartTime;                              // ✅ 이 방식을 권장
if (now - room.roundStartTime >= Config::ROUND_DURATION_MS) { ... }
```

### ④ 화면 반영 → 프론트엔드

```js
// display.js 의 applyState 안
if (msg.remainingMs != null) {
  timerValue.textContent = Math.ceil(msg.remainingMs / 1000) + '초';
}
// onMessage 안
else if (msg.type === 'roundEnded') { gameStateValue.textContent = '종료'; }
```

`display.html`에 표시할 자리(`<div class="stat-row">`)도 추가합니다.

### ⑤ 예외 상황 → "이럴 땐 어떻게?"

만들자마자 이 질문들을 스스로 던지세요.

- 라운드 중에 새 사람이 들어오면? → 남은 시간만 알려주면 됨 (`state`에 이미 포함)
- 라운드가 끝났는데 아무도 없으면? → `gameStarted = false`로 이미 처리됨
- 라운드 중에 술래가 나가면? → 14단계의 새 술래 선정이 그대로 동작
- 시간이 끝난 뒤 다시 시작하려면? → "게임 시작" 버튼을 다시 누르면 됨 (B-7과 연결)
- 라운드 종료 시 승자는? → **규칙을 정해야 함.** (예: 마지막 술래가 패배)

### 마지막으로: 문서를 고치세요

기능을 넣었으면 **5단계의 메시지 규격표에 `roundEnded`를 추가**하고, **10단계의 `state` 필드 설명에 `remainingMs`를 추가**하세요.

> 10단계에서 봤듯이, **"맵은 절대 안 바뀐다"는 문장을 고치지 않아서** 문서 전체의 신뢰도가 떨어졌던 일이 이 프로젝트에서 실제로 있었습니다. **코드와 문서는 같이 고쳐야 합니다.**

---

이 15단계와 5단계 레시피를 반복하면, 게임 규칙이 아무리 복잡해 보여도 결국 잘게 쪼개서 하나씩 눈으로 확인하며 올라갈 수 있습니다. **처음부터 완벽한 설계를 그리려 하지 말고, 항상 "지금 눈으로 확인할 수 있는 다음 한 걸음"을 찾으세요.**

