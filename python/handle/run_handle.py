from daisycontrol import slcanfd, bldc_drive
import time
import numpy as np
from ruckig import Ruckig, InputParameter, OutputParameter, Result, ControlInterface

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
            drive.update_state()

    def get_state(self):
        state = []
        for i, drive in enumerate(self.drives):
            state.append([drive.state.position_rads / self.ratios[i] - self.zeros[i], drive.state.velocity_radsps / self.ratios[i], drive.state.torque_Nm * self.ratios[i]])
        return np.array(state)

    def zero_gains(self):
        for drive in self.drives:
            drive.set_controller_settings(0.0, 0.0, 0.0, 0.99)

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
            time.sleep(0.01)

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
                return False
            
        return True


DT = 0.005

def bound(value, low, high):
    return max(low, min(high, value))

class Arm():
    def __init__(self, motor_manager):
        self.motors = motor_manager
        self.traj = Ruckig(3, DT)
        self.traj_input = InputParameter(3)
        self.traj_output = OutputParameter(3)

        self.traj_input.max_velocity = [2.0, 2.0, 2.0]
        self.traj_input.max_acceleration = [15.0, 15.0, 15.0]
        self.traj_input.max_jerk = [50.0, 50.0, 50.0]

        self.max_position = [2.2, 2.8, 2.0]
        self.min_position = [-2.2, -2.8, -2.0]

        self.tracking_error_limit = 0.4

        self.link_length_m = 0.28

    def enable(self):
        self.motors.init_all()
        self.motors.enable_all()

        self.motors.drives[0].set_controller_settings(60000.0, 800.0, 8.5, 0.995)
        self.motors.drives[1].set_controller_settings(60000.0, 800.0, 8.5, 0.995)
        self.motors.drives[2].set_controller_settings(100000.0, 1000.0, 7.0, 0.995)

        self.motors.read_all_position()

        # Initialize trajectory generator
        current_state = self.motors.get_state()
        self.traj_input.current_position = current_state[:, 0]
        self.traj_input.current_velocity = current_state[:, 1]

        self.motors.set_target_states(current_state)
        self.motors.send_saved_targets_async()
        
        time.sleep(0.01)
        self.motors.impedance_control_all()

    def get_state(self):
        self.motors.get_state()

    def make_ik_target(self, target_x, target_y, target_angle):
        q = np.zeros(3)

        L0 = 0.28
        L1 = 0.285
        L2 = 0.06


        # Radius limit to keep IK from exploding
        r = bound(np.sqrt(target_x**2 + target_y**2), 0.1, 0.5)
        angle = np.arctan2(target_y, target_x)
        
        target_x = r * np.cos(angle)
        target_y = r * np.sin(angle)

        # Calculate offset due to gripper offset
        sub_target_x = target_x - np.sin(-target_angle) * L2
        sub_target_y = target_y - np.cos(-target_angle) * L2


        q[1] = np.arccos((sub_target_x**2 + sub_target_y**2 - L0**2 - L1**2) / (2 * L0 * L1))
        q[0] = np.arctan2(sub_target_x, sub_target_y) - np.arctan(( L1 * np.sin(q[1]) ) / (L0 + L1 * np.cos(q[1])))
        q[2] = q[0] + q[1] + target_angle

        # Some flips for these joints
        q[1] = -q[1]
        q[0] = q[0]

        return q
    
    def set_target_angles(self, target_angles):
        for i in range(len(target_angles)):
            target_angles[i] = bound(target_angles[i], self.min_position[i], self.max_position[i])

        arm.traj_input.target_position = target_angles


    def update(self):
        self.traj.update(self.traj_input, self.traj_output)
        self.motors.read_all_feedback()
        self.traj_output.pass_to_input(self.traj_input)

        for i, drive in enumerate(self.motors.drives):
            motors.set_target_state(i, arm.traj_output.new_position[i], arm.traj_output.new_velocity[i], 0.0)

        self.motors.send_saved_targets_async()

        if(not self.motors.all_within_error_limit(self.tracking_error_limit)):
            self.motors.disable_all()
            raise Exception("Motor tracking fault")
        

    # def set_arm_target_xy():

