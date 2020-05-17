/* LAST UPDATE 2020-05-15
=============================================
Use these defines in your main program, then include qqq_rfm69.h
=============================================
#define RFM69_SPI_SS_PIN   10
#define RFM69_HIGHPOWER    false
#define RFM69_SYNC1        0x2d
#define RFM69_SYNC2        0xd4
#define RFM69_FREQ         868.0
#define RFM69_POWER        13
//#define RFM69_USE_AUTOMODE //experimental...
#include <qqq_rfm69.h>
==============================================
Minified version of RadioHead driver
- No interrupts (saves a pin)
- Removed RadioHead data packet header bytes
- One modem config: GFSK_Rb250Fd250
- Set things as HIGHPOWER with #defines
- Use sleep instead of standby when not transmitting/receiving
============================================*/

#include <stdlib.h> //NULL
#include <inttypes.h>

class RH_RF69
{
	public:
	uint8_t   init();
	void   setFrequency(float mhz);
	//void setSyncWords(const uint8_t* syncWords, uint8_t len);
	void   setEncryptionKey(uint8_t* key);
	void   setTxPower(int8_t power);

	bool   send(const uint8_t* data, uint8_t len);
	bool   sendWait(const uint8_t* data, uint8_t len);
	bool   sending();
	bool   recv(uint8_t* buf, uint8_t* len);
	bool   available();
	int8_t lastRssi;
	void   sleep();
	bool   sleepWait();
	uint8_t getOpMode();

	private:
	int8_t _mode;
	int8_t _power; /// The selected output power in dBm
	void   setOpMode(uint8_t mode, bool setmode = true);
};


//end of header - c++ code pasted here so #defines from main program work...

//#include "qqqRFM69.h"

//=============================================================
// SPI
//=============================================================

// This is the bit in the SPI address that marks it as a write
#define RH_RF69_SPI_WRITE_MASK 0x80


#if defined (__AVR_ATmega328P__) || (defined __AVR_ATmega328__)
//arduino specific
#include <qqqFastPin.h>
#include <SPI.h>
void spi_setup() {
	fdigitalWrite(RFM69_SPI_SS_PIN, HIGH);
	fpinMode(RFM69_SPI_SS_PIN, OUTPUT);
	SPI.begin();
	SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0)); //max 10Mhz SPI speed for RFM69, will be reduced to max speed for used arduino
}
#define spi_ss() fdigitalWrite(RFM69_SPI_SS_PIN, LOW)
#define spi_nss() fdigitalWrite(RFM69_SPI_SS_PIN, HIGH)
#define spi_transfer(b) SPI.transfer(b)
#else
#include "qqq_spi.h"
#endif

uint8_t spiRead(uint8_t addr)
{
	spi_ss();
	spi_transfer(addr & 0x7F);
	uint8_t regval = spi_transfer(0);
	spi_nss();
	return regval;
}

void spiWrite(uint8_t addr, uint8_t value)
{
	spi_ss();
	spi_transfer(addr | 0x80);
	spi_transfer(value);
	spi_nss();
}

uint8_t spiBurstRead(uint8_t reg, uint8_t* dest, uint8_t len)
{
	uint8_t status = 0;
	//    ATOMIC_BLOCK_START;
	//    _spi.beginTransaction();
	spi_ss();
	status = spi_transfer(reg & ~RH_RF69_SPI_WRITE_MASK); // Send the start address with the write mask off
	while (len--) *dest++ = spi_transfer(0);
	spi_nss();
	//    _spi.endTransaction();
	//    ATOMIC_BLOCK_END;
	return status;
}

uint8_t spiBurstWrite(uint8_t reg, const uint8_t* src, uint8_t len)
{
	uint8_t status = 0;
	//    ATOMIC_BLOCK_START;
	//    _spi.beginTransaction();
	spi_ss();
	status = spi_transfer(reg | RH_RF69_SPI_WRITE_MASK); // Send the start address with the write mask on
	while (len--) spi_transfer(*src++);
	spi_nss();
	//    _spi.endTransaction();
	//    ATOMIC_BLOCK_END;
	return status;
}


//=============================================================
// RFM69
//=============================================================
// The crystal oscillator frequency of the RF69 module
#define RH_RF69_FXOSC 32000000.0

// The Frequency Synthesizer step = RH_RF69_FXOSC / 2^^19
#define RH_RF69_FSTEP  (RH_RF69_FXOSC / 524288)

