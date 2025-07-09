let playlist = [];
let filteredPlaylist = [];
let currentPage = 0;
const channelsPerPage = 50;
let currentPlaylistChannel = -1;
function getPlaylistState() {
    return {
        currentPlaylistChannel: currentPlaylistChannel,
        currentPage: currentPage,
    };
}

async function loadPlaylist() {
     const loadingEl = document.getElementById('loading-channels');
     const channelsListEl = document.getElementById('channels-list');
     const loadButton = document.getElementById('load-playlist-btn');

    fetch('/api/playlist', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: 'update'
    }).catch(error => console.error('Ошибка при отправке команды обновления плейлиста:', error));

    const oldPlaylist = playlist.length > 0 ? [...playlist] : JSON.parse(localStorage.getItem('playlistCache') || '[]');

    try {
         if (loadingEl) loadingEl.style.display = 'block';
         if (loadButton) loadButton.disabled = true;
         channelsListEl.innerHTML = '<div class="no-channels">Загрузка...</div>';

         const githubUrl = 'https://raw.githubusercontent.com/RootShell-coder/pcradio.m3u/refs/heads/master/pcradio.m3u';

         const content = await fetchWithFallback(githubUrl);
         if (!content || !content.trim()) throw new Error("Получено пустое содержимое плейлиста");

         playlist = parseM3U(content);

         if (oldPlaylist.length > 0) {
             playlist.forEach(ch => {
                 const old = oldPlaylist.find(o => o.number === ch.number);
                 if (old) {
                     ch.favorite = old.favorite || false;
                     ch.playCount = old.playCount || 0;
                 }
             });
         }

         filteredPlaylist = [...playlist];
         currentPage = 0;

         localStorage.setItem('playlistCache', JSON.stringify(playlist));
         localStorage.setItem('playlistCacheTime', Date.now().toString());

         document.getElementById('channels-count').textContent = `Каналов: ${playlist.length}`;

         displayChannels();
         displayTopChannels();
         renderFavorites();
         updatePagination();

         const controls = document.querySelector('.playlist-controls');
         if (controls) controls.style.display = '';
         const searchInput = document.getElementById('channel-search');
         if (searchInput) searchInput.disabled = false;

    } catch (error) {
        console.error('Ошибка загрузки плейлиста:', error);
        if (playlist.length === 0) {
            channelsListEl.innerHTML = '<div class="no-channels">Не удалось загрузить плейлист.<br>Попробуйте еще раз.</div>';
            document.getElementById('channels-count').textContent = 'Ошибка';
        }
    } finally {
        if (loadingEl) loadingEl.style.display = 'none';
        if (loadButton) loadButton.disabled = false;
    }
}

async function fetchWithFallback(url) {
    const endpoints = [
        url,
        'https://api.allorigins.win/get?url=' + encodeURIComponent(url),
        'https://api.allorigins.win/raw?url=' + encodeURIComponent(url),
        'https://corsproxy.io/?' + encodeURIComponent(url)
    ];
    for (const ep of endpoints) {
        try {
            const resp = await fetch(ep);
            if (!resp.ok) throw new Error();
            const text = ep.includes('allorigins.win/get') ? (await resp.json()).contents : await resp.text();
            if (text) return text;
        } catch (e) { continue; }
    }
    throw new Error('Не удалось загрузить плейлист');
}

function parseM3U(content) {
    if (!content || typeof content !== 'string') {
        console.error('Некорректное содержимое плейлиста');
        return [];
    }

    const lines = content.split(/\r?\n/);
    const channels = [];
    let channelNumber = 1;

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();

        if (line.startsWith('#EXTINF:')) {
            const commaIndex = line.indexOf(',');
            if (commaIndex !== -1) {
                let channelName = line.substring(commaIndex + 1).trim();

                channelName = channelName
                    .replace(/&amp;/g, '&')
                    .replace(/&lt;/g, '<')
                    .replace(/&gt;/g, '>')
                    .replace(/&quot;/g, '"')
                    .replace(/&#39;/g, "'")
                    .replace(/\s+/g, ' ')
                    .trim();

                if (channelName && channelName.length > 0) {
                    channels.push({
                        number: channelNumber,
                        name: channelName,
                        searchText: channelName.toLowerCase(),
                        favorite: false,
                        playCount: 0
                    });
                    channelNumber++;
                }
            }
        }
    }

    console.log(`Парсинг завершен: найдено ${channels.length} каналов`);
    return channels;
}

