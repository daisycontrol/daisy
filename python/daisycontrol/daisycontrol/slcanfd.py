
import time
import logging
from typing import Any, Optional, Tuple, Union

from can import BusABC, CanProtocol, Message, BitTiming, BitTimingFd, typechecking
from can.exceptions import (
    CanInitializationError,
    CanInterfaceNotImplementedError,
    error_check
)

logger = logging.getLogger(__name__)


dlc_codes = [
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    12,
    16,
    20,
    24,
    32,
    48,
    64
]

def data_length_to_dlc_code(data_length):
    for dlc in range(len(dlc_codes)):
        if(data_length == dlc_codes[dlc]):
            return dlc

try:
    import serial
except ImportError:
    logger.warning(
        "You won't be able to use the slcan can backend without "
        "the serial module installed!"
    )
    serial = None

class slcanfdBus(BusABC):

    LINE_TERMINATOR = b"\r"

    _OK = b"\r"
    _ERROR = b"\a"

    _SLEEP_AFTER_SERIAL_OPEN = 0 # seconds to sleep after the interface has opened


    def __init__(
            self,
            channel: typechecking.ChannelStr,
            tty_baudrate: int = 115200,
            timing: Optional[Union[BitTiming, BitTimingFd]] = None,
            sleep_after_open: float = _SLEEP_AFTER_SERIAL_OPEN,
            rtscts: bool = False,
            listen_only: bool = False,
            timeout: float = 0.001,
            **kwargs: Any,
    ) -> None:
        
        self._listen_only = listen_only

        if serial is None:
            raise CanInterfaceNotImplementedError("The serial module is not installed")
        
        if not channel:
            raise ValueError("Must specify a serial port.")
        if "@" in channel:
            (channel, baudrate) = channel.split("@")
            tty_baudrate = int(baudrate)

        # Make serial port
        # NOTE: Opens I think
        with error_check(exception_type=CanInitializationError):
            self.serialPortOrig = serial.serial_for_url(
                channel,
                baudrate=tty_baudrate,
                rtscts=rtscts,
                timeout=timeout,
            )

        self._buffer = bytearray()
        self._can_protocol = CanProtocol.CAN_FD

        time.sleep(sleep_after_open) # TODO: WHAT

        # with error_check(exception_type=CanInitializationError):
        #     if 


        super().__init__(channel, **kwargs)


    # Write string to serial interface
    def _write(self, string: str) -> None:
        # print(string)
        with error_check("Could not write to serial device"):
            self.serialPortOrig.write(string.encode() + self.LINE_TERMINATOR)
            self.serialPortOrig.flush()


    def _read(self, timeout: Optional[float]) -> Optional[str]:
        _timeout = serial.Timeout(timeout)

        with error_check("Could not read from serial device"):
            while True:

                in_waiting = self.serialPortOrig.in_waiting
                for _ in range(max(1, in_waiting)):
                    new_byte = self.serialPortOrig.read(size=1)
                    if new_byte:
                        self._buffer.extend(new_byte)
                    else:
                        break

                    if new_byte in (self._ERROR, self._OK):
                        string = self._buffer.decode()
                        self._buffer.clear()
                        return string
                    
                if _timeout.expired():
                    break

            return None
        
    def flush(self) -> None:
        self._buffer.clear()
        with error_check("Could not flush"):
            self.serialPortOrig.reset_input_buffer()

    def open(self) -> None:
        if self._listen_only:
            self._write("L")
        else:
            self._write("O")

    def close(self) -> None:
        self._write("C")


    def send(self, msg: Message, timeout: Optional[float] = None) -> None:

        # Convert msg dlc into fc can dlc
        dlc = 0

        # TODO: make this a real exception
        if(not msg.dlc in dlc_codes):
            raise Exception("DLC is not in list of standard FD CAN dlc codes")
        
        dlc = data_length_to_dlc_code(msg.dlc)


        if timeout != self.serialPortOrig.write_timeout:
            self.serialPortOrig.write_timeout = timeout
        if msg.is_remote_frame:
            if msg.is_extended_id:
                sendStr = f"R{msg.arbitration_id:08X}{dlc:01X}"
            else:
                sendStr = f"r{msg.arbitration_id:03X}{dlc:01X}"
        else:
            if msg.is_extended_id:
                sendStr = f"T{msg.arbitration_id:08X}{dlc:01X}"
            else:
                sendStr = f"t{msg.arbitration_id:03X}{dlc:01X}"
            sendStr += msg.data.hex().upper()

        self._write(sendStr)


    def _recv_internal(
        self, timeout: Optional[float]
    ) -> Tuple[Optional[Message], bool]:
        canId = None
        remote = False
        extended = False
        data = None

        string = self._read(timeout)

        if not string:
            pass
        elif string[0] in (
            "T",
            "x",  # x is an alternative extended message identifier for CANDapter
        ):
            # extended frame
            canId = int(string[1:9], 16)
            dlc = dlc_codes[int(string[9], 16)]
            extended = True
            data = bytearray.fromhex(string[10 : 10 + dlc * 2])
        elif string[0] == "t":
            # normal frame
            canId = int(string[1:4], 16)
            dlc = dlc_codes[int(string[4], 16)]
            data = bytearray.fromhex(string[5 : 5 + dlc * 2])
        elif string[0] == "r":
            # remote frame
            canId = int(string[1:4], 16)
            dlc = dlc_codes[int(string[4], 16)]
            remote = True
        elif string[0] == "R":
            # remote extended frame
            canId = int(string[1:9], 16)
            dlc = dlc_codes[int(string[9], 16)]
            extended = True
            remote = True

        if canId is not None:
            msg = Message(
                arbitration_id=canId,
                is_extended_id=extended,
                timestamp=time.time(),  # Better than nothing...
                is_remote_frame=remote,
                dlc=dlc,
                data=data,
            )
            return msg, False
        return None, False