// Register names
#define RH_RF69_REG_00_FIFO                                 0x00
#define RH_RF69_REG_01_OPMODE                               0x01
#define RH_RF69_REG_02_DATAMODUL                            0x02
#define RH_RF69_REG_03_BITRATEMSB                           0x03
#define RH_RF69_REG_04_BITRATELSB                           0x04
#define RH_RF69_REG_05_FDEVMSB                              0x05
#define RH_RF69_REG_06_FDEVLSB                              0x06
#define RH_RF69_REG_07_FRFMSB                               0x07
#define RH_RF69_REG_08_FRFMID                               0x08
#define RH_RF69_REG_09_FRFLSB                               0x09
#define RH_RF69_REG_0A_OSC1                                 0x0a
#define RH_RF69_REG_0B_AFCCTRL                              0x0b
#define RH_RF69_REG_0C_RESERVED                             0x0c
#define RH_RF69_REG_0D_LISTEN1                              0x0d
#define RH_RF69_REG_0E_LISTEN2                              0x0e
#define RH_RF69_REG_0F_LISTEN3                              0x0f
#define RH_RF69_REG_10_VERSION                              0x10
#define RH_RF69_REG_11_PALEVEL                              0x11
#define RH_RF69_REG_12_PARAMP                               0x12
#define RH_RF69_REG_13_OCP                                  0x13
#define RH_RF69_REG_14_RESERVED                             0x14
#define RH_RF69_REG_15_RESERVED                             0x15
#define RH_RF69_REG_16_RESERVED                             0x16
#define RH_RF69_REG_17_RESERVED                             0x17
#define RH_RF69_REG_18_LNA                                  0x18
#define RH_RF69_REG_19_RXBW                                 0x19
#define RH_RF69_REG_1A_AFCBW                                0x1a
#define RH_RF69_REG_1B_OOKPEAK                              0x1b
#define RH_RF69_REG_1C_OOKAVG                               0x1c
#define RH_RF69_REG_1D_OOKFIX                               0x1d
#define RH_RF69_REG_1E_AFCFEI                               0x1e
#define RH_RF69_REG_1F_AFCMSB                               0x1f
#define RH_RF69_REG_20_AFCLSB                               0x20
#define RH_RF69_REG_21_FEIMSB                               0x21
#define RH_RF69_REG_22_FEILSB                               0x22
#define RH_RF69_REG_23_RSSICONFIG                           0x23
#define RH_RF69_REG_24_RSSIVALUE                            0x24
#define RH_RF69_REG_25_DIOMAPPING1                          0x25
#define RH_RF69_REG_26_DIOMAPPING2                          0x26
#define RH_RF69_REG_27_IRQFLAGS1                            0x27
#define RH_RF69_REG_28_IRQFLAGS2                            0x28
#define RH_RF69_REG_29_RSSITHRESH                           0x29
#define RH_RF69_REG_2A_RXTIMEOUT1                           0x2a
#define RH_RF69_REG_2B_RXTIMEOUT2                           0x2b
#define RH_RF69_REG_2C_PREAMBLEMSB                          0x2c
#define RH_RF69_REG_2D_PREAMBLELSB                          0x2d
#define RH_RF69_REG_2E_SYNCCONFIG                           0x2e
#define RH_RF69_REG_2F_SYNCVALUE1                           0x2f
#define RH_RF69_REG_30_SYNCVALUE2                           0x30
// another 7 sync word bytes follow, 30 through 36 inclusive
#define RH_RF69_REG_37_PACKETCONFIG1                        0x37
#define RH_RF69_REG_38_PAYLOADLENGTH                        0x38
#define RH_RF69_REG_39_NODEADRS                             0x39
#define RH_RF69_REG_3A_BROADCASTADRS                        0x3a
#define RH_RF69_REG_3B_AUTOMODES                            0x3b
#define RH_RF69_REG_3C_FIFOTHRESH                           0x3c
#define RH_RF69_REG_3D_PACKETCONFIG2                        0x3d
#define RH_RF69_REG_3E_AESKEY1                              0x3e
// Another 15 AES key bytes follow
#define RH_RF69_REG_4E_TEMP1                                0x4e
#define RH_RF69_REG_4F_TEMP2                                0x4f
#define RH_RF69_REG_58_TESTLNA                              0x58
#define RH_RF69_REG_5A_TESTPA1                              0x5a
#define RH_RF69_REG_5C_TESTPA2                              0x5c
#define RH_RF69_REG_6F_TESTDAGC                             0x6f
#define RH_RF69_REG_71_TESTAFC                              0x71

// These register masks etc are named wherever possible
// corresponding to the bit and field names in the RFM69 Manual

