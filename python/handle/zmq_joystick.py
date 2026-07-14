import pygame
import zmq
import struct

pygame.init()

def main():
    port = "5556"

    context = zmq.Context()
    socket = context.socket(zmq.PUB)
    socket.bind("tcp://localhost:%s" % port)

    joysticks = {}

    axes_values = [0] * 6
    button_values = [0] * 15

    clock = pygame.time.Clock()

    done = False
    while not done:
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

        joystick_count = pygame.joystick.get_count()

        for joystick in joysticks.values():
            jid = joystick.get_instance_id()

            for i in range(len(axes_values)):
                axes_values[i] = joystick.get_axis(i)

            for i in range(len(button_values)):
                button_values[i] = joystick.get_button(i)

            # joystick.rumble(0.0, 0.5, 100)

        joystick_send_data = axes_values + button_values
        socket.send(struct.pack('%sf' % len(joystick_send_data), *joystick_send_data))
        print(joystick_send_data)

        clock.tick(30)

if __name__ == "__main__":
    main()
    pygame.quit()