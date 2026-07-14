import zmq
import zmq.asyncio
import struct
import asyncio

port = "5556"
host = "localhost"

context = zmq.asyncio.Context()
socket = context.socket(zmq.SUB)
socket.connect(f"tcp://{host}:{port}")
socket.setsockopt(zmq.SUBSCRIBE, b"")

async def recv_joystick():
    msg = await socket.recv()
    joystick_values = list(struct.unpack("21f", msg))
    print(joystick_values)

asyncio.run(recv_joystick())

# print("start")
# while(True):
