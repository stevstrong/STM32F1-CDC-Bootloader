/*
 * usb.h
 *
 *  Created on: Apr 25, 2020
 *      Author: stevestrong
 */
// Use USB as virtual COM Port

#include <stddef.h>
#include "usbstd.h"
#include "usb_func.h"
#include "usb_desc.h"


//-----------------------------------------------------------------------------
//  Variables
//-----------------------------------------------------------------------------
command_t CMD;
lineCoding_t lineCoding;

typedef union status_t {
	// access as 16 bit in one read instruction
	uint16_t both;
	// -- or as separate 8 bit values --
	struct {
		uint8_t suspended;
		uint8_t configured;
	};
} status_t;
status_t usb_state;

uint16_t Dtr_Rts;
uint8_t deviceAddress;
const epTableAddress_t epTableAddr[2] = { //; // number of EPs
	{ .txAddr = (uint32*)EP_CTRL_TX_BUF_ADDRESS, .rxAddr = (uint32*)EP_CTRL_RX_BUF_ADDRESS },
	{ .txAddr = (uint32*)EP_DATA_TX_BUF_ADDRESS, .rxAddr = (uint32*)EP_DATA_RX_BUF_ADDRESS },
//	{ .txAddr = (uint32*)EP_COMM_TX_BUF_ADDRESS, .rxAddr = (uint32*)EP_COMM_RX_BUF_ADDRESS },
};

// constant to send zero byte packets
const uint8_t ZERO = 0;

//-----------------------------------------------------------------------------
// Function to initialize the VCP
//-----------------------------------------------------------------------------
void Class_Start(void)
{
	trace("START-");
	lineCoding.baudRate = BAUD_RATE;
	lineCoding.stopBits = 0;
	lineCoding.parityType = 0;
	lineCoding.dataBits = 8;
	Dtr_Rts = 0;
	deviceAddress = 0;
    usb_state.both = false;
}

//-----------------------------------------------------------------------------
// helper routines
//-----------------------------------------------------------------------------
void EnableUsbIRQ (void)
{
    NVIC_ISER[USB_IRQ_NUMBER/32] = ((uint32_t) 1) << (USB_IRQ_NUMBER % 32);
}

void DisableUsbIRQ (void)
{
    NVIC_ICER[USB_IRQ_NUMBER/32] = ((uint32_t) 1) << (USB_IRQ_NUMBER % 32);
}
//-----------------------------------------------------------------------------
void Stall_EPAddr(int epNum)
{
	trace("stall\n");
	uint32_t shift, mask;

	if ( epNum & 0x80 ) // IN EP
	{
		epNum &= 0x7F; // remove bit 7
		mask = EP_MASK_NoToggleBits | STAT_RX; // without STAT_TX and without both DTOG_x
		shift = 1 << 12;
	}
	else
	{
		mask = EP_MASK_NoToggleBits | STAT_TX; // without STAT_RX and without both DTOG_x
		shift = 1 << 4;
	}
	uint32_t data = USB_EpRegs(epNum);
	USB_EpRegs(epNum) = (data ^ shift) & mask;
}
//-----------------------------------------------------------------------------
void UnStall_EPAddr(int epNum)
{
	trace("unstall\n");
	uint32_t shift, mask;

	if ( epNum & 0x80 )
	{
		epNum &= 0x7F; // remove bit 7
		mask = EP_MASK_NoToggleBits | STAT_RX; // without STAT_TX and without both DTOG_x
		shift = 3 << 12; // RX = VALID
	}
	else
	{
		mask = EP_MASK_NoToggleBits | STAT_TX; // without STAT_RX and without both DTOG_x
		shift = 2 << 4; // TX = NAK
	}
	uint32_t data = USB_EpRegs(epNum);
	USB_EpRegs(epNum) = (data ^ shift) & mask;
}

//-----------------------------------------------------------------------------
void Stall(int ep)
{
	Stall_EPAddr(USB_EP_ADDR_OUT(ep));
	Stall_EPAddr(USB_EP_ADDR_IN(ep));
}
//-----------------------------------------------------------------------------
void UnStall(int ep)
{
	UnStall_EPAddr(USB_EP_ADDR_OUT(ep));
	UnStall_EPAddr(USB_EP_ADDR_IN(ep));
}

