## Mini IPC

The mini IPC system is a way to route messages from the host-side to functions on the micro-side and receive the response from that function.

Messages are built as follows:
[can_id][ipc_id][]

The ipc_id is NOT a message id. It is a function ID.

Each device will have different ipc_ids based on.

Why not "topics"? Topics are great for streaming data, but often you want the request and response from a function.

Daisy comms are always 