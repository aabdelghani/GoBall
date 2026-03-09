"""FastAPI backend for GoBall Fleet Monitor."""

from __future__ import annotations

import asyncio
import json
import logging
import os
import uuid
from contextlib import asynccontextmanager

from fastapi import FastAPI, UploadFile, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from device_manager import DeviceManager
from models import DeviceStatus
from mqtt_client import MQTTClient
from firmware_deploy import deploy_firmware
from ssh_terminal import terminal_session
from websocket_manager import WebSocketManager

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
log = logging.getLogger(__name__)

dm = DeviceManager(db_path=os.environ.get("SQLITE_DB_PATH", "./data/fleet.db"))
ws_mgr = WebSocketManager()
mqtt = MQTTClient(dm, ws_mgr)


@asynccontextmanager
async def lifespan(app: FastAPI):
    await dm.start()
    loop = asyncio.get_event_loop()
    mqtt.start(loop)
    # Prune old events once at startup
    await dm.prune_old_events()
    log.info("Fleet backend started")
    yield
    mqtt.stop()
    await dm.stop()
    log.info("Fleet backend stopped")


app = FastAPI(title="GoBall Fleet Monitor", lifespan=lifespan)

# Serve frontend static files
static_dir = os.environ.get("STATIC_DIR", "./static")
if os.path.isdir(static_dir):
    app.mount("/static", StaticFiles(directory=static_dir), name="static")


@app.get("/")
async def index():
    return FileResponse(os.path.join(static_dir, "index.html"))


@app.get("/alerts")
async def alerts_page():
    return FileResponse(os.path.join(static_dir, "alerts.html"))


@app.get("/api/devices")
async def list_devices():
    return dm.get_all_summaries()


@app.get("/api/devices/{serial}")
async def get_device(serial: str):
    dev = dm.get_device(serial)
    if not dev:
        return {"error": "not found"}
    return dev.model_dump()


@app.get("/api/devices/{serial}/events")
async def get_device_events(serial: str, limit: int = 50):
    return await dm.get_events(serial, limit)


@app.get("/api/alerts")
async def get_alerts():
    return dm.get_alerts()


@app.get("/api/stats")
async def get_stats():
    return await dm.get_stats()


@app.get("/logs")
async def logs_page():
    return FileResponse(os.path.join(static_dir, "logs.html"))


@app.get("/api/logs")
async def get_all_logs(limit: int = 200):
    """All error logs across all devices."""
    return await dm.get_error_logs(serial=None, limit=limit)


@app.get("/api/logs/{serial}")
async def get_device_logs(serial: str, limit: int = 200):
    """Error logs for a specific device."""
    return await dm.get_error_logs(serial=serial, limit=limit)


@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws_mgr.connect(ws)
    # Send initial device list
    await ws.send_json({"type": "init", "devices": dm.get_all_summaries(), "alerts": dm.get_alerts()})
    try:
        while True:
            await ws.receive_text()  # Keep connection alive
    except WebSocketDisconnect:
        ws_mgr.disconnect(ws)


# --- Remote commands ---

class CommandRequest(BaseModel):
    action: str
    params: dict = {}


@app.post("/api/devices/{serial}/command")
async def send_command(serial: str, cmd: CommandRequest):
    dev = dm.get_device(serial)
    if not dev:
        return {"error": "Device not found"}
    if dev.status != DeviceStatus.ONLINE:
        return {"error": "Device is offline"}
    cmd_id = str(uuid.uuid4())[:8]
    payload = {"id": cmd_id, "action": cmd.action, **cmd.params}
    mqtt.client.publish(f"goball/{serial}/command", json.dumps(payload), qos=1)
    return {"ok": True, "command_id": cmd_id}


# --- Firmware update ---

@app.post("/api/devices/{serial}/firmware")
async def upload_firmware(serial: str, file: UploadFile):
    dev = dm.get_device(serial)
    if not dev:
        return {"error": "Device not found"}
    if dev.status != DeviceStatus.ONLINE:
        return {"error": "Device is offline"}
    if not dev.system or not dev.system.ip:
        return {"error": "Device IP unknown"}

    # Read uploaded binary
    data = await file.read()
    if len(data) < 1000:
        return {"error": "File too small to be a valid binary"}
    if len(data) > 100_000_000:
        return {"error": "File too large (max 100MB)"}

    # Check ELF header (aarch64)
    if data[:4] != b'\x7fELF':
        return {"error": "Not a valid ELF binary"}

    log.info("Firmware upload for %s: %s (%d bytes)", serial, file.filename, len(data))

    # Deploy via SSH in background
    result = await deploy_firmware(dev.system.ip, data)

    # Notify via MQTT command result so dashboard gets feedback
    if result["status"] == "ok":
        mqtt.client.publish(
            f"goball/{serial}/command/result",
            json.dumps({"id": "fw-update", "action": "firmware_update", "ts": __import__('time').time(),
                         "status": "ok", "message": result["message"]}),
            qos=1,
        )

    return result


# --- SSH terminal ---

@app.websocket("/ws/terminal/{serial}")
async def terminal_endpoint(ws: WebSocket, serial: str):
    dev = dm.get_device(serial)
    if not dev or not dev.system or not dev.system.ip:
        await ws.accept()
        await ws.send_json({"type": "error", "message": "Device not found or no IP"})
        await ws.close()
        return
    await terminal_session(ws, dev.system.ip)


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("app:app", host="0.0.0.0", port=8000, reload=True)
