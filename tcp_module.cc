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

#define S_SYN_ACK 0
#define S_RST 1
#define S_ACK 2
#define S_FIN 3

using std::cout;
using std::endl;
using std::cerr;
using std::string;

void muxHandler(const MinetHandle &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist);
void sockHandler(const MinetHandl &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist);

int main(int argc, char *argv[])
{
  MinetHandle mux, sock;
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
      if (event.handle==sock)
	sockHandler(mux, sock, clist);
<<<<<<< HEAD

=======
	//write interface to sock
	SockRequestResponse s;
	MinetReceive(sock,s);
>>>>>>> 48fbe6dfeb326611295ae59dc7d7679f8fffff78
	cerr << "Received Socket Request:" << s << endl;
      }
    }
  }

  return 0;
}

void muxHandler(const MinetHandle &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist)
{
	Packet p;
	Packet out_packet;  //syn/ack packet for listen
	SocketRequestResponse req, repl;
	MinetReceive(mux,p);

	p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
	IPHeader iph=p.FindHeader(Headers::IPHeader);
	TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

	cerr << "TCP Packet: IP Header is "<<iph<<" and ";
	cerr << "TCP Header is "<<tcph << " and ";
	bool checksumok;
	unsigned int acknum, seqnum;
	unsigned char flags;
	unsigned short len, winsize;

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

	 ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);

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
	        (*cs).state.N = winsize/(TCP_MAXIMUM_SEGMENT_SIZE * TCP_MAXIMUM_SEGMENT_SIZE);
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
<<<<<<< HEAD
			    //(*cs).bTmrActive = true;   //turn on the timer
			    (*cs).connection = c;   //reset the connection
			    //since we have the sequence number, we can set it as the last received
			    (*cs).state.SetLastRecvd(seqnum);
			    //(*cs).timeout = Time() + 10; //I dunno why 10 would be good or not...
			    
=======
			    (*cs).connection = c;   //reset the connection NECESSARY?

			    (*cs).state.SetLastRecvd(seqnum); //receiver side - set LastRecvd to the seqnum we just received

			    //turn on the timer
			    (*cs).bTmrActive = true;
			    (*cs).timeout = Time() + 50; //I dunno why 50 would be good or not...

>>>>>>> 48fbe6dfeb326611295ae59dc7d7679f8fffff78
			    //create the SYN/ACK packet   (RFC)  --> out_packet comes from this
			    packetMaker(out_packet, *cs, 0, S_SYN_ACK);
			    MinetSend(mux, out_packet);

			    //update LastAcked and LastSent
			    (*cs).state.SetLastSent(300); //arbitrarily set our ISN to be 300
			    (*cs).state.SetLastAcked(seq+1);
			}
			else if (IS_FIN(flags)) {
			    //create a RST packet   (RFC)  --> out_packet comes from this
			    packetMaker(out_packet, *cs, 0, S_RST);
			    MinetSend(mux, out_packet);
			}
		     break;
		   case SYN_RCVD:
		     // handle state SYN_RCVD;
		     printf("syn_rcvd\n");
		     if (IS_ACK(flags)) {
			//make sure the last ack is the sequence number we just received
			if ((*cs).state.GetLastAcked() == seqnum) {  //p266 (3rd arrow)
			    printf("Established\n");
			    (*cs).state.SetState(ESTABLISHED);  //since we got here that means we are established!
			    (*cs).state.SetLastRecvd(seqnum);
			    (*cs).state.SetSendRwnd(winsize);  //set the window size
<<<<<<< Updated upstream
			    (*cs).bTmrActive = false;  //we recieved the packet so turn off the timer!
=======
			    (*cs).bTmrActive = false //turn off the timer?
>>>>>>> Stashed changes

				/****  IS THIS NEEDED   ????   *****/
			    //Now we need to talk with the socket...
			    repl.type = WRITE; //ssrtype = WRITE
			    repl.connection = c;  //give it the tcp connection
			    repl.error = EOK;  //EOK = 0 means good
			    repl.bytes = 0;   //initialize at 0
			    MinetSend(sock, repl);  //send to sock layer
			}
		     } else if (IS_RST(flags)){
		       printf("Reset SYN_RCVD to LISTEN\n");
		       (*cs).state.SetState(LISTEN);
		       (*cs).state.SetLastRecvd(seqnum);
		       (*cs).state.SetSendRwnd(winsize);
		       (*cs).bTmrActive = false;

		       //not sure if it is needed to talk to socket about the status change
		       repl.type = STATUS;
		       repl.connection = c;
		       repl.error = EOK;
		       repl.bytes = 0;
		       MinetSend(sock,repl);
		     }
		     else if (IS_RST(flags))
		     {
			(*cs).state.SetState(LISTEN);

			SockRequestResponse repl;
			repl.type = STATUS:
			repl.connection = c;
			repl.error = EOK;
			repl.bytes = 0;
			MinetSend(sock, repl);			
		     }
		     break;
		   case SYN_SENT:
		     //handle state SYN_SENT;
		     	printf("syn_sent\n");
