import asyncio
from daisycontrol import slcanfd
from handle_lib.suction_gripper import SuctionGripper
from handle_lib.remote_joystick import RemoteJoystick

bus = slcanfd.slcanfdBus("/dev/serial/by-id/usb-Daisy_50x50-Drive_205634955130-if00")
gripper = SuctionGripper(bus)


joystick = RemoteJoystick("Thomass-MacBook-Pro.local", "5556")

async def main():
    task = asyncio.create_task(joystick.recv_loop())

    await gripper.enable()

    while(True):
        gripper_current = gripper.get_current()
        if(gripper_current > 260.0):
            print(gripper_current)
            print("RUMBLE")
            joystick.set_rumble(0.0, 0.4)
        else:
            joystick.set_rumble(0.0, 0.0)

        if(joystick.A()):
            gripper.set_voltage(30.0)
        else:
            gripper.set_voltage(0.0)
        await asyncio.sleep(0.01)

asyncio.run(main())