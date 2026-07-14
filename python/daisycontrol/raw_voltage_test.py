from daisycontrol import slcanfd, bldc_drive
import time
from hardware.bldc_drive.bldc_drive_messages import DriveMemory

bus = slcanfd.slcanfdBus("usb-Daisy_50x50-Drive_205634955130-if00")
drive = bldc_drive.BLDC_Drive(bus, 100)


drive.set_raw_voltage(0.0, 0.0, 0.0)
drive.request_state_change(bldc_drive.states.DriveState.DRIVE_RAW_VOLTAGE.value)

while(True):
    input("ON?")
    drive.set_raw_voltage(-12.0, 12.0, 0.0)

    input("OFF?")
    drive.set_raw_voltage(0.0, 0.0, 0.0)
    # drive.request_state_change(bldc_drive.states.DriveState.DRIVE_DISABLE.value)