<<<<<<< HEAD
		     	if (IS_SYN(flags) && IS_ACK(flags))
		     	{
			    
			    (*cs).state.SetState(ESTABLISHED);

			    (*cs).state.SetSendRwnd(winsize);  //set the send window 
			    (*cs).bTmrActive = false;

			    /*****  WHY DOES THIS GO TO SEQNUM + 1 ??? *****/ 
			    (*cs).state.SetLastRecvd(seqnum + 1);
			    //send an ACK packet
			    packetMaker(out_packet, *cs, 0, S_ACK);
			    MinetSend(mux, out_packet);
			    
			}
		 	else if (IS_SYN(flags))
			{
			    (*cs).state.SetState(SYN_RCVD);

			    packetMaker(out_packet, *cs, 0, S_SYN_ACK);
			    MinetSend(mux, out_packet);
=======
		     	if (IS_SYN(flags) && IS_ACK(flags)){
                		if ((*cs).state.GetLastSent() == acknum-1){
			    		(*cs).state.SetState(ESTABLISHED); //change state into established
			    		(*cs).state.SetSendRwnd(winsize);  //set the send window
			    		(*cs).state.SetLastRecvd(seqnum);  //set LastRecvd to be seq we just received

					    //send an ACK packet
					    packetMaker(out_packet, *cs, 0, S_ACK);
					    MinetSend(mux, out_packet);

					    //change sender side fields after sending packet
					    (*cs).state.SetLastSent(acknum); //sent seq number equal to our received ack num
					    (*cs).state.SetLastAcked(seqnum+1); //sent ack number equal to the next seq num we want to receive

					    //turn on timer
					    (*cs).bTmrActive = true;
					    (*cs).timeout = Time() + 50;

					    //talk to the socket about ESTABLISH state
					    repl.type = WRITE;
					    repl.connection = c;
					    repl.error=EOK;
					    repl.bytes=0;
					    MinetSend(sock,repl);
			  	}		
			} else if (IS_SYN(flags)){
			    (*cs).state.SetState(SET_RCVD);
			    (*cs).staet.SetSendRwnd(winsize);
			    (*cs).state.SetLastRecvd(seqnum);

			    //send an SYN/ACK packet
			    packetMaker(out_packet, *cs, 0, S_SYN_ACK);
			    MinetSend(mux, out_packet);

			    //change sender side fields after sending packet
			    /* (*cs).state.SetLastSent(acknum); LastSent shouldn't be changed in this case if you read RFC 793 simultaneous open */
			    (*cs).state.SetLastAcked(seqnum+1);

			    //turn on timer
			    (*cs).bTmrActive = true;
			    (*cs).timeout = Time() + 50;

			    //talk to the socket about SYN_RCVD state
			    repl.type = STATUS;
			    repl.connection = c;
			    repl.error = EOK;
			    repl.bytes = 0;
			    MinetSend(sock,repl);
>>>>>>> 48fbe6dfeb326611295ae59dc7d7679f8fffff78
			}
		     break;
		   case ESTABLISHED:
			if (IS_FIN(flags))  //p267
			{
			    //set to close wait state
			    (*cs).state.SetState(CLOSE_WAIT);   //DIAGRAM boulder.ibm

			    packetMaker(out_packet, *cs, 0, S_ACK);   //create ack packet
          		    MinetSend(mux, out_packet);


			}
			else if (IS_PSH(flags))  //p247
			{

			}
			else if (IS_ACK(flags))  //p31 RFC
			{

			}
		     //handle state ESTABLISHED;
		     break;
		   case SEND_DATA:
		     //handle state SEND_DATA;
		     break;
		   case CLOSE_WAIT:
			(*cs).state.SetState(LAST_ACK);

			packetMaker(out_packet, *cs, 0, S_FIN);
			MinetSend(mux, out_packet);
		     //handle state CLOSE_WAIT;
		     break;
		   case FIN_WAIT1:
		     //handle case FIN_WAIT1;
			if (IS_FIN(flags) && IS_ACK(flags))
			{
			    (*cs).state.SetState(TIME_WAIT);

			    packetMaker(out_packet, *cs, 0, S_ACK);
			    MinetSend(mux, out_packet);
			}
			else if (IS_FIN(flags))
			{
			    (*cs).state.SetState(LAST_ACK);

			    packetMaker(out_packet, *cs, 0, S_ACK);
			    MinetSend(mux, out_packet);
			}
			else if (IS_ACK(flags))
			{
			    (*cs).state.SetState(FIN_WAIT2);
			}
		     break;
		   case CLOSING:
		     //handle case CLOSING;
		     break;
		   case LAST_ACK:

		     	if (IS_ACK(flags))
			{
			    (*cs).state.SetState(CLOSED);
			    //send nothing
			}


		     //handle case LASK_ACK;
		     break;
		   case FIN_WAIT2:
		     //handle case FIN_WAIT2;
			if (IS_FIN(flags))
			{
			    (*cs).state.SetState(TIME_WAIT);

			    packetMaker(out_packet, *cs, 0, S_ACK);
			    MinetSend(mux, out_packet);
			}
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

	cerr << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID");
}

