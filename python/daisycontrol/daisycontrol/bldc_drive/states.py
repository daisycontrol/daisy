# TODO, codegen this
from enum import Enum


class DriveState(Enum):
    DRIVE_DISABLE = 0
    DRIVE_INIT = 1
    DRIVE_CALIBRATE = 2
    DRIVE_IDLE = 3
    DRIVE_IMPEDANCE_CONTROL = 4
    DRIVE_RAW_VOLTAGE = 5
    DRIVE_ERROR = 6