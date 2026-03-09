"""SSH terminal proxy — bridges browser WebSocket to Pi SSH sessions."""

import asyncio
import json
import logging
import os

import asyncssh
from fastapi import WebSocket, WebSocketDisconnect

log = logging.getLogger(__name__)

SSH_KEY_PATH = os.environ.get("SSH_KEY_PATH", os.path.expanduser("~/.ssh/id_ed25519"))
SSH_USERNAME = os.environ.get("SSH_USERNAME", "q")


async def terminal_session(ws: WebSocket, ip: str):
    """Handle a single terminal WebSocket session."""
    await ws.accept()

    # Wait for initial resize message to get correct terminal size
    cols, rows = 80, 24
    try:
        raw = await asyncio.wait_for(ws.receive_text(), timeout=5)
        msg = json.loads(raw)
        if msg.get("type") == "resize":
            cols = msg.get("cols", 80)
            rows = msg.get("rows", 24)
    except Exception:
        pass

    try:
        conn = await asyncssh.connect(
            ip,
            username=SSH_USERNAME,
            client_keys=[SSH_KEY_PATH],
            known_hosts=None,
            connect_timeout=10,
        )
    except Exception as e:
        log.error("SSH connection to %s failed: %s", ip, e)
        await ws.send_json({"type": "error", "message": f"SSH connection failed: {e}"})
        await ws.close()
        return

    try:
        process = await conn.create_process(
            term_type="xterm-256color",
            term_size=(cols, rows),
        )

        async def ssh_to_ws():
            try:
                async for data in process.stdout:
                    await ws.send_json({"type": "output", "data": data})
            except Exception:
                pass

        async def ws_to_ssh():
            try:
                while True:
                    raw = await ws.receive_text()
                    msg = json.loads(raw)
                    if msg.get("type") == "input":
                        process.stdin.write(msg["data"])
                    elif msg.get("type") == "resize":
                        process.change_terminal_size(
                            msg.get("cols", 80),
                            msg.get("rows", 24),
                        )
            except WebSocketDisconnect:
                pass

        done, pending = await asyncio.wait(
            [asyncio.create_task(ssh_to_ws()), asyncio.create_task(ws_to_ssh())],
            return_when=asyncio.FIRST_COMPLETED,
        )
        for task in pending:
            task.cancel()
    finally:
        conn.close()
        try:
            await ws.close()
        except Exception:
            pass
