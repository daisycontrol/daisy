from daisycontrol import slcanfd, bldc_drive
import time
import numpy as np
from hardware.bldc_drive.bldc_drive_messages import DriveMemory

bus = slcanfd.make_bus("50x50-Drive")
drive = bldc_drive.BLDC_Drive(bus, 1)

old_memory = drive.read_memory()
print("Old memory", old_memory)

memory = DriveMemory(
    magic = 0x1234,
    can_id = 1,
    drive_pole_pairs = 8,
    reversed = 0,
    drive_encoder_offset = 0,
    drive_resistance_mOhm = 0,
    drive_inductance_uH = 0,
    drive_torque_constant_mNm_A = 0,
    padding = 0
)

drive.memory = old_memory
print("Written memory", drive.write_memory())

bus.shutdown()