// RH_RF69_REG_01_OPMODE
#define RH_RF69_OPMODE_SEQUENCEROFF                         0x80
#define RH_RF69_OPMODE_LISTENON                             0x40
#define RH_RF69_OPMODE_LISTENABORT                          0x20
#define RH_RF69_OPMODE_MODE                                 0x1c
#define RH_RF69_OPMODE_MODE_SLEEP                           0x00
#define RH_RF69_OPMODE_MODE_STDBY                           0x04
#define RH_RF69_OPMODE_MODE_FS                              0x08
#define RH_RF69_OPMODE_MODE_TX                              0x0c
#define RH_RF69_OPMODE_MODE_RX                              0x10

// RH_RF69_REG_02_DATAMODUL
#define RH_RF69_DATAMODUL_DATAMODE                          0x60
#define RH_RF69_DATAMODUL_DATAMODE_PACKET                   0x00
#define RH_RF69_DATAMODUL_DATAMODE_CONT_WITH_SYNC           0x40
#define RH_RF69_DATAMODUL_DATAMODE_CONT_WITHOUT_SYNC        0x60
#define RH_RF69_DATAMODUL_MODULATIONTYPE                    0x18
#define RH_RF69_DATAMODUL_MODULATIONTYPE_FSK                0x00
#define RH_RF69_DATAMODUL_MODULATIONTYPE_OOK                0x08
#define RH_RF69_DATAMODUL_MODULATIONSHAPING                 0x03
#define RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_NONE        0x00
#define RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_BT1_0       0x01
#define RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_BT0_5       0x02
#define RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_BT0_3       0x03
#define RH_RF69_DATAMODUL_MODULATIONSHAPING_OOK_NONE        0x00
#define RH_RF69_DATAMODUL_MODULATIONSHAPING_OOK_BR          0x01
#define RH_RF69_DATAMODUL_MODULATIONSHAPING_OOK_2BR         0x02

// RH_RF69_REG_11_PALEVEL
#define RH_RF69_PALEVEL_PA0ON                               0x80
#define RH_RF69_PALEVEL_PA1ON                               0x40
#define RH_RF69_PALEVEL_PA2ON                               0x20
#define RH_RF69_PALEVEL_OUTPUTPOWER                         0x1f

// RH_RF69_REG_23_RSSICONFIG
#define RH_RF69_RSSICONFIG_RSSIDONE                         0x02
#define RH_RF69_RSSICONFIG_RSSISTART                        0x01

// RH_RF69_REG_25_DIOMAPPING1
#define RH_RF69_DIOMAPPING1_DIO0MAPPING                     0xc0
#define RH_RF69_DIOMAPPING1_DIO0MAPPING_00                  0x00
#define RH_RF69_DIOMAPPING1_DIO0MAPPING_01                  0x40
#define RH_RF69_DIOMAPPING1_DIO0MAPPING_10                  0x80
#define RH_RF69_DIOMAPPING1_DIO0MAPPING_11                  0xc0

#define RH_RF69_DIOMAPPING1_DIO1MAPPING                     0x30
#define RH_RF69_DIOMAPPING1_DIO1MAPPING_00                  0x00
#define RH_RF69_DIOMAPPING1_DIO1MAPPING_01                  0x10
#define RH_RF69_DIOMAPPING1_DIO1MAPPING_10                  0x20
#define RH_RF69_DIOMAPPING1_DIO1MAPPING_11                  0x30

#define RH_RF69_DIOMAPPING1_DIO2MAPPING                     0x0c
#define RH_RF69_DIOMAPPING1_DIO2MAPPING_00                  0x00
#define RH_RF69_DIOMAPPING1_DIO2MAPPING_01                  0x04
#define RH_RF69_DIOMAPPING1_DIO2MAPPING_10                  0x08
#define RH_RF69_DIOMAPPING1_DIO2MAPPING_11                  0x0c

#define RH_RF69_DIOMAPPING1_DIO3MAPPING                     0x03
#define RH_RF69_DIOMAPPING1_DIO3MAPPING_00                  0x00
#define RH_RF69_DIOMAPPING1_DIO3MAPPING_01                  0x01
#define RH_RF69_DIOMAPPING1_DIO3MAPPING_10                  0x02
#define RH_RF69_DIOMAPPING1_DIO3MAPPING_11                  0x03