void sockHandler(const MinetHandle &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist)
{
    SockRequestResponse s;
    MinetReceive(sock,s);

    switch(s.type)
    {
	case CONNECT:
	{
	    Packet out_packet;

	    unsigned int timerTries = 3;
	    //TCPState(unsigned int initialSequenceNum, unsigned int state, unsigned int timertries);
	    TCPState newTCPState = TCPState(rand(), SYN_SENT, timerTries);  //SYN_SENT because diagram for *active open ; also p15 Minet
	     
	    ConnectionToStateMapping<TCPState> constate;
	    constate.connection = s.connection;
            //constate.timeout = Time() + ACK_TIMEOUT;
	    constate.state = newTCPState;

	    //now need to creat a syn packet to send out, diagram for *active open to SYN_SENT
	    packetMaker(out_packet, constate, 0, S_SYN);
	    MinetSend(mux, out_packet);

	    //make this connection to the front on the connection list
	    clist.push_front(constate);

	    //send out a STATUS sock reply
	    SockRequestResponse repl;
	    repl.type = STATUS;
	    repl.connection = s.connection;
	    repl.bytes = 0;
	    repl.error = EOK;
	    MinetSend(sock, repl);

	}
	break;
        
	case ACCEPT:
	{
	    unsigned int timerTries = 3;

	    TCTCPState newTCPState = TCPState(rand(), LISTEN, timerTries); //LISTEN because of diagram for *passive open ; also p15 Minet
	   
	    ConnectionToStateMapping<TCPState> constate;
	    constate.connection = s.connection;
            //constate.timeout = Time() + ACK_TIMEOUT;
	    constate.state = newTCPState;

	    //Send Nothing as per diagram

	    clist.push_front(constate);


	    /*****      DO WE NEED THIS TOO ?   (p15 Minet) ****/
	    //send out a STATUS sock reply
	    SockRequestResponse repl;
	    repl.type = STATUS;
	    repl.error = EOK;
	    MinetSend(sock, repl);

	}
	break;

	case WRITE:
	{
	    Packet out_packet;

	    unsigned int bytes = s.data.GetSize();  //the request's size

	    ConnectionToStateMapping<TCPState> constate;
	    constate.connection = s.connection;

	    /****  WHERE DOES Buffer COME FROM?     p15 Minet ****/

	    packetMaker(out_packet, constate, bytes, S_ACK);  //****WHAT DO WE NEED TO SEND??? AN ACK? ****/
	    MinetSend(mux, out_packet); 

	    //p15 Minet
	    SockRequestResponse repl;
	    repl.connection = s.connection;
	    repl.type = STATUS;
	    repl.bytes = bytes;
	    repl.error = EOK;
	    MinetSend(sock, repl);

	}
	break;

	case FORWARD:
	{
	    SockRequestResponse repl;
	    repl.type = STATUS;
	    repl.error = EOK;
	    MinetSend(sock, repl);
	}
	break;

	case CLOSE:
	{
	     
	}
	break;

	case STATUS:
	{
	    //nothing for here apparently
	}
	break;

	case 

    }

}

void packetMaker(Packet &packet, ConnectionToStateMapping<TCPState>& constate, int size_of_data, int whichFlag)
{

  unsigned int flags = 0;

  int packet_length = size_of_data + TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH;
  IPHeader ip;
  TCPHeader tcp;
  IPAddress source = constate.connection.src;
  IPAddress destination = constate.connection.dest;

  //create the IP packet with source, destination, length, and protocol
  ip.SetSourceIP(source);
  ip.SetDestIP(destination);
  ip.SetTotalLength(packet_length);
  ip.SetProtocol(IP_PROTO_TCP);

  packet.PushFrontHeader(ip);

  switch (whichFlag){
  case S_SYN_ACK:
    SET_SYN(flags);
    SET_ACK(flags);
    break;
  case S_RST:
    SET_RST(flags);
    break;
  case S_ACK:
    SET_ACK(flags);
    break;
  case S_FIN:
    SET_FIN(flags);
    break;
  default:
    break;
  }

  //just followed all the tcp.h stuff top to bottom

  tcp.SetSourcePort(constate.connection.srcport, packet);
  tcp.SetDestPort(constate.connection.destport, packet);
  tcp.SetHeaderLen(TCP_HEADER_BASE_LENGTH, packet);
  tcp.SetFlags(0, packet);
  tcp.SetAckNum(constate.state.GetLastRecvd(), packet);
  tcp.SetSeqNum(constate.state.GetLastAcked()+1, packet);
  tcp.SetWinSize(constate.state.GetN(), packet);
  tcp.SetUrgentPtr(0, packet);
  tcp.RecomputeChecksum(packet);
  packet.PushBackHeader(tcp);

}


