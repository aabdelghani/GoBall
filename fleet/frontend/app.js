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

        // Commands
        cmdLoading: false,
        cmdResult: null,
        _cmdWaitId: null,

        // Terminal
        terminalOpen: false,
        terminalSerial: null,
        _term: null,
        _termWs: null,
        _termResizeHandler: null,

        // Firmware update
        fwOpen: false,
        fwFile: null,
        fwUploading: false,
        fwProgress: 0,
        fwStage: '',
        fwResult: null,

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
                } else if (msg.type === 'device_removed') {
                    this.devices = this.devices.filter(d => d.serial !== msg.serial);
                    this.alerts = this.alerts.filter(a => a.serial !== msg.serial);
                    if (this.selectedDevice === msg.serial) this.closeDetail();
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

            // Command results
            if (msg.category === 'command/result') {
                const p = msg.payload;
                if (p.id === this._cmdWaitId) {
                    this.cmdResult = { status: p.status, message: p.message };
                    this.cmdLoading = false;
                }
            }

            // Activity feed
            let feedMsg = '';
            if (msg.category === 'status') {
                feedMsg = `${msg.device.serial} went ${msg.payload}`;
            } else if (msg.category === 'command/result') {
                const p = msg.payload;
                feedMsg = `${msg.device.serial} — Command ${p.action}: ${p.status}`;
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
            this.cmdResult = null;
            this.cmdLoading = false;
        },

        async sendCommand(serial, action) {
            this.cmdLoading = true;
            this.cmdResult = null;
            try {
                const res = await fetch(`/api/devices/${serial}/command`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ action }),
                });
                const data = await res.json();
                if (data.error) {
                    this.cmdResult = { status: 'error', message: data.error };
                    this.cmdLoading = false;
                    return;
                }
                this._cmdWaitId = data.command_id;
                setTimeout(() => {
                    if (this.cmdLoading && this._cmdWaitId === data.command_id) {
                        this.cmdResult = { status: 'error', message: 'Timed out waiting for response' };
                        this.cmdLoading = false;
                    }
                }, 15000);
            } catch (e) {
                this.cmdResult = { status: 'error', message: 'Request failed: ' + e.message };
                this.cmdLoading = false;
            }
        },

        openTerminal(serial) {
            this.terminalSerial = serial;
            this.terminalOpen = true;
            this.$nextTick(() => this._initTerminal(serial));
        },

        _initTerminal(serial) {
            const container = document.getElementById('terminal-container');
            if (!container) return;

            if (this._term) { this._term.dispose(); this._term = null; }
            if (this._termWs) { this._termWs.close(); this._termWs = null; }
            container.innerHTML = '';

            const term = new Terminal({
                cursorBlink: true,
                fontSize: 14,
                fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
                theme: {
                    background: '#0f172a',
                    foreground: '#e2e8f0',
                    cursor: '#22c55e',
                },
            });
            const fitAddon = new FitAddon.FitAddon();
            term.loadAddon(fitAddon);
            term.loadAddon(new WebLinksAddon.WebLinksAddon());
            term.open(container);
            fitAddon.fit();
            this._term = term;

            const proto = location.protocol === 'https:' ? 'wss' : 'ws';
            const ws = new WebSocket(`${proto}://${location.host}/ws/terminal/${serial}`);
            this._termWs = ws;

            ws.onopen = () => {
                ws.send(JSON.stringify({ type: 'resize', cols: term.cols, rows: term.rows }));
                term.writeln('\x1b[32mConnecting to ' + serial + '...\x1b[0m');
            };

            ws.onmessage = (e) => {
                const msg = JSON.parse(e.data);
                if (msg.type === 'output') {
                    term.write(msg.data);
                } else if (msg.type === 'error') {
                    term.writeln('\x1b[31mError: ' + msg.message + '\x1b[0m');
                }
            };

            ws.onclose = () => {
                term.writeln('\r\n\x1b[31mConnection closed.\x1b[0m');
            };

            term.onData((data) => {
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'input', data }));
                }
            });

            term.onResize(({ cols, rows }) => {
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ type: 'resize', cols, rows }));
                }
            });

            this._termResizeHandler = () => fitAddon.fit();
            window.addEventListener('resize', this._termResizeHandler);
        },

        closeTerminal() {
            this.terminalOpen = false;
            this.terminalSerial = null;
            if (this._termWs) { this._termWs.close(); this._termWs = null; }
            if (this._term) { this._term.dispose(); this._term = null; }
            if (this._termResizeHandler) {
                window.removeEventListener('resize', this._termResizeHandler);
                this._termResizeHandler = null;
            }
        },

        async deployFirmware() {
            if (!this.fwFile || !this.selectedDetail) return;
            this.fwUploading = true;
            this.fwResult = null;
            this.fwProgress = 10;
            this.fwStage = 'Uploading binary...';

            try {
                const formData = new FormData();
                formData.append('file', this.fwFile);

                const xhr = new XMLHttpRequest();
                const serial = this.selectedDetail.serial;

                await new Promise((resolve, reject) => {
                    xhr.upload.onprogress = (e) => {
                        if (e.lengthComputable) {
                            this.fwProgress = Math.round((e.loaded / e.total) * 50);
                            this.fwStage = 'Uploading binary...';
                        }
                    };
                    xhr.onload = () => {
                        this.fwProgress = 50;
                        this.fwStage = 'Deploying to device...';
                        resolve();
                    };
                    xhr.onerror = () => reject(new Error('Upload failed'));
                    xhr.open('POST', `/api/devices/${serial}/firmware`);
                    xhr.send(formData);
                });

                // Parse response
                const resp = JSON.parse(xhr.responseText);
                this.fwProgress = 100;
                this.fwStage = resp.status === 'ok' ? 'Complete!' : 'Failed';
                this.fwResult = resp;
                if (resp.status === 'ok') {
                    this.fwFile = null;
                }
            } catch (e) {
                this.fwResult = { status: 'error', message: 'Upload failed: ' + e.message };
                this.fwStage = 'Failed';
            } finally {
                this.fwUploading = false;
            }
        },

        // --- Remove device ---
        async removeDevice(serial) {
            if (!confirm(`Remove device ${serial}? This deletes all its data.`)) return;
            await fetch(`/api/devices/${serial}`, { method: 'DELETE' });
            this.devices = this.devices.filter(d => d.serial !== serial);
            this.alerts = this.alerts.filter(a => a.serial !== serial);
            if (this.selectedDevice === serial) this.closeDetail();
        },

        // --- Clear actions ---
        async clearErrors(serial) {
            await fetch(`/api/devices/${serial}/errors`, { method: 'DELETE' });
            if (this.selectedDetail) this.selectedDetail.error_count = 0;
            const dev = this.devices.find(d => d.serial === serial);
            if (dev) dev.error_count = 0;
        },

        async clearAlerts(serial) {
            await fetch(`/api/devices/${serial}/alerts`, { method: 'DELETE' });
            this.alerts = this.alerts.filter(a => a.serial !== serial);
        },

        async clearLogs(serial) {
            if (!confirm('Delete all error logs for this device? This cannot be undone.')) return;
            await fetch(`/api/devices/${serial}/logs`, { method: 'DELETE' });
            if (this.selectedDetail) this.selectedDetail.error_count = 0;
            const dev = this.devices.find(d => d.serial === serial);
            if (dev) dev.error_count = 0;
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
