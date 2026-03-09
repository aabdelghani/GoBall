/**
 * GoBall Fleet Monitor — Alpine.js application
 */
document.addEventListener('alpine:init', () => {
    Alpine.data('dashboard', () => ({
        // Navigation
        tab: 'home', // home, devices, settings
        panel: null,  // null, 'alerts', 'logs'

        // Data
        devices: [],
        alerts: [],
        feed: [],
        errorLogs: [],
        stats: null,

        // Activity feed resize
        feedHeight: 160,
        feedDragging: false,
        feedDragStartY: 0,
        feedDragStartH: 0,

        // Devices tab state
        filter: 'all',
        search: '',
        sortBy: 'status',
        selectedDevice: null,
        selectedDetail: null,
        selectedEvents: [],

        // Zoom
        zoomLevels: [1, 1.2, 1.5, 1.7],
        zoomIndex: 0,

        // WebSocket
        wsConnected: false,
        ws: null,

        // Charts
        _charts: {},
        _map: null,
        _markers: null,

        init() {
            this.connectWebSocket();
            this.loadStats();
            setInterval(() => { if (this.tab === 'home') this.loadStats(); }, 10000);

            // Feed resize drag handlers
            window.addEventListener('mousemove', (e) => {
                if (!this.feedDragging) return;
                const delta = this.feedDragStartY - e.clientY;
                this.feedHeight = Math.max(60, Math.min(600, this.feedDragStartH + delta));
            });
            window.addEventListener('mouseup', () => { this.feedDragging = false; });
        },

        // --- Stats ---
        async loadStats() {
            try {
                const res = await fetch('/api/stats');
                this.stats = await res.json();
                this.$nextTick(() => this.renderCharts());
            } catch (e) { console.error('Failed to load stats', e); }
        },

        renderCharts() {
            if (!this.stats || this.tab !== 'home') return;
            this.renderDonut('chartGameModes', 'Game Modes', this.stats.game_modes,
                ['#64748b', '#3b82f6', '#22c55e', '#a855f7']);
            this.renderBar('chartTemps', 'CPU Temperature Distribution', this.stats.temp_buckets,
                ['#22c55e', '#22c55e', '#eab308', '#eab308', '#f97316', '#ef4444']);
            this.renderMap();
        },

        renderDonut(canvasId, label, dataObj, colors) {
            const el = document.getElementById(canvasId);
            if (!el) return;
            if (this._charts[canvasId]) this._charts[canvasId].destroy();
            this._charts[canvasId] = new Chart(el, {
                type: 'doughnut',
                data: {
                    labels: Object.keys(dataObj).map(k => k.charAt(0).toUpperCase() + k.slice(1)),
                    datasets: [{
                        data: Object.values(dataObj),
                        backgroundColor: colors,
                        borderWidth: 0,
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: { position: 'bottom', labels: { color: '#94a3b8', font: { size: 11 } } },
                    },
                    cutout: '65%',
                },
            });
        },

        renderBar(canvasId, label, dataObj, colors) {
            const el = document.getElementById(canvasId);
            if (!el) return;
            if (this._charts[canvasId]) this._charts[canvasId].destroy();
            this._charts[canvasId] = new Chart(el, {
                type: 'bar',
                data: {
                    labels: Object.keys(dataObj),
                    datasets: [{
                        data: Object.values(dataObj),
                        backgroundColor: colors,
                        borderRadius: 4,
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: { legend: { display: false } },
                    scales: {
                        x: { ticks: { color: '#94a3b8' }, grid: { display: false } },
                        y: { ticks: { color: '#94a3b8' }, grid: { color: '#1e293b' }, beginAtZero: true },
                    },
                },
            });
        },

        renderMap() {
            if (!this.stats || !this.stats.locations.length) return;
            const el = document.getElementById('deviceMap');
            if (!el) return;

            if (!this._map) {
                this._map = L.map(el, { zoomControl: true, attributionControl: false }).setView([20, 0], 2);
                L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
                    maxZoom: 18,
                }).addTo(this._map);
                this._markers = L.layerGroup().addTo(this._map);
            }

            this._markers.clearLayers();
            for (const loc of this.stats.locations) {
                const color = loc.status === 'online' ? '#22c55e' : '#ef4444';
                const icon = L.divIcon({
                    html: `<div style="width:12px;height:12px;border-radius:50%;background:${color};border:2px solid ${color}44;box-shadow:0 0 6px ${color}88;"></div>`,
                    className: '',
                    iconSize: [12, 12],
                    iconAnchor: [6, 6],
                });
                const marker = L.marker([loc.lat, loc.lng], { icon });
                marker.bindPopup(`<div style="color:#000;font-size:12px;"><b>${loc.hostname || loc.serial}</b><br>Status: ${loc.status}<br>Game: ${loc.game_mode}</div>`);
                this._markers.addLayer(marker);
            }

            // Force map to redraw (fixes grey tiles on tab switch)
            setTimeout(() => this._map.invalidateSize(), 100);
        },

        // --- Panels ---
        togglePanel(name) {
            if (this.panel === name) {
                this.panel = null;
            } else {
                this.panel = name;
                if (name === 'logs') this.loadErrorLogs();
            }
        },

        async loadErrorLogs() {
            try {
                const res = await fetch('/api/logs?limit=100');
                this.errorLogs = await res.json();
            } catch (e) { console.error('Failed to load logs', e); }
        },

        // --- WebSocket ---
        connectWebSocket() {
            const proto = location.protocol === 'https:' ? 'wss' : 'ws';
            this.ws = new WebSocket(`${proto}://${location.host}/ws`);
            this.ws.onopen = () => { this.wsConnected = true; };
            this.ws.onclose = () => {
                this.wsConnected = false;
                setTimeout(() => this.connectWebSocket(), 3000);
            };
            this.ws.onmessage = (e) => {
                const msg = JSON.parse(e.data);
                if (msg.type === 'init') {
                    this.devices = msg.devices;
                    this.alerts = msg.alerts;
                } else if (msg.type === 'device_update') {
                    this.handleDeviceUpdate(msg);
                }
            };
        },

        handleDeviceUpdate(msg) {
            const idx = this.devices.findIndex(d => d.serial === msg.device.serial);
            if (idx >= 0) {
                this.devices[idx] = msg.device;
            } else {
                this.devices.push(msg.device);
            }

            // Activity feed
            let feedMsg = '';
            if (msg.category === 'status') {
                feedMsg = `${msg.device.serial} went ${msg.payload}`;
            } else if (msg.category === 'game/event') {
                const p = msg.payload;
                feedMsg = `${msg.device.serial} — ${p.event}: Player ${p.player} +${p.value}`;
            } else if (msg.category === 'errors') {
                feedMsg = `${msg.device.serial} — ERROR: ${msg.payload.message || msg.payload}`;
            } else if (msg.category === 'game/state') {
                feedMsg = `${msg.device.serial} — Game: ${msg.payload.mode}, ${msg.payload.player_count}P`;
            }
            if (feedMsg) {
                this.feed.unshift({ ts: new Date().toLocaleTimeString(), msg: feedMsg, category: msg.category });
                if (this.feed.length > 200) this.feed.length = 200;
            }

            if (msg.alerts) this.alerts = msg.alerts;

            // Live error logs
            if (msg.category === 'errors' && this.panel === 'logs') {
                this.errorLogs.unshift({
                    serial: msg.device.serial,
                    message: msg.payload.message || JSON.stringify(msg.payload),
                    ts: msg.payload.ts || Date.now() / 1000,
                });
                if (this.errorLogs.length > 500) this.errorLogs.length = 500;
            }
        },

        // --- Devices tab ---
        get filteredDevices() {
            let list = this.devices;
            if (this.filter === 'online') list = list.filter(d => d.status === 'online');
            else if (this.filter === 'offline') list = list.filter(d => d.status === 'offline');
            else if (this.filter === 'errors') list = list.filter(d => d.error_count > 0);
            else if (this.filter === 'playing') list = list.filter(d => d.game_mode === 'playing');

            if (this.search) {
                const q = this.search.toLowerCase();
                list = list.filter(d =>
                    d.serial.toLowerCase().includes(q) ||
                    (d.hostname && d.hostname.toLowerCase().includes(q)) ||
                    (d.ip && d.ip.includes(q))
                );
            }
            return [...list].sort((a, b) => {
                if (this.sortBy === 'status') {
                    if (a.status !== b.status) return a.status === 'offline' ? -1 : 1;
                    return a.serial.localeCompare(b.serial);
                }
                if (this.sortBy === 'temp') return (b.cpu_temp || 0) - (a.cpu_temp || 0);
                if (this.sortBy === 'serial') return a.serial.localeCompare(b.serial);
                if (this.sortBy === 'game') return a.game_mode.localeCompare(b.game_mode);
                return 0;
            });
        },

        // Zoom controls
        get zoomLabel() {
            return ['1x', '1.2x', '1.5x', '1.7x'][this.zoomIndex];
        },
        applyZoom() {
            const base = 16 * this.zoomLevels[this.zoomIndex];
            document.documentElement.style.fontSize = base + 'px';
        },
        zoomIn() {
            if (this.zoomIndex < this.zoomLevels.length - 1) {
                this.zoomIndex++;
                this.applyZoom();
            }
        },
        zoomOut() {
            if (this.zoomIndex > 0) {
                this.zoomIndex--;
                this.applyZoom();
            }
        },

        get onlineCount() { return this.devices.filter(d => d.status === 'online').length; },
        get offlineCount() { return this.devices.filter(d => d.status === 'offline').length; },
        get alertCount() { return this.alerts.length; },

        async selectDevice(serial) {
            this.selectedDevice = serial;
            try {
                const [detailRes, eventsRes] = await Promise.all([
                    fetch(`/api/devices/${serial}`),
                    fetch(`/api/devices/${serial}/events?limit=30`),
                ]);
                this.selectedDetail = await detailRes.json();
                this.selectedEvents = await eventsRes.json();
            } catch (e) { console.error('Failed to fetch device detail', e); }
        },

        closeDetail() {
            this.selectedDevice = null;
            this.selectedDetail = null;
            this.selectedEvents = [];
        },

        // --- Helpers ---
        statusDot(s) { return s === 'online' ? 'text-green-400' : 'text-red-400'; },
        cardBorder(d) {
            if (d.status === 'offline') return 'border-red-500/50';
            if (d.error_count > 0) return 'border-yellow-500/50';
            if (d.game_mode === 'playing') return 'border-blue-500/50';
            return 'border-gray-700';
        },
        tempColor(t) { return t > 80 ? 'text-red-400' : t > 65 ? 'text-yellow-400' : 'text-green-400'; },
        feedColor(c) {
            if (c === 'errors') return 'text-red-400';
            if (c === 'status') return 'text-yellow-300';
            if (c && c.startsWith('game')) return 'text-blue-300';
            return 'text-gray-300';
        },
        formatAge(ts) {
            if (!ts) return '';
            const s = Math.floor(Date.now() / 1000 - ts);
            if (s < 60) return s + 's ago';
            if (s < 3600) return Math.floor(s / 60) + 'm ago';
            if (s < 86400) return Math.floor(s / 3600) + 'h ago';
            return Math.floor(s / 86400) + 'd ago';
        },
        formatUptime(s) {
            if (!s) return '—';
            const d = Math.floor(s / 86400);
            const h = Math.floor((s % 86400) / 3600);
            if (d > 0) return d + 'd ' + h + 'h';
            const m = Math.floor((s % 3600) / 60);
            return h + 'h ' + m + 'm';
        },
        gameModeLabel(m) { return ({ idle: 'Idle', setup: 'Setup', playing: 'Playing', finished: 'Done' })[m] || m; },
        gameModeIcon(m) { return ({ idle: '⏸', setup: '⚙', playing: '▶', finished: '✓' })[m] || ''; },

        switchTab(t) {
            this.tab = t;
            if (t === 'home') {
                this.loadStats();
                this.$nextTick(() => this.renderCharts());
            }
        },
    }));
});
