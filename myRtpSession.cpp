#include <time.h>
#include <stdio.h>
#include "myRtpSession.h"
#include "MediaSession.h"

#define RTP_OK 		1
#define RTP_ERROR 	0

using namespace jrtplib;

//
// This function checks if there was a RTP error. If so, it displays an error
// message and exists.
//

int MyRTPSession::IsError(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		return RTP_ERROR;
	}
	return RTP_OK;
}

int MyRTPSession::MyRTP_SetUp(MediaSession * media_session)
{
	if(!media_session) {
		fprintf(stderr, "%s: Invalid media session\n", __func__);
		return RTP_ERROR;
	}
	if(0 == media_session->TimeRate) {
		fprintf(stderr, "%s: Invalid MediaSession::TimeRate\n", __func__);
		return RTP_ERROR;
	}
	if(0 == media_session->RTPPort) {
		fprintf(stderr, "%s: Invalid MediaSession::RTPPort\n", __func__);
		return RTP_ERROR;
	}

	int status;

	// Now, we'll create a RTP session, set the destination
	// and poll for incoming data.

	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;

	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be just use 8000 samples per second.
	sessparams.SetOwnTimestampUnit(1.0/media_session->TimeRate);         

	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(media_session->RTPPort);
	status = Create(sessparams,&transparams);  
	return IsError(status);
}

void MyRTPSession::MyRTP_Teardown(MediaSession * media_session, struct timeval * tval)
{
	struct timeval Timeout;

	if(!tval) {
		Timeout.tv_sec = 1; 
		Timeout.tv_usec = 0; 
	} else {
		Timeout.tv_sec = tval->tv_sec;
		Timeout.tv_usec = tval->tv_usec;
	}

	media_session->RTPPort = 0;
	BYEDestroy(RTPTime(Timeout.tv_sec, Timeout.tv_usec), 0, 0);
}

void MyRTPSession::OnNewSource(RTPSourceData *dat)
{
	if (dat->IsOwnSSRC())
		return;

	uint32_t ip;
	uint16_t port;

	if (dat->GetRTPDataAddress() != 0)
	{
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort();
	}
	else if (dat->GetRTCPDataAddress() != 0)
	{
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort()-1;
	}
	else
		return;

	RTPIPv4Address dest(ip,port);
	AddDestination(dest);

	struct in_addr inaddr;
	inaddr.s_addr = htonl(ip);
	std::cout << "Adding destination " << std::string(inet_ntoa(inaddr)) << ":" << port << std::endl;
}

void MyRTPSession::OnBYEPacket(RTPSourceData *dat)
{
	if (dat->IsOwnSSRC())
		return;

	uint32_t ip;
	uint16_t port;

	if (dat->GetRTPDataAddress() != 0)
	{
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort();
	}
	else if (dat->GetRTCPDataAddress() != 0)
	{
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort()-1;
	}
	else
		return;

	RTPIPv4Address dest(ip,port);
	DeleteDestination(dest);

	struct in_addr inaddr;
	inaddr.s_addr = htonl(ip);
	std::cout << "Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port << std::endl;
}

void MyRTPSession::OnRemoveSource(RTPSourceData *dat)
{
	if (dat->IsOwnSSRC())
		return;
	if (dat->ReceivedBYE())
		return;

	uint32_t ip;
	uint16_t port;

	if (dat->GetRTPDataAddress() != 0)
	{
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort();
	}
	else if (dat->GetRTCPDataAddress() != 0)
	{
		const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
		ip = addr->GetIP();
		port = addr->GetPort()-1;
	}
	else
		return;

	RTPIPv4Address dest(ip,port);
	DeleteDestination(dest);

	struct in_addr inaddr;
	inaddr.s_addr = htonl(ip);
	std::cout << "Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port << std::endl;
}

uint8_t * MyRTPSession::GetMyRTPData(uint8_t * data_buf, size_t * size)
{
#ifndef RTP_SUPPORT_THREAD
	int status = Poll();
	if(!IsError(status)) return NULL;
#endif 

	if(!data_buf) {
		fprintf(stderr, "%s: Invalide argument('data_buf==NULL')", __func__);
		return NULL;
	}

	if(!size) {
		fprintf(stderr, "%s: Invalide argument('size==NULL')", __func__);
		return NULL;
	}
	BeginDataAccess();

	// check incoming packets
	if (!GotoFirstSourceWithData()) {
		EndDataAccess();
		return NULL;
	}
	RTPPacket *pack;

	if(!(pack = GetNextPacket()))
	{
		EndDataAccess();
		return NULL;
	}

	size_t PacketSize = 0;
	uint8_t * Packet = NULL;
	Packet = pack->GetPayloadData();
	PacketSize = pack->GetPayloadLength();
	printf("data length: %lu\n", PacketSize);

	*size = PacketSize;
	memcpy(data_buf, Packet, PacketSize);

	// we don't longer need the packet, so
	// we'll delete it
	DeletePacket(pack);
	EndDataAccess();

	return data_buf;
}

uint8_t * MyRTPSession::GetMyRTPPacket(uint8_t * packet_buf, size_t * size)
{
#ifndef RTP_SUPPORT_THREAD
	int status = Poll();
	if(!IsError(status)) return NULL;
#endif 

	if(!packet_buf) {
		fprintf(stderr, "%s: Invalide argument('packet_buf==NULL')", __func__);
		return NULL;
	}

	if(!size) {
		fprintf(stderr, "%s: Invalide argument('size==NULL')", __func__);
		return NULL;
	}
	BeginDataAccess();

	// check incoming packets
	if (!GotoFirstSourceWithData()) {
		EndDataAccess();
		return NULL;
	}
	RTPPacket *pack;

	if(!(pack = GetNextPacket()))
	{
		EndDataAccess();
		return NULL;
	}

	size_t PacketSize = 0;
	uint8_t * Packet = NULL;
	Packet = pack->GetPacketData();
	PacketSize = pack->GetPacketLength();
	printf("packet length: %lu\n", PacketSize);

	*size = PacketSize;
	memcpy(packet_buf, Packet, PacketSize);

	// we don't longer need the packet, so
	// we'll delete it
	DeletePacket(pack);
	EndDataAccess();

	return packet_buf;
}
