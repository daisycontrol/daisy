import zmq
import zmq.asyncio
import struct
import asyncio
import time

class RemoteJoystick():
    def __init__(self, host, port):
        self.host = host
        self.port = port

        self.context = zmq.asyncio.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.connect(f"tcp://{self.host}:{self.port}")
        self.socket.setsockopt(zmq.SUBSCRIBE, b"")

        self.joystick_values = [0] * 21
        self.last_time = 0.0

    async def recv_joystick(self):
        msg = await self.socket.recv()
        self.joystick_values = list(struct.unpack("21f", msg))
        self.last_time = time.monotonic()

    async def recv_loop(self):
        while(True):
            await self.recv_joystick()

    def A(self):
        return bool(self.joystick_values[6])
    
    def B(self):
        return bool(self.joystick_values[7])
    
    def Y(self):
        return bool(self.joystick_values[8])
    
    def Z(self):
        return bool(self.joystick_values[9])

    def time_since_last(self):
        return time.monotonic() - self.last_time
