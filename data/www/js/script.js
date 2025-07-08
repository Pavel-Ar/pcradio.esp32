let isPlaying = false;
let currentChannel = -1;
let volume = 50;
let isMuted = false;
function savePlayerState() {
    const playlistState = typeof getPlaylistState === 'function' ? getPlaylistState() : { currentPlaylistChannel: -1, currentPage: 0 };
    const state = {
        volume: volume,
        isMuted: isMuted,
        isPlaying: isPlaying,
        currentChannel: currentChannel,
        currentPlaylistChannel: playlistState.currentPlaylistChannel,
        currentPage: playlistState.currentPage,
        stationName: document.getElementById('frequency').textContent,
        stationSubName: document.getElementById('station-name').textContent
    };
    localStorage.setItem('playerState', JSON.stringify(state));
}
function loadPlayerState() {
    const savedStateJSON = localStorage.getItem('playerState');
    if (!savedStateJSON) return;

    const state = JSON.parse(savedStateJSON);

    volume = state.volume !== undefined ? state.volume : 50;
    isMuted = state.isMuted || false;
    isPlaying = state.isPlaying || false;
    currentChannel = state.currentChannel !== undefined ? state.currentChannel : -1;

    const volumeSlider = document.getElementById('volume-slider');
    if (volumeSlider) volumeSlider.value = volume;
    const volumeValue = document.getElementById('volume-value');
    if (volumeValue) volumeValue.textContent = volume;

    if (state.stationName && state.stationName !== 'Веб Радио') {
        document.getElementById('frequency').textContent = state.stationName;
        if (state.stationSubName) {
            document.getElementById('station-name').textContent = state.stationSubName;
        }
    }

    updateUI();
}
let connectionStatus = 'disconnected';
let signalStrength = 0;

document.addEventListener('DOMContentLoaded', function() {
    initializeRadio();
    updateTime();
    setInterval(updateTime, 1000);
});
function initializeRadio() {
    loadPlayerState();
    updateConnectionStatus('connecting');
    updateUI();

    setTimeout(() => {
        updateConnectionStatus('connected');
        updateSignalStrength(85);
    }, 2000);
}
function togglePlay() {
    if (isPlaying) {
        stopRadio();
    } else {
        startRadio();
    }
}

function startRadio() {
    isPlaying = true;
    savePlayerState();
    document.getElementById('loading').style.display = 'block';

    fetch('/api/player', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            channel: currentChannel,
            playlistChannel: getPlaylistState?.()?.currentPlaylistChannel || -1
        })
    })
    .then(response => response.json())
    .then(data => {
        console.log('Радио запущено:', data);
        setTimeout(() => {
            document.getElementById('loading').style.display = 'none';
        }, 1000);
    })
    .catch(error => {
        console.error('Ошибка запуска радио:', error);
        isPlaying = false;
        document.getElementById('loading').style.display = 'none';
    });

    updateUI();
}

function stopRadio() {
    isPlaying = false;
    document.getElementById('loading').style.display = 'none';

    document.getElementById('frequency').textContent = 'Веб Радио';
    document.getElementById('station-name').textContent = 'Выберите станцию';
    currentChannel = -1;
    if (typeof currentPlaylistChannel !== 'undefined') {
        currentPlaylistChannel = -1;
    }

    savePlayerState();

    fetch('/api/player', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: "stop"
    })
    .then(response => response.json())
    .then(data => {
        console.log('Радио остановлено:', data);
    })
    .catch(error => {
        console.error('Ошибка остановки радио:', error);
    });

    updateUI();

    if (typeof displayChannels === 'function') {
        displayChannels();
        displayTopChannels();
    }
}
function toggleMute() {
    isMuted = !isMuted;
    savePlayerState();

    fetch('/api/mute', {
        method: 'POST',
        headers: {
            'Content-Type': 'text/plain',
        },
        body: isMuted ? '1' : '0'
    })
    .catch(error => console.error('Ошибка управления звуком:', error));

    updateUI();
}