class Legs():
    def __init__(self, motor_manager):
        self.motors = motor_manager

        self.wheel_traj = Ruckig(2, DT)
        self.wheel_traj_input = InputParameter(2)
        self.wheel_traj_output = OutputParameter(2)

        self.wheel_traj_input.max_velocity = [3.0, 3.0]
        self.wheel_traj_input.max_acceleration = [15.0, 15.0]
        self.wheel_traj_input.max_jerk = [20.0, 20.0]
        self.wheel_traj_input.current_velocity = [0.0, 0.0]
        self.wheel_traj_input.control_interface = ControlInterface.Velocity

        self.wheel_traj_input.target_velocity = [0.0, 0.0]

        self.max_position_difference = 0.1

    def enable(self):
        self.motors.init_all()
        self.motors.enable_all()
        self.motors.zero_gains()
        self.motors.impedance_control_all()
        self.motors.read_all_position()

        self.wheel_traj_input.current_position = [left_wheel.state.position_rads, right_wheel.state.position_rads]

        self.WHEEL_KD = 2000.0
        self.WHEEL_FF = 0.0

        left_wheel.set_controller_setpoint(left_wheel.state.position_rads, 0.0, 0.0)
        right_wheel.set_controller_setpoint(right_wheel.state.position_rads, 0.0, 0.0)

        left_wheel.set_controller_settings(100000.0, self.WHEEL_KD, 10.0, 0.99)
        right_wheel.set_controller_settings(100000.0, self.WHEEL_KD, 10.0, 0.99)
        time.sleep(1.0)

    def set_wheel_velocity_target(self, velocity, steering):
        left_velocity = velocity + steering
        right_velocity = - velocity + steering
        
        self.wheel_traj_input.target_velocity = [left_velocity, right_velocity]

    def update(self):
        self.motors.read_all_feedback()

        self.wheel_traj.update(self.wheel_traj_input, self.wheel_traj_output)
        self.wheel_traj_output.pass_to_input(self.wheel_traj_input)

        position = self.wheel_traj_input.current_position
        # Leash position differences
        if((position[0] - left_wheel.state.position_rads) > self.max_position_difference):
            position[0] = left_wheel.state.position_rads + self.max_position_difference
        if((position[0] - left_wheel.state.position_rads) < -self.max_position_difference):
            position[0] = left_wheel.state.position_rads - self.max_position_difference

        if((position[1] - right_wheel.state.position_rads) > self.max_position_difference):
            position[1] = right_wheel.state.position_rads + self.max_position_difference
        if((position[1] - right_wheel.state.position_rads) < -self.max_position_difference):
            position[1] = right_wheel.state.position_rads - self.max_position_difference

        self.wheel_traj_input.current_position = position

        left_wheel.setpoint.position_target_rads = self.wheel_traj_output.new_position[0]
        left_wheel.setpoint.velocity_radsps = self.wheel_traj_output.new_velocity[0]
        left_wheel.setpoint.torque_feedforward_Nm = self.WHEEL_FF * self.wheel_traj_output.new_velocity[0]

        right_wheel.setpoint.position_target_rads = self.wheel_traj_output.new_position[1]
        right_wheel.setpoint.velocity_radsps = self.wheel_traj_output.new_velocity[1]
        right_wheel.setpoint.torque_feedforward_Nm = self.WHEEL_FF * self.wheel_traj_output.new_velocity[1]


        left_wheel.set_saved_controller_setpoint_async()
        right_wheel.set_saved_controller_setpoint_async()

    def disable(self):
        self.motors.disable_all()
        

leg_motors = MotorManager(bus)
left_wheel = bldc_drive.BLDC_Drive(bus, 12)
right_wheel = bldc_drive.BLDC_Drive(bus, 13)
leg_motors.add_motor(left_wheel, 1.0, 0.0)
leg_motors.add_motor(right_wheel, 1.0, 0.0)

legs = Legs(leg_motors)


try:
    print("enable")
    legs.enable()
    while(True):
        print(legs.wheel_traj_output)
        print(legs.wheel_traj_input)
        legs.set_wheel_velocity_target(3.0, 0.0)
        legs.update()

        time.sleep(DT)

except KeyboardInterrupt:
    legs.disable()

# motors = MotorManager(bus)
# motors.add_motor(arm_joint_0, 8.6666, 2.1)
# motors.add_motor(arm_joint_1, 8.6666, 2.8)
# motors.add_motor(arm_joint_2, 62.0/28.0, -0.37)

# arm = Arm(motors)


# arm.enable()

# arm_zero_position = [-2.0, -2.73, 0.0]
# arm.traj_input.target_position = arm.make_ik_target(0.0, 0.28, 0.0)

# print("___RUNNING___")

# elapsed_time = 0.0
# try:
#     while(True):
#         arm.update()

#         arm.set_target_angles(arm.make_ik_target(0.3, 0.35, -0.9 + 1.5 * np.sin(elapsed_time * 1.4)))

#         if(elapsed_time > 25.0):
#             arm.set_target_angles(arm_zero_position)

#         # print(arm.traj_output.new_position)
#         print(arm.motors.get_state()[:, 0])

#         # TODO: BAD! please fix this
#         time.sleep(DT)
#         elapsed_time += DT

# except Exception as e:
#     motors.disable_all()
#     raise(e)

# except KeyboardInterrupt as e:
#     motors.disable_all()