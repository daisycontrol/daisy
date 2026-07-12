from typing import overload

import can

from schema.common_messages import StateChange, EmptyMessage
from hardware.bldc_drive.bldc_drive_messages import EncoderState, DriveMemory, ControllerSettings, ControllerSetpoint, ControllerFeedback
from daisycontrol import ipc

STATE_CHANGE_ID = 1

CONTROLLER_SETTINGS_ID = 10
CONTROLLER_SETPOINT_ID = 11
ENCODER_STATE_ID = 12

READ_MEMORY_ID = 100
WRITE_MEMORY_ID = 101


class BLDC_Drive():
    def __init__(self, bus, can_id):
        self.bus = bus
        self.can_id = can_id

        self.memory = None

        self.state = None
        self.setpoint = ControllerSetpoint(
            0.0,
            0.0,
            0.0,
            0
        )

    def request_state_change(self, new_state):
        frame = ipc.make_ipc_frame(
            self.can_id,
            STATE_CHANGE_ID,
            StateChange(new_state, 0)
        )

        self.bus.send(frame)

        response = self.bus.recv()
        return StateChange.decode(response.data[1:])
    
    def set_controller_settings(self, kp, kd, max_current_A, position_filter):
        settings = ControllerSettings(
            kp,
            kd,
            int(max_current_A*1000.0),
            position_filter,
            0,
            0
        )

        frame = ipc.make_ipc_frame(
            self.can_id,
            CONTROLLER_SETTINGS_ID,
            settings
        )

        self.bus.send(frame)

        response = self.bus.recv()
        return EmptyMessage.decode(response.data[1:])
    
    def set_controller_setpoint(self, position_rads, velocity_radsps, torque_feedforward_Nm):
        setpoint = ControllerSetpoint(
            position_rads,
            velocity_radsps,
            torque_feedforward_Nm,
            0
        )

        frame = ipc.make_ipc_frame(
            self.can_id,
            CONTROLLER_SETPOINT_ID,
            setpoint
        )

        self.bus.send(frame)

        response = self.bus.recv()
        return ControllerFeedback.decode(response.data[1:])

    def set_controller_setpoint_async(self, position_rads, velocity_radsps, torque_feedforward_Nm):
        setpoint = ControllerSetpoint(
            position_rads,
            velocity_radsps,
            torque_feedforward_Nm,
            0
        )

        frame = ipc.make_ipc_frame(
            self.can_id,
            CONTROLLER_SETPOINT_ID,
            setpoint
        )

        self.bus.send(frame)

    def set_saved_controller_setpoint_async(self):
        frame = ipc.make_ipc_frame(
            self.can_id,
            CONTROLLER_SETPOINT_ID,
            self.setpoint
        )

        self.bus.send(frame)

    def read_controller_response_async(self, can_frame):
        if(can_frame.arbitration_id == self.can_id and can_frame.data[0] == CONTROLLER_SETPOINT_ID):
            self.state = ControllerFeedback.decode(can_frame.data[1:])
        

    def read_encoder_state(self):
        frame = ipc.make_ipc_frame(
            self.can_id,
            ENCODER_STATE_ID,
            EmptyMessage(0)
        )

        self.bus.send(frame)

        response = self.bus.recv()
        return EncoderState.decode(response.data[1:])

    def read_memory(self):
        frame = ipc.make_ipc_frame(
            self.can_id,
            READ_MEMORY_ID,
            EmptyMessage(0)
        )

        self.bus.send(frame)

        response = self.bus.recv()
        self.memory = DriveMemory.decode(response.data[1:])
        return self.memory
    
    def write_memory(self):
        if(not self.memory):
            raise Exception("Memory is empty: either read memory from drive, or write to drive.memory")
        if(self.memory.magic != 0x1234):
            raise Exception(f"Memory magic ({self.memory.magic}) is incorrect")

        frame = ipc.make_ipc_frame(
            self.can_id,
            WRITE_MEMORY_ID,
            self.memory
        )

        self.bus.send(frame)

        response = self.bus.recv()
        self.memory = DriveMemory.decode(response.data[1:])
        return self.memory