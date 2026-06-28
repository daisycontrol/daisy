from schema.common_messages import StateChange, HeartBeat
from daisycontrol import slcanfd
import numpy as np
import can

message = StateChange(1,2)



bus = slcanfd.slcanfdBus("/dev/cu.usbmodem2056349551301")
bus.send(
    can.Message(
        arbitration_id = 0x123,
        data = StateChange.encode(message),
        is_fd = True,
        is_extended_id = False
    )
)
print("recv")
msg = bus.recv()
print(StateChange.decode(msg.data))

bus.shutdown()