#include "stm32f1xx_hal.h"
#include "NRF24L01.h"

extern SPI_HandleTypeDef hspi1;
#define NRF24_SPI &hspi1

#define NRF24_CE_PORT GPIOA
#define NRF24_CE_PIN GPIO_PIN_3

#define NRF24_CSN_PORT GPIOA
#define NRF24_CSN_PIN GPIO_PIN_4

void CS_Select (void){
    HAL_GPIO_WritePin(NRF24_CSN_PORT, NRF24_CSN_PIN, GPIO_PIN_RESET);
}

void CS_UnSelect (void){
    HAL_GPIO_WritePin(NRF24_CSN_PORT, NRF24_CSN_PIN, GPIO_PIN_SET);
}

void CE_Enable (void){
    HAL_GPIO_WritePin(NRF24_CE_PORT, NRF24_CE_PIN, GPIO_PIN_SET);
}

void CE_Disable (void){
    HAL_GPIO_WritePin(NRF24_CE_PORT, NRF24_CE_PIN, GPIO_PIN_SET);
}

//Write a single byte to a register.
void nrf24_W_REGISTER (uint8_t Reg, uint8_t Data){
    uint8_t buf[2];
    buf[0] = Reg|1<<5;
    buf[1] = Data;

    // Pull the CS Pin LOW to select the device
    CS_Select();

    HAL_SPI_Transmit(NRF24_SPI, buf, 2, 1000);

    // Pull the CS Pin HIGH to release the device
    CS_UnSelect();
}

//Write a single byte to a register.
void nrf24_W_REGISTERmulti (uint8_t Reg, uint8_t *data, int size){
    uint8_t buf[2];
    buf[0] = Reg|1<<5;

    // Pull the CS Pin LOW to select the device
    CS_Select();

    HAL_SPI_Transmit(NRF24_SPI, buf, 1, 100);
    HAL_SPI_Transmit(NRF24_SPI, data, size, 1000);

    // Pull the CS Pin HIGH to release the device
    CS_UnSelect();
}

uint8_t nrf24_R_REGISTER (uint8_t Reg){
    uint8_t data=0;

    // Pull the CS Pin LOW to select the device
    CS_Select();

    HAL_SPI_Transmit(NRF24_SPI, &Reg, 1, 100);
    HAL_SPI_Transmit(NRF24_SPI, &data, 1, 100);

    // Pull the CS Pin HIGH to release the device
    CS_UnSelect();
}

void nrf24_R_REGISTERmulti (uint8_t Reg, uint8_t *data, int size){
    uint8_t data=0;

    // Pull the CS Pin LOW to select the device
    CS_Select();

    HAL_SPI_Transmit(NRF24_SPI, &Reg, 1, 100);
    HAL_SPI_Transmit(NRF24_SPI, data, size, 1000);

    // Pull the CS Pin HIGH to release the device
    CS_UnSelect();
}

void NRF24_SendCMD (uint8_t cmd){
    CS_Select();

    HAL_SPI_Transmit(NRF24_SPI, &cmd, 1, 100);

    CS_UnSelect();
}

void nrf24_Init (void){
    CE_Disable();

    nrf24_W_REGISTER(CONFIG, 0); //will configure later
    nrf24_W_REGISTER(EN_AA, 0); //no auto Ack
    nrf24_W_REGISTER(EN_RXADDR, 0); //Not enabling any data pipe atm
    nrf24_W_REGISTER(SETUP_AW, 0x03); //5 bytes for the tx/rx
    nrf24_W_REGISTER(SETUP_RETR, 0); //No retransmission
    nrf24_W_REGISTER(RF_CH, 0); //RF Channel being used.
    nrf24_W_REGISTER(RF_SETUP, 0x0E); //Power= 0db, data rate = 2Mbps

    CE_Enable();
}

//TX Setup
void NRF24_TxMode (uint8_t *Address, uint8_t channel){
    CE_Disable();

    nrf24_W_REGISTER(RF_CH, channel); // select the channel
    nrf24_W_REGISTERmulti(TX_ADDR, Address, 5); // write the TX address

    //power up device
    uint8_t config = nrf24_R_REGISTER(CONFIG);
    config = config | (1<<1);
    nrf24_W_REGISTER(CONFIG, config);

    CE_Enable();
}


uint8_t NRF24_Transmit (uint8_t *data){
    uint8_t cmdtosend = 0;

    CS_Select();

    //payload command
    cmdtosend = W_TX_PAYLOAD;
    HAL_SPI_Transmit (NRF24_SPI, &cmdtosend, 1, 100);

    // send the payload
    HAL_SPI_Transmit (NRF24_SPI, data, 32, 1000);

    CS_UnSelect();

    HAL_DeInit();

    HAL_Delay(1);

    uint8_t fifostatus = nrf24_R_REGISTER(FIFO_STATUS);

    if ((fifostatus&(1<<4)) && (!(fifostatus&(1<<3)))){
        cmdtosend = FLUSH_TX;
        NRF24_SendCMD(cmdtosend);
        return 1;
    }
    return 0;
}

void NRF24_RxMode (uint8_t *Address, uint8_t channel){
    CE_Disable();

    nrf24_W_REGISTER(RF_CH, channel); // select the channel

    //select data pipe 1
    uint8_t en_rxaddr = nrf24_R_REGISTER(EN_RXADDR);
    en_rxaddr = en_rxaddr | (1<<1);
    nrf24_W_REGISTER(EN_RXADDR, en_rxaddr);
    nrf24_W_REGISTERmulti(RX_ADDR_P1, Address, 5); // write the TX address
    nrf24_W_REGISTER(RX_PW_P1, 32); // 32 byte payload for pipe 1

    //power up device
    uint8_t config = nrf24_R_REGISTER(CONFIG);
    config = config | (1<<1) | (1<<0);
    nrf24_W_REGISTER(CONFIG, config);

    CE_Enable();
}

uint8_t isDataAvailable (int pipenum){
    uint8_t status = nrf24_R_REGISTER(STATUS);

    if ((status&(1<<6))&&(status&(pipenum<<1))){
        nrf24_W_REGISTER(STATUS, (1<<6));
        return 1;
    }
    return 0;
}

nrf24_Receive (uint8_t *data){
    uint8_t cmdtosend = 0;

    CS_Select();

    //payload command
    cmdtosend = R_RX_PAYLOAD;
    HAL_SPI_Transmit (NRF24_SPI, &cmdtosend, 1, 100);

    // send the payload
    HAL_SPI_Receive (NRF24_SPI, data, 32, 1000);

    CS_UnSelect();

    HAL_DeInit();

    HAL_Delay(1);

    cmdtosend = FLUSH_RX;
    NRF24_SendCMD(cmdtosend);
}