//-----------------------------------------------------------------------------
// mark EP ready to receive, set STAT_RX to "11" by toggling
//-----------------------------------------------------------------------------
void MarkBufferRxDone(int ep)
{
	strace("clrBuf logEpNum=%i\n", ep);
	uint32_t mask = EP_MASK_NoToggleBits | STAT_RX; // without STAT_TX and without both DTOG_x
	uint32_t data = USB_EpRegs(ep);
	USB_EpRegs(ep) = (data ^ STAT_RX) & mask;
}

//-----------------------------------------------------------------------------
// mark EP ready to transmit, set STAT_TX to "11" by toggling
//-----------------------------------------------------------------------------
void MarkBufferTxReady(int ep)
{
	strace("validateBuf logEpNum=%i\n", ep);
	uint32_t mask = EP_MASK_NoToggleBits | STAT_TX; // without STAT_RX and without both DTOG_x
	uint32_t data = USB_EpRegs(ep);
	USB_EpRegs(ep) = (data ^ STAT_TX) & mask;
}

//-----------------------------------------------------------------------------
// initialize the EPs
//-----------------------------------------------------------------------------
void InitEndpoints(void)
{
	trace("InitEPs\n");
	USB_CNTR = 1;          // Reset and clear Ints
	CMD.configuration = 0;
	CMD.transferLen = 0;   // no transfers
	CMD.packetLen = 0;
	CMD.transferPtr = 0;
	USB_CNTR = 0;          // release reset
	usb_state.suspended = false;
	usb_state.configured = false;

	// EP0 ist always reserved for control
	// the other endpoints must match the numbers written in the descriptors
	// see usb_desc.c: EP_DATA_IN, EP_DATA_OUT, EP_COMM_IN

	// EP0 = Control, IN und OUT
	EpTable[EP_CTRL].txOffset = EP_CTRL_TX_OFFSET;
	EpTable[EP_CTRL].txCount = 0;
	EpTable[EP_CTRL].rxOffset = EP_CTRL_RX_OFFSET;
	EpTable[EP_CTRL].rxCount = EP_RX_LEN_ID;

	// EP1 = Bulk IN and OUT
	EpTable[EP_DATA].txOffset = EP_DATA_TX_OFFSET;
	EpTable[EP_DATA].txCount = 0;
	EpTable[EP_DATA].rxOffset = EP_DATA_RX_OFFSET;
	EpTable[EP_DATA].rxCount = EP_RX_LEN_ID;

	// EP2 = Int IN and OUT
	EpTable[EP_COMM].txOffset = EP_COMM_TX_OFFSET;
	EpTable[EP_COMM].txCount = 0;
	EpTable[EP_COMM].rxOffset = EP_COMM_RX_OFFSET;
	EpTable[EP_COMM].rxCount = EP_RX_LEN_ID;

	USB_BTABLE = EP_TABLE_OFFSET;

	// CTRL EP
	USB_EP0R =			// EP0 = Control, IN and OUT
		(3 << 12) |		// STAT_RX = 3, Rx enabled
		(2 << 4) |		// STAT_TX = 2, NAK
		(1 << 9) |		// EP_TYPE = 1, Control
		EP_CTRL;
	// DATA EP
	USB_EP1R =			// EP1 = Bulk IN and OUT
		(3 << 12) |		// STAT_RX = 3, Rx enabled
		(2 << 4) |		// STAT_TX = 2, NAK
		(0 << 9) |		// EP_TYPE = 0, Bulk
		EP_DATA;
	// COMM EP
	USB_EP2R =			// EP2 = Int, IN und OUT
		(3 << 12) |		// STAT_RX = 3, Rx enabled
		(2 << 4) |		// STAT_TX = 2, NAK
		(3 << 9) |		// EP_TYPE = 3, INT
		EP_COMM;

	USB_ISTR = 0;          // clear pending Interrupts
	USB_CNTR =
		CTRM |             // Irq by ACKed Pakets
		RESETM |           // Irq by Reset
		SUSPM | WKUPM;

	USB_SetAddress(0);
}

