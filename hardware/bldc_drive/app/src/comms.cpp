#include "comms.h"
#include "common/ipc/ipc.h"
#include "stm32g4xx_hal_fdcan.h"
#include <cstddef>
#include "drive_memory.h"

using namespace comms;

// Preallocate headers and data buffers
volatile FDCAN_TxHeaderTypeDef TxHeader;
FDCAN_RxHeaderTypeDef RxHeader;
// uint8_t TxData[64];
uint8_t RxData[64];

FDCAN_TxHeaderTypeDef TxHeader_slcan;

ipc::IPCRouter* ipc_router;

volatile uint8_t TxData_slcan[64];

bool slcan_open = false;


const uint8_t dlc_to_data_length[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, // 2.0
    12, 16, 20, 24, 32, 48, 64 // FD
};

uint8_t data_length_to_dlc(size_t len){
    switch(len){
        case(0):
            return FDCAN_DLC_BYTES_0;
        case(1):
            return FDCAN_DLC_BYTES_1;
        case(2):
            return FDCAN_DLC_BYTES_2;
        case(3):
            return FDCAN_DLC_BYTES_3;
        case(4):
            return FDCAN_DLC_BYTES_4;
        case(5):
            return FDCAN_DLC_BYTES_5;
        case(6):
            return FDCAN_DLC_BYTES_6;
        case(7):
            return FDCAN_DLC_BYTES_7;
        case(8):
            return FDCAN_DLC_BYTES_8;
        case(12):
            return FDCAN_DLC_BYTES_12;
        case(16):
            return FDCAN_DLC_BYTES_16;
        case(20):
            return FDCAN_DLC_BYTES_20;
        case(24):
            return FDCAN_DLC_BYTES_24;
        case(32):
            return FDCAN_DLC_BYTES_32;
        case(48):
            return FDCAN_DLC_BYTES_48;
        case(64):
            return FDCAN_DLC_BYTES_64;
        default:
            return FDCAN_DLC_BYTES_0;
    }
}

// CAN handle to use
FDCAN_HandleTypeDef* hfdcan;

int comms::transmit(const uint8_t* data, size_t len)
{
    TxHeader.Identifier = drive_memory.can_id;
    TxHeader.IdType = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    TxHeader.DataLength = data_length_to_dlc(len);
    TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
    TxHeader.FDFormat = FDCAN_FD_CAN;

    return can_transmit(const_cast<FDCAN_TxHeaderTypeDef*>(&TxHeader), data);
}

int comms::can_transmit(FDCAN_TxHeaderTypeDef* TxHeader, const uint8_t* TxData)
{
    if(slcan_open){
        send_slcan_string(convert_tx_header(TxHeader), TxData);
    }

    if(HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) == 0){
        return 0;
    }
    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, TxHeader, TxData) != HAL_OK)
    {
        return 0;
    }
    return 1;
}

void comms::can_receive(FDCAN_RxHeaderTypeDef *RxHeader, uint8_t *RxData)
{
    // Route message to ipc callback
    // 2000 is the generic address; all devices respond
    if(RxHeader->Identifier == drive_memory.can_id || RxHeader->Identifier == 2000){
        ipc_router->on_can_frame(RxData, static_cast<size_t>(dlc_to_data_length[RxHeader->DataLength]));
    }
}

void comms::initialize(FDCAN_HandleTypeDef* _hfdcan, ipc::IPCRouter* _router)
{
    hfdcan = _hfdcan;
    ipc_router = _router;

    // // TODO: Filter doesn't seem to be working yet
    FDCAN_FilterTypeDef sFilterConfig;

    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex = 1;
    sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1 = 0x00;
    sFilterConfig.FilterID2 = 0xFF;


    if (HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig) != HAL_OK)
    {
        /* Filter configuration Error */
        Error_Handler();
    }
    
    if (HAL_FDCAN_Init(hfdcan) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_FDCAN_Start(hfdcan) != HAL_OK)
    {
        Error_Handler();
    }

    // Activate the notification for new data in FIFO0 for FDCAN1
    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
    {
        /* Notification Error */
        Error_Handler();
    }
}

