from daisycontrol import slcanfd, bldc_drive
import time
import numpy as np

bus = slcanfd.make_bus("50x50-Drive")
drive = bldc_drive.BLDC_Drive(bus, 1)

print("read memory")
print(drive.read_memory())


bus.shutdown()