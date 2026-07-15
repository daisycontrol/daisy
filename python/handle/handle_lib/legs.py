from daisycontrol import slcanfd, bldc_drive
from handle_lib.motor_manager import MotorManager
import time
import numpy as np
from ruckig import Ruckig, InputParameter, OutputParameter, Result, ControlInterface

class Legs():
    def __init__(self, bus, DT):
        self.left_wheel = bldc_drive.BLDC_Drive(bus, 12)
        self.right_wheel = bldc_drive.BLDC_Drive(bus, 13)

        self.motors =  MotorManager(bus)


        self.motors.add_motor(self.left_wheel, 1.0, 0.0)
        self.motors.add_motor(self.right_wheel, 1.0, 0.0)

        self.wheel_traj = Ruckig(2, DT)
        self.wheel_traj_input = InputParameter(2)
        self.wheel_traj_output = OutputParameter(2)

        self.wheel_traj_input.max_velocity = [8.0, 8.0]
        self.wheel_traj_input.max_acceleration = [100.0, 100.0]
        self.wheel_traj_input.max_jerk = [5000.0, 5000.0]
        self.wheel_traj_input.current_velocity = [0.0, 0.0]
        self.wheel_traj_input.control_interface = ControlInterface.Velocity

        self.wheel_traj_input.target_velocity = [0.0, 0.0]

        self.max_position_difference = 0.6

    async def enable(self):
        await self.motors.init_all()
        await self.motors.enable_all()
        self.motors.zero_gains()
        await self.motors.impedance_control_all()
        self.motors.read_all_position()

        self.wheel_traj_input.current_position = [self.left_wheel.state.position_rads, self.right_wheel.state.position_rads]

        self.WHEEL_KD = 1000.0
        self.WHEEL_FF = 0.5

        self.left_wheel.set_controller_setpoint(self.left_wheel.state.position_rads, 0.0, 0.0)
        self.right_wheel.set_controller_setpoint(self.right_wheel.state.position_rads, 0.0, 0.0)

        self.left_wheel.set_controller_settings(150000.0, self.WHEEL_KD, 20.0, 0.99)
        self.right_wheel.set_controller_settings(150000.0, self.WHEEL_KD, 20.0, 0.99)

    def set_wheel_velocity_target(self, velocity, steering):
        left_velocity = velocity + steering
        right_velocity = - velocity + steering
        
        print(left_velocity)
        self.wheel_traj_input.target_velocity = [left_velocity, right_velocity]

    def update(self):
        self.motors.read_all_feedback()

        self.wheel_traj.update(self.wheel_traj_input, self.wheel_traj_output)
        self.wheel_traj_output.pass_to_input(self.wheel_traj_input)

        # position = self.wheel_traj_input.current_position
        # Leash position differences
        # if((position[0] - self.left_wheel.state.position_rads) > self.max_position_difference):
        #     position[0] = self.left_wheel.state.position_rads + self.max_position_difference
        # if((position[0] - self.left_wheel.state.position_rads) < -self.max_position_difference):
        #     position[0] = self.left_wheel.state.position_rads - self.max_position_difference

        # if((position[1] - self.right_wheel.state.position_rads) > self.max_position_difference):
        #     position[1] = self.right_wheel.state.position_rads + self.max_position_difference
        # if((position[1] - self.right_wheel.state.position_rads) < -self.max_position_difference):
        #     position[1] = self.right_wheel.state.position_rads - self.max_position_difference

        # self.wheel_traj_input.current_position = position

        self.left_wheel.setpoint.position_target_rads = self.wheel_traj_output.new_position[0]
        self.left_wheel.setpoint.velocity_target_radsps = self.wheel_traj_output.new_velocity[0]
        self.left_wheel.setpoint.torque_feedforward_Nm = self.WHEEL_FF * self.wheel_traj_output.new_velocity[0]

        self.right_wheel.setpoint.position_target_rads = self.wheel_traj_output.new_position[1]
        self.right_wheel.setpoint.velocity_target_radsps = self.wheel_traj_output.new_velocity[1]
        self.right_wheel.setpoint.torque_feedforward_Nm = self.WHEEL_FF * self.wheel_traj_output.new_velocity[1]


        self.left_wheel.set_saved_controller_setpoint_async()
        self.right_wheel.set_saved_controller_setpoint_async()

    async def disable(self):
        await self.motors.disable_all()