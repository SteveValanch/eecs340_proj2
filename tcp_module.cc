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
#include "tcp.h"

using std::cout;
using std::endl;
using std::cerr;
using std::string;

void muxHandler(const MinetHandle &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist);
void sockHandler(const MinetHandl &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist);

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

	muxHandler(mux, sock, clist);
	
      }
      //  Data from the Sockets layer above  //
      if (event.handle==sock) {

	sockHandler(mux, sock, clist);
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

void muxHandler(const MinetHandle &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist)
{
	Packet p;
	Packet out_packet;  //syn/ack packet for listen
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
	   //receiver window
	   (*cs).state.rwnd = winsize;
	   printf("Receiver window size: %d", winsize);
	   //if receiver window size is smaller than sending window size
	   if (winsize <= (*cs).state.GetN()) 
	   {
		//   winsize / (MSS*MSS)
		// N is the window size for tcpstate
		(*cs).state.N = winsize/TCP_MAXIMUM_SEGMENT_SIZE * TCP_MAXIMUM_SEGMENT_SIZE;
		printf("New Sender window size: %d", (*cs).state.GetN());
	   }

	   switch (state) {
	   case CLOSED:
	       //handle state CLOSED;
	     break;
	   case LISTEN:
	       //handle state LISTEN;
		//want to check if we find an SYN because we are Listening...
		//we may want to check for FIN is case there needs to be an RST in the connection (reset)
		if (IS_SYN(flags)) {
		    (*cs).state.SetState(SYN_RCVD);  //set the state to SYN_RCVD because we just received a SYN  (RFC)
		    (*cs).bTmrActive = true;   //turn on the timer
		    (*cs).connection = c;   //reset the connection
		    //since we have the sequence number, we can set it as the last received (incremented by one)
		    (*cs).state.SetLastRecvd(seq+1);
		    (*cs).timeout = Time() + 10; //I dunno why 10 would be good or not...
		    (*cs).state.last_acked = (*cs).state.last_sent - 1;
		    
		    //create the SYN/ACK packet   (RFC)  --> out_packet comes from this
		    MinetSend(mux, out_packet);
		}
		else if (IS_FIN(flags)) {
		    //create a RST packet   (RFC)  --> out_packet comes from this
		    MinetSend(mux, out_packet);
		}
	     break;
	   case SYN_RCVD:
	     // handle state SYN_RCVD;
	     printf("syn_rcvd\n");
	     if (IS_ACK(flags)) {
		//.GetLastSent() will get the last sent ack, but we want to increment by one to see if it matches with what we expect our ack to be due to sequential acknowledgement
		if ((*cs).state.GetLastSent() + 1 == ack) {
		    printf("Established\n");
		    (*cs).state.SetState(ESTABLISHED);  //since we got here that means we are established!
		    (*cs).state.SetSendRwnd(winsize);  //set the window size
		    (*cs).state.SetLastAcked(ack);  //reset the last recieved ack
		    (*cs).bTmrActive = false;  //we recieved the packet so turn off the timer!

		    //Now we need to talk with the socket...
		    SockRequestResponse repl;
		    repl.type = WRITE; //ssrtype = WRITE  
		    repl.connection = c;  //give it the tcp connection
		    repl.error = EOK;  //EOK = 0 means good
		    repl.bytes = 0;   //initialize at 0
		    MinetSend(sock, repl);  //send to sock layer
		}
	     }
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


