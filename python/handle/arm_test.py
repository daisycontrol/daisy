from handle_lib.arm import Arm
from handle_lib.suction_gripper import SuctionGripper
from handle_lib.bus import arm_bus, suction_gripper_bus
from handle_lib.remote_joystick import RemoteJoystick
import asyncio
import time
import numpy as np
from enum import Enum

DT = 0.001
arm = Arm(arm_bus, DT)
gripper = SuctionGripper(suction_gripper_bus)

joystick = RemoteJoystick("Thomass-MacBook-Pro.local", "5556")

class State(Enum):
    RUN = 1
    PAUSE = 2

async def main():
    task = asyncio.create_task(joystick.recv_loop())

    await arm.enable()
    gripper.enable()
    
    target_x = 0.3
    target_y = 0.15
    target_angle = -np.pi/2.0

    state = State.PAUSE

    while True:
        if(joystick.A()):
            gripper.set_voltage(24.0)
        else:
            gripper.set_voltage(0.0)

        target_x += 0.5 * joystick.left_X() * DT
        target_y += 0.5 * joystick.left_Y() * DT
        target_angle += -3.0 * joystick.right_X() * DT

        print(target_x, target_y, target_angle)


        if(joystick.time_since_last() > 2.0):
            state = State.PAUSE
        if(joystick.B() and state != State.PAUSE):
            state = State.PAUSE
        if(joystick.X() and state != State.RUN):
            state = State.RUN


        if(state == State.PAUSE):
            arm.set_target_angles(arm.zero_position)
        elif(state == State.RUN):
            arm.set_target_angles(arm.make_ik_target(target_x, target_y, target_angle))

        await arm.update()
        await asyncio.sleep(DT)


try:
    asyncio.run(main())
except KeyboardInterrupt:
    asyncio.run(arm.motors.disable_all())
except Exception as e:
    asyncio.run(arm.motors.disable_all())
    raise e