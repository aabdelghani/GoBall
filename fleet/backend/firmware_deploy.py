"""Deploy firmware binary to Pi via SSH (SCP + service restart)."""

import logging
import os
import tempfile

import asyncssh

log = logging.getLogger(__name__)

SSH_KEY_PATH = os.environ.get("SSH_KEY_PATH", os.path.expanduser("~/.ssh/id_ed25519"))
SSH_USERNAME = os.environ.get("SSH_USERNAME", "root")
REMOTE_BINARY = "/usr/bin/goball"
SERVICE_NAME = "goball.service"


async def deploy_firmware(ip: str, data: bytes) -> dict:
    """Upload binary to Pi, replace /usr/bin/goball, restart service."""
    try:
        conn = await asyncssh.connect(
            ip,
            username=SSH_USERNAME,
            client_keys=[SSH_KEY_PATH],
            known_hosts=None,
            connect_timeout=10,
        )
    except Exception as e:
        log.error("SSH to %s failed: %s", ip, e)
        return {"status": "error", "message": f"SSH connection failed: {e}"}

    try:
        # 1. Stop the app so binary isn't locked
        log.info("Stopping %s on %s", SERVICE_NAME, ip)
        result = await conn.run(f"systemctl stop {SERVICE_NAME}", check=False, timeout=15)

        # 2. Upload binary to temp location
        log.info("Uploading firmware to %s (%d bytes)", ip, len(data))
        with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as tmp:
            tmp.write(data)
            tmp_path = tmp.name

        try:
            async with conn.start_sftp_client() as sftp:
                await sftp.put(tmp_path, "/tmp/goball_update")
        finally:
            os.unlink(tmp_path)

        # 3. Replace binary and set permissions
        log.info("Installing firmware on %s", ip)
        cmds = [
            f"cp {REMOTE_BINARY} {REMOTE_BINARY}.bak",
            f"mv /tmp/goball_update {REMOTE_BINARY}",
            f"chmod +x {REMOTE_BINARY}",
            f"systemctl start {SERVICE_NAME}",
        ]
        for cmd in cmds:
            r = await conn.run(cmd, check=False, timeout=30)
            if r.exit_status != 0:
                err = r.stderr.strip() or r.stdout.strip()
                log.error("Command failed on %s: %s -> %s", ip, cmd, err)
                # Try to restore backup if install failed
                if "mv" in cmd or "chmod" in cmd:
                    await conn.run(f"cp {REMOTE_BINARY}.bak {REMOTE_BINARY} && chmod +x {REMOTE_BINARY}", check=False)
                    await conn.run(f"systemctl start {SERVICE_NAME}", check=False)
                return {"status": "error", "message": f"Deploy failed: {err}"}

        # 4. Verify app started
        r = await conn.run("sleep 2 && pgrep -f goball | head -1", check=False, timeout=10)
        pid = r.stdout.strip()

        msg = f"Firmware updated and service restarted"
        if pid:
            msg += f" (PID {pid})"
        log.info("Firmware deploy to %s complete: %s", ip, msg)
        return {"status": "ok", "message": msg}

    except Exception as e:
        log.error("Firmware deploy to %s failed: %s", ip, e)
        # Try to restart service even on failure
        try:
            await conn.run(f"systemctl start {SERVICE_NAME}", check=False)
        except Exception:
            pass
        return {"status": "error", "message": f"Deploy failed: {e}"}
    finally:
        conn.close()
