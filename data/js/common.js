// =====================================================================
// common.js
// index.html / controller.js / display.js 가 공통으로 사용하는 유틸리티 모음.
// window.TagGame 이라는 하나의 객체 안에 모든 기능을 담아서 전역 변수 오염을 줄인다.
// =====================================================================
window.TagGame = (function () {
  'use strict';

  var CHANNEL_KEY = 'tagGame.lastChannel';

  // 채널 이름 형식 검사: 영문/숫자 1~16자 (예: A, B, TEST1)
  function isValidChannelInput(channel) {
    return /^[A-Za-z0-9]{1,16}$/.test(channel);
  }

  function saveLastChannel(channel) {
    try { localStorage.setItem(CHANNEL_KEY, channel.toUpperCase()); } catch (e) { /* localStorage 사용 불가 시 무시 */ }
  }

  function loadLastChannel() {
    try { return localStorage.getItem(CHANNEL_KEY) || ''; } catch (e) { return ''; }
  }

  function tokenKey(channel) {
    return 'tagGame.token.' + channel.toUpperCase();
  }

  // 재접속용 토큰은 채널별로 따로 저장한다 (채널이 다르면 다른 플레이어이기 때문).
  function saveToken(channel, token) {
    try { localStorage.setItem(tokenKey(channel), token); } catch (e) { /* 무시 */ }
  }

  function loadToken(channel) {
    try { return localStorage.getItem(tokenKey(channel)) || ''; } catch (e) { return ''; }
  }

  function getQueryParam(name) {
    var params = new URLSearchParams(window.location.search);
    return params.get(name) || '';
  }

  function buildWsUrl() {
    var protocol = (window.location.protocol === 'https:') ? 'wss:' : 'ws:';
    return protocol + '//' + window.location.host + '/ws';
  }

  // 자동 재연결 기능이 있는 WebSocket 래퍼.
  // handlers: { onOpen, onMessage(obj), onClose, onReconnecting }
  function createGameSocket(handlers) {
    var socket = null;
    var reconnectTimer = null;
    var reconnectDelay = 1000;
    var manuallyClosed = false;

    function connect() {
      socket = new WebSocket(buildWsUrl());

      socket.onopen = function () {
        reconnectDelay = 1000;
        if (handlers.onOpen) handlers.onOpen();
      };

      socket.onmessage = function (event) {
        var data;
        try {
          data = JSON.parse(event.data);
        } catch (e) {
          return; // 형식이 이상한 메시지는 조용히 무시
        }
        if (handlers.onMessage) handlers.onMessage(data);
      };

      socket.onclose = function () {
        if (handlers.onClose) handlers.onClose();
        if (!manuallyClosed) {
          if (handlers.onReconnecting) handlers.onReconnecting();
          reconnectTimer = setTimeout(connect, reconnectDelay);
          reconnectDelay = Math.min(reconnectDelay * 1.5, 8000); // 재시도 간격을 점점 늘림
        }
      };

      socket.onerror = function () {
        socket.close();
      };
    }

    connect();

    return {
      send: function (obj) {
        if (socket && socket.readyState === WebSocket.OPEN) {
          socket.send(JSON.stringify(obj));
          return true;
        }
        return false;
      },
      close: function () {
        manuallyClosed = true;
        if (reconnectTimer) clearTimeout(reconnectTimer);
        if (socket) socket.close();
      }
    };
  }

  var DIRECTION_NAMES = ['위', '오른쪽', '아래', '왼쪽']; // 위, 오른쪽, 아래, 왼쪽
  var DIRECTION_ARROWS = ['↑', '→', '↓', '←'];

  return {
    isValidChannelInput: isValidChannelInput,
    saveLastChannel: saveLastChannel,
    loadLastChannel: loadLastChannel,
    saveToken: saveToken,
    loadToken: loadToken,
    getQueryParam: getQueryParam,
    createGameSocket: createGameSocket,
    DIRECTION_NAMES: DIRECTION_NAMES,
    DIRECTION_ARROWS: DIRECTION_ARROWS
  };
})();
