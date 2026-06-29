from schema.common_messages import StateChange, HeartBeat, CANFDTest
from hardware.bldc_drive.bldc_drive_messages import ControllerFeedback
from daisycontrol import slcanfd
import numpy as np
import can

message = CANFDTest(1000,2,-3000,4,5,6,700000)

message = ControllerFeedback(10,10,10,10)


bus = slcanfd.slcanfdBus("/dev/cu.usbmodem2056349551301")

send_msg = can.Message(
        arbitration_id = 0x123,
        data = ControllerFeedback.encode(message),
        is_fd = True,
        is_extended_id = False
    )

print(send_msg)

bus.send(
    send_msg
)


print("recv")
msg = bus.recv()
print(ControllerFeedback.decode(msg.data))

bus.shutdown()