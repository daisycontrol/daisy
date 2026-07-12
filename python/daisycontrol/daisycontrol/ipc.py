import can

STATE_CHANGE_ID = 1
ENCODER_STATE_ID = 5

READ_MEMORY_ID = 10
WRITE_MEMORY_ID = 11


def make_ipc_frame(can_id, ipc_id, message):
    return can.Message(
        arbitration_id = can_id,
        data = bytes([ipc_id]) + message.encode(message),
        is_fd = True,
        is_extended_id = False
    )