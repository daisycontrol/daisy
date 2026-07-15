from daisycontrol import bldc_drive
import asyncio

class SuctionGripper():
    def __init__(self, bus):
        self.bus = bus
        self.motor_id = 100

        self.drive = bldc_drive.BLDC_Drive(bus, 100)

        self.current_filtered = 0
    
    async def enable(self):
        self.drive.request_state_change(bldc_drive.states.DriveState.DRIVE_INIT.value)
        await asyncio.sleep(0.1)
        self.drive.request_state_change(bldc_drive.states.DriveState.DRIVE_DISABLE.value)
        await asyncio.sleep(0.1)
        self.drive.set_raw_voltage(0.0, 0.0, 0.0)
        self.drive.request_state_change(bldc_drive.states.DriveState.DRIVE_RAW_VOLTAGE.value)

    def get_current(self):
        KF = 0.9
        self.current_filtered = KF * self.current_filtered + (1-KF) * (-self.drive.read_foc_state().phaseA_mA)
        return self.current_filtered


    def disable(self):
        self.drive.request_state_change(bldc_drive.states.DriveState.DRIVE_DISABLE.value)

    def set_voltage(self, voltage_V):
        self.drive.set_raw_voltage(-voltage_V / 2.0, voltage_V / 2.0, 0.0)
    