function searchChannels(query) {
    const searchQuery = query.toLowerCase().trim();

    if (searchQuery === '') {
        if (playlist.some(c => c.searchScore)) {
            playlist.forEach(channel => delete channel.searchScore);
        }
        filteredPlaylist = [...playlist];
    } else {
        const searchWords = searchQuery.split(/\s+/).filter(word => word.length > 0);

        filteredPlaylist = playlist.filter(channel => {
            const channelText = channel.searchText;

            if (searchWords.length === 1) {
                const word = searchWords[0];

                if (channelText.startsWith(word)) {
                    channel.searchScore = 100;
                    return true;
                }

                if (channelText.split(/\s+/).some(channelWord => channelWord === word)) {
                    channel.searchScore = 90;
                    return true;
                }

                if (channelText.includes(word)) {
                    channel.searchScore = 70;
                    return true;
                }

                if (fuzzyMatch(channelText, word)) {
                    channel.searchScore = 50;
                    return true;
                }
            } else {
                const allWordsMatch = searchWords.every(word =>
                    channelText.includes(word) || fuzzyMatch(channelText, word)
                );

                if (allWordsMatch) {
                    const exactMatches = searchWords.filter(word => channelText.includes(word)).length;
                    channel.searchScore = (exactMatches / searchWords.length) * 80;
                    return true;
                }
            }

            return false;
        });

        filteredPlaylist.sort((a, b) => (b.searchScore || 0) - (a.searchScore || 0));
    }

    currentPage = 0;
    displayChannels();
    updatePagination();

    updateSearchResults(searchQuery);
}

function fuzzyMatch(text, pattern) {
    if (pattern.length === 0) return true;
    if (text.length === 0) return false;

    let patternIndex = 0;
    let textIndex = 0;

    while (textIndex < text.length && patternIndex < pattern.length) {
        if (text[textIndex] === pattern[patternIndex]) {
            patternIndex++;
        }
        textIndex++;
    }

    return patternIndex === pattern.length;
}

function updateSearchResults(query) {
    const countEl = document.getElementById('channels-count');
    if (query && query.trim() !== '') {
        countEl.textContent = `Найдено: ${filteredPlaylist.length} из ${playlist.length}`;
    } else {
        countEl.textContent = `Каналов: ${playlist.length}`;
    }
}

function displayChannels() {
    const q = (document.getElementById('channel-search')||{}).value.toLowerCase().trim();
    const html = filteredPlaylist.slice(currentPage*channelsPerPage, (currentPage+1)*channelsPerPage)
        .map(ch => {
            const active = ch.number===currentPlaylistChannel?' active':'';
            const name = q?highlightSearchTerms(ch.name,q):escapeHtml(ch.name);
            const score = q && ch.searchScore?`<span class="search-score">${Math.round(ch.searchScore)}</span>`:'';
            const count= ch.playCount?`<span class="play-count" title="Очистить" onclick="resetPlayCount(${ch.number});event.stopPropagation();">${ch.playCount}</span>`:'';
            const fav = `<img class="favorite-icon${ch.favorite?' active':''}" src="images/favorites.png" onclick="toggleFavorite(${ch.number});event.stopPropagation();" title="${ch.favorite?'Удалить из избранного':'Добавить в избранное'}"/>`;
            return `<div class="channel-item${active}" onclick='selectPlaylistChannel(${ch.number})'><div class="channel-info"><span class="channel-number">${ch.number}.</span><span class="channel-name">${name}</span>${score}${count}${fav}</div></div>`;
        }).join('');
    document.getElementById('channels-list').innerHTML = html || `<div class="no-channels">${q?'Каналы не найдены':'Нет каналов для отображения'}</div>`;
    renderFavorites(); updatePagination();
}

function displayTopChannels() {
    const container = document.getElementById('top-channels-list');
    if (!container) return;
    const group = container.parentElement.parentElement;
    const top = playlist
        .filter(ch => ch.playCount > 0)
        .sort((a, b) => b.playCount - a.playCount)
        .slice(0, 30);
    if (top.length === 0) {
        group.style.display = 'none';
        return;
    }
    group.style.display = '';
    const html = top.map(ch => {
        const active = ch.number === currentPlaylistChannel ? ' active' : '';
        const name = escapeHtml(ch.name);
        const count = `<span class="play-count" title="Очистить" onclick="resetPlayCount(${ch.number});event.stopPropagation();">${ch.playCount}</span>`;
        const fav = `<img class="favorite-icon${ch.favorite ? ' active' : ''}" src="images/favorites.png" onclick="toggleFavorite(${ch.number});event.stopPropagation();" title="${ch.favorite?'Удалить из избранного':'Добавить в избранное'}"/>`;
        return `<div class="channel-item${active}" onclick='selectPlaylistChannel(${ch.number})'><div class="channel-info"><span class="channel-number">${ch.number}.</span><span class="channel-name">${name}</span>${count}${fav}</div></div>`;
    }).join('');
    container.innerHTML = html;
}

