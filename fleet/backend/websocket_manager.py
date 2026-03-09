"""WebSocket connection manager for broadcasting updates to dashboards."""

from __future__ import annotations

import json
import logging
from typing import Any

from fastapi import WebSocket

log = logging.getLogger(__name__)


class WebSocketManager:
    def __init__(self):
        self.connections: list[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.connections.append(ws)
        log.info("Dashboard connected, total=%d", len(self.connections))

    def disconnect(self, ws: WebSocket):
        if ws in self.connections:
            self.connections.remove(ws)
        log.info("Dashboard disconnected, total=%d", len(self.connections))

    async def broadcast(self, message: dict[str, Any]):
        data = json.dumps(message)
        dead = []
        for ws in self.connections:
            try:
                await ws.send_text(data)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.disconnect(ws)