// RH_RF69_REG_26_DIOMAPPING2
#define RH_RF69_DIOMAPPING2_DIO4MAPPING                     0xc0
#define RH_RF69_DIOMAPPING2_DIO4MAPPING_00                  0x00
#define RH_RF69_DIOMAPPING2_DIO4MAPPING_01                  0x40
#define RH_RF69_DIOMAPPING2_DIO4MAPPING_10                  0x80
#define RH_RF69_DIOMAPPING2_DIO4MAPPING_11                  0xc0

#define RH_RF69_DIOMAPPING2_DIO5MAPPING                     0x30
#define RH_RF69_DIOMAPPING2_DIO5MAPPING_00                  0x00
#define RH_RF69_DIOMAPPING2_DIO5MAPPING_01                  0x10
#define RH_RF69_DIOMAPPING2_DIO5MAPPING_10                  0x20
#define RH_RF69_DIOMAPPING2_DIO5MAPPING_11                  0x30

#define RH_RF69_DIOMAPPING2_CLKOUT                          0x07
#define RH_RF69_DIOMAPPING2_CLKOUT_FXOSC_                   0x00
#define RH_RF69_DIOMAPPING2_CLKOUT_FXOSC_2                  0x01
#define RH_RF69_DIOMAPPING2_CLKOUT_FXOSC_4                  0x02
#define RH_RF69_DIOMAPPING2_CLKOUT_FXOSC_8                  0x03
#define RH_RF69_DIOMAPPING2_CLKOUT_FXOSC_16                 0x04
#define RH_RF69_DIOMAPPING2_CLKOUT_FXOSC_32                 0x05
#define RH_RF69_DIOMAPPING2_CLKOUT_FXOSC_RC                 0x06
#define RH_RF69_DIOMAPPING2_CLKOUT_FXOSC_OFF                0x07

// RH_RF69_REG_27_IRQFLAGS1
#define RH_RF69_IRQFLAGS1_MODEREADY                         0x80
#define RH_RF69_IRQFLAGS1_RXREADY                           0x40
#define RH_RF69_IRQFLAGS1_TXREADY                           0x20
#define RH_RF69_IRQFLAGS1_PLLLOCK                           0x10
#define RH_RF69_IRQFLAGS1_RSSI                              0x08
#define RH_RF69_IRQFLAGS1_TIMEOUT                           0x04
#define RH_RF69_IRQFLAGS1_AUTOMODE                          0x02
#define RH_RF69_IRQFLAGS1_SYNADDRESSMATCH                   0x01

// RH_RF69_REG_28_IRQFLAGS2
#define RH_RF69_IRQFLAGS2_FIFOFULL                          0x80
#define RH_RF69_IRQFLAGS2_FIFONOTEMPTY                      0x40
#define RH_RF69_IRQFLAGS2_FIFOLEVEL                         0x20
#define RH_RF69_IRQFLAGS2_FIFOOVERRUN                       0x10
#define RH_RF69_IRQFLAGS2_PACKETSENT                        0x08
#define RH_RF69_IRQFLAGS2_PAYLOADREADY                      0x04
#define RH_RF69_IRQFLAGS2_CRCOK                             0x02

// RH_RF69_REG_2E_SYNCCONFIG
#define RH_RF69_SYNCCONFIG_SYNCON                           0x80
#define RH_RF69_SYNCCONFIG_FIFOFILLCONDITION_MANUAL         0x40
#define RH_RF69_SYNCCONFIG_SYNCSIZE                         0x38
#define RH_RF69_SYNCCONFIG_SYNCSIZE_1                       0x00
#define RH_RF69_SYNCCONFIG_SYNCSIZE_2                       0x08
#define RH_RF69_SYNCCONFIG_SYNCSIZE_3                       0x10
#define RH_RF69_SYNCCONFIG_SYNCSIZE_4                       0x18
#define RH_RF69_SYNCCONFIG_SYNCSIZE_5                       0x20
#define RH_RF69_SYNCCONFIG_SYNCSIZE_6                       0x28
#define RH_RF69_SYNCCONFIG_SYNCSIZE_7                       0x30
#define RH_RF69_SYNCCONFIG_SYNCSIZE_8                       0x38
#define RH_RF69_SYNCCONFIG_SYNCSIZE_SYNCTOL                 0x07

