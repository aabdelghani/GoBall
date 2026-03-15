"""Pydantic models for GoBall fleet monitoring."""

from __future__ import annotations

from enum import Enum
from typing import Optional

from pydantic import BaseModel


class DeviceStatus(str, Enum):
    ONLINE = "online"
    OFFLINE = "offline"


class GameMode(str, Enum):
    IDLE = "idle"
    SETUP = "setup"
    PLAYING = "playing"
    FINISHED = "finished"


class SystemMetrics(BaseModel):
    ts: float
    hostname: str = ""
    ip: str = ""
    cpu_temp: float = 0.0
    mem_used_mb: int = 0
    mem_total_mb: int = 0
    disk_used_pct: float = 0.0
    uptime_s: int = 0
    wifi_ssid: str = ""
    wifi_signal_dbm: int = 0
    app_running: bool = False
    app_pid: int = 0
    lat: float = 0.0
    lng: float = 0.0
    firmware_version: str = ""
    fan_pwm: int = -1       # 0-255 duty cycle, -1 = no fan
    fan_rpm: int = -1        # RPM, -1 = no tach/no fan


class GameState(BaseModel):
    mode: GameMode = GameMode.IDLE
    player_count: int = 0
    scores: list[int] = []
    hole: int = 0


class HardwareState(BaseModel):
    led_active: bool = False
    audio_active: bool = False
    sensor_ok: bool = True


class DeviceState(BaseModel):
    serial: str
    status: DeviceStatus = DeviceStatus.OFFLINE
    last_seen: float = 0.0
    system: Optional[SystemMetrics] = None
    game: Optional[GameState] = None
    hardware: Optional[HardwareState] = None
    errors: list[str] = []

    def to_summary(self) -> dict:
        """Compact representation for the device grid."""
        return {
            "serial": self.serial,
            "status": self.status.value,
            "last_seen": self.last_seen,
            "hostname": self.system.hostname if self.system else "",
            "ip": self.system.ip if self.system else "",
            "cpu_temp": self.system.cpu_temp if self.system else 0,
            "app_running": self.system.app_running if self.system else False,
            "game_mode": self.game.mode.value if self.game else "idle",
            "player_count": self.game.player_count if self.game else 0,
            "error_count": len(self.errors),
            "lat": self.system.lat if self.system else 0,
            "lng": self.system.lng if self.system else 0,
            "uptime_s": self.system.uptime_s if self.system else 0,
            "mem_used_mb": self.system.mem_used_mb if self.system else 0,
            "mem_total_mb": self.system.mem_total_mb if self.system else 0,
            "disk_used_pct": self.system.disk_used_pct if self.system else 0,
            "firmware_version": self.system.firmware_version if self.system else "",
            "fan_pwm": self.system.fan_pwm if self.system else -1,
            "fan_rpm": self.system.fan_rpm if self.system else -1,
        }


class Alert(BaseModel):
    serial: str
    alert_type: str  # "offline", "high_temp", "disk_full", "app_down", "error", "fan_failure"
    message: str
    ts: float
