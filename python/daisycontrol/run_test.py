# from schema.common_messages import StateChange, EmptyMessage
# from hardware.bldc_drive.bldc_drive_messages import EncoderState, DriveMemory, FOCInfo
from daisycontrol import slcanfd, bldc_drive
import time
import numpy as np

bus = slcanfd.make_bus("50x50-Drive")

drive_1 = bldc_drive.BLDC_Drive(bus, 2000)

print(drive_1.read_memory())
time.sleep(0.1)
print(drive_1.set_controller_settings(0.0, 500.0, 2.0, 0.99))
time.sleep(0.1)
print(drive_1.request_state_change(bldc_drive.states.DriveState.DRIVE_INIT.value))
time.sleep(0.1)
print(drive_1.request_state_change(bldc_drive.states.DriveState.DRIVE_IDLE.value))
time.sleep(0.1)
print("IMPEDANCE CONTROL")
print(drive_1.request_state_change(bldc_drive.states.DriveState.DRIVE_IMPEDANCE_CONTROL.value))

t = 0
while(True):
    t += 0.01
    send_time = time.time()
    state = drive_1.set_controller_setpoint(0.0, 5.0 * np.sin(t), 0.0)
    rx_time = time.time()

    print(send_time - rx_time)
    time.sleep(0.001)

    # print(drive.set_controller_setpoint(4.0, 0.0, 0.0))
    # time.sleep(0.5)

bus.shutdown()

# -0.0004901885986328125