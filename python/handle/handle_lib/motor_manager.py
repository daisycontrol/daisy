import asyncio
import numpy as np

from daisycontrol import slcanfd, bldc_drive

class MotorManager():
    def __init__(self, bus):
        self.bus = bus
        self.drives = []
        
        self.ratios = []
        self.zeros = []
    
    def add_motor(self, motor, ratio, zero):
        self.drives.append(motor)
        self.ratios.append(ratio)
        self.zeros.append(zero)

    def route_feedback(self, can_frame):
        for drive in self.drives:
            if(drive.can_id == can_frame.arbitration_id):
                drive.read_controller_response_async(can_frame)

    # Drain all pending frames
    # TODO: this should route through an ipc handler
    def read_all_feedback(self):
        while(True):
            next_frame = self.bus.recv(0.0)
            if(next_frame == None):
                return
            self.route_feedback(next_frame)

    def send_saved_targets_async(self):
        for drive in self.drives:
            drive.set_saved_controller_setpoint_async()

    def read_all_position(self):
        for drive in self.drives:
            drive.update_state()

    def get_state(self):
        state = []
        for i, drive in enumerate(self.drives):
            state.append([drive.state.position_rads / self.ratios[i] - self.zeros[i], drive.state.velocity_radsps / self.ratios[i], drive.state.torque_Nm * self.ratios[i]])
        return np.array(state)

    def zero_gains(self):
        for drive in self.drives:
            drive.set_controller_settings(0.0, 0.0, 0.0, 0.99)

    async def init_all(self):
        for drive in self.drives:
            drive.request_state_change(bldc_drive.states.DriveState.DRIVE_INIT.value)
            await asyncio.sleep(0.1)
    
    async def enable_all(self):
        for drive in self.drives:
            drive.request_state_change(bldc_drive.states.DriveState.DRIVE_IDLE.value)
        await asyncio.sleep(0.01)

    async def impedance_control_all(self):
        for drive in self.drives:
            drive.request_state_change(bldc_drive.states.DriveState.DRIVE_IMPEDANCE_CONTROL.value)
        await asyncio.sleep(0.01)

    async def disable_all(self):
        for drive in self.drives:
            drive.request_state_change(bldc_drive.states.DriveState.DRIVE_DISABLE.value)
            await asyncio.sleep(0.01)

    def set_target_states(self, target_states):
        for i, drive in enumerate(self.drives):
            self.set_target_state(i, target_states[i][0], target_states[i][1], target_states[i][2])

    def set_target_state(self, index, position, velocity, torque):
        self.drives[index].setpoint.position_target_rads = (position + self.zeros[index]) * self.ratios[index]
        self.drives[index].velocity_target_radsps = velocity * self.ratios[index]
        self.drives[index].torque_feedforward_Nm = torque / self.ratios[index]

    def all_within_error_limit(self, limit):
        for i, drive in enumerate(self.drives):
            if(abs(drive.setpoint.position_target_rads - drive.state.position_rads) > limit * self.ratios[i]):
                print(i, "NOT WITHIN LIMIT")
                return False
            
        return True