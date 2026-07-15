from daisycontrol import slcanfd, bldc_drive
import time
import numpy as np
from ruckig import Ruckig, InputParameter, OutputParameter, Result, ControlInterface

from .motor_manager import MotorManager
from .utils import bound



class Arm():
    def __init__(self, bus, DT):
        self.arm_joint_0 = bldc_drive.BLDC_Drive(bus, 1)
        self.arm_joint_1 = bldc_drive.BLDC_Drive(bus, 2)
        self.arm_joint_2 = bldc_drive.BLDC_Drive(bus, 10)


        self.motors = MotorManager(bus)

        self.motors.add_motor(self.arm_joint_0, 8.6666, 2.1)
        self.motors.add_motor(self.arm_joint_1, 8.6666, 2.8)
        self.motors.add_motor(self.arm_joint_2, 62.0/28.0, 1.3)

        self.traj = Ruckig(3, DT)
        self.traj_input = InputParameter(3)
        self.traj_output = OutputParameter(3)

        self.traj_input.max_velocity = [8.0, 8.0, 15.0]
        self.traj_input.max_acceleration = [80.0, 80.0, 80.0]
        self.traj_input.max_jerk = [1000.0, 1000.0, 1000.0]

        self.max_position = [2.2, 2.8, 2.0]
        self.min_position = [-2.15, -2.8, -2.0]
        self.zero_position = [-2.1, -2.75, 0.0]

        self.tracking_error_limit = 1.5

        self.link_length_m = 0.28

        self.zero_angle = 0.6

    async def enable(self):
        await self.motors.init_all()
        await self.motors.enable_all()

        self.motors.drives[0].set_controller_settings(100000.0, 800.0, 12.0, 0.995)
        self.motors.drives[1].set_controller_settings(60000.0, 800.0, 7.5, 0.995)
        self.motors.drives[2].set_controller_settings(200000.0, 1000.0, 14.0, 0.995)

        self.motors.read_all_position()

        # Initialize trajectory generator
        current_state = self.motors.get_state()
        self.traj_input.current_position = current_state[:, 0]
        self.traj_input.current_velocity = current_state[:, 1]

        self.motors.set_target_states(current_state)
        self.motors.send_saved_targets_async()
        
        time.sleep(0.01)
        await self.motors.impedance_control_all()

    def get_state(self):
        self.motors.get_state()

    def make_ik_target(self, target_x, target_y, target_angle):
        q = np.zeros(3)

        L0 = 0.27
        L1 = 0.28
        L2 = 0.08

        target_angle -= 1.0*self.zero_angle

        # Calculate offset due to gripper offset
        sub_target_x = target_x - np.sin(-target_angle) * L2
        sub_target_y = target_y - np.cos(-target_angle) * L2

        # Radius limit to keep IK from exploding
        r = bound(np.sqrt(sub_target_x**2 + sub_target_y**2), 0.15, 0.53)
        angle = np.arctan2(sub_target_y, sub_target_x)
        
        sub_target_x = r * np.cos(angle + self.zero_angle)
        sub_target_y = r * np.sin(angle + self.zero_angle)


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

        self.traj_input.target_position = target_angles


    async def update(self):
        self.traj.update(self.traj_input, self.traj_output)
        self.motors.read_all_feedback()
        self.traj_output.pass_to_input(self.traj_input)

        for i, drive in enumerate(self.motors.drives):
            self.motors.set_target_state(i, self.traj_output.new_position[i], self.traj_output.new_velocity[i], 0.0)

        self.motors.send_saved_targets_async()

        if(not self.motors.all_within_error_limit(self.tracking_error_limit)):
            await self.motors.disable_all()
            raise Exception("Motor tracking fault")