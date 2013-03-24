#include "ipstack/TCPIP.h"
#include "gw.h"
#include "appconfig.h"
#include "FrequencyScanner.h"

#define GW_CLIENT_PORT                (60000)
#define GW_SERVER_PORT                (9930)
#define GW_REQUEST_MESSAGE            (0x01)
#define GW_REPLY_MESSAGE              (0x02)
#define GW_UNKNOWN_MESSAGE            (0)

typedef enum _SM_GW {
	SM_GW_IDLE,
	SM_GW_CONFIG_SCAN,
	SM_GW_START_SCAN,
	SM_GW_RECV_PARAM,
	SM_GW_SEND_DATA,
	SM_GW_DONE
} SM_GW;

#define PROT_ID0 'G'
#define PROT_ID1 'W'
typedef struct _protHeader {
	char protId[2];
	UINT16 protLen;
}GW_PROT_HEADER;

static SM_GW smGWState;
static UDP_SOCKET GWSocket = INVALID_UDP_SOCKET;

static void GWSendPacket();
static BOOL _GWReceive(void);

//Indicates if an ARP resolution request is on the way
static BOOL arpRequested=FALSE;
//Header of the UDP protocol
static GW_PROT_HEADER protHeader;
//Header of the received package
static GW_PROT_HEADER rxHeader;
//Pointer to the scan configuration structure
static SCAN_CONFIG * pscanParameters=NULL;
//Scan parameters received through the UDP socket
static SCAN_CONFIG rxParameters;
//Boolean to indicate if the scanner is already configured
static BOOL scanConfigured=FALSE;
//Boolean to indicate if we received a new set of parameters via UDP
static BOOL newRxConfig=FALSE;
//Pointer to the array of RSSI values
static BYTE * rssiPtr=NULL;
//Amount of bytes stored in the rssiArray
static unsigned int rssiLen=0;

static DWORD gwTimer;

//Timer to restart an ARP resolution if the previous showed no results
static DWORD arpTimer;

static DWORD currentTimer;

static NODE_INFO GWServerNode;

void GWInit(BYTE * arrayPtr, BOOL * boolArrayPtr, unsigned int arraySize) {
	GWServerNode.IPAddr.Val = AppConfig.PrimaryTFTPServer.Val;

	//We resolve the MAC address of the server
	ARPResolve(&GWServerNode.IPAddr);
	arpTimer=TickGet();
	arpRequested=TRUE;
	smGWState = SM_GW_IDLE;
	scanConfigured=FALSE;
	newRxConfig=FALSE;

	//Reserve memory for the frequency scanner,
	//4KB should do for the moment
	rssiPtr=(BYTE *)malloc(sizeof(UINT8)*4096);

}

