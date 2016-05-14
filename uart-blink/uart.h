#ifndef __UART_H
#define __UART_H

#define ESP8266_REG(addr) *((volatile uint32_t *)(0x60000000+(addr)))
#define ESP8266_DREG(addr) *((volatile uint32_t *)(0x3FF00000+(addr)))

//UART INT Status
#define UIS   ESP8266_DREG(0x20020)
#define UIS0  0
#define UIS1  2

//UART 0 Registers
#define U0F   ESP8266_REG(0x000) //UART FIFO
#define U0IR  ESP8266_REG(0x004) //INT_RAW
#define U0IS  ESP8266_REG(0x008) //INT_STATUS
#define U0IE  ESP8266_REG(0x00c) //INT_ENABLE
#define U0IC  ESP8266_REG(0x010) //INT_CLEAR
#define U0D   ESP8266_REG(0x014) //CLKDIV
#define U0A   ESP8266_REG(0x018) //AUTOBAUD
#define U0S   ESP8266_REG(0x01C) //STATUS
#define U0C0  ESP8266_REG(0x020) //CONF0
#define U0C1  ESP8266_REG(0x024) //CONF1
#define U0LP  ESP8266_REG(0x028) //LOW_PULSE
#define U0HP  ESP8266_REG(0x02C) //HIGH_PULSE
#define U0PN  ESP8266_REG(0x030) //PULSE_NUM
#define U0DT  ESP8266_REG(0x078) //DATE
#define U0ID  ESP8266_REG(0x07C) //ID

//UART 1 Registers
#define U1F   ESP8266_REG(0xF00) //UART FIFO
#define U1IR  ESP8266_REG(0xF04) //INT_RAW
#define U1IS  ESP8266_REG(0xF08) //INT_STATUS
#define U1IE  ESP8266_REG(0xF0c) //INT_ENABLE
#define U1IC  ESP8266_REG(0xF10) //INT_CLEAR
#define U1D   ESP8266_REG(0xF14) //CLKDIV
#define U1A   ESP8266_REG(0xF18) //AUTOBAUD
#define U1S   ESP8266_REG(0xF1C) //STATUS
#define U1C0  ESP8266_REG(0xF20) //CONF0
#define U1C1  ESP8266_REG(0xF24) //CONF1
#define U1LP  ESP8266_REG(0xF28) //LOW_PULSE
#define U1HP  ESP8266_REG(0xF2C) //HIGH_PULSE
#define U1PN  ESP8266_REG(0xF30) //PULSE_NUM
#define U1DT  ESP8266_REG(0xF78) //DATE
#define U1ID  ESP8266_REG(0xF7C) //ID

//UART(uart) Registers
#define USF(u)   ESP8266_REG(0x000+(0xF00*(u&1))) //UART FIFO
#define USIR(u)  ESP8266_REG(0x004+(0xF00*(u&1))) //INT_RAW
#define USIS(u)  ESP8266_REG(0x008+(0xF00*(u&1))) //INT_STATUS
#define USIE(u)  ESP8266_REG(0x00c+(0xF00*(u&1))) //INT_ENABLE
#define USIC(u)  ESP8266_REG(0x010+(0xF00*(u&1))) //INT_CLEAR
#define USD(u)   ESP8266_REG(0x014+(0xF00*(u&1))) //CLKDIV
#define USA(u)   ESP8266_REG(0x018+(0xF00*(u&1))) //AUTOBAUD
#define USS(u)   ESP8266_REG(0x01C+(0xF00*(u&1))) //STATUS
#define USC0(u)  ESP8266_REG(0x020+(0xF00*(u&1))) //CONF0
#define USC1(u)  ESP8266_REG(0x024+(0xF00*(u&1))) //CONF1
#define USLP(u)  ESP8266_REG(0x028+(0xF00*(u&1))) //LOW_PULSE
#define USHP(u)  ESP8266_REG(0x02C+(0xF00*(u&1))) //HIGH_PULSE
#define USPN(u)  ESP8266_REG(0x030+(0xF00*(u&1))) //PULSE_NUM
#define USDT(u)  ESP8266_REG(0x078+(0xF00*(u&1))) //DATE
#define USID(u)  ESP8266_REG(0x07C+(0xF00*(u&1))) //ID

//UART INT Registers Bits
#define UITO    8 //RX FIFO TimeOut
#define UIBD    7 //Break Detected
#define UICTS   6 //CTS Changed
#define UIDSR   5 //DSR Change
#define UIOF    4 //RX FIFO OverFlow
#define UIFR    3 //Frame Error
#define UIPE    2 //Parity Error
#define UIFE    1 //TX FIFO Empty
#define UIFF    0 //RX FIFO Full

//UART STATUS Registers Bits
#define USTX    31 //TX PIN Level
#define USRTS   30 //RTS PIN Level
#define USDTR   39 //DTR PIN Level
#define USTXC   16 //TX FIFO COUNT (8bit)
#define USRXD   15 //RX PIN Level
#define USCTS   14 //CTS PIN Level
#define USDSR   13 //DSR PIN Level
#define USRXC    0 //RX FIFO COUNT (8bit)

//UART CONF0 Registers Bits
#define UCDTRI  24 //Invert DTR
#define UCRTSI  23 //Invert RTS
#define UCTXI   22 //Invert TX
#define UCDSRI  21 //Invert DSR
#define UCCTSI  20 //Invert CTS
#define UCRXI   19 //Invert RX
#define UCTXRST 18 //Reset TX FIFO
#define UCRXRST 17 //Reset RX FIFO
#define UCTXHFE 15 //TX Harware Flow Enable
#define UCLBE   14 //LoopBack Enable
#define UCBRK   8  //Send Break on the TX line
#define UCSWDTR 7  //Set this bit to assert DTR
#define UCSWRTS 6  //Set this bit to assert RTS
#define UCSBN   4  //StopBits Count (2bit) 0:disable, 1:1bit, 2:1.5bit, 3:2bit
#define UCBN    2  //DataBits Count (2bin) 0:5bit, 1:6bit, 2:7bit, 3:8bit
#define UCPAE   1  //Parity Enable
#define UCPA    0  //Parity 0:even, 1:odd

//UART CONF1 Registers Bits
#define UCTOE   31 //RX TimeOut Enable
#define UCTOT   24 //RX TimeOut Treshold (7bit)
#define UCRXHFE 23 //RX Harware Flow Enable
#define UCRXHFT 16 //RX Harware Flow Treshold (7bit)
#define UCFET   8  //TX FIFO Empty Treshold (7bit)
#define UCFFT   0  //RX FIFO Full Treshold (7bit)

typedef enum {
    EMPTY,
    UNDER_WRITE,
    WRITE_OVER
} RcvMsgBuffState;

typedef struct {
    uint32     RcvBuffSize;
    uint8     *pRcvMsgBuff;
    uint8     *pWritePos;
    uint8     *pReadPos;
    uint8      TrigLvl; //JLU: may need to pad
    RcvMsgBuffState  BuffState;
} RcvMsgBuff;

#endif
