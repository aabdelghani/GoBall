"""In-memory device state management with SQLite persistence."""

from __future__ import annotations

import json
import logging
import time
from pathlib import Path
from typing import Optional

import aiosqlite

from models import Alert, DeviceState, DeviceStatus, GameMode, GameState, HardwareState, SystemMetrics

log = logging.getLogger(__name__)

ALERT_THRESHOLDS = {
    "offline_seconds": 300,  # 5 minutes
    "cpu_temp_c": 80.0,
    "disk_used_pct": 90.0,
}
EVENT_RETENTION_DAYS = 7


class DeviceManager:
    def __init__(self, db_path: str = "./data/fleet.db"):
        self.devices: dict[str, DeviceState] = {}
        self.alerts: list[Alert] = []
        self.db_path = db_path
        self._db: Optional[aiosqlite.Connection] = None

    async def start(self):
        Path(self.db_path).parent.mkdir(parents=True, exist_ok=True)
        self._db = await aiosqlite.connect(self.db_path)
        await self._db.execute("""
            CREATE TABLE IF NOT EXISTS devices (
                serial TEXT PRIMARY KEY,
                first_seen REAL,
                last_seen REAL,
                hostname TEXT,
                ip TEXT
            )
        """)
        await self._db.execute("""
            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                serial TEXT,
                event_type TEXT,
                payload TEXT,
                ts REAL
            )
        """)
        await self._db.execute("""
            CREATE INDEX IF NOT EXISTS idx_events_ts ON events(ts)
        """)
        await self._db.execute("""
            CREATE TABLE IF NOT EXISTS error_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                serial TEXT NOT NULL,
                message TEXT NOT NULL,
                ts REAL NOT NULL
            )
        """)
        await self._db.execute("""
            CREATE INDEX IF NOT EXISTS idx_error_logs_serial ON error_logs(serial, ts)
        """)
        await self._db.execute("""
            CREATE INDEX IF NOT EXISTS idx_error_logs_ts ON error_logs(ts)
        """)
        await self._db.commit()
        log.info("DeviceManager started, db=%s", self.db_path)

    async def stop(self):
        if self._db:
            await self._db.close()

    def get_or_create(self, serial: str) -> DeviceState:
        if serial not in self.devices:
            self.devices[serial] = DeviceState(serial=serial)
        return self.devices[serial]

    async def update_status(self, serial: str, status_str: str):
        dev = self.get_or_create(serial)
        dev.status = DeviceStatus(status_str)
        dev.last_seen = time.time()
        if status_str == "offline":
            await self._add_alert(serial, "offline", f"{serial} went offline")
        elif status_str == "online":
            # Clear offline alert
            self.alerts = [a for a in self.alerts if not (a.serial == serial and a.alert_type == "offline")]
        await self._persist_device(dev)

    async def update_system(self, serial: str, data: dict):
        dev = self.get_or_create(serial)
        dev.system = SystemMetrics(**data)
        dev.status = DeviceStatus.ONLINE
        dev.last_seen = time.time()
        # Check alert conditions
        if dev.system.cpu_temp > ALERT_THRESHOLDS["cpu_temp_c"]:
            await self._add_alert(serial, "high_temp", f"{serial} CPU {dev.system.cpu_temp}°C")
        if dev.system.disk_used_pct > ALERT_THRESHOLDS["disk_used_pct"]:
            await self._add_alert(serial, "disk_full", f"{serial} disk {dev.system.disk_used_pct}%")
        if not dev.system.app_running:
            await self._add_alert(serial, "app_down", f"{serial} app not running")
        else:
            self.alerts = [a for a in self.alerts if not (a.serial == serial and a.alert_type == "app_down")]
        await self._persist_device(dev)

    async def update_game_state(self, serial: str, data: dict):
        dev = self.get_or_create(serial)
        dev.game = GameState(
            mode=GameMode(data.get("mode", "idle")),
            player_count=data.get("player_count", 0),
            scores=data.get("scores", []),
            hole=data.get("hole", 0),
        )
        dev.last_seen = time.time()
        await self._store_event(serial, "game_state", data)

    async def update_hardware(self, serial: str, data: dict):
        dev = self.get_or_create(serial)
        dev.hardware = HardwareState(**data)
        dev.last_seen = time.time()

    async def add_error(self, serial: str, data: dict):
        dev = self.get_or_create(serial)
        msg = data.get("message", str(data))
        dev.errors.append(msg)
        if len(dev.errors) > 50:
            dev.errors = dev.errors[-50:]
        dev.last_seen = time.time()
        await self._add_alert(serial, "error", f"{serial}: {msg}")
        await self._store_event(serial, "error", data)
        await self._store_error_log(serial, msg)

    async def add_game_event(self, serial: str, data: dict):
        await self._store_event(serial, "game_event", data)

    def get_all_summaries(self) -> list[dict]:
        return [d.to_summary() for d in self.devices.values()]

    def get_device(self, serial: str) -> Optional[DeviceState]:
        return self.devices.get(serial)

    def get_alerts(self) -> list[dict]:
        return [a.model_dump() for a in self.alerts[-200:]]

    async def get_events(self, serial: str, limit: int = 50) -> list[dict]:
        if not self._db:
            return []
        cursor = await self._db.execute(
            "SELECT event_type, payload, ts FROM events WHERE serial = ? ORDER BY ts DESC LIMIT ?",
            (serial, limit),
        )
        rows = await cursor.fetchall()
        return [{"event_type": r[0], "payload": json.loads(r[1]), "ts": r[2]} for r in rows]

    async def prune_old_events(self):
        if not self._db:
            return
        cutoff = time.time() - EVENT_RETENTION_DAYS * 86400
        await self._db.execute("DELETE FROM events WHERE ts < ?", (cutoff,))
        await self._db.commit()

    async def _persist_device(self, dev: DeviceState):
        if not self._db:
            return
        await self._db.execute(
            """INSERT INTO devices (serial, first_seen, last_seen, hostname, ip)
               VALUES (?, ?, ?, ?, ?)
               ON CONFLICT(serial) DO UPDATE SET last_seen=?, hostname=?, ip=?""",
            (
                dev.serial,
                dev.last_seen,
                dev.last_seen,
                dev.system.hostname if dev.system else "",
                dev.system.ip if dev.system else "",
                dev.last_seen,
                dev.system.hostname if dev.system else "",
                dev.system.ip if dev.system else "",
            ),
        )
        await self._db.commit()

    async def _store_event(self, serial: str, event_type: str, payload: dict):
        if not self._db:
            return
        await self._db.execute(
            "INSERT INTO events (serial, event_type, payload, ts) VALUES (?, ?, ?, ?)",
            (serial, event_type, json.dumps(payload), time.time()),
        )
        await self._db.commit()

    async def _add_alert(self, serial: str, alert_type: str, message: str):
        # Deduplicate: only one active alert per serial+type
        self.alerts = [a for a in self.alerts if not (a.serial == serial and a.alert_type == alert_type)]
        self.alerts.append(Alert(serial=serial, alert_type=alert_type, message=message, ts=time.time()))
        if len(self.alerts) > 1000:
            self.alerts = self.alerts[-500:]

    async def _store_error_log(self, serial: str, message: str):
        if not self._db:
            return
        await self._db.execute(
            "INSERT INTO error_logs (serial, message, ts) VALUES (?, ?, ?)",
            (serial, message, time.time()),
        )
        await self._db.commit()

    async def get_stats(self) -> dict:
        """Aggregate statistics for the stats dashboard."""
        devs = list(self.devices.values())
        online = [d for d in devs if d.status == DeviceStatus.ONLINE]
        offline = [d for d in devs if d.status == DeviceStatus.OFFLINE]

        # Game mode distribution
        game_modes = {"idle": 0, "setup": 0, "playing": 0, "finished": 0}
        for d in devs:
            mode = d.game.mode.value if d.game else "idle"
            game_modes[mode] = game_modes.get(mode, 0) + 1

        # Temperature distribution (buckets)
        temp_buckets = {"<40": 0, "40-50": 0, "50-60": 0, "60-70": 0, "70-80": 0, ">80": 0}
        for d in online:
            t = d.system.cpu_temp if d.system else 0
            if t < 40: temp_buckets["<40"] += 1
            elif t < 50: temp_buckets["40-50"] += 1
            elif t < 60: temp_buckets["50-60"] += 1
            elif t < 70: temp_buckets["60-70"] += 1
            elif t < 80: temp_buckets["70-80"] += 1
            else: temp_buckets[">80"] += 1

        # Top uptimes
        uptime_top = sorted(
            [{"serial": d.serial, "hostname": d.system.hostname if d.system else "", "uptime_s": d.system.uptime_s if d.system else 0} for d in online],
            key=lambda x: x["uptime_s"], reverse=True
        )[:10]

        # Locations for heatmap
        locations = []
        for d in devs:
            if d.system and (d.system.lat != 0 or d.system.lng != 0):
                locations.append({
                    "serial": d.serial,
                    "hostname": d.system.hostname,
                    "lat": d.system.lat,
                    "lng": d.system.lng,
                    "status": d.status.value,
                    "game_mode": d.game.mode.value if d.game else "idle",
                })

        # Total games from events DB
        total_games = 0
        total_score_events = 0
        total_errors = 0
        if self._db:
            cur = await self._db.execute("SELECT COUNT(*) FROM events WHERE event_type = 'game_state'")
            row = await cur.fetchone()
            total_games = row[0] if row else 0

            cur = await self._db.execute("SELECT COUNT(*) FROM events WHERE event_type = 'game_event'")
            row = await cur.fetchone()
            total_score_events = row[0] if row else 0

            cur = await self._db.execute("SELECT COUNT(*) FROM error_logs")
            row = await cur.fetchone()
            total_errors = row[0] if row else 0

        # Avg metrics
        avg_temp = sum(d.system.cpu_temp for d in online if d.system) / max(len(online), 1)
        avg_mem = sum(d.system.mem_used_mb for d in online if d.system) / max(len(online), 1)
        avg_disk = sum(d.system.disk_used_pct for d in online if d.system) / max(len(online), 1)

        return {
            "total_devices": len(devs),
            "online": len(online),
            "offline": len(offline),
            "game_modes": game_modes,
            "temp_buckets": temp_buckets,
            "avg_temp": round(avg_temp, 1),
            "avg_mem_mb": round(avg_mem),
            "avg_disk_pct": round(avg_disk, 1),
            "uptime_top": uptime_top,
            "locations": locations,
            "total_games": total_games,
            "total_score_events": total_score_events,
            "total_errors": total_errors,
            "active_alerts": len(self.alerts),
        }

    async def get_error_logs(self, serial: str | None = None, limit: int = 200) -> list[dict]:
        """Get error logs. If serial is None, return all devices collectively."""
        if not self._db:
            return []
        if serial:
            cursor = await self._db.execute(
                "SELECT serial, message, ts FROM error_logs WHERE serial = ? ORDER BY ts DESC LIMIT ?",
                (serial, limit),
            )
        else:
            cursor = await self._db.execute(
                "SELECT serial, message, ts FROM error_logs ORDER BY ts DESC LIMIT ?",
                (limit,),
            )
        rows = await cursor.fetchall()
        return [{"serial": r[0], "message": r[1], "ts": r[2]} for r in rows]
