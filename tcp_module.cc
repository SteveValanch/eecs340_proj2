#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include <iostream>

#include "Minet.h"
#include "tcpstate.h"

using std::cout;
using std::endl;
using std::cerr;
using std::string;

int main(int argc, char *argv[])
{
  MinetHandle mux, sock;
<<<<<<< HEAD

=======
>>>>>>> 93db8eeeaa6bf5aa211711ed10394effc91e6d60
  ConnectionList<TCPState> clist;

  MinetInit(MINET_TCP_MODULE);

  mux=MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
  sock=MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

  if (MinetIsModuleInConfig(MINET_IP_MUX) && mux==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to mux"));
    return -1;
  }

  if (MinetIsModuleInConfig(MINET_SOCK_MODULE) && sock==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't accept from sock module"));
    return -1;
  }

  MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

  MinetEvent event;

  while (MinetGetNextEvent(event)==0) {
    // if we received an unexpected type of event, print error
    if (event.eventtype!=MinetEvent::Dataflow 
	|| event.direction!=MinetEvent::IN) {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    // if we received a valid event from Minet, do processing
    } else {
      //  Data from the IP layer below  //
      if (event.handle==mux) {
	Packet p;
<<<<<<< HEAD
	MinetReceive(mux,p);
	unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
	cerr << "estimated header len="<<tcphlen<<"\n";
	p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
	IPHeader ipl=p.FindHeader(Headers::IPHeader);
	TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

		

	cerr << "TCP Packet: IP Header is "<<ipl<<" and ";
	cerr << "TCP Header is "<<tcph << " and ";
=======
	bool checksumok;
	unsigned int acknum, seqnum, winsize, flags;
	unsigned short len;
	MinetReceive(mux,p);
	unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
	unsigned iphlen=IPHeader::EstimateIPHeaderLength(p);
	//cerr << "estimated header len="<<tcphlen<<"\n";
	p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
	IPHeader iph=p.FindHeader(Headers::IPHeader);
	TCPHeader tcph=p.FindHeader(Headers::TCPHeader);
	
	//checksum
	checksumok = tcph.IsCorrectChecksum(p);

	Connection c;
	
	//retrieve info from ip and tcp headers
	iph.GetDestIP(c.src);
	iph.GetSourceIP(c.dest);
	iph.GetProtocol(c.protocol);
	tcph.GetDestPort(c.srcport);
	tcph.GetSourcePort(c.destport);
	tcph.GetSeqNum(seqnum);
	tcph.GetAckNum(acknum);
	tcph.GetWinSize(winsize);
	tcph.GetFlags(flags);

	 ConnectionList<TCPState>::iterator cs=clist.FindMatching(c);

	 if (cs!=clist.end()) {
	   iph.GetTotalLength(len);
	   len = len - tcphlen - iphlen;
	   Buffer &data=p.GetPayLoad().ExtractFront(len);
	   
	   unsigned int state = (*cs).state.GetState();

	   switch (state) {
	   case CLOSED:
	       //handle state CLOSED;
	     break;
	   case LISTEN:
	       //handle state LISTEN;
	     break;
	   case SYN_RCVD:
	     // handle state SYN_RCVD;
	     break;
	   case SYN_SENT:
	     //handle state SYN_SENT;
	     break;
	   case SYN_SENT1:
	     //handle state SYN_SENT1;
	     break;
	   case ESTABLISHED:
	     //handle state ESTABLISHED;
	     break;
	   case SEND_DATA:
	     //handle state SEND_DATA;
	     break;
	   case CLOSE_WAIT:
	     //handle state CLOSE_WAIT;
	     break;
	   case FIN_WAIT1:
	     //handle case FIN_WAIT1;
	     break;
	   case CLOSING:
	     //handle case CLOSING;
	     break;
	   case LAST_ACK:
	     //handle case LASK_ACK;
	     break;
	   case FIN_WAIT2:
	     //handle case FIN_WAIT2;
	     break;
	   case TIME_WAIT:
	     //handle case TIME_WAIT;
	     break;
	   default:
	     break;
	   }
	}
	
	//cerr << "TCP Packet: IP Header is "<<ipl<<" and ";
	//cerr << "TCP Header is "<<tcph << " and ";
>>>>>>> 93db8eeeaa6bf5aa211711ed10394effc91e6d60

	cerr << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID");
	
      }
      //  Data from the Sockets layer above  //
      if (event.handle==sock) {
<<<<<<< HEAD
=======
	//write interface to sock
>>>>>>> 93db8eeeaa6bf5aa211711ed10394effc91e6d60
	SockRequestResponse s;
	MinetReceive(sock,s);
	cerr << "Received Socket Request:" << s << endl;
      }
    }
  }
<<<<<<< HEAD
=======

  
>>>>>>> 93db8eeeaa6bf5aa211711ed10394effc91e6d60
  return 0;
}
