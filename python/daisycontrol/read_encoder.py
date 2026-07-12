from schema.common_messages import StateChange, EmptyMessage
from hardware.bldc_drive.bldc_drive_messages import EncoderState, DriveMemory
from daisycontrol import slcanfd
import numpy as np
import can
import time

STATE_CHANGE_ID = 1
ENCODER_STATE_ID = 5

READ_MEMORY_ID = 10
WRITE_MEMORY_ID = 11


def make_ipc_frame(can_id, ipc_id, message):
    return can.Message(
        arbitration_id = can_id,
        data = bytes([ipc_id]) + message.encode(message),
        is_fd = True,
        is_extended_id = False
    )



bus = slcanfd.slcanfdBus("/dev/cu.usbmodem365D385930361")


while(True):
    bus.send( make_ipc_frame(10, ENCODER_STATE_ID, EmptyMessage(0)) )
    msg = bus.recv()
    print(msg)
    state = EncoderState.decode(msg.data[1:])
    # print("%+06.4f"%(state.speed_raw / 32768))
    print((state.angle_raw / 32768 + state.revolutions) * np.pi );
    print(state.angle_rads);
    time.sleep(0.01)