function setVolume(value) {
    volume = parseInt(value);
    document.getElementById('volume-value').textContent = volume;
    savePlayerState();

    fetch('/api/volume', {
        method: 'POST',
        headers: {
            'Content-Type': 'text/plain',
        },
        body: String(volume)
    })
    .catch(error => console.error('Ошибка установки громкости:', error));
}
function setPreset(presetNumber, stationId, stationName) {
    currentChannel = presetNumber;

    if (typeof currentPlaylistChannel !== 'undefined') {
        currentPlaylistChannel = -1;
    }

    document.getElementById('frequency').textContent = stationName;
    document.getElementById('station-name').textContent = 'Предустановка ' + (presetNumber + 1);

    savePlayerState();

    updateUI();

    if (typeof displayChannels === 'function') {
        displayChannels();
    }

    fetch('/api/player', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            preset: presetNumber,
            stationId: stationId,
            stationName: stationName
        })
    })
    .catch(error => console.error('Ошибка выбора предустановки:', error));
}
function setEqPreset(preset) {
    if (preset < 0 || preset > 9) {
        console.error(`Ошибка: недопустимый номер пресета эквалайзера: ${preset}. Допустимы значения от 0 до 9.`);
        return;
    }

    fetch('/api/eq', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: String(preset)
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.message || 'Ошибка сервера'); });
        }
        return response.json();
    })
    .then(data => {
        if (data.status === 'success') {
            console.log(`Эквалайзер установлен: ${data.preset_name}`);
        } else {
            console.error(`Ошибка установки эквалайзера: ${data.message}`);
        }
    })
    .catch(error => {
        console.error('Критическая ошибка при установке эквалайзера:', error.message);
    });
}
function updateUI() {
    const muteBtn = document.getElementById('mute-btn');
    if (isMuted) {
        muteBtn.innerHTML = '🔇 Выкл';
        muteBtn.className = 'btn btn-secondary';
    } else {
        muteBtn.innerHTML = '🔊 Звук';
        muteBtn.className = 'btn btn-warning';
    }

     updateActivePresets();
}

function updateActivePresets() {
    const presetBtns = document.querySelectorAll('.preset-btn');
    presetBtns.forEach((btn, index) => {
        if (index === currentChannel && (typeof currentPlaylistChannel === 'undefined' || currentPlaylistChannel === -1)) {
            btn.classList.add('active');
        } else {
            btn.classList.remove('active');
        }
    });
}
function updateConnectionStatus(status) {
    connectionStatus = status;
    const statusEl = document.getElementById('connection-status');

    switch(status) {
        case 'connected':
            statusEl.textContent = '🟢 Подключено';
            statusEl.style.color = '#27ae60';
            break;
        case 'connecting':
            statusEl.textContent = '🟡 Подключение...';
            statusEl.style.color = '#f39c12';
            break;
        case 'disconnected':
            statusEl.textContent = '🔴 Отключено';
            statusEl.style.color = '#e74c3c';
            break;
    }
}

function updateSignalStrength(strength) {
    signalStrength = strength;
    const signalEl = document.getElementById('signal-strength');

    if (strength >= 80) {
        signalEl.textContent = `📶 Отличный (${strength}%)`;
        signalEl.style.color = '#27ae60';
    } else if (strength >= 60) {
        signalEl.textContent = `📶 Хороший (${strength}%)`;
        signalEl.style.color = '#f39c12';
    } else if (strength >= 40) {
        signalEl.textContent = `📶 Слабый (${strength}%)`;
        signalEl.style.color = '#e67e22';
    } else {
        signalEl.textContent = `📶 Очень слабый (${strength}%)`;
        signalEl.style.color = '#e74c3c';
    }
}

function updateTime() {
    const now = new Date();
    const timeString = now.toLocaleTimeString('ru-RU', {
        hour: '2-digit',
        minute: '2-digit'
    });
    document.getElementById('current-time').textContent = timeString;
}
function updateStatus() {
    return;
}
function saveCollapseState() {
    const groups = document.querySelectorAll('.control-group');
    const state = Array.from(groups).map(g => g.classList.contains('collapsed'));
    localStorage.setItem('controlGroupState', JSON.stringify(state));
}
function loadCollapseState() {
    const stored = localStorage.getItem('controlGroupState');
    const groups = document.querySelectorAll('.control-group');
    if (!stored) {
        groups.forEach(g => {
            g.classList.add('collapsed');
            const content = g.querySelector('.control-content');
            if (content) content.style.display = 'none';
        });
        return;
    }
    const state = JSON.parse(stored);
    groups.forEach((g, i) => {
        const collapsed = state[i];
        const content = g.querySelector('.control-content');
        if (collapsed) {
            g.classList.add('collapsed');
            if (content) content.style.display = 'none';
        } else {
            g.classList.remove('collapsed');
            if (content) content.style.display = 'block';
        }
    });
}
function toggleControlGroup(element) {
    const controlGroup = element.parentElement;
    const content = controlGroup.querySelector('.control-content');

    controlGroup.classList.toggle('collapsed');

    if (controlGroup.classList.contains('collapsed')) {
        content.style.display = 'none';
    } else {
        content.style.display = 'block';
    }
    saveCollapseState();
}

document.addEventListener('DOMContentLoaded', function() {
    loadCollapseState();
});