// RH_RF69_REG_37_PACKETCONFIG1
#define RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE         0x80
#define RH_RF69_PACKETCONFIG1_DCFREE                        0x60
#define RH_RF69_PACKETCONFIG1_DCFREE_NONE                   0x00
#define RH_RF69_PACKETCONFIG1_DCFREE_MANCHESTER             0x20
#define RH_RF69_PACKETCONFIG1_DCFREE_WHITENING              0x40
#define RH_RF69_PACKETCONFIG1_DCFREE_RESERVED               0x60
#define RH_RF69_PACKETCONFIG1_CRC_ON                        0x10
#define RH_RF69_PACKETCONFIG1_CRCAUTOCLEAROFF               0x08
#define RH_RF69_PACKETCONFIG1_ADDRESSFILTERING              0x06
#define RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NONE         0x00
#define RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NODE         0x02
#define RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NODE_BC      0x04
#define RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_RESERVED     0x06

// RH_RF69_REG_3C_FIFOTHRESH
#define RH_RF69_FIFOTHRESH_TXSTARTCONDITION_NOTEMPTY        0x80
#define RH_RF69_FIFOTHRESH_FIFOTHRESHOLD                    0x7f

// RH_RF69_REG_3D_PACKETCONFIG2
#define RH_RF69_PACKETCONFIG2_INTERPACKETRXDELAY            0xf0
#define RH_RF69_PACKETCONFIG2_RESTARTRX                     0x04
#define RH_RF69_PACKETCONFIG2_AUTORXRESTARTON               0x02
#define RH_RF69_PACKETCONFIG2_AESON                         0x01

// RH_RF69_REG_4E_TEMP1
#define RH_RF69_TEMP1_TEMPMEASSTART                         0x08
#define RH_RF69_TEMP1_TEMPMEASRUNNING                       0x04

// RH_RF69_REG_5A_TESTPA1
#define RH_RF69_TESTPA1_NORMAL                              0x55
#define RH_RF69_TESTPA1_BOOST                               0x5d

// RH_RF69_REG_5C_TESTPA2
#define RH_RF69_TESTPA2_NORMAL                              0x70
#define RH_RF69_TESTPA2_BOOST                               0x7c

// RH_RF69_REG_6F_TESTDAGC
#define RH_RF69_TESTDAGC_CONTINUOUSDAGC_NORMAL              0x00
#define RH_RF69_TESTDAGC_CONTINUOUSDAGC_IMPROVED_LOWBETAON  0x20
#define RH_RF69_TESTDAGC_CONTINUOUSDAGC_IMPROVED_LOWBETAOFF 0x30

// These are indexed by the values of ModemConfigChoice
// Stored in flash (program) memory to save SRAM
// It is important to keep the modulation index for FSK between 0.5 and 10
// modulation index = 2 * Fdev / BR
// Note that I have not had much success with FSK with Fd > ~5
// You have to construct these by hand, using the data from the RF69 Datasheet :-(
// or use the SX1231 starter kit software (Ctl-Alt-N to use that without a connected radio)
#define CONFIG_FSK (RH_RF69_DATAMODUL_DATAMODE_PACKET | RH_RF69_DATAMODUL_MODULATIONTYPE_FSK | RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_NONE)
#define CONFIG_GFSK (RH_RF69_DATAMODUL_DATAMODE_PACKET | RH_RF69_DATAMODUL_MODULATIONTYPE_FSK | RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_BT1_0)
#define CONFIG_OOK (RH_RF69_DATAMODUL_DATAMODE_PACKET | RH_RF69_DATAMODUL_MODULATIONTYPE_OOK | RH_RF69_DATAMODUL_MODULATIONSHAPING_OOK_NONE)

// Choices for RH_RF69_REG_37_PACKETCONFIG1:
#define CONFIG_NOWHITE (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE | RH_RF69_PACKETCONFIG1_DCFREE_NONE | RH_RF69_PACKETCONFIG1_CRC_ON | RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NONE)
#define CONFIG_WHITE (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE | RH_RF69_PACKETCONFIG1_DCFREE_WHITENING | RH_RF69_PACKETCONFIG1_CRC_ON | RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NONE)
#define CONFIG_MANCHESTER (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE | RH_RF69_PACKETCONFIG1_DCFREE_MANCHESTER | RH_RF69_PACKETCONFIG1_CRC_ON | RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NONE)


