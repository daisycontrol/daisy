from schema.common_messages import StateChange, EmptyMessage
from hardware.bldc_drive.bldc_drive_messages import EncoderState, DriveMemory
from daisycontrol import slcanfd
import numpy as np
from matplotlib import pyplot as plt
import can
import time

STATE_CHANGE_ID = 1
ENCODER_STATE_ID = 12

READ_MEMORY_ID = 10
WRITE_MEMORY_ID = 11


def make_ipc_frame(can_id, ipc_id, message):
    return can.Message(
        arbitration_id = can_id,
        data = bytes([ipc_id]) + message.encode(message),
        is_fd = True,
        is_extended_id = False
    )



bus = slcanfd.make_bus("50x50-Drive")

can_id = 1


# pole_pairs = 7
# pole_pairs = None

# # Read out existing memory
# bus.send( make_ipc_frame(10, READ_MEMORY_ID, EmptyMessage(0)) )
# msg = bus.recv()
# memory = DriveMemory.decode(msg.data[1:])
# if(memory.magic != 0x1234):
#     raise Exception("Memory magic wrong, drive might not have been programmed")

# print("Existing memory:", memory)

# if(pole_pairs):
#     print("Writing pole count")
#     memory.drive_pole_pairs = pole_pairs

#     bus.send( make_ipc_frame(10, WRITE_MEMORY_ID, memory) )
#     msg = bus.recv()

# State change to calibration
print("Calibration state change")
bus.send( make_ipc_frame(can_id, STATE_CHANGE_ID, StateChange(2, 0)) )
msg = bus.recv()
print(StateChange.decode(msg.data[1:]))

data = []

for i in range(80):
    bus.send( make_ipc_frame(can_id, ENCODER_STATE_ID, EmptyMessage(0)) )
   
    msg = bus.recv()
    state = EncoderState.decode(msg.data[1:])
    print(state.angle_raw, state.speed_raw)
    data.append([state.angle_raw, state.speed_raw])

    time.sleep(0.05)


# Read out existing memory
# bus.send( make_ipc_frame(2, READ_MEMORY_ID, EmptyMessage(0)) )
# msg = bus.recv()
# memory = DriveMemory.decode(msg.data[1:])

# print("Encoder offset", memory.drive_encoder_offset)

plt.plot(data)
plt.show()

bus.shutdown()