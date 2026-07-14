import asyncio
from daisycontrol import slcanfd, bldc_drive
from hardware.bldc_drive.bldc_drive_messages import DriveMemory
from remote_joystick import RemoteJoystick

bus = slcanfd.slcanfdBus("/dev/serial/by-id/usb-Daisy_50x50-Drive_205634955130-if00")
drive = bldc_drive.BLDC_Drive(bus, 100)


joystick = RemoteJoystick("Thomass-MacBook-Pro.local", "5556")

async def main():
    task = asyncio.create_task(joystick.recv_loop())

    drive.set_raw_voltage(0.0, 0.0, 0.0)
    drive.request_state_change(bldc_drive.states.DriveState.DRIVE_RAW_VOLTAGE.value)

    while(True):

        if(joystick.A()):
            drive.set_raw_voltage(-12.0, 12.0, 0.0)
        else:
            drive.set_raw_voltage(0.0, 0.0, 0.0)
        await asyncio.sleep(0.05)

asyncio.run(main())