uint8_t RH_RF69::init()
{
	spi_setup();
	
	//if (!RHSPIDriver::init())  return false;

	// Get the device type and check it
	// This also tests whether we are really connected to a device
	// My test devices return 0x24

	uint8_t attempt=1;
	while(1) {
	  uint8_t deviceType = spiRead(RH_RF69_REG_10_VERSION);
	  if (deviceType != 00 && deviceType != 0xff) break;
	  attempt++;
	  if(attempt>100) return 1;
	  //_delay_ms(1);
    }
//#if DEBUG
//	uart_printf("success attempt=%d\n",attempt);
//#endif

	// Configure important RH_RF69 registers
	// Here we set up the standard packet format for use by the RH_RF69 library:
	// 4 bytes preamble
	// 2 SYNC words 2d, d4
	// 2 CRC CCITT octets computed on the header, length and data (this in the modem config data)
	// 0 to 60 bytes data
	// RSSI Threshold -114dBm
	// We dont use the RH_RF69s address filtering: instead we prepend our own headers to the beginning
	// of the RH_RF69 payload
	spiWrite(RH_RF69_REG_3C_FIFOTHRESH, RH_RF69_FIFOTHRESH_TXSTARTCONDITION_NOTEMPTY | 0x0f); // thresh 15 is default
	// RSSITHRESH is default
	//    spiWrite(RH_RF69_REG_29_RSSITHRESH, 220); // -110 dbM
	// SYNCCONFIG is default. SyncSize is set later by setSyncWords()
	//    spiWrite(RH_RF69_REG_2E_SYNCCONFIG, RH_RF69_SYNCCONFIG_SYNCON); // auto, tolerance 0
	// PAYLOADLENGTH is default
	//    spiWrite(RH_RF69_REG_38_PAYLOADLENGTH, RH_RF69_FIFO_SIZE); // max size only for RX
	// PACKETCONFIG 2 is default
	spiWrite(RH_RF69_REG_6F_TESTDAGC, RH_RF69_TESTDAGC_CONTINUOUSDAGC_IMPROVED_LOWBETAOFF);
	// If high power boost set previously, disable it
	spiWrite(RH_RF69_REG_5A_TESTPA1, RH_RF69_TESTPA1_NORMAL);
	spiWrite(RH_RF69_REG_5C_TESTPA2, RH_RF69_TESTPA2_NORMAL);

	//sync words
	spiWrite(RH_RF69_REG_2E_SYNCCONFIG, 0b10001000); //sync=on, fill=syncadrint, words=2, tolerance=0
	spiWrite(RH_RF69_REG_2F_SYNCVALUE1, RFM69_SYNC1);
	spiWrite(RH_RF69_REG_30_SYNCVALUE2, RFM69_SYNC2);
	
	//setModemConfig(GFSK_Rb250Fd250);
	spiWrite(RH_RF69_REG_02_DATAMODUL,     CONFIG_GFSK);
	spiWrite(RH_RF69_REG_03_BITRATEMSB,    0x00);
	spiWrite(RH_RF69_REG_04_BITRATELSB,    0x80);
	spiWrite(RH_RF69_REG_05_FDEVMSB,       0x10);
	spiWrite(RH_RF69_REG_06_FDEVLSB,       0x00);
	spiWrite(RH_RF69_REG_19_RXBW,          0xe0);
	spiWrite(RH_RF69_REG_1A_AFCBW,         0xe0);
	spiWrite(RH_RF69_REG_37_PACKETCONFIG1, CONFIG_WHITE);
	
	spiWrite(RH_RF69_REG_2C_PREAMBLEMSB,   0x00);
	spiWrite(RH_RF69_REG_2D_PREAMBLELSB,   0x04);   // 3 would be sufficient, but this is the same as RF22's
	
	setFrequency(RFM69_FREQ);
	// No encryption
	setEncryptionKey(NULL);
	// +13dBm, same as power-on default
	setTxPower(RFM69_POWER);
	
	if(!sleepWait()) return 2; //NOTE: sleep() does not put module to sleep... (stays in standby mode)
	
	return 0;
}

void RH_RF69::setFrequency(float mhz)
{
	// Frf = FRF / FSTEP
	uint32_t frf = (uint32_t)((mhz * 1000000.0) / RH_RF69_FSTEP);
	spiWrite(RH_RF69_REG_07_FRFMSB, (frf >> 16) & 0xff);
	spiWrite(RH_RF69_REG_08_FRFMID, (frf >> 8) & 0xff);
	spiWrite(RH_RF69_REG_09_FRFLSB, frf & 0xff);
}

void RH_RF69::setEncryptionKey(uint8_t* key)
{
	if (key)
	{
		spiBurstWrite(RH_RF69_REG_3E_AESKEY1, key, 16);
		spiWrite(RH_RF69_REG_3D_PACKETCONFIG2, spiRead(RH_RF69_REG_3D_PACKETCONFIG2) | RH_RF69_PACKETCONFIG2_AESON);
	}
	else
	{
		spiWrite(RH_RF69_REG_3D_PACKETCONFIG2, spiRead(RH_RF69_REG_3D_PACKETCONFIG2) & ~RH_RF69_PACKETCONFIG2_AESON);
	}
}