//-----------------------------------------------------------------------------
// reads up to a given number of even bytes from control EP receive buffer
//-----------------------------------------------------------------------------
void Read_PMA(uint8_t* dest, uint32_t * src, int count)
{
	trace("rx="); ntrace(count, 0); trace("-");

	if (!count) return;

	if (count > EP_DATA_LEN)
		count = EP_DATA_LEN;

	int i = count/2;
	while (i--)
	{
		UMEM_FAKEWIDTH val = *src++;
		*dest++ = val;
		*dest++ = val>>8;
	}
	if (count&1) { // read last odd byte if any
		UMEM_FAKEWIDTH val = *src;
		*dest = val;
	}
}
//-----------------------------------------------------------------------------
// reads up to a given number of even bytes from control EP receive buffer
//-----------------------------------------------------------------------------
void ReadData(int ep, uint8_t* dest, int count)
{
	int rd = EpTable[ep].rxCount & 0x3FF;
	if (count>rd)
		count = rd;
	Read_PMA(dest, epTableAddr[ep].rxAddr, count);
	if (ep==EP_CTRL)
		MarkBufferRxDone(EP_CTRL); // release EP0 Rx
}
//-----------------------------------------------------------------------------
// writes up to 64 bytes into the specified EP transmit buffer
//-----------------------------------------------------------------------------
int SendData(int ep, uint8_t * src, int count)
{
	trace("tx="); ntrace(count, 0);

	if (count > EP_DATA_LEN)
		count = EP_DATA_LEN;

	EpTable[ep].txCount = count;
	if (count)
	{
		UMEM_FAKEWIDTH* dest = epTableAddr[ep].txAddr;
		int j = count/2;
		while (j--)
		{
		    register UMEM_FAKEWIDTH val = *src++;
		    val |= (*src++) << 8;
		    *dest++ = val;
		}
		if (count&1) // send last odd byte if any
		{
		    register UMEM_FAKEWIDTH val = *src;
		    *dest = val;
		}
	}
	MarkBufferTxReady(ep); // mark Tx buffer ready to be sent
	return count;
}

//-----------------------------------------------------------------------------
// Control-Transfers block transmission start
//-----------------------------------------------------------------------------
void TransmitSetupPacket(void)
{
	if ((CMD.setupPacket.bmRequestType & 0x80) == 0)
	{
		trace("DESC reqType&0x80=0 !\n");
		return;
	}
	int i = CMD.transferLen;
	if (i > CMD.packetLen)
		i = CMD.packetLen;
	uint8_t* Q = CMD.transferPtr; // source
	int j = SendData(EP_CTRL, Q, i);
	CMD.transferPtr = Q + j; // update pointer for any remaining data
	CMD.transferLen = CMD.transferLen - j; // number of remaining data
	if (CMD.transferLen <= 0)
		CMD.transferLen = 0;

	if (CMD.transferLen == 0) { trace("\n"); }
	else { trace("..\n"); }
}

/**********************************************************************/
/************    Handling of incoming Requests   **********************/
/**********************************************************************/
//-----------------------------------------------------------------------------
// USB-Request "SET FEATURE" and "CLEAR FEATURE"
//-----------------------------------------------------------------------------
void DoSetClearFeature(bool value)
{
    int feature = CMD.setupPacket.wValue;
    int reqType = CMD.setupPacket.bmRequestType;

    strace("scFeature for %02x\n",reqType);

    switch (reqType)
    {
    case USB_REQ_TYPE_DEVICE: // for Device
        trace("DEV\n");
        if (feature == 1)
            CMD.remoteWakeup = value;
        break;

    case USB_REQ_TYPE_INTERFACE: // for Interface
        trace("IFACE\n");
        break;

    case USB_REQ_TYPE_ENDPOINT: // for an Endpoint
        trace("EP\n");
        if (feature == 0)
        {
		int ep = CMD.setupPacket.wIndex;
            if (value == false)
                Stall(ep);
            else
                UnStall(ep);
        }
        break;

    default: // should not happen
        trace("?!?-");
        //Stall_EPAddr(EP_CTRL_ADDR_IN); // send NAK ???
        Stall_EPAddr(EP_CTRL_ADDR_OUT); // send NAK ???
        break;
    }
}

