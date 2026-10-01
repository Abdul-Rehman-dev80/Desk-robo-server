import os
from dataclasses import dataclass


@dataclass(frozen=True)
class Settings:
    device_token: str
    gemini_api_key: str | None


def load_settings() -> Settings:
    token = os.environ.get("DEVICE_TOKEN", "")
    if not token or token == "change-me":
        raise RuntimeError("Set DEVICE_TOKEN (see server/.env.example)")
    return Settings(
        device_token=token,
        gemini_api_key=os.environ.get("GEMINI_API_KEY") or None,
    )