void RH_RF69::setTxPower(int8_t power)
{
	_power = power;
	uint8_t palevel;

#if RFM69_HIGHPOWER	
	if (_power < -2) _power = -2; //RFM69HW only works down to -2.
	if (_power <= 13)
	{
		// -2dBm to +13dBm
		//Need PA1 exclusively on RFM69HW
		palevel = RH_RF69_PALEVEL_PA1ON | ((_power + 18) &
		RH_RF69_PALEVEL_OUTPUTPOWER);
	}
	else if (_power >= 18)
	{
		// +18dBm to +20dBm
		// Need PA1+PA2
		// Also need PA boost settings change when tx is turned on and off, see setModeTx()
		palevel = RH_RF69_PALEVEL_PA1ON
		| RH_RF69_PALEVEL_PA2ON
		| ((_power + 11) & RH_RF69_PALEVEL_OUTPUTPOWER);
	}
	else
	{
		// +14dBm to +17dBm
		// Need PA1+PA2
		palevel = RH_RF69_PALEVEL_PA1ON
		| RH_RF69_PALEVEL_PA2ON
		| ((_power + 14) & RH_RF69_PALEVEL_OUTPUTPOWER);
	}
#else
	if (_power < -18) _power = -18;
	if (_power > 13) _power = 13; //limit for RFM69W
	palevel = RH_RF69_PALEVEL_PA0ON	| ((_power + 18) & RH_RF69_PALEVEL_OUTPUTPOWER);
#endif
	spiWrite(RH_RF69_REG_11_PALEVEL, palevel);
}

bool RH_RF69::send(const uint8_t* data, uint8_t len)
{
	//XXX if (len > RH_RF69_MAX_MESSAGE_LEN) len=RH_RF69_MAX_MESSAGE_LEN;

	// Make sure we don't interrupt an outgoing message
	if(sending()) return false;

	//set mode of class + set power, but do not set actual mode of RFM69 to TX as this is handled by automode
	setOpMode(RH_RF69_OPMODE_MODE_TX,false);
		
	// Prevent RX while filling the fifo
	if(_mode != RH_RF69_OPMODE_MODE_SLEEP) setOpMode(RH_RF69_OPMODE_MODE_SLEEP);
	
	//clear fifo (fifo cleared on: stby/sleep->RX, RX->TX, TX->any; so fifo status unknown at this point)
	spiWrite(RH_RF69_REG_28_IRQFLAGS2,0b00010000);

#ifdef RFM69_USE_AUTOMODE
	//AutoMode: start TX on FIFO not empty, return to current state (sleep) on PacketSent
	spiWrite(RH_RF69_REG_3B_AUTOMODES,0b00111011);
#endif

	//Load FIFO with packet
	//ATOMIC_BLOCK_START;
	//_spi.beginTransaction();
	spi_ss();
	spi_transfer(RH_RF69_REG_00_FIFO | RH_RF69_SPI_WRITE_MASK); // Send the start address with the write mask on
	spi_transfer(len);
	while (len--) spi_transfer(*data++);
	spi_nss();
	//_spi.endTransaction();
	//ATOMIC_BLOCK_END;

#ifndef RFM69_USE_AUTOMODE
	// Start the transmitter
	setOpMode(RH_RF69_OPMODE_MODE_TX); 
#endif

	return true;
}

#ifdef RFM69_USE_AUTOMODE
bool RH_RF69::sending() 
{
	if(_mode != RH_RF69_OPMODE_MODE_TX) return false;
	_mode = getOpMode();
	return (_mode == RH_RF69_OPMODE_MODE_TX);
}
#else
bool RH_RF69::sending()
{
	if(_mode != RH_RF69_OPMODE_MODE_TX) return false;
	
	if (spiRead(RH_RF69_REG_28_IRQFLAGS2) & RH_RF69_IRQFLAGS2_PACKETSENT)
	{
		// A transmitter message has been fully sent
		setOpMode(RH_RF69_OPMODE_MODE_SLEEP); 
		return false;
	}
	return true;
}
#endif

bool RH_RF69::sendWait(const uint8_t* data, uint8_t len) {
	while(!send(data,len)) {}
	while(sending()) {}
	return true;
}

