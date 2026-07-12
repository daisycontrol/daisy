from daisycontrol import slcanfd, bldc_drive
import time
import numpy as np
from ruckig import Ruckig, InputParameter, OutputParameter, Result

bus = slcanfd.make_bus("50x50-Drive")

arm_joint_0 = bldc_drive.BLDC_Drive(bus, 1)
arm_joint_1 = bldc_drive.BLDC_Drive(bus, 2)
arm_joint_2 = bldc_drive.BLDC_Drive(bus, 10)

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
            drive.set_controller_setpoint_async(0.0, 0.0, 0.0)
        time.sleep(0.1)
        self.read_all_feedback()

    def get_state(self):
        state = []
        for i, drive in enumerate(self.drives):
            state.append([drive.state.position_rads / self.ratios[i] - self.zeros[i], drive.state.velocity_radsps / self.ratios[i], drive.state.torque_Nm * self.ratios[i]])
        return np.array(state)

    def zero_gains(self):
        for drive in self.drives:
            drive.set_controller_settings(0.0, 0.0, 1.0, 0.99)

    def init_all(self):
        for drive in self.drives:
            drive.request_state_change(bldc_drive.states.DriveState.DRIVE_INIT.value)
        time.sleep(0.05)
    
    def enable_all(self):
        for drive in self.drives:
            drive.request_state_change(bldc_drive.states.DriveState.DRIVE_IDLE.value)
        time.sleep(0.01)

    def impedance_control_all(self):
        for drive in self.drives:
            drive.request_state_change(bldc_drive.states.DriveState.DRIVE_IMPEDANCE_CONTROL.value)
        time.sleep(0.01)

    def disable_all(self):
        for drive in self.drives:
            drive.request_state_change(bldc_drive.states.DriveState.DRIVE_DISABLE.value)

    def set_target_states(self, target_states):
        for i, drive in enumerate(self.drives):
            self.set_target_state(i, target_states[i][0], target_states[i][1], target_states[i][2])

    def set_target_state(self, index, position, velocity, torque):
        self.drives[index].setpoint.position_target_rads = position * self.ratios[index] - self.zeros[index]
        self.drives[index].velocity_target_radsps = velocity * self.ratios[index]
        self.drives[index].torque_feedforward_Nm = torque / self.ratios[index]

    def all_within_error_limit(self, limit):
        for drive in self.drives:
            if(abs(drive.setpoint.position_target_rads - drive.state.position_rads) > limit):
                return False
            
        return True


DT = 0.005

class Arm():
    def __init__(self, motor_manager):
        self.motors = motor_manager
        self.traj = Ruckig(3, DT)
        self.traj_input = InputParameter(3)
        self.traj_output = OutputParameter(3)

        self.traj_input.max_velocity = [2.0, 2.0, 2.0]
        self.traj_input.max_acceleration = [10.0, 10.0, 10.0]
        self.traj_input.max_jerk = [50.0, 50.0, 50.0]

        self.tracking_error_limit = 3.0

        self.link_length_m = 0.28

    def enable(self):
        self.motors.init_all()
        self.motors.enable_all()
        self.motors.read_all_position()

        # Initialize trajectory generator
        current_state = self.motors.get_state()
        self.traj_input.current_position = current_state[:, 0]
        self.traj_input.current_velocity = current_state[:, 1]

        self.motors.drives[0].set_controller_settings(30000.0, 800.0, 6.0, 0.995)
        self.motors.drives[1].set_controller_settings(30000.0, 800.0, 6.0, 0.995)
        self.motors.drives[2].set_controller_settings(30000.0, 600.0, 5.0, 0.995)
        
        time.sleep(0.01)
        self.motors.impedance_control_all()

    def get_state(self):
        self.motors.get_state()


    def update(self):
        self.traj.update(self.traj_input, self.traj_output)
        print(self.traj_output)
        self.motors.read_all_feedback()
        self.traj_output.pass_to_input(self.traj_input)

        for i, drive in enumerate(self.motors.drives):
            motors.set_target_state(i, arm.traj_output.new_position[i], arm.traj_output.new_velocity[i], 0.0)

        self.motors.send_saved_targets_async()

        if(not self.motors.all_within_error_limit(self.tracking_error_limit)):
            self.motors.disable_all()
            raise Exception("Motor tracking fault")
        

    # def set_arm_target_xy():



motors = MotorManager(bus)
motors.add_motor(arm_joint_0, 8.6666, 0.0)
motors.add_motor(arm_joint_1, 8.6666, 0.0)
motors.add_motor(arm_joint_2, 62.0/28.0, 0.0)

arm = Arm(motors)

arm_zero_position = [1.0, 1.0, 0.0]

arm.enable()
arm.traj_input.target_position = [1.0, 1.0, 1.0]

print("___RUNNING___")

elapsed_time = 0.0
try:
    while(True):
        arm.update()

        if(elapsed_time > 5.0):
            arm.traj_input.target_position = arm_zero

        print(arm.traj_output.new_position)
        print(arm.motors.get_state())

        # TODO: BAD! please fix this
        time.sleep(DT)
        elapsed_time += DT

except Exception as e:
    motors.disable_all()
    raise(e)

except KeyboardInterrupt as e:
    motors.disable_all()