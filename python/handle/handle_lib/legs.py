class Legs():
    def __init__(self, bus):
        self.left_wheel = bldc_drive.BLDC_Drive(bus, 12)
        self.right_wheel = bldc_drive.BLDC_Drive(bus, 13)
        leg_motors.add_motor(left_wheel, 1.0, 0.0)
        leg_motors.add_motor(right_wheel, 1.0, 0.0)


        self.motors =  MotorManager(bus)

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