//-----------------------------------------------------------------------------
// CDC specific functions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// store DTR and RTS values
//-----------------------------------------------------------------------------
static void CDC_Set_DTR_RTS(void)
{
    trace("CDC_SetCtrl-");
    Dtr_Rts = CMD.setupPacket.wValue >> 8;
}
//-----------------------------------------------------------------------------
// read line coding parameters from the EP buffer
//-----------------------------------------------------------------------------
static void CDC_SetLineCoding(void)
{
	trace("CDC_SetLC-");
	ReadData(EP_CTRL, (uint8_t*) &lineCoding, sizeof(lineCoding_t));
}
//-----------------------------------------------------------------------------
// send line coding parameters to the host
//-----------------------------------------------------------------------------
static void CDC_GetLineCoding(void)
{
	trace("CDC_GetLC-");
	CMD.packetLen = EP_DATA_LEN;
	CMD.transferLen = sizeof(lineCoding_t);
	CMD.transferPtr = (uint8_t*) &lineCoding;
	TransmitSetupPacket();
}

//-----------------------------------------------------------------------------
// Setup request functions
//-----------------------------------------------------------------------------
void Req_GetStatus()
{
    trace("GET_STAT-");
//-----------------------------------------------------------------------------
// handling of USB-Request "GET STATUS"
//-----------------------------------------------------------------------------
	uint8_t buf[2];
	buf[0] = 0;

	int reqType = CMD.setupPacket.bmRequestType;

	switch (reqType&0x7F)
	{
	case USB_REQ_TYPE_DEVICE: // for Device
		trace("DEV\n");
		if (CMD.remoteWakeup)
			buf[0] |= 2;
		if (CMD.selfPowered)
			buf[0] |= 1;
		break;

	case USB_REQ_TYPE_INTERFACE: // for Interface
		trace("IFACE\n");
		break;

	case USB_REQ_TYPE_ENDPOINT: // for an Endpoint
	{
		int ep = CMD.setupPacket.wIndex;
		trace("EP\n");
		if ( (ep >= EP_CTRL) && (ep <= EP_DATA) )
			buf[0] = 1;
		break;
	}

	default:
		trace("?!?-");
		Stall_EPAddr(EP_CTRL_ADDR_OUT); // send NAK
		return;
	}

	buf[1] = 0;
	CMD.packetLen = EP_DATA_LEN;
	CMD.transferLen = 2;
	CMD.transferPtr = buf;
	TransmitSetupPacket();
}
//-----------------------------------------------------------------------------
void Req_ClearFeature()
{
    trace("CLR_FEAT-");
    DoSetClearFeature(false);
}
//-----------------------------------------------------------------------------
void Req_SetFeature()
{
    trace("SET_FEAT-");
    DoSetClearFeature(true);
}
//-----------------------------------------------------------------------------
void Req_SetAddress()
{
	trace("SET_ADDR-");
	deviceAddress = CMD.setupPacket.wValue;
	ACK();
}
//-----------------------------------------------------------------------------
// Send descriptors to the host
//-----------------------------------------------------------------------------
void Req_GetDescriptor()
{
    trace("GET_DESC-");
	const uint8_t* ptr;
	int aLen = -1;
	uint16_t ind = CMD.setupPacket.wValue & 0xFF;
	uint16_t type = CMD.setupPacket.wValue >> 8;

	#if ENABLE_TRACING
		char buf[30];
		sprintf(buf,"doGetDescr type %04x\n",type);
		trace(buf);
	#endif

	switch (type)
	{
	case USB_DT_DEVICE: // Get Device Descriptor
		{
			trace("DEV-");
			// descriptor index
			extern const uint8_t deviceDescriptor[];
			ptr = deviceDescriptor;
			aLen = ptr[0]; // bLength
			break;
		}
	case USB_DT_CONFIGURATION: // Get Configuration Descriptor
		{
			trace("CFG-");
			extern const uint8_t configDescriptor[];
			ptr = configDescriptor;
			aLen = ptr[2];			// wTotalLength low byte
			aLen |= (ptr[3] << 8);	// wTotalLength high byte
			break;
		}
	case USB_DT_STRING: // Get String Descriptor
		{
			trace("STR:"); ntrace(ind, 0); trace("-");
			extern const usb_string_descriptor* const descriptors [];

			if (ind<USB_STRING_LAST) {
				ptr = (const uint8_t*)descriptors[ind];
				aLen = ptr[0];
			} else {
				goto doStall; //Stall_EPAddr(EP_CTRL_ADDR_OUT); // out of range index. send NAK
			}
			break;
		}
	case USB_DT_DEVICE_QUALIFIER:
			trace("QUALIF-");
			goto doStall;
	default:
		{
			trace("!!!-");
			ntrace(type,0);
doStall:
			Stall_EPAddr(EP_CTRL_ADDR_OUT); // unknown. send NAK
			break;
		}
	}

	if (aLen < 0)
		return;

	// limit the number to that requested by the host
	if (aLen > CMD.setupPacket.wLength)
		aLen = CMD.setupPacket.wLength;
	CMD.packetLen = EP_DATA_LEN;
	CMD.transferLen = aLen;
	CMD.transferPtr = (uint8_t*) ptr;
	TransmitSetupPacket();
}
//-----------------------------------------------------------------------------
void Req_GetConfiguration()
{
	trace("GET_CFG-");

	CMD.packetLen = EP_DATA_LEN;
	CMD.transferLen = 1;
	CMD.transferPtr = (uint8_t*) &CMD.configuration;
	TransmitSetupPacket();
}
//-----------------------------------------------------------------------------
void Req_SetConfiguration()
{
	trace("SET_CFG-");

	if (CMD.setupPacket.wValue == 0)
	{
		trace("000-");
		CMD.configuration = CMD.setupPacket.wValue & 0xFF;
		usb_state.configured = false;
	}
	else
	{
		Class_Start();
		CMD.configuration = CMD.setupPacket.wValue & 0xFF;
		usb_state.configured = true;
	}
	ACK();
}
//-----------------------------------------------------------------------------
void Req_GetInterface()
{
	trace("GET_IFACE-");

	CMD.transferLen = 1;
	CMD.transferPtr = (uint8_t*) &ZERO;
	TransmitSetupPacket();
}
//-----------------------------------------------------------------------------
void Req_SetInterface()
{
	trace("SET_IFACE-");

	Class_Start();
	ACK();
}
//-----------------------------------------------------------------------------
//------------------------ Setup-Event ----------------------------------------
//-----------------------------------------------------------------------------
/*
 Note:
 1. take packet and clear buffer.
 2. Setup packets
 - if nothing follows, e.g. if there is not data phase, ACK is sent.
 - if something has to be sent to the host, this will be done right away.
   If the data length is greater than the EP buffer, only the first part is sent.
   The rest of the packets are be sent on the following Control-In stage(s).
   After the last data packet an ACK (0 length packet) is sent.
 - if the host is transmitting something, this is signalized in the setup packet.
   The data itself is retrieved in the IN and CTRL OUT packets.
   The read process is terminated by sending an ACK packet.
*/
//-----------------------------------------------------------------------------
voidFuncPtr const reqFunc[] = {
		Req_GetStatus,			// USB_REQ_GET_STATUS = 0
		Req_ClearFeature,		// USB_REQ_CLEAR_FEATURE = 1
		NULL,					// RESERVED = 2
		Req_SetFeature,			// USB_REQ_SET_FEATURE = 3
		NULL,					// 4
		Req_SetAddress,			// USB_REQ_SET_ADDRESS = 5
		Req_GetDescriptor,		// USB_REQ_GET_DESCRIPTOR = 6
		NULL, //Req_SetDescriptor,		// USB_REQ_SET_DESCRIPTOR = 7
		Req_GetConfiguration,	// USB_REQ_GET_CONFIGURATION = 8
		Req_SetConfiguration,	// USB_REQ_SET_CONFIGURATION = 9
		Req_GetInterface,		// USB_REQ_GET_INTERFACE = 10
		Req_SetInterface,		// USB_REQ_SET_INTERFACE = 11
		NULL,				// USB_REQ_SET_SYNCH_FRAME = 12
};
//-----------------------------------------------------------------------------
void OnSetup(void)
{
	// read the setup packet form CTRL EP buffer
	ReadData(EP_CTRL, (uint8_t*)&CMD.setupPacket, 8);

	if (IsStandardRequest()) // Type = Standard
	{
		trace("STD-");

		int bReq = CMD.setupPacket.bRequest;
		if (bReq<USB_REQ_LAST)
		{
			voidFuncPtr func = reqFunc[bReq];
			if (func) {
				func();
				return;
			}
		}
        // any other request will receive Stall, also see below
    }
	else if (IsClassRequest()) // Type = Class
	{
		trace("CLASS-");
		int bReq = CMD.setupPacket.bRequest;
		switch (bReq)
		{
		// CDC requests
		case USB_CDC_REQ_GET_LINE_CODING:
			CDC_GetLineCoding(); //  transmitted data
			return;
		case USB_CDC_REQ_SET_LINE_CODING:
			trace("CDC_SetLC-");
			break;

		case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
			trace("CDC_Set_Ctrl-");
			break;

		case USB_CDC_REQ_SEND_BREAK:
			trace("CDC_Send_Brk-");
			break;
		default:
			trace("?!?-");
			ntrace(bReq, 0);
			break;

		// handling of any other class specific requests should be done here

		}
		ACK();
		return;
	}
	trace("REQ_?!?-");

	// if request not recognized then send NAK
	Stall_EPAddr(EP_CTRL_ADDR_OUT);
}