function highlightSearchTerms(text, searchQuery) {
    if (!searchQuery || !text) return escapeHtml(text);

    const searchWords = searchQuery.split(/\s+/).filter(word => word.length > 0);
    if (searchWords.length === 0) {
        return escapeHtml(text);
    }

    const pattern = searchWords.map(word => escapeRegex(word)).join('|');
    const regex = new RegExp(`(${pattern})`, 'gi');

    return text
        .split(regex)
        .map((part, index) => {
            if (index % 2 === 1) {
                return `<mark class="search-highlight">${escapeHtml(part)}</mark>`;
            } else {
                return escapeHtml(part);
            }
        })
        .join('');
}

function escapeRegex(string) {
    return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function escapeHtml(text) {
    if (!text || typeof text !== 'string') {
        return '';
    }

    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function selectPlaylistChannel(channelNumber) {
    currentPlaylistChannel = channelNumber;
    const playCh = playlist.find(c => c.number === channelNumber);
    if (playCh) {
        playCh.playCount = (playCh.playCount || 0) + 1;
        localStorage.setItem('playlistCache', JSON.stringify(playlist));
    }
    const channel = playlist.find(ch => ch.number === channelNumber);
    const originalChannelName = channel ? channel.name : '';

    if (typeof currentChannel !== 'undefined') {
        currentChannel = -1;
    }

    document.getElementById('frequency').textContent = originalChannelName;
    document.getElementById('station-name').textContent = `Канал ${channelNumber}`;

    if (typeof savePlayerState === 'function') {
        savePlayerState();
    }

    if (typeof updateUI === 'function') {
        updateUI();
    }

    displayChannels();
    displayTopChannels();

    fetch('/api/channel', {
        method: 'POST',
        headers: {
            'Content-Type': 'text/plain',
        },
        body: String(channelNumber)
    }).catch(error => console.error('Ошибка выбора канала:', error));
}

function nextPage() {
    const totalPages = Math.ceil(filteredPlaylist.length / channelsPerPage);
    if (currentPage < totalPages - 1) {
        currentPage++;
        localStorage.setItem('playlistPage', currentPage);
        if (typeof savePlayerState === 'function') savePlayerState();
        displayChannels();
        updatePagination();
    }
}

function previousPage() {
    if (currentPage > 0) {
        currentPage--;
        localStorage.setItem('playlistPage', currentPage);
        if (typeof savePlayerState === 'function') savePlayerState();
        displayChannels();
        updatePagination();
    }
}

function firstPage() {
    if (currentPage !== 0) {
        currentPage = 0;
        localStorage.setItem('playlistPage', currentPage);
        if (typeof savePlayerState === 'function') savePlayerState();
        displayChannels();
        updatePagination();
    }
}

function lastPage() {
    const totalPages = Math.ceil(filteredPlaylist.length / channelsPerPage);
    if (currentPage < totalPages - 1) {
        currentPage = totalPages - 1;
        localStorage.setItem('playlistPage', currentPage);
        if (typeof savePlayerState === 'function') savePlayerState();
        displayChannels();
        updatePagination();
    }
}

function updatePagination() {
    const totalPages = Math.ceil(filteredPlaylist.length / channelsPerPage);
    const pageInfo = document.getElementById('page-info');
    const prevBtn = document.getElementById('prev-btn');
    const nextBtn = document.getElementById('next-btn');
    const firstBtn = document.getElementById('first-btn');
    const lastBtn = document.getElementById('last-btn');

    pageInfo.textContent = `Страница ${currentPage + 1} из ${totalPages}`;

    prevBtn.disabled = currentPage === 0;
    nextBtn.disabled = currentPage >= totalPages - 1;
    firstBtn.disabled = currentPage === 0;
    lastBtn.disabled = currentPage >= totalPages - 1;

    localStorage.setItem('playlistPage', currentPage.toString());
}

let searchTimeout = null;

function debouncedSearch(query) {
    const searchStatus = document.getElementById('search-status');
    if (searchStatus && query.trim() !== '') {
        searchStatus.style.display = 'inline';
    }

    if (searchTimeout) {
        clearTimeout(searchTimeout);
    }

    searchTimeout = setTimeout(() => {
        searchChannels(query);
        if (searchStatus) {
            searchStatus.style.display = 'none';
        }
    }, 300);
}

function initializeSearch() {
    const searchInput = document.getElementById('channel-search');
    if (searchInput) {
        searchInput.oninput = function() {
            debouncedSearch(this.value);
        };

        searchInput.onkeydown = function(event) {
            if (event.key === 'Enter') {
                if (searchTimeout) {
                    clearTimeout(searchTimeout);
                }
                searchChannels(this.value);
            }
        };

        searchInput.onkeyup = function(event) {
            if (event.key === 'Escape') {
                this.value = '';
                searchChannels('');
            }
        };
    }
}

function clearSearch() {
    const searchInput = document.getElementById('channel-search');
    if (searchInput) {
        searchInput.value = '';
        searchChannels('');
        searchInput.focus();
    }
}

function renderFavorites() {
     const favContainer = document.getElementById('favorites-list');
     if (!favContainer) return;
     favContainer.innerHTML = '';
     const favChannels = playlist.filter(ch => ch.favorite).sort((a, b) => a.number - b.number);
     if (favChannels.length === 0) {
         favContainer.innerHTML = '<div class="no-favorites">Нет избранных</div>';
         return;
     }
     const html = favChannels.map(ch => {
         const safeName = escapeHtml(ch.name).replace(/'/g, '&#39;');
         return `
         <button class="preset-btn favorite-item" onclick="selectPlaylistChannel(${ch.number});">
             ${safeName}<br><small>${ch.number}</small>
         </button>`;
     }).join('');
     favContainer.innerHTML = html;
}

function initializePlaylist() {
    const cachedPlaylist = localStorage.getItem('playlistCache');
    const channelsListEl = document.getElementById('channels-list');
    const controls = document.querySelector('.playlist-controls');
    const searchInput = document.getElementById('channel-search');

    if (cachedPlaylist) {
        console.log("Загрузка плейлиста из кеша.");
        playlist = JSON.parse(cachedPlaylist);
        filteredPlaylist = [...playlist];

        const playerState = JSON.parse(localStorage.getItem('playerState') || '{}');

        const pagesCount = Math.ceil(filteredPlaylist.length / channelsPerPage);
        currentPage = (playerState.currentPage !== undefined && playerState.currentPage >= 0 && playerState.currentPage < pagesCount) ? playerState.currentPage : 0;
        currentPlaylistChannel = playerState.currentPlaylistChannel !== undefined ? playerState.currentPlaylistChannel : -1;

        document.getElementById('channels-count').textContent = `Каналов: ${playlist.length}`;
        if (controls) controls.style.display = '';
        if (searchInput) searchInput.disabled = false;

        displayChannels();
        displayTopChannels();
        renderFavorites();
        updatePagination();
    } else {
        console.log("Кеш плейлиста пуст. Ожидание загрузки пользователем.");
        channelsListEl.innerHTML = '<div class="no-channels">Плейлист не загружен.<br>Нажмите "Загрузить/Обновить".</div>';
        document.getElementById('channels-count').textContent = 'Каналов: 0';
        if (controls) {
            controls.style.display = '';
            const playlistGroup = controls.closest('.control-group');
            if (playlistGroup && playlistGroup.classList.contains('collapsed')) {
                playlistGroup.classList.remove('collapsed');
                const content = playlistGroup.querySelector('.control-content');
                if (content) content.style.display = 'block';
            }
        }
        if (searchInput) searchInput.disabled = true;
    }
}

document.addEventListener('DOMContentLoaded', function() {
    initializeSearch();
    initializePlaylist();

    const loadButton = document.getElementById('load-playlist-btn');
    if (loadButton) {
        loadButton.addEventListener('click', () => loadPlaylist());
    }
});

window.playlistFunctions = {
    searchChannels: searchChannels,
    debouncedSearch: debouncedSearch,
    initializeSearch: initializeSearch
};

function toggleFavorite(channelNumber) {
    const ch = playlist.find(c => c.number === channelNumber);
    if (!ch) return;
    ch.favorite = !ch.favorite;
    localStorage.setItem('playlistCache', JSON.stringify(playlist));
    displayChannels();
    renderFavorites();
    displayTopChannels();
}

function resetPlayCount(channelNumber) {
    const ch = playlist.find(c => c.number === channelNumber);
    if (!ch) return;
    ch.playCount = 0;
    localStorage.setItem('playlistCache', JSON.stringify(playlist));
    displayChannels();
    displayTopChannels();
}
