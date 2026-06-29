#include "comms.h"

using namespace comms;

// Preallocate headers and data buffers
FDCAN_TxHeaderTypeDef TxHeader;
FDCAN_RxHeaderTypeDef RxHeader;
uint8_t TxData[64];
uint8_t RxData[64];

FDCAN_TxHeaderTypeDef TxHeader_slcan;
uint8_t TxData_slcan[64];

bool slcan_open = false;


const uint16_t dlc_to_data_length[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, // 2.0
    12, 16, 20, 24, 32, 48, 64 // FD
};

const uint32_t dlc_to_dlc_def[] = {
    FDCAN_DLC_BYTES_0,
    FDCAN_DLC_BYTES_1,
    FDCAN_DLC_BYTES_2,
    FDCAN_DLC_BYTES_3,
    FDCAN_DLC_BYTES_4,
    FDCAN_DLC_BYTES_5,
    FDCAN_DLC_BYTES_6,
    FDCAN_DLC_BYTES_7,
    FDCAN_DLC_BYTES_8,
    FDCAN_DLC_BYTES_12,
    FDCAN_DLC_BYTES_16,
    FDCAN_DLC_BYTES_20,
    FDCAN_DLC_BYTES_24,
    FDCAN_DLC_BYTES_32,
    FDCAN_DLC_BYTES_48,
    FDCAN_DLC_BYTES_64
};

// CAN handle to use
FDCAN_HandleTypeDef* hfdcan;

// Device ID, also used as CAN ID
uint16_t device_id = 0;

void comms::set_device_id(uint16_t _device_id){
    device_id = _device_id;
}

uint16_t comms::get_device_id(){
    return device_id;
}

int comms::can_transmit(FDCAN_TxHeaderTypeDef *TxHeader, uint8_t *TxData)
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

void comms::can_receive(FDCAN_RxHeaderTypeDef *TxHeader, uint8_t *TxData)
{
    return;
}

void comms::initialize(FDCAN_HandleTypeDef* _hfdcan)
{
    hfdcan = _hfdcan;

    // Set up CAN header
    TxHeader.Identifier = device_id;
    TxHeader.IdType = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    TxHeader.DataLength = FDCAN_DLC_BYTES_8;
    TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
    TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.FDFormat = FDCAN_CLASSIC_CAN;

    // TODO: Filter doesn't seem to be working yet
    FDCAN_FilterTypeDef sFilterConfig;

    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex = 1;
    sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1 = 0x00;
    sFilterConfig.FilterID2 = 0x01;


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

// Convert from STM32 define to CAN FD dlc
// Use dlc_to_data_length to convert to number of bytes
uint32_t dlc_def_to_dlc(uint32_t dlc_def){
    for(int dlc = 0; dlc <= 15; dlc++){
        if(dlc_to_dlc_def[dlc] == dlc_def){
            return dlc;
        }
    }
    return 0;
}


void comms::slcan_usb_rx(uint8_t *Buf, uint32_t buffer_length)
{
    FDCAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[64];

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

        TxHeader_slcan.DataLength = dlc_to_dlc_def[dlc];
        TxHeader_slcan.Identifier = strtol(id_substring, NULL, 16);

        for (int i = 0; i < dlc_to_data_length[dlc]; i++)
        {
            char byte_substring[3];
            byte_substring[2] = '\0';
            strncpy(byte_substring, reinterpret_cast<const char*>(Buf + 1 + 4 + i * 2), 2);
            TxData_slcan[i] = strtol(byte_substring, NULL, 16);
        }

        // Fake loopback
        send_slcan_string(convert_tx_header(&TxHeader_slcan), TxData_slcan);

        slcan_open = false;
        can_transmit(&TxHeader_slcan, TxData_slcan);
        slcan_open = true;

        RxHeader = convert_tx_header(&TxHeader_slcan);
        can_receive(&RxHeader, TxData_slcan);
    }
}

void comms::send_slcan_string(FDCAN_RxHeaderTypeDef RxHeader, uint8_t *RxData)
{
    char tx_string[134]; // 5 + 2 * 62 + 1

    uint32_t number_of_data_bytes = dlc_to_data_length[dlc_def_to_dlc(RxHeader.DataLength)];

    // Set message type and ID
    // sprintf is valid here as we will override the termination character later
    if (RxHeader.IdType == FDCAN_STANDARD_ID)
    {
        if (RxHeader.RxFrameType == FDCAN_REMOTE_FRAME)
        {
            tx_string[0] = 'r';
        }
        else if (RxHeader.RxFrameType == FDCAN_DATA_FRAME)
        {
            tx_string[0] = 't';
        }

        sprintf(tx_string + 1, "%03x", RxHeader.Identifier);
    }
    else if (RxHeader.IdType == FDCAN_EXTENDED_ID)
    {
        if (RxHeader.RxFrameType == FDCAN_REMOTE_FRAME)
        {
            tx_string[0] = 'R';
        }
        else if (RxHeader.RxFrameType == FDCAN_DATA_FRAME)
        {
            tx_string[0] = 'T';
        }
        sprintf(tx_string + 1, "%011x", RxHeader.Identifier);
    }

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