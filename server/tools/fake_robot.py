"""Pretend to be the robot, to test a deployed server.

    DEVICE_TOKEN=... python tools/fake_robot.py ws://localhost:8000/ws/robot
"""
import asyncio
import json
import os
import sys

import websockets


async def main(url: str) -> None:
    headers = {"Authorization": f"Bearer {os.environ['DEVICE_TOKEN']}"}
    async with websockets.connect(url, additional_headers=headers) as ws:
        await ws.send(json.dumps({"type": "hello", "device_id": "fake-robot", "fw": "dev"}))
        print("<", await ws.recv())
        await ws.send(json.dumps({"type": "ping"}))
        print("<", await ws.recv())


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1] if len(sys.argv) > 1 else "ws://localhost:8000/ws/robot"))
