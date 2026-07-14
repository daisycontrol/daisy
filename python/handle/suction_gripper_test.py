import asyncio
from daisycontrol import slcanfd
from handle_lib.suction_gripper import SuctionGripper
from handle_lib.remote_joystick import RemoteJoystick

bus = slcanfd.slcanfdBus("/dev/serial/by-id/usb-Daisy_50x50-Drive_205634955130-if00")
gripper = SuctionGripper(bus)


joystick = RemoteJoystick("Thomass-MacBook-Pro.local", "5556")

async def main():
    task = asyncio.create_task(joystick.recv_loop())

    gripper.enable()

    while(True):

        if(joystick.A()):
            gripper.set_voltage(24.0)
        else:
            gripper.set_voltage(0.0)
        await asyncio.sleep(0.05)

asyncio.run(main())