//-----------------------------------------------------------------------------
//  specific EP interrupt service routines in case of received data
//-----------------------------------------------------------------------------
void OnEpCtrlOut(void) // Control-EP OUT
{
	if (IsStandardRequest()) // reqType = Standard
	{
		// usually only zero length packets, ACK from Host, but also possible (although not yet seen) bRequest=7 = SET_DESCRIPTOR
		// nothing to do, the buffer was already read out
		trace("STD-");
	}
	else if (IsClassRequest()) // reqType = Class
	{
		trace("CLASS-");
		switch (CMD.setupPacket.bRequest)
		{
		case USB_CDC_REQ_SET_LINE_CODING:
			CDC_SetLineCoding();
			break;

		case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
            CDC_Set_DTR_RTS();
			break;

		case USB_CDC_REQ_SEND_BREAK:
			trace("CDC_Send_Brk-");
			// not implemented, but send anyway an ACK
			break;

		default:
			break;
		}
	}
	else { trace("?!?-"); } // should not happen
	// we do not handle Vendor-Request
	//ACK(); // not necessary, it is just for us to know that everything went ok.
	trace("Ok\n");
}
//-----------------------------------------------------------------------------
// finished control packet transmitted from device to host
//-----------------------------------------------------------------------------
void OnEpCtrlIn(void) // Control-EP IN
{
	if (IsStandardRequest()) // if Type = Standard
	{
		trace("STD-");
		if (CMD.transferLen > 0) // continue transmitting the rest of data if any
		{
			trace("(cont)-");
			TransmitSetupPacket();
			return;
		}
	}
	else if (IsClassRequest()) // reqType = Class
	{
		trace("CLASS-");
	}
	else { trace("?!?-"); }
	//ACK(); // not necessary, is just for us to know that packet has been sent.
	trace("Done\n");
}

