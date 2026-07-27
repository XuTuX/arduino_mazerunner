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
      // 맵은 참가 직후에만 전송되므로, 받았을 때만 캔버스 크기를 다시 계산한다.
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

  function draw() {
    if (!mapData) return;
    var w = mapData.width, h = mapData.height;

    ctx.fillStyle = '#12161f';
    ctx.fillRect(0, 0, w * cellSize, h * cellSize);

    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var tile = tileAt(x, y);
        var px = x * cellSize, py = y * cellSize;

        if (tile === 1) {
          // 벽: 밝은 회색 사각형
          ctx.fillStyle = '#4a5568';
          ctx.fillRect(px, py, cellSize, cellSize);
        } else if (tile === 2) {
          // 장애물: 갈색 바탕 + X자 무늬로 벽과 구분
          ctx.fillStyle = '#3a2f1f';
          ctx.fillRect(px, py, cellSize, cellSize);
          ctx.strokeStyle = '#8a6d3b';
          ctx.lineWidth = 2;
          ctx.beginPath();
          ctx.moveTo(px + 4, py + 4);
          ctx.lineTo(px + cellSize - 4, py + cellSize - 4);
          ctx.moveTo(px + cellSize - 4, py + 4);
          ctx.lineTo(px + 4, py + cellSize - 4);
          ctx.stroke();
        } else {
          ctx.fillStyle = '#1a1f2b';
          ctx.fillRect(px, py, cellSize, cellSize);
        }

        // 구분선(격자)
        ctx.strokeStyle = 'rgba(255,255,255,0.06)';
        ctx.lineWidth = 1;
        ctx.strokeRect(px + 0.5, py + 0.5, cellSize - 1, cellSize - 1);
      }
    }

    for (var i = 0; i < players.length; i++) {
      drawPlayer(players[i]);
    }
  }

  function drawPlayer(p) {
    if (p.x < 0 || p.y < 0) return;
    var cx = p.x * cellSize + cellSize / 2;
    var cy = p.y * cellSize + cellSize / 2;
    var radius = cellSize * 0.36;
    var color = PLAYER_COLORS[(p.id - 1) % PLAYER_COLORS.length];

    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, Math.PI * 2);
    ctx.fillStyle = p.connected ? color : 'rgba(150,150,150,0.4)';
    ctx.fill();

    if (p.isTagger) {
      // 술래는 굵은 빨간 테두리로 확실히 구분한다.
      ctx.lineWidth = Math.max(2, cellSize * 0.1);
      ctx.strokeStyle = '#ff2d55';
      ctx.stroke();
    } else {
      ctx.lineWidth = 1.5;
      ctx.strokeStyle = 'rgba(0,0,0,0.5)';
      ctx.stroke();
    }

    ctx.fillStyle = '#0a0a0a';
    ctx.font = 'bold ' + Math.floor(cellSize * 0.36) + 'px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(String(p.id), cx, cy);

    drawDirectionArrow(cx, cy, radius, p.direction);
  }

  function drawDirectionArrow(cx, cy, radius, direction) {
    var angleMap = [-Math.PI / 2, 0, Math.PI / 2, Math.PI]; // 위, 오른쪽, 아래, 왼쪽
    var angle = angleMap[direction] || 0;
    var dist = radius + cellSize * 0.18;
    var tipX = cx + Math.cos(angle) * dist;
    var tipY = cy + Math.sin(angle) * dist;
    var baseDist = dist - cellSize * 0.16;
    var baseAngle1 = angle + 2.6;
    var baseAngle2 = angle - 2.6;

    ctx.beginPath();
    ctx.moveTo(tipX, tipY);
    ctx.lineTo(cx + Math.cos(baseAngle1) * baseDist, cy + Math.sin(baseAngle1) * baseDist);
    ctx.lineTo(cx + Math.cos(baseAngle2) * baseDist, cy + Math.sin(baseAngle2) * baseDist);
    ctx.closePath();
    ctx.fillStyle = '#ffffff';
    ctx.fill();
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
