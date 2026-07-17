import pygame
import zmq
import zmq.asyncio
import struct
import asyncio

pygame.init()

def deadzone(value, deadzone):
    if(value < deadzone and value > -deadzone):
        return 0.0
    return value



rumble = [0.0, 0.0]

async def main():
    port = "5556"
    host = "handle.here"

    context = zmq.asyncio.Context()
    socket = context.socket(zmq.PUB)
    socket.bind("tcp://*:%s" % port)

    sub_socket = context.socket(zmq.SUB)
    sub_socket.connect(f"tcp://{host}:{5557}")
    sub_socket.setsockopt(zmq.SUBSCRIBE, b"")

    async def rumble_recv():
        global rumble
        while(True):
            msg = await sub_socket.recv()
            values = list(struct.unpack("2f", msg))
            rumble[0] = values[0]
            rumble[1] = values[1]
            rumble_high = values[0]

    task = asyncio.create_task(rumble_recv())
    

    self.context = zmq.asyncio.Context()
    self.socket = self.context.socket(zmq.SUB)
    self.socket.connect(f"tcp://{self.host}:{self.port}")
    self.socket.setsockopt(zmq.SUBSCRIBE, b"")

    joysticks = {}

    axes_values = [0] * 6
    button_values = [0] * 15

    axes_values[4] = -1.0
    axes_values[5] = -1.0

    clock = pygame.time.Clock()

    screen = pygame.display.set_mode((500, 500))
    pygame.display.set_caption("Handle Control")

    font1 = pygame.font.SysFont("Robotto.ttf", 50)
    text1 = font1.render("hello", True, (255,255,255))

    textrect = text1.get_rect()
    textrect.center = (250, 250)


    done = False
    while not done:

        # Draw
        screen.fill((10,10,10))

        screen.blit(text1, textrect)

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                done = True

            if(event.type == pygame.JOYDEVICEADDED):
                joy = pygame.joystick.Joystick(event.device_index)
                joysticks[joy.get_instance_id()] = joy
                print(f"Joystick {joy.get_instance_id()} connencted")

            if(event.type == pygame.JOYDEVICEREMOVED):
                print("Joystick removed")
                done = True

        print(pygame.key.get_pressed()[pygame.K_a])

        joystick_count = pygame.joystick.get_count()

        for joystick in joysticks.values():
            jid = joystick.get_instance_id()

            for i in range(len(axes_values)):
                if(i < 4):
                    axes_values[i] = deadzone(joystick.get_axis(i), 0.15)
                else:
                    axes_values[i] = deadzone((joystick.get_axis(i) + 1.0) / 2.0, 0.15)
            for i in range(len(button_values)):
                button_values[i] = joystick.get_button(i)


            print(rumble)
            joystick.rumble(rumble[0], rumble[1], 100)

        joystick_send_data = axes_values + button_values
        socket.send(struct.pack('%sf' % len(joystick_send_data), *joystick_send_data))
        print(joystick_send_data)

        pygame.display.update()
        await asyncio.sleep(0.05)

if __name__ == "__main__":
    asyncio.run(main())
    pygame.quit()