void comms::slcan_usb_rx(uint8_t *Buf, uint32_t buffer_length)
{
    FDCAN_RxHeaderTypeDef RxHeader;
    // uint8_t RxData[64];

    /////// Open interface
    if (Buf[0] == 'O')
    {
        // Check if CAN is already started
        if (HAL_FDCAN_GetState(&hfdcan1) == HAL_FDCAN_STATE_READY)
        {
            // Else, attempt to start CAN interface
            if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
            {
                Error_Handler();
            }
        }

        slcan_open = true;
    }

    /////// Close interface
    if (Buf[0] == 'C')
    {
        // Ensure that CAN has already been started
        if (HAL_FDCAN_GetState(&hfdcan1) != HAL_FDCAN_STATE_BUSY)
        {
            return;
        }

        // Else, attempt to close CAN interface
        if (HAL_FDCAN_Stop(&hfdcan1) != HAL_OK)
        {
            Error_Handler();
        }

        slcan_open = false;
    }

    TxHeader_slcan.FDFormat = FDCAN_FD_CAN;
    TxHeader_slcan.BitRateSwitch = FDCAN_BRS_OFF;
    TxHeader_slcan.TxFrameType = FDCAN_DATA_FRAME;


    /////// Send message
    if (Buf[0] == 't' || Buf[0] == 'T')
    { // 11 bit data frame

        int dlc;
        char id_substring[12];

        if (Buf[0] == 't')
        {
            strncpy(id_substring, reinterpret_cast<const char*>(Buf + 1), 3);
            id_substring[3] = '\0';
            TxHeader_slcan.IdType = FDCAN_STANDARD_ID;
            dlc = Buf[4] - '0';
            if(Buf[4] >= 'A'){
                dlc = Buf[4] - 'A' + 10;
            }
        }
        else
        {
            strncpy(id_substring, reinterpret_cast<const char*>(Buf + 1), 11);
            id_substring[11] = '\0';
            TxHeader_slcan.IdType = FDCAN_EXTENDED_ID;
            dlc = Buf[9] - '0';
            if(Buf[9] >= 'A'){
                dlc = Buf[9] - 'A' + 10;
            }
        }

        TxHeader_slcan.DataLength = dlc;
        TxHeader_slcan.Identifier = strtol(id_substring, NULL, 16);

        for (int i = 0; i < dlc_to_data_length[dlc]; i++)
        {
            char byte_substring[3];
            byte_substring[2] = '\0';
            strncpy(byte_substring, reinterpret_cast<const char*>(Buf + 1 + 4 + i * 2), 2);
            TxData_slcan[i] = strtol(byte_substring, NULL, 16);
        }

        // Fake loopback
        // send_slcan_string(convert_tx_header(&TxHeader_slcan), TxData_slcan);

        slcan_open = false;
        can_transmit(&TxHeader_slcan, const_cast<uint8_t*>(TxData_slcan));
        slcan_open = true;

        RxHeader = convert_tx_header(&TxHeader_slcan);
        can_receive(&RxHeader, const_cast<uint8_t*>(TxData_slcan));
    }
}

void comms::send_slcan_string(FDCAN_RxHeaderTypeDef RxHeader, const uint8_t *RxData)
{
    char tx_string[134]; // 5 + 2 * 62 + 1

    uint32_t number_of_data_bytes = dlc_to_data_length[RxHeader.DataLength];

    // Set message type and ID
    // sprintf is valid here as we will override the termination character later
    // if (RxHeader.IdType == FDCAN_STANDARD_ID)
    // {
    //     if (RxHeader.RxFrameType == FDCAN_REMOTE_FRAME)
    //     {
    //         tx_string[0] = 'r';
    //     }
    //     else if (RxHeader.RxFrameType == FDCAN_DATA_FRAME)
    //     {
    //         tx_string[0] = 't';
    //     }

    //     sprintf(tx_string + 1, "%03x", RxHeader.Identifier);
    // }
    // else if (RxHeader.IdType == FDCAN_EXTENDED_ID)
    // {
    //     if (RxHeader.RxFrameType == FDCAN_REMOTE_FRAME)
    //     {
    //         tx_string[0] = 'R';
    //     }
    //     else if (RxHeader.RxFrameType == FDCAN_DATA_FRAME)
    //     {
    //         tx_string[0] = 'T';
    //     }
    //     sprintf(tx_string + 1, "%011x", RxHeader.Identifier);
    // }

    // Assume we always have a data frame with standard id length.
    tx_string[0] = 't';
    sprintf(tx_string + 1, "%03x", RxHeader.Identifier);

    // Set DLC
    sprintf(tx_string + 4, "%01x",RxHeader.DataLength);

    for (int i = 0; i < number_of_data_bytes; i++)
    {
        // char byte_substring[3];
        sprintf(tx_string + 5 + i*2, "%02x", RxData[i]);
        // tx_string[5 + i*2] = byte_substring[0];
        // tx_string[6 + i*2] = byte_substring[0];
    }
    
    tx_string[5 + 2*number_of_data_bytes] = '\r';

    CDC_Transmit_FS((uint8_t*) tx_string, 5 + 2*number_of_data_bytes + 1);
}

// There must be a better way to do this
FDCAN_RxHeaderTypeDef comms::convert_tx_header(FDCAN_TxHeaderTypeDef *TxHeader){
    FDCAN_RxHeaderTypeDef RxHeader;
    RxHeader.BitRateSwitch = TxHeader->BitRateSwitch;
    RxHeader.DataLength = TxHeader->DataLength;
    RxHeader.Identifier = TxHeader->Identifier;
    RxHeader.IdType = TxHeader->IdType;

    RxHeader.RxFrameType = TxHeader->TxFrameType;

    return RxHeader;
}

// FDCAN1 Callback
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET)
  {
    // /* Retreive Rx messages from RX FIFO0 */
    // if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
    // {
    //   /* Reception Error */
    //   Error_Handler();
    // }

    // if(slcan_open){
    //     send_slcan_string(RxHeader, RxData);
    // }

    // // Handle device RX
    // can_receive(&RxHeader, RxData);


    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
    {
      /* Notification Error */
      Error_Handler();
    }
  }
}


void comms::handle_can_rx(FDCAN_HandleTypeDef *hfdcan){
    FDCAN_RxHeaderTypeDef RxHeaderRX;
    /* Retreive Rx messages from RX FIFO0 */
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeaderRX, RxData) != HAL_OK)
    {
      /* Reception Error */
      Error_Handler();
    }

    if(slcan_open){
        send_slcan_string(RxHeaderRX, RxData);
    }

    // Handle device RX
    can_receive(&RxHeaderRX, RxData);
}