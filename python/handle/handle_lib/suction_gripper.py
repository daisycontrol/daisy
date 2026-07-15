from daisycontrol import bldc_drive

class SuctionGripper():
    def __init__(self, bus):
        self.bus = bus
        self.motor_id = 100

        self.drive = bldc_drive.BLDC_Drive(bus, 100)
    
    def enable(self):
        self.drive.set_raw_voltage(0.0, 0.0, 0.0)
        self.drive.request_state_change(bldc_drive.states.DriveState.DRIVE_RAW_VOLTAGE.value)

    def get_current(self):
        return self.drive.


    def disable(self):
        self.drive.request_state_change(bldc_drive.states.DriveState.DRIVE_DISABLE.value)

    def set_voltage(self, voltage_V):
        self.drive.set_raw_voltage(-voltage_V / 2.0, voltage_V / 2.0, 0.0)
    