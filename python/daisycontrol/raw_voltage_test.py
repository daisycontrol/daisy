from daisycontrol import slcanfd, bldc_drive
import time
import numpy as np
from hardware.bldc_drive.bldc_drive_messages import DriveMemory

bus = slcanfd.make_bus("50x50-Drive")
drive = bldc_drive.BLDC_Drive(bus, 2000)


drive.set_raw_voltage(0.0, 0.0, 0.0)
drive.request_state_change(bldc_drive.states.DriveState.DRIVE_RAW_VOLTAGE.value)

while(True):
    input("ON?")
    drive.set_raw_voltage(-12.0, 12.0, 0.0)

    input("OFF?")
    drive.set_raw_voltage(0.0, 0.0, 0.0)
    # drive.request_state_change(bldc_drive.states.DriveState.DRIVE_DISABLE.value)