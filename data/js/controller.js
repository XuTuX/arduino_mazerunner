// =====================================================================
// controller.js
// 휴대폰 조작기 화면 전용 스크립트.
// - 서버에 join(controller) 메시지를 보낸다.
// - "시계 방향 90도 회전" 버튼을 누르면 turn 메시지만 보낸다.
//   실제 이동 계산은 절대 여기서 하지 않는다 (서버가 유일한 권한을 가진다).
// - state 메시지를 받아서 내 방향/역할/연결 상태만 화면에 표시한다.
// =====================================================================
(function () {
  'use strict';

  var channel = TagGame.getQueryParam('channel').toUpperCase();
  var initialName = TagGame.getQueryParam('name');

  if (!channel) {
    window.location.href = 'index.html';
    return;
  }

  var channelValue = document.getElementById('channelValue');
  var nameValue = document.getElementById('nameValue');
  var idValue = document.getElementById('idValue');
  var roleValue = document.getElementById('roleValue');
  var connStatus = document.getElementById('connStatus');
  var directionArrow = document.getElementById('directionArrow');
  var directionLabel = document.getElementById('directionLabel');
  var rotateBtn = document.getElementById('rotateBtn');
  var errorBanner = document.getElementById('errorBanner');

  channelValue.textContent = channel;

  var myPlayerId = null;
  var lastTurnSentAt = 0;
  // 서버 최소 간격(100ms)보다 넉넉하게 잡아서, 버튼을 연타해도
  // 불필요한 메시지를 서버로 계속 보내지 않도록 한다 (2차 방어선).
  var MIN_CLIENT_TURN_GAP_MS = 150;

  function setConnStatus(state) {
    connStatus.classList.remove('conn-connected', 'conn-disconnected', 'conn-reconnecting');
    if (state === 'connected') {
      connStatus.textContent = '연결됨';
      connStatus.classList.add('conn-connected');
    } else if (state === 'reconnecting') {
      connStatus.textContent = '재연결 중...';
      connStatus.classList.add('conn-reconnecting');
    } else {
      connStatus.textContent = '연결 끊김';
      connStatus.classList.add('conn-disconnected');
    }
  }

  function showError(msg) {
    errorBanner.textContent = msg;
    errorBanner.hidden = false;
    setTimeout(function () { errorBanner.hidden = true; }, 3000);
  }

  function updateDirection(dirIndex) {
    directionArrow.textContent = TagGame.DIRECTION_ARROWS[dirIndex] || '↑';
    directionLabel.textContent = TagGame.DIRECTION_NAMES[dirIndex] || '-';
  }

  function updateRole(isTagger) {
    if (isTagger) {
      roleValue.textContent = '술래';
      roleValue.className = 'info-value role-tagger';
    } else {
      roleValue.textContent = '도망자';
      roleValue.className = 'info-value role-runner';
    }
  }

  var socket = TagGame.createGameSocket({
    onOpen: function () {
      setConnStatus('connected');
      socket.send({
        type: 'join',
        role: 'controller',
        channel: channel,
        name: initialName,
        token: TagGame.loadToken(channel) // 재접속용 토큰 (없으면 빈 문자열 -> 새 플레이어)
      });
    },
    onClose: function () {
      setConnStatus('disconnected');
    },
    onReconnecting: function () {
      setConnStatus('reconnecting');
    },
    onMessage: function (msg) {
      if (msg.type === 'joined' && msg.role === 'controller') {
        myPlayerId = msg.playerId;
        nameValue.textContent = msg.name;
        idValue.textContent = '#' + msg.playerId;
        if (msg.token) {
          TagGame.saveToken(channel, msg.token); // 다음 접속을 위해 토큰 저장
        }
      } else if (msg.type === 'state') {
        if (!msg.players) return;
        for (var i = 0; i < msg.players.length; i++) {
          var p = msg.players[i];
          if (p.id === myPlayerId) {
            updateDirection(p.direction);
            updateRole(p.isTagger);
            break;
          }
        }
      } else if (msg.type === 'error') {
        showError(msg.message);
      }
    }
  });

  function sendTurn() {
    var now = Date.now();
    if (now - lastTurnSentAt < MIN_CLIENT_TURN_GAP_MS) return;
    lastTurnSentAt = now;

    // 눌렀다는 시각적 피드백
    rotateBtn.classList.add('pressed');
    setTimeout(function () { rotateBtn.classList.remove('pressed'); }, 120);

    socket.send({ type: 'turn', direction: 'clockwise' });
  }

  // 모바일 더블탭 확대/중복 입력 방지:
  // touchend에서 preventDefault()로 기본 동작(확대, 유령 클릭)을 막고 직접 처리한다.
  // 이후 300~500ms 이내에 발생하는 click 이벤트는 같은 터치가 만든 것으로 보고 무시한다.
  var lastTouchTime = 0;
  rotateBtn.addEventListener('touchend', function (e) {
    e.preventDefault();
    lastTouchTime = Date.now();
    sendTurn();
  }, { passive: false });

  rotateBtn.addEventListener('click', function () {
    if (Date.now() - lastTouchTime < 500) return;
    sendTurn();
  });

  // 두 손가락 핀치 줌 방지
  document.addEventListener('gesturestart', function (e) { e.preventDefault(); });
})();