bool RH_RF69::available() {
	//exit if we're transmitting
	if (sending()) return false;

	// Make sure we are receiving
	if(_mode != RH_RF69_OPMODE_MODE_RX) {
		setOpMode(RH_RF69_OPMODE_MODE_RX);
		//clear fifo (fifo cleared on: stby/sleep->RX, RX->TX, TX->any; so fifo status unknown at this point)
		spiWrite(RH_RF69_REG_28_IRQFLAGS2,0b00010000);
	}
	
	// Must look for PAYLOADREADY, not CRCOK, since only PAYLOADREADY occurs _after_ AES decryption has been done
	if (!(spiRead(RH_RF69_REG_28_IRQFLAGS2) & RH_RF69_IRQFLAGS2_PAYLOADREADY)) return false;
	
	// A complete message has been received with good CRC
	lastRssi = -((int8_t)(spiRead(RH_RF69_REG_24_RSSIVALUE) >> 1));

	return true;
}


//returns true if message received, with len set to the actual received length which could be larger than the buffer size.
bool RH_RF69::recv(uint8_t* buf, uint8_t* len)
{
	if(!available()) return false;

	setOpMode(RH_RF69_OPMODE_MODE_SLEEP); //prevent receiving while reading fifo buffer
	
	// Save fifo to our buffer (Any junk remaining in the FIFO will be cleared next time we go to receive mode.)
	//ATOMIC_BLOCK_START;
	spi_ss();
	//SPI.beginTransaction();
	spi_transfer(RH_RF69_REG_00_FIFO); // Send the start address with the write mask off
	uint8_t payloadlen = spi_transfer(0); // First byte is payload len
	uint8_t rxlen = (payloadlen < *len ? payloadlen : *len);
	for (uint8_t i = 0; i < rxlen; i++) buf[i] = spi_transfer(0);
	spi_nss();
	//_spi.endTransaction();
	//ATOMIC_BLOCK_END;

	*len = payloadlen;
	return true;
}


void RH_RF69::setOpMode(uint8_t mode, bool setmode)
{
	//  if(_mode == mode) return;

#if RFM69_HIGHPOWER
	switch(mode) {
	case RH_RF69_OPMODE_MODE_STDBY:
		if (_power >= 18)
		{
			// If high power boost, return power amp to receive mode
			spiWrite(RH_RF69_REG_5A_TESTPA1, RH_RF69_TESTPA1_NORMAL);
			spiWrite(RH_RF69_REG_5C_TESTPA2, RH_RF69_TESTPA2_NORMAL);
		}
		break;
	case RH_RF69_OPMODE_MODE_RX:
		if (_power >= 18)
		{
			// If high power boost, return power amp to receive mode
			spiWrite(RH_RF69_REG_5A_TESTPA1, RH_RF69_TESTPA1_NORMAL);
			spiWrite(RH_RF69_REG_5C_TESTPA2, RH_RF69_TESTPA2_NORMAL);
		}
		break;
	case RH_RF69_OPMODE_MODE_TX:
		if (_power >= 18)
		{
			// Set high power boost mode
			// Note that OCP defaults to ON so no need to change that.
			spiWrite(RH_RF69_REG_5A_TESTPA1, RH_RF69_TESTPA1_BOOST);
			spiWrite(RH_RF69_REG_5C_TESTPA2, RH_RF69_TESTPA2_BOOST);
		}
		break;
	}
#endif

	if(mode == RH_RF69_OPMODE_MODE_SLEEP) {
		spiWrite(RH_RF69_REG_01_OPMODE, RH_RF69_OPMODE_MODE_SLEEP);
	}else{
		//set the requested mode on the rfm module
		if(setmode) {
			uint8_t opmode = spiRead(RH_RF69_REG_01_OPMODE);
			opmode &= ~RH_RF69_OPMODE_MODE;
			opmode |= (mode & RH_RF69_OPMODE_MODE);
			spiWrite(RH_RF69_REG_01_OPMODE, opmode);
		}
	}

	//set the requested mode in software
	_mode = mode;
}

inline void RH_RF69::sleep() {
	setOpMode(RH_RF69_OPMODE_MODE_SLEEP);
}

bool RH_RF69::sleepWait() {
	int cnt = 0;
	while(cnt<200){
		cnt++;
		spiWrite(RH_RF69_REG_01_OPMODE, RH_RF69_OPMODE_MODE_SLEEP );
		uint8_t mode = spiRead(RH_RF69_REG_01_OPMODE);
		if( (mode & RH_RF69_OPMODE_MODE) == RH_RF69_OPMODE_MODE_SLEEP) return true;
	}
	return false;
}

uint8_t RH_RF69::getOpMode()
{
	return spiRead(RH_RF69_REG_01_OPMODE) & RH_RF69_OPMODE_MODE;
}
