import json
import logging
import secrets
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, status

from . import protocol
from .config import Settings, load_settings

log = logging.getLogger("deskrobo")


def create_app(settings: Settings | None = None) -> FastAPI:
    @asynccontextmanager
    async def lifespan(app: FastAPI):
        app.state.settings = settings or load_settings()
        app.state.robots = {}
        yield

    app = FastAPI(title="Desk Robo server", lifespan=lifespan)

    @app.get("/health")
    async def health():
        return {"ok": True, "robots_online": sorted(app.state.robots)}

    @app.websocket("/ws/robot")
    async def robot_ws(ws: WebSocket):
        token = ws.headers.get("authorization", "").removeprefix("Bearer ").strip()
        if not secrets.compare_digest(token, app.state.settings.device_token):
            await ws.close(code=status.WS_1008_POLICY_VIOLATION)
            return
        await ws.accept()

        hello = await ws.receive_json()
        if hello.get("type") != protocol.HELLO or not hello.get("device_id"):
            await ws.send_json({"type": protocol.ERROR, "message": "expected hello"})
            await ws.close(code=status.WS_1002_PROTOCOL_ERROR)
            return

        device_id = str(hello["device_id"])
        app.state.robots[device_id] = ws
        log.info("robot %s connected (fw %s)", device_id, hello.get("fw", "?"))
        await ws.send_json({
            "type": protocol.WELCOME,
            "protocol": protocol.PROTOCOL_VERSION,
            "audio": {"rate": protocol.AUDIO_SAMPLE_RATE, "bits": 16, "channels": 1},
        })

        audio_bytes = 0
        try:
            while True:
                msg = await ws.receive()
                if msg["type"] == "websocket.disconnect":
                    break
                if msg.get("bytes") is not None:
                    # Mic audio. Relayed to Gemini from phase 5.
                    audio_bytes += len(msg["bytes"])
                    continue
                await handle_text(ws, device_id, msg.get("text") or "")
        except WebSocketDisconnect:
            pass
        finally:
            if app.state.robots.get(device_id) is ws:
                del app.state.robots[device_id]
            log.info("robot %s disconnected (%d audio bytes received)", device_id, audio_bytes)

    return app


async def handle_text(ws: WebSocket, device_id: str, text: str) -> None:
    try:
        msg = json.loads(text)
    except json.JSONDecodeError:
        await ws.send_json({"type": protocol.ERROR, "message": "invalid JSON"})
        return

    kind = msg.get("type")
    if kind == protocol.PING:
        await ws.send_json({"type": protocol.PONG})
    elif kind == protocol.EVENT:
        log.info("robot %s event: %s", device_id, msg)
    else:
        await ws.send_json({"type": protocol.ERROR, "message": f"unknown type {kind!r}"})