//-----------------------------------------------------------------------------
// manage data to be transmitted to host via EP_DATA IN
//-----------------------------------------------------------------------------
void OnEpBulkIn(void)
{
//	SendData(EP_DATA, (uint8*)&ZERO, 0);
	trace("done\n");
}
//-----------------------------------------------------------------------------
// Read received bytes and write them directly into the ring buffer
// This need special handling due to the "ring" characteristic,
// to safeguard the write pointer against going out of boundary
//-----------------------------------------------------------------------------
void SendError(error_t err)
{
	trace("ERR:"); ntrace(err, 0); trace("-");
	SendData(EP_DATA, &err, sizeof(error_t));
	trace("\n");
}

//-----------------------------------------------------------------------------
uint8_t rx_buf[EP_DATA_LEN];
int num_pages; // number of total pages to flash
int crt_page; // currently flashed pages
int page_offset, page_len, header_ok;
cmd_t _cmd;
//-----------------------------------------------------------------------------
error_t CheckHeader(uint16 rxd, uint8 _id)
{
	// data should be command, plausibility check
	if (rxd!=sizeof(cmd_t))
	{
		trace("~NO_LEN~");
		return CMD_WRONG_LENGTH;
	}
	// read header
	ReadData(EP_DATA, _cmd.data, rxd);
	// check crc
	if ( Check_CRC(_cmd.data, sizeof(cmd_t))==0 )
	{
		trace("~NO_CRC~");
		return CMD_WRONG_CRC;
	}
	// valid command received. check for id
	if (_cmd.id!=_id)
	{
		trace("~NO_ID~");
		return CMD_WRONG_ID;
	}
	// echo back the header
	SendData(EP_DATA, _cmd.data, sizeof(cmd_t));
	return NO_ERROR;
}

