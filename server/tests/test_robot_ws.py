import pytest
from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

from app.config import Settings
from app.main import create_app

TOKEN = "test-token"
AUTH = {"Authorization": f"Bearer {TOKEN}"}


@pytest.fixture
def client():
    app = create_app(Settings(device_token=TOKEN, gemini_api_key=None))
    with TestClient(app) as c:
        yield c


def connect(client):
    ws = client.websocket_connect("/ws/robot", headers=AUTH).__enter__()
    ws.send_json({"type": "hello", "device_id": "robo-1", "fw": "0.1.0"})
    return ws


def test_health(client):
    assert client.get("/health").json() == {"ok": True, "robots_online": []}


def test_rejects_bad_token(client):
    with pytest.raises(WebSocketDisconnect) as exc:
        with client.websocket_connect("/ws/robot", headers={"Authorization": "Bearer nope"}):
            pass
    assert exc.value.code == 1008


def test_rejects_missing_hello(client):
    with client.websocket_connect("/ws/robot", headers=AUTH) as ws:
        ws.send_json({"type": "ping"})
        assert ws.receive_json()["type"] == "error"
        with pytest.raises(WebSocketDisconnect):
            ws.receive_json()


def test_welcome_ping_and_presence(client):
    ws = connect(client)
    welcome = ws.receive_json()
    assert welcome["type"] == "welcome"
    assert welcome["audio"] == {"rate": 16000, "bits": 16, "channels": 1}
    assert client.get("/health").json()["robots_online"] == ["robo-1"]

    ws.send_bytes(b"\x00\x00" * 160)  # 10 ms of silence
    ws.send_json({"type": "ping"})
    assert ws.receive_json() == {"type": "pong"}

    ws.send_text("not json")
    assert ws.receive_json()["message"] == "invalid JSON"

    ws.__exit__(None, None, None)
    assert client.get("/health").json()["robots_online"] == []


def test_settings_require_token(monkeypatch):
    from app.config import load_settings

    monkeypatch.setenv("DEVICE_TOKEN", "change-me")
    with pytest.raises(RuntimeError):
        load_settings()
