// =====================================================================
// display.js
// 컴퓨터 게임 화면 전용 스크립트.
// - 서버에 join(display) 메시지를 보낸다.
// - state 메시지를 받아 Canvas에 맵/플레이어를 그린다.
// - "게임 시작" 버튼을 누르면 startGame 메시지를 보낸다.
// =====================================================================
(function () {
  'use strict';

  var channel = TagGame.getQueryParam('channel').toUpperCase();
  if (!channel) {
    window.location.href = 'index.html';
    return;
  }

  var channelValue = document.getElementById('channelValue');
  var playerCountValue = document.getElementById('playerCountValue');
  var taggerValue = document.getElementById('taggerValue');
  var gameStateValue = document.getElementById('gameStateValue');
  var wsStatusValue = document.getElementById('wsStatusValue');
  var startBtn = document.getElementById('startBtn');
  var errorBanner = document.getElementById('errorBanner');
  var playerListEl = document.getElementById('playerList');
  var canvas = document.getElementById('gameCanvas');
  var ctx = canvas.getContext('2d');

  channelValue.textContent = channel;

  var PLAYER_COLORS = ['#3fa7ff', '#ff6b6b', '#ffd93d', '#6bff8f', '#c77dff', '#ff9f43', '#4dd4ff', '#ff7bd0'];

  var mapData = null; // { width, height, tiles: [...] }
  var players = [];
  var animFrame = 0; // 애니메이션 프레임 카운터

  // 부드러운 애니메이션용 타이머
  var animInterval = setInterval(function () {
    animFrame++;
    draw();
  }, 80);

  function showError(msg) {
    errorBanner.textContent = msg;
    errorBanner.hidden = false;
    setTimeout(function () { errorBanner.hidden = true; }, 3000);
  }

  function setWsStatus(state) {
    wsStatusValue.classList.remove('ws-connected', 'ws-disconnected', 'ws-reconnecting');
    if (state === 'connected') {
      wsStatusValue.textContent = '연결됨';
      wsStatusValue.classList.add('ws-connected');
    } else if (state === 'reconnecting') {
      wsStatusValue.textContent = '재연결 중...';
      wsStatusValue.classList.add('ws-reconnecting');
    } else {
      wsStatusValue.textContent = '끊김';
      wsStatusValue.classList.add('ws-disconnected');
    }
  }

  var socket = TagGame.createGameSocket({
    onOpen: function () {
      setWsStatus('connected');
      socket.send({ type: 'join', role: 'display', channel: channel });
    },
    onClose: function () { setWsStatus('disconnected'); },
    onReconnecting: function () { setWsStatus('reconnecting'); },
    onMessage: function (msg) {
      if (msg.type === 'state') {
        applyState(msg);
      } else if (msg.type === 'gameStarted') {
        gameStateValue.textContent = '진행 중';
      } else if (msg.type === 'playerCaught') {
        showError(msg.caughtPlayerName + ' 님이 잡혔습니다! 새 술래: ' + msg.caughtPlayerName);
      } else if (msg.type === 'playerDisconnected') {
        showError(msg.name + ' 님의 연결이 끊어졌습니다.');
      } else if (msg.type === 'error') {
        showError(msg.message);
      }
      // 'joined' 메시지는 참가 확인용이라 별도 처리가 필요 없다.
    }
  });

  function applyState(msg) {
    if (msg.map) {
      // 맵은 참가 직후 또는 장애물 동적 변경 시에 전송된다.
      mapData = msg.map;
      resizeCanvas();
    }
    players = msg.players || [];

    gameStateValue.textContent = msg.gameStarted ? '진행 중' : '대기 중';
    playerCountValue.textContent = String(msg.playerCount != null ? msg.playerCount : players.length);

    var tagger = null;
    for (var i = 0; i < players.length; i++) {
      if (players[i].isTagger) { tagger = players[i]; break; }
    }
    taggerValue.textContent = tagger ? tagger.name : '-';

    renderPlayerList();
    draw();
  }

  function renderPlayerList() {
    playerListEl.innerHTML = '';
    for (var i = 0; i < players.length; i++) {
      var p = players[i];
      var li = document.createElement('li');
      li.className = 'player-item' +
        (p.isTagger ? ' player-item-tagger' : '') +
        (!p.connected ? ' player-item-offline' : '');

      var swatch = document.createElement('span');
      swatch.className = 'player-swatch';
      swatch.style.background = PLAYER_COLORS[(p.id - 1) % PLAYER_COLORS.length];
      li.appendChild(swatch);

      var label = document.createElement('span');
      label.textContent = '#' + p.id + ' ' + p.name +
        (p.isTagger ? ' (술래)' : '') +
        (!p.connected ? ' - 끊김' : '');
      li.appendChild(label);

      playerListEl.appendChild(li);
    }
  }

  // ---- Canvas 렌더링 ----
  var cellSize = 32;

  function resizeCanvas() {
    if (!mapData) return;
    var area = canvas.parentElement;
    var maxWidth = area.clientWidth - 16;
    var maxHeight = area.clientHeight - 16;
    var size = Math.max(100, Math.min(maxWidth, maxHeight));
    cellSize = Math.floor(size / mapData.width);
    var pixelSize = cellSize * mapData.width;

    // 고해상도(레티나 등) 화면에서 흐릿해지지 않도록 devicePixelRatio를 반영한다.
    var dpr = window.devicePixelRatio || 1;
    canvas.style.width = pixelSize + 'px';
    canvas.style.height = pixelSize + 'px';
    canvas.width = Math.floor(pixelSize * dpr);
    canvas.height = Math.floor(pixelSize * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  function tileAt(x, y) {
    if (!mapData) return 0;
    return mapData.tiles[y * mapData.width + x];
  }

  // ---- 타일 렌더링 함수들 ----

  function drawFloorTile(px, py) {
    // 깔끔한 어두운 바닥 + 미묘한 체크 패턴
    ctx.fillStyle = '#1a1e2e';
    ctx.fillRect(px, py, cellSize, cellSize);

    // 체크 패턴 (은은하게)
    ctx.fillStyle = 'rgba(255,255,255,0.015)';
    var half = cellSize / 2;
    ctx.fillRect(px, py, half, half);
    ctx.fillRect(px + half, py + half, half, half);
  }

  function drawWallTile(px, py) {
    // 입체감 있는 벽 — 그라데이션 + 내부 라인
    var grad = ctx.createLinearGradient(px, py, px + cellSize, py + cellSize);
    grad.addColorStop(0, '#3d4559');
    grad.addColorStop(1, '#2d3344');
    ctx.fillStyle = grad;
    ctx.fillRect(px, py, cellSize, cellSize);

    // 벽돌 라인 (가로)
    ctx.strokeStyle = 'rgba(0,0,0,0.25)';
    ctx.lineWidth = 1;
    var third = cellSize / 3;
    ctx.beginPath();
    ctx.moveTo(px, py + third);
    ctx.lineTo(px + cellSize, py + third);
    ctx.moveTo(px, py + third * 2);
    ctx.lineTo(px + cellSize, py + third * 2);
    ctx.stroke();

    // 벽돌 라인 (세로 — 엇갈리게)
    ctx.beginPath();
    ctx.moveTo(px + cellSize / 2, py);
    ctx.lineTo(px + cellSize / 2, py + third);
    ctx.moveTo(px + cellSize / 4, py + third);
    ctx.lineTo(px + cellSize / 4, py + third * 2);
    ctx.moveTo(px + cellSize * 3 / 4, py + third);
    ctx.lineTo(px + cellSize * 3 / 4, py + third * 2);
    ctx.moveTo(px + cellSize / 2, py + third * 2);
    ctx.lineTo(px + cellSize / 2, py + cellSize);
    ctx.stroke();

    // 윗면 밝은 하이라이트
    ctx.fillStyle = 'rgba(255,255,255,0.06)';
    ctx.fillRect(px, py, cellSize, 2);
    ctx.fillRect(px, py, 2, cellSize);
  }

  function drawObstacleTile(px, py) {
    // 크리스탈/보석 모양 장애물 — 반투명 + 빛나는 효과
    var cx = px + cellSize / 2;
    var cy = py + cellSize / 2;
    var r = cellSize * 0.38;

    // 바닥 (약간 밝은 배경)
    ctx.fillStyle = '#1a1e2e';
    ctx.fillRect(px, py, cellSize, cellSize);

    // 글로우 효과
    var glow = ctx.createRadialGradient(cx, cy, r * 0.2, cx, cy, r * 1.2);
    glow.addColorStop(0, 'rgba(100,200,255,0.15)');
    glow.addColorStop(1, 'rgba(100,200,255,0)');
    ctx.fillStyle = glow;
    ctx.fillRect(px, py, cellSize, cellSize);

    // 다이아몬드(마름모) 모양
    ctx.beginPath();
    ctx.moveTo(cx, cy - r);          // 위
    ctx.lineTo(cx + r, cy);          // 오른쪽
    ctx.lineTo(cx, cy + r);          // 아래
    ctx.lineTo(cx - r, cy);          // 왼쪽
    ctx.closePath();

    // 그라데이션 채우기
    var grad = ctx.createLinearGradient(cx - r, cy - r, cx + r, cy + r);
    grad.addColorStop(0, 'rgba(80,180,255,0.6)');
    grad.addColorStop(0.5, 'rgba(120,220,255,0.8)');
    grad.addColorStop(1, 'rgba(60,140,220,0.6)');
    ctx.fillStyle = grad;
    ctx.fill();

    // 테두리
    ctx.strokeStyle = 'rgba(150,220,255,0.7)';
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // 내부 하이라이트 라인
    ctx.beginPath();
    ctx.moveTo(cx, cy - r * 0.4);
    ctx.lineTo(cx + r * 0.4, cy);
    ctx.lineTo(cx, cy + r * 0.4);
    ctx.lineTo(cx - r * 0.4, cy);
    ctx.closePath();
    ctx.strokeStyle = 'rgba(200,240,255,0.35)';
    ctx.lineWidth = 1;
    ctx.stroke();

    // 반짝이는 점 (애니메이션)
    var sparkle = Math.sin(animFrame * 0.3 + px * 0.1) * 0.3 + 0.5;
    ctx.fillStyle = 'rgba(255,255,255,' + sparkle.toFixed(2) + ')';
    ctx.beginPath();
    ctx.arc(cx - r * 0.25, cy - r * 0.3, 1.5, 0, Math.PI * 2);
    ctx.fill();
  }

  function draw() {
    if (!mapData) return;
    var w = mapData.width, h = mapData.height;

    // 전체 배경
    ctx.fillStyle = '#0f1220';
    ctx.fillRect(0, 0, w * cellSize, h * cellSize);

    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var tile = tileAt(x, y);
        var px = x * cellSize, py = y * cellSize;

        if (tile === 1) {
          drawWallTile(px, py);
        } else if (tile === 2) {
          drawObstacleTile(px, py);
        } else {
          drawFloorTile(px, py);
        }

        // 구분선(격자) — 매우 은은하게
        ctx.strokeStyle = 'rgba(255,255,255,0.03)';
        ctx.lineWidth = 0.5;
        ctx.strokeRect(px + 0.5, py + 0.5, cellSize - 1, cellSize - 1);
      }
    }

    for (var i = 0; i < players.length; i++) {
      drawPlayer(players[i]);
    }
  }

  // ---- 플레이어 렌더링 ----

  function drawPlayer(p) {
    if (p.x < 0 || p.y < 0) return;
    var cx = p.x * cellSize + cellSize / 2;
    var cy = p.y * cellSize + cellSize / 2;
    var radius = cellSize * 0.36;
    var color = PLAYER_COLORS[(p.id - 1) % PLAYER_COLORS.length];

    if (p.isTagger) {
      drawTagger(cx, cy, radius, p, color);
    } else {
      drawRunner(cx, cy, radius, p, color);
    }
  }

  function drawRunner(cx, cy, radius, p, color) {
    if (!p.connected) {
      // 오프라인 플레이어: 회색 반투명
      ctx.globalAlpha = 0.4;
      ctx.beginPath();
      ctx.arc(cx, cy, radius, 0, Math.PI * 2);
      ctx.fillStyle = '#888';
      ctx.fill();
      ctx.globalAlpha = 1;
      return;
    }

    // 몸체 그림자
    ctx.beginPath();
    ctx.arc(cx + 1, cy + 2, radius, 0, Math.PI * 2);
    ctx.fillStyle = 'rgba(0,0,0,0.3)';
    ctx.fill();

    // 몸체 — 둥근 캐릭터
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, Math.PI * 2);
    var bodyGrad = ctx.createRadialGradient(cx - radius * 0.3, cy - radius * 0.3, radius * 0.1, cx, cy, radius);
    bodyGrad.addColorStop(0, lightenColor(color, 30));
    bodyGrad.addColorStop(1, color);
    ctx.fillStyle = bodyGrad;
    ctx.fill();

    // 테두리
    ctx.lineWidth = 1.5;
    ctx.strokeStyle = darkenColor(color, 40);
    ctx.stroke();

    // 눈 (방향에 따라 위치가 달라짐)
    drawEyes(cx, cy, radius, p.direction, '#fff', '#1a1a2e');

    // 플레이어 번호 (아래에 작게)
    ctx.fillStyle = '#fff';
    ctx.font = 'bold ' + Math.floor(cellSize * 0.2) + 'px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(String(p.id), cx, cy + radius + cellSize * 0.15);
  }

  function drawTagger(cx, cy, radius, p, color) {
    if (!p.connected) {
      ctx.globalAlpha = 0.4;
      ctx.beginPath();
      ctx.arc(cx, cy, radius, 0, Math.PI * 2);
      ctx.fillStyle = '#888';
      ctx.fill();
      ctx.globalAlpha = 1;
      return;
    }

    // 붉은 글로우 효과 (맥동 애니메이션)
    var pulse = Math.sin(animFrame * 0.25) * 0.15 + 0.35;
    var glowR = radius * 1.6;
    var glow = ctx.createRadialGradient(cx, cy, radius * 0.5, cx, cy, glowR);
    glow.addColorStop(0, 'rgba(255,50,50,' + pulse.toFixed(2) + ')');
    glow.addColorStop(1, 'rgba(255,50,50,0)');
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.arc(cx, cy, glowR, 0, Math.PI * 2);
    ctx.fill();

    // 몸체 그림자
    ctx.beginPath();
    ctx.arc(cx + 1, cy + 2, radius, 0, Math.PI * 2);
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.fill();

    // 몸체 — 빨간 계열 위협적인 캐릭터
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, Math.PI * 2);
    var taggerGrad = ctx.createRadialGradient(cx - radius * 0.3, cy - radius * 0.3, radius * 0.1, cx, cy, radius);
    taggerGrad.addColorStop(0, '#ff5555');
    taggerGrad.addColorStop(1, '#cc2233');
    ctx.fillStyle = taggerGrad;
    ctx.fill();

    // 붉은 테두리
    ctx.lineWidth = 2.5;
    ctx.strokeStyle = '#ff2d55';
    ctx.stroke();

    // 뿔 2개
    var hornH = radius * 0.5;
    var hornW = radius * 0.2;
    // 왼쪽 뿔
    ctx.beginPath();
    ctx.moveTo(cx - radius * 0.4, cy - radius * 0.7);
    ctx.lineTo(cx - radius * 0.55, cy - radius - hornH);
    ctx.lineTo(cx - radius * 0.15, cy - radius * 0.5);
    ctx.closePath();
    ctx.fillStyle = '#ff4466';
    ctx.fill();
    // 오른쪽 뿔
    ctx.beginPath();
    ctx.moveTo(cx + radius * 0.4, cy - radius * 0.7);
    ctx.lineTo(cx + radius * 0.55, cy - radius - hornH);
    ctx.lineTo(cx + radius * 0.15, cy - radius * 0.5);
    ctx.closePath();
    ctx.fillStyle = '#ff4466';
    ctx.fill();

    // 눈 — 술래는 화난 눈
    drawAngryEyes(cx, cy, radius, p.direction);

    // 플레이어 번호
    ctx.fillStyle = '#fff';
    ctx.font = 'bold ' + Math.floor(cellSize * 0.2) + 'px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(String(p.id), cx, cy + radius + cellSize * 0.15);
  }

  function drawEyes(cx, cy, radius, direction, scleraColor, pupilColor) {
    // 방향에 따른 눈 위치 오프셋
    var dx = 0, dy = 0;
    var lookDist = radius * 0.12;
    if (direction === 0) dy = -lookDist;      // 위
    else if (direction === 1) dx = lookDist;   // 오른쪽
    else if (direction === 2) dy = lookDist;   // 아래
    else if (direction === 3) dx = -lookDist;  // 왼쪽

    var eyeR = radius * 0.22;
    var pupilR = eyeR * 0.55;
    var eyeSpacing = radius * 0.35;

    // 왼눈
    ctx.beginPath();
    ctx.arc(cx - eyeSpacing + dx, cy - radius * 0.1 + dy, eyeR, 0, Math.PI * 2);
    ctx.fillStyle = scleraColor;
    ctx.fill();
    ctx.beginPath();
    ctx.arc(cx - eyeSpacing + dx * 2, cy - radius * 0.1 + dy * 2, pupilR, 0, Math.PI * 2);
    ctx.fillStyle = pupilColor;
    ctx.fill();

    // 오른눈
    ctx.beginPath();
    ctx.arc(cx + eyeSpacing + dx, cy - radius * 0.1 + dy, eyeR, 0, Math.PI * 2);
    ctx.fillStyle = scleraColor;
    ctx.fill();
    ctx.beginPath();
    ctx.arc(cx + eyeSpacing + dx * 2, cy - radius * 0.1 + dy * 2, pupilR, 0, Math.PI * 2);
    ctx.fillStyle = pupilColor;
    ctx.fill();
  }

  function drawAngryEyes(cx, cy, radius, direction) {
    var dx = 0, dy = 0;
    var lookDist = radius * 0.12;
    if (direction === 0) dy = -lookDist;
    else if (direction === 1) dx = lookDist;
    else if (direction === 2) dy = lookDist;
    else if (direction === 3) dx = -lookDist;

    var eyeR = radius * 0.24;
    var pupilR = eyeR * 0.5;
    var eyeSpacing = radius * 0.35;

    // 왼눈 흰자
    ctx.beginPath();
    ctx.arc(cx - eyeSpacing + dx, cy - radius * 0.05 + dy, eyeR, 0, Math.PI * 2);
    ctx.fillStyle = '#ffe0e0';
    ctx.fill();
    // 왼눈 동공 (빨간)
    ctx.beginPath();
    ctx.arc(cx - eyeSpacing + dx * 2, cy - radius * 0.05 + dy * 2, pupilR, 0, Math.PI * 2);
    ctx.fillStyle = '#660011';
    ctx.fill();

    // 오른눈 흰자
    ctx.beginPath();
    ctx.arc(cx + eyeSpacing + dx, cy - radius * 0.05 + dy, eyeR, 0, Math.PI * 2);
    ctx.fillStyle = '#ffe0e0';
    ctx.fill();
    // 오른눈 동공
    ctx.beginPath();
    ctx.arc(cx + eyeSpacing + dx * 2, cy - radius * 0.05 + dy * 2, pupilR, 0, Math.PI * 2);
    ctx.fillStyle = '#660011';
    ctx.fill();

    // 화난 눈썹 — V 모양
    ctx.strokeStyle = '#440000';
    ctx.lineWidth = Math.max(1.5, cellSize * 0.04);
    ctx.lineCap = 'round';
    // 왼쪽 눈썹 (\/  모양)
    ctx.beginPath();
    ctx.moveTo(cx - eyeSpacing - eyeR * 0.8, cy - radius * 0.25 - eyeR * 0.6);
    ctx.lineTo(cx - eyeSpacing + eyeR * 0.3, cy - radius * 0.25 - eyeR * 0.1);
    ctx.stroke();
    // 오른쪽 눈썹
    ctx.beginPath();
    ctx.moveTo(cx + eyeSpacing + eyeR * 0.8, cy - radius * 0.25 - eyeR * 0.6);
    ctx.lineTo(cx + eyeSpacing - eyeR * 0.3, cy - radius * 0.25 - eyeR * 0.1);
    ctx.stroke();
  }

  // ---- 색상 유틸 ----
  function lightenColor(hex, amount) {
    var r = parseInt(hex.slice(1, 3), 16);
    var g = parseInt(hex.slice(3, 5), 16);
    var b = parseInt(hex.slice(5, 7), 16);
    r = Math.min(255, r + amount);
    g = Math.min(255, g + amount);
    b = Math.min(255, b + amount);
    return '#' + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
  }

  function darkenColor(hex, amount) {
    var r = parseInt(hex.slice(1, 3), 16);
    var g = parseInt(hex.slice(3, 5), 16);
    var b = parseInt(hex.slice(5, 7), 16);
    r = Math.max(0, r - amount);
    g = Math.max(0, g - amount);
    b = Math.max(0, b - amount);
    return '#' + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
  }

  startBtn.addEventListener('click', function () {
    socket.send({ type: 'startGame' });
  });

  var resizeTimer = null;
  window.addEventListener('resize', function () {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(function () {
      resizeCanvas();
      draw();
    }, 150);
  });
})();
