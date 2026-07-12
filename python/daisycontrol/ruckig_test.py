import time
from ruckig import Ruckig, InputParameter, OutputParameter, Result

DOF = 1
CONTROL_CYCLE = 0.01  # seconds, match your control loop rate

otg = Ruckig(DOF, CONTROL_CYCLE)
inp = InputParameter(DOF)
out = OutputParameter(DOF)


inp.current_position = [10.0]
inp.current_velocity = [0.0]
inp.current_acceleration = [0.0]

inp.max_velocity = [2.0]
inp.max_acceleration = [5.0]
inp.max_jerk = [30.0]

inp.target_position = [0.0]
inp.target_velocity = [0.0]
inp.target_acceleration = [0.0]

while(True):

    result = otg.update(inp, out)

    print(out)

    out.pass_to_input(inp)
    time.sleep(CONTROL_CYCLE)