//-----------------------------------------------------------------------------
void OnEpBulkOut(void)
{
	error_t err = NO_ERROR;
	// read number of available bytes
	uint16_t rxd = EpTable[EP_DATA].rxCount & 0x3FF;

	if (num_pages==0)
	{	// check for header to set number of pages
		err = CheckHeader(rxd, 0x20);
		if ( err==NO_ERROR )
		{
			num_pages = _cmd.page; // this will be used to detect flash_complete
		}
	}
	else if (header_ok==0)
	{	// check for data header
		err = CheckHeader(rxd, 0x21);
		if ( err==NO_ERROR )
		{ // prepare data stage
			TIME_STAMP
			page_offset = 0;
			header_ok = 1;
			page_len = _cmd.data_len;
			MarkBufferRxDone(EP_DATA); // release data EP Rx for next OUT packet
			// erase the corresponding page
			LED_ON;
			flash_erase_page( (uint16_t*) ( USER_PROGRAM + (crt_page * PAGE_SIZE) ));
			LED_OFF;
			return;
		}
	}
	else if (page_len>0)
	{	// data stage. store data packet into buffer
		ReadData(EP_DATA, rx_buf, rxd);
		// flash the data from buffer
		flash_write_data( (uint16_t*) ( USER_PROGRAM + (crt_page * PAGE_SIZE) + page_offset), (uint16_t*)rx_buf, (rxd+1)>>1);
		// update page index
		page_offset += rxd;
		// check if buffer full
		if (page_offset>=page_len)
		{	// it was the last data packet from the current page. prepare header stage
			++crt_page;
			header_ok = 0;
			page_len = 0;
		}
	}

	if (err)
		SendError(err);

	MarkBufferRxDone(EP_DATA); // release data EP Rx for next OUT packet

}
//-----------------------------------------------------------------------------
//--------------- USB-Interrupt-Handler ---------------------------------------
//-----------------------------------------------------------------------------
void NAME_OF_USB_IRQ_HANDLER(void)
{
    uint32_t irqStatus = USB_ISTR; // Interrupt-Status

	if (irqStatus & WKUP) // Suspend-->Resume
	{
		trace("WKUP\n");
		USB_CNTR &= ~(FSUSP | LP_MODE);
		usb_state.suspended = false;
	}
	else if (irqStatus & SUSP) // after 3 ms break -->Suspend
	{
		trace("SUSP\n");
		usb_state.suspended = true;
		USB_CNTR |= (FSUSP | LP_MODE);
	}

	// clear here the other interrupt bits
	USB_ISTR = ~(PMAOVR|ERR|WKUP|SUSP|RESET|SOF|ESOF); // clear diverse IRQ bits

	if (irqStatus & RESET) // Bus Reset
	{
		trace("RESET\n");
		CMD.configuration = 0;
		InitEndpoints();
	}
	else
	{	// Endpoint Interrupts
		while ( (irqStatus = USB_ISTR) & CTR )
		{
			USB_ISTR = ~CTR; // clear IRQ bit
			uint8_t ep = irqStatus & MASK_EP;
			uint16_t epStatus = USB_EpRegs(ep);

			if (irqStatus & DIR) // OUT packet sent by host and received by device
			{
				USB_EpRegs(ep) = epStatus & ~CTR_RX & EP_MASK_NoToggleBits;

				trace("->");

				if (ep == EP_CTRL)
				{
					if (epStatus & SETUP)
					{
						trace("SETUP-");
						OnSetup(); // Handle the Setup-Packet
					}
					else
					{
						trace("OUT-");
						OnEpCtrlOut(); // finished TX on CTRL endpoint
					}
				}
				else if (ep == EP_DATA)
				{
					trace("DATA-");
					OnEpBulkOut();
				}
				else if (ep == EP_COMM)
				{
					trace("COMM\n");
//					OnEpIntOut();
				}
			}
			else // IN, finished packet transmitted from device to host
			{
				// Apply new device address
				if (deviceAddress)
				{
					USB_SetAddress(deviceAddress);
					deviceAddress = 0;
				}

				USB_EpRegs(ep) = epStatus & ~CTR_TX & EP_MASK_NoToggleBits;

				trace("<-");

				if (ep == EP_CTRL)
				{
					trace("CTRL-");
					OnEpCtrlIn();
				}
				else if (ep == EP_DATA)
				{
					trace("DATA-");
					OnEpBulkIn();
				}
				else if (ep == EP_COMM)
				{
					trace("COMM\n");
//					OnEpIntIn();
				}
			}
		}
	}
}