void GWTask(void) {
	switch (smGWState) {
	//We wait until the server IP address is resolved
	//to open the UDP socket
	case SM_GW_IDLE:
		if (ARPIsResolved(&GWServerNode.IPAddr, &GWServerNode.MACAddr)) {
			arpRequested=FALSE;
			//INVALID_UDP_SOCKET is the starting value of the socket,
			//any other value indicates that the socket needs to be restarted
			if (GWSocket!=INVALID_UDP_SOCKET){
				UDPClose(GWSocket);
			}
			//We open the UDP socket if it isn't open already
			if(!UDPIsOpened(GWSocket)){
				GWSocket = UDPOpenEx((DWORD) &GWServerNode, UDP_OPEN_NODE_INFO,
									GW_CLIENT_PORT, GW_SERVER_PORT);
			}
			rssiLen=0;
			smGWState = SM_GW_RECV_PARAM;
		}
		else{
			//Send an ARP request if none is on the way
			//or if the previous one timed out
			if(!arpRequested||TickGet()-arpTimer>TICK_SECOND){
				ARPResolve(&GWServerNode.IPAddr);
				arpTimer=TickGet();
				arpRequested=TRUE;
			}
		}
		break;

	case SM_GW_RECV_PARAM:
		newRxConfig=_GWReceive();
		if(newRxConfig || !scanConfigured)
			smGWState=SM_GW_CONFIG_SCAN;
		else
			smGWState=SM_GW_START_SCAN;
		break;

	case SM_GW_CONFIG_SCAN:
		//If we got no parameter configuration, we use the default
		if(newRxConfig){
			pscanParameters=configScanner(&rxParameters,rssiPtr);
			newRxConfig=FALSE;
			scanConfigured=TRUE;
		}
		else{
			pscanParameters=configScanner(NULL,rssiPtr);
			scanConfigured=TRUE;
		}
		if(pscanParameters!=NULL)
			smGWState=SM_GW_START_SCAN;
		break;

	case SM_GW_START_SCAN:
		rssiLen=scanFreqBands();
		smGWState=SM_GW_SEND_DATA;
		gwTimer=TickGet();
		break;

	case SM_GW_SEND_DATA:
		//Do not send UDP messages if the stack is
		//running the DHCP process, we may not have
		//an IP yet.
		currentTimer=TickGet();
		if(!StackIsInConfigMode()&&currentTimer-gwTimer>=TICK_SECOND){
			if (UDPIsOpened(GWSocket)) {
				GWSendPacket();
				gwTimer=TickGet();
				smGWState=SM_GW_DONE;
			}
			//If there's something wrong with the socket
			//we restart it.
			else{
				smGWState=SM_GW_IDLE;
			}
		}
		break;

	case SM_GW_DONE:
		smGWState=SM_GW_RECV_PARAM;
		break;
	}
}

//TODO: Implement a timer to abort transmission
static void GWSendPacket(){
	static BYTE * bufferPtr;
	WORD txBufferSize;
	WORD bytesSent;
	WORD msgLen;
	WORD packetBytes;

	//Set the protocol header
	protHeader.protId[0]=PROT_ID0;
	protHeader.protId[1]=PROT_ID1;
	protHeader.protLen=sizeof(GW_PROT_HEADER)+sizeof(SCAN_CONFIG)+rssiLen;

	msgLen=protHeader.protLen;
	bytesSent=0;
	packetBytes=0;
	bufferPtr=(BYTE*)&protHeader;
	txBufferSize=UDPIsPutReady(GWSocket);

	if(txBufferSize>=msgLen){
		while(bytesSent<msgLen){
			if(txBufferSize>0 && bytesSent<msgLen){
				UDPPut(*bufferPtr++);
				bytesSent++;
				packetBytes++;
			}
			if(bytesSent==sizeof(GW_PROT_HEADER)){
				bufferPtr=(BYTE*)pscanParameters;
			}
			else if(bytesSent==sizeof(GW_PROT_HEADER)+sizeof(SCAN_CONFIG)){
				bufferPtr=rssiPtr;
			}
		}
		UDPFlush();
	}
}

static BOOL _GWReceive(void)
{
	WORD i;
	BYTE * tempPtr=(BYTE *)&rxHeader;
	//Check if we received a message of the proper size
	if (UDPIsGetReady(GWSocket)>=sizeof(GW_PROT_HEADER)+sizeof(SCAN_CONFIG)){
		//Extract and check the protocol header
		for(i=0; i<sizeof(GW_PROT_HEADER); i++){
			UDPGet(tempPtr++);
		}
		//Discard the packet if it does not match the protocol format
		if(rxHeader.protId[0]!=PROT_ID0 || rxHeader.protId[1]!=PROT_ID1 ||
				rxHeader.protLen!=(sizeof(GW_PROT_HEADER)+sizeof(SCAN_CONFIG))){
			UDPDiscard();
			return FALSE;
		}

		//Extract the scan options
		tempPtr=(BYTE*)&rxParameters;
		for(i=0; i<sizeof(SCAN_CONFIG); i++){
			UDPGet(tempPtr++);
		}
		return TRUE;
	}
	return FALSE;
}
