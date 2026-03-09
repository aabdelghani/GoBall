"""MQTT subscriber that routes messages to DeviceManager."""

from __future__ import annotations

import asyncio
import json
import logging
import os
import re

import paho.mqtt.client as mqtt

from device_manager import DeviceManager
from websocket_manager import WebSocketManager

log = logging.getLogger(__name__)

# Topic pattern: goball/{serial}/{category}[/{subcategory}]
TOPIC_RE = re.compile(r"^goball/([^/]+)/(.+)$")


class MQTTClient:
    def __init__(self, dm: DeviceManager, ws: WebSocketManager):
        self.dm = dm
        self.ws = ws
        self._loop: asyncio.AbstractEventLoop | None = None
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id="fleet-backend",
            protocol=mqtt.MQTTv5,
        )
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    def start(self, loop: asyncio.AbstractEventLoop):
        self._loop = loop
        host = os.environ.get("MQTT_BROKER_HOST", "localhost")
        port = int(os.environ.get("MQTT_BROKER_PORT", "1883"))
        user = os.environ.get("MQTT_USERNAME", "")
        pwd = os.environ.get("MQTT_PASSWORD", "")
        if user:
            self.client.username_pw_set(user, pwd)
        log.info("Connecting to MQTT broker %s:%d", host, port)
        self.client.connect_async(host, port)
        self.client.loop_start()

    def stop(self):
        self.client.loop_stop()
        self.client.disconnect()

    def _on_connect(self, client, userdata, flags, rc, properties=None):
        log.info("Connected to MQTT broker, rc=%s", rc)
        client.subscribe("goball/#", qos=1)

    def _on_message(self, client, userdata, msg: mqtt.MQTTMessage):
        # Ignore empty retained messages (cleared topics)
        if not msg.payload:
            return
        m = TOPIC_RE.match(msg.topic)
        if not m:
            return
        serial = m.group(1)
        category = m.group(2)
        try:
            if category == "status":
                payload = msg.payload.decode()
            else:
                payload = json.loads(msg.payload.decode())
        except (json.JSONDecodeError, UnicodeDecodeError):
            log.warning("Bad payload on %s", msg.topic)
            return

        if self._loop:
            asyncio.run_coroutine_threadsafe(
                self._handle(serial, category, payload), self._loop
            )

    async def _handle(self, serial: str, category: str, payload):
        if category == "status":
            await self.dm.update_status(serial, payload)
        elif category == "system":
            await self.dm.update_system(serial, payload)
        elif category == "game/state":
            await self.dm.update_game_state(serial, payload)
        elif category == "game/event":
            await self.dm.add_game_event(serial, payload)
        elif category == "hardware":
            await self.dm.update_hardware(serial, payload)
        elif category == "errors":
            await self.dm.add_error(serial, payload)
        elif category == "command/result":
            pass  # Just broadcast to dashboards below
        else:
            return

        # Broadcast update to dashboards
        dev = self.dm.get_device(serial)
        if dev:
            await self.ws.broadcast({
                "type": "device_update",
                "device": dev.to_summary(),
                "category": category,
                "payload": payload,
                "alerts": self.dm.get_alerts(),
            })
