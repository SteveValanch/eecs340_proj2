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
#include "sockint.h"

#define S_SYN_ACK 0
#define S_RST 1
#define S_ACK 2
#define S_FIN 3
#define S_SYN 4

using std::cout;
using std::endl;
using std::cerr;
using std::string;


void packetMaker(Packet &packet, ConnectionToStateMapping<TCPState>& constate, int size_of_data, int whichFlag);
void muxHandler(const MinetHandle &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist);
void sockHandler(const MinetHandle &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist);
void timeoutHandler(const MinetHandle &mux, ConnectionList<TCPState>::iterator cs, ConnectionList<TCPState> &clist);


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
      if (event.eventtype!=MinetEvent::Dataflow || event.direction!=MinetEvent::IN) {
      	    MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
    	    // if we received a valid event from Minet, do processing
	  if (event.eventtype==MinetEvent::Timeout)  
	  {
	      // timeout ! probably need to resend some packets
	      ConnectionList<TCPState>::iterator cs = clist.FindEarliest();
	      // Remember clist is a list which keeps the information of connections.
	      if( cs != clist.end() )
	      {
	    	  if (Time().operator > ((*cs).timeout))
		  	timeoutHandler(mux, cs, clist);
	      }
	  }

      } 
      else 
      {
          //  Data from the IP layer below  //
          if (event.handle==mux) {
	      muxHandler(mux, sock, clist);
          }
          //  Data from the Sockets layer above  //
          if (event.handle==sock)
	  {
	      sockHandler(mux, sock, clist);
          }
      }
  }

  return 0;
}

void muxHandler(const MinetHandle &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist)
{
	Packet p;
	Packet out_packet; 
	SockRequestResponse req, repl;
	MinetReceive(mux,p);

	bool checksumok;
	unsigned int acknum, seqnum;
	unsigned char flags;
	unsigned short total_len, seg_len, data_len, winsize;

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
	   iph.GetTotalLength(total_len); //total length including ip header
	   seg_len = total_len - iphlen; //segment length including tcp header
	   data_len = seg_len - tcphlen; //actual data length
	   Buffer data = p.GetPayload().ExtractFront(data_len);

	   unsigned int state = (*cs).state.GetState();

	   //receiver window
	   (*cs).state.rwnd = winsize;
	   printf("Receiver window size: %d", winsize);

	   switch (state) {
		   case CLOSED:
		       //handle state CLOSED;

		     break;
		   case LISTEN:
		       //handle state LISTEN;
			//want to check if we find an SYN because we are Listening...
			//we may want to check for FIN is case there needs to be an RST in the connection (reset)
		     
		     //initialize sender and receiver window pointers
		     (*cs).state.SetLastSent(0); 
		     (*cs).state.SetLastRecvd(0);
		     (*cs).state.SetLastAcked(0);
		     
		     if (IS_SYN(flags)) {
			    (*cs).state.SetState(SYN_RCVD);  
			    (*cs).connection = c;   //reset the connection
			    
			    //turn on the timer
			    (*cs).bTmrActive = true;
			    (*cs).timeout = Time() + 50; 

			    (*cs).state.SetLastRecvd(seqnum); 
			    (*cs).state.SetSendRwnd(winsize);

			    packetMaker(out_packet, *cs, 0, S_SYN_ACK);
			    MinetSend(mux, out_packet);

			    (*cs).state.SetLastSent((*cs).state.GetLastSent()+1); //shifted last_sent one byte to the right    		       
	           	}
		     break;
		   case SYN_RCVD:
		     // handle state SYN_RCVD;
		     printf("syn_rcvd\n");
		     if ((*cs).state.GetLastRecvd() == seqnum) {  //p266 (3rd arrow)
		     	if (IS_ACK(flags)) {
			    	printf("Established\n");
			    	(*cs).state.SetState(ESTABLISHED);  //since we got here that means we are established!
			    	(*cs).state.SetSendRwnd(winsize);  //set the window size
			    	(*cs).state.SetLastAcked((*cs).state.GetLastAcked()+1); //we received ack, so move last_acked pointer to right
			    	(*cs).bTmrActive = false;  //last_acked = last_sent, turn off timer

			    	//Now we need to talk with the socket...
				repl.type = WRITE; //ssrtype = WRITE
			    	repl.connection = c;  //give it the tcp connection
			    	repl.error = EOK;  //EOK = 0 means good
			    	repl.bytes = 0;   //initialize at 0
			    	MinetSend(sock, repl);  //send to socket
		     	} else if (IS_RST(flags)) { //reset if RST flag is set
		       		printf("Reset SYN_RCVD to LISTEN\n");
		       		(*cs).state.SetState(LISTEN);
		      		(*cs).bTmrActive = false;

		       		//not sure if it is needed to talk to socket about the status change
				repl.type = STATUS;
		       		repl.connection = c;
		       		repl.error = EOK;
		       		repl.bytes = 0;
		       		MinetSend(sock,repl);
		      	}
		     }
		     break;
		   case SYN_SENT:
		     //handle state SYN_SENT;
		     	printf("syn_sent\n");
		     	    if (IS_SYN(flags) && IS_ACK(flags)) {
			      if ((*cs).state.GetLastSent() == acknum) {
			  	(*cs).state.SetState(ESTABLISHED); //change state into established
			  	(*cs).state.SetSendRwnd(winsize);  //set the send window
				(*cs).state.SetLastAcked(acknum); //received ack, move last_acked to right			   	
				(*cs).state.SetLastRecvd(seqnum);   		    

    				//turn on timer
				(*cs).bTmrActive = true;
				(*cs).timeout = Time() + 50;
				    
				//send ack packet
				packetMaker(out_packet, *cs, 0, S_ACK);
				MinetSend(mux, out_packet);
				
				//talk to the socket about ESTABLISH state
				repl.type = WRITE;
				repl.connection = c;
				repl.error=EOK;
				repl.bytes=0;
				MinetSend(sock,repl);
			      } else {
			       	packetMaker(out_packet, *cs, 0, S_RST);
				MinetSend(mux, out_packet);
			      }
			    } else if (IS_SYN(flags)) { //simultaneous connection 
			        (*cs).state.SetState(SYN_RCVD);
			        (*cs).state.SetSendRwnd(winsize);
			        (*cs).state.SetLastRecvd(seqnum);
			
				//send an SYN/ACK packet
				packetMaker(out_packet, *cs, 0, S_SYN_ACK);
				MinetSend(mux, out_packet);
				
			        //turn on timer
			        (*cs).bTmrActive = true;
			        (*cs).timeout = Time() + 50;

			        //talk to the socket about SYN_RCVD state
			        repl.type = STATUS;
			        repl.connection = c;
			        repl.error = EOK;
			        repl.bytes = 0;
			        MinetSend(sock,repl);
			   } 
		     break;
		   case ESTABLISHED:
		     {
		     //handle state ESTABLISHED
		     bool data_ok = (*cs).state.SetLastRecvd(seqnum,data_len); //check and set last_received
		     if (checksumok && data_ok){  //if checksum ok and expected seq num ok, process packet and pass data to socket
		       
			if (IS_ACK(flags)) {
				(*cs).state.SetLastAcked(acknum); //accumulative ack, so anything before acknum should be acked already
			}
				
			if (IS_FIN(flags)) {
				(*cs).state.SetLastRecvd(seqnum); //no more data
				(*cs).state.SetState(CLOSE_WAIT); //change state into CLOSE_WAIT
			}	

		       	(*cs).state.SetSendRwnd(winsize); //update winsize
		       	packetMaker(out_packet, *cs, 0, S_ACK); //send an ack to sender
		       	MinetSend(mux, out_packet);
	
		       	//send packet to socket
		       	repl.type = WRITE;
		       	repl.connection = c;
		       	repl.error = EOK;
		       	repl.bytes = data_len;
		       	repl.data = data;
		       	MinetSend(sock,repl);
		      } else { //something is not right, re-acked OR we can probably do nothing and wait for timeout 
			packetMaker(out_packet, *cs, 0, S_ACK);
			MinetSend(mux, out_packet);
		      }
		      break;
		     }
		  case FIN_WAIT1:
		     //handle case FIN_WAIT1;
			if (IS_FIN(flags) && IS_ACK(flags))
			{
			    (*cs).state.SetState(TIME_WAIT);
			    (*cs).state.SetLastAcked(acknum);
			    (*cs).state.SetLastRecvd(seqnum);			     	
			    packetMaker(out_packet, *cs, 0, S_ACK);
			    MinetSend(mux, out_packet);
			}
			else if (IS_FIN(flags))
			{
			    (*cs).state.SetState(CLOSING);
		            (*cs).state.SetLastRecvd(seqnum);		
			    packetMaker(out_packet, *cs, 0, S_ACK);
			    MinetSend(mux, out_packet);
			}
			else if (IS_ACK(flags))
			{
			    (*cs).state.SetState(FIN_WAIT2);
			    (*cs).state.SetLastAcked(acknum);
		            (*cs).state.SetLastRecvd(seqnum);	
			}
		     break;
		   case CLOSING:
		     //handle case CLOSING;
			if (IS_ACK(flags))
			{
			    (*cs).state.SetState(TIME_WAIT);
			    (*cs).state.SetLastAcked(acknum);
			    (*cs).state.SetLastRecvd(seqnum);
			}
		     break;
		   case LAST_ACK:
			//handle case LASK_ACK;
		     	if (IS_ACK(flags))
			{
			    (*cs).state.SetState(CLOSED);
			    (*cs).state.SetLastAcked(acknum);
			    (*cs).state.SetLastRecvd(seqnum);
			    //send nothing
			}   
		     break;
		   case FIN_WAIT2:
		     //handle case FIN_WAIT2;
			if (IS_FIN(flags))
			{
			    (*cs).state.SetState(TIME_WAIT);
			    (*cs).state.SetLastRecvd(seqnum);
			    packetMaker(out_packet, *cs, 0, S_ACK);
			    MinetSend(mux, out_packet);
			}
		     break;
		   case TIME_WAIT:
		     sleep(2*MSL_TIME_SECS);
		     (*cs).state.SetState(CLOSE);
		     break;
		   default:
	     break;
	   }
	}
}

void sockHandler(const MinetHandle &mux, const MinetHandle &sock, ConnectionList<TCPState> &clist)
{
    SockRequestResponse s;
    MinetReceive(sock,s);

    ConnectionList<TCPState>::iterator cs = clist.FindMatching(s.connection);


    switch(s.type)
    {
	case CONNECT:
	{
	    Packet out_packet;

	    unsigned int timerTries = 3;
	    
	    TCPState newTCPState = TCPState(rand(), SYN_SENT, timerTries);  //SYN_SENT because diagram for *active open ; also p15 Minet
	     
	    ConnectionToStateMapping<TCPState> constate;
	    constate.connection = s.connection;
            //constate.timeout = Time() + ACK_TIMEOUT;
	    constate.state = newTCPState;
	    constate.state.SetLastSent(constate.state.GetLastSent()+1);

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
	    unsigned int state = (*cs).state.GetState();

	    if (state != FIN_WAIT1)
	    {
	    
		unsigned int timerTries = 3;

		//LISTEN because of diagram for *passive open ; also p15 Minet
	        TCPState newTCPState = TCPState(rand(), LISTEN, timerTries); 
		   
		ConnectionToStateMapping<TCPState> constate;
		constate.connection = s.connection;
		//constate.timeout = Time() + ACK_TIMEOUT;
		constate.state = newTCPState;
		
                //Send Nothing as per diagram ; no setting last sent

		clist.push_front(constate);


		//(p15 Minet)
		//send out a STATUS sock reply
		SockRequestResponse repl;
		repl.type = STATUS;
		repl.error = EOK;
		MinetSend(sock, repl);
	    }
	
	}
	break;

	case WRITE:
	{
	    unsigned int state = (*cs).state.GetState();

	    if (state == ESTABLISHED)
	    {
		Packet out_packet;
		Buffer buff = s.data;

		unsigned int bytes = s.data.GetSize();  //the request's size
		
	        Packet dataSend = Packet(s.data.ExtractFront(bytes));

		(*cs).state.SetLastSent((*cs).state.GetLastSent() + bytes);

		packetMaker(dataSend, *cs, bytes, S_ACK);  //****WHAT DO WE NEED TO SEND??? AN ACK? ****/
		MinetSend(mux, dataSend); 

		    //p15 Minet
		SockRequestResponse repl;
		repl.connection = s.connection;
		repl.type = STATUS;
		repl.bytes = bytes;
		repl.error = EOK;
		MinetSend(sock, repl);
	    }
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
	     unsigned int state = (*cs).state.GetState();

	     switch (state)
		{
		    case SYN_SENT:
		    {
			//start everything over
			clist.erase(cs);
		    }
		    break;
		
		    case SYN_RCVD:
		    {
			Packet out_packet;
			packetMaker(out_packet, *cs, 0, S_FIN);//create fin packet
        		MinetSend(mux, out_packet);
			(*cs).state.SetLastSent((*cs).state.GetLastSent() + 1);
        		(*cs).state.SetState(FIN_WAIT1);
		    }
		    break;

		    case ESTABLISHED:
		    {
			Packet out_packet;
			packetMaker(out_packet, *cs, 0, S_FIN);//create fin packet
        		MinetSend(mux, out_packet);
			(*cs).state.SetLastSent((*cs).state.GetLastSent() + 1);
        		(*cs).state.SetState(FIN_WAIT1);
		    }
		    break;

		    case CLOSE_WAIT:
		    {
			Packet out_packet;
			packetMaker(out_packet, *cs, 0, S_FIN);//create fin packet
        		MinetSend(mux, out_packet);
			(*cs).state.SetLastSent((*cs).state.GetLastSent()+1);
        		(*cs).state.SetState(LAST_ACK);
		    }
		    break;

		    case CLOSING:
		    {
			clist.erase(cs);
			TCPState newTCPState = TCPState(rand(), LISTEN, 3);
			ConnectionToStateMapping<TCPState> constate;
                	constate.connection = s.connection;
			constate.state = newTCPState;
			constate.bTmrActive = false;

			clist.push_front(constate);
		
			SockRequestResponse repl;
			repl.type = STATUS;
			repl.connection = s.connection;
			repl.bytes = 0;
			repl.error = EOK;
			MinetSend(sock, repl);
		    }
		    break;
		}
	}
	break;

	case STATUS:
	{
	    //nothing for here apparently
	}
	break;

	default:
	{
		SockRequestResponse repl;
		repl.type=STATUS;
		repl.error=EWHAT;
		MinetSend(sock,repl);
	}
	break;
	

    }

}

void packetMaker(Packet &packet, ConnectionToStateMapping<TCPState>& constate, int size_of_data, int whichFlag)
{

  unsigned char flags = 0;

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
    case S_SYN:
      SET_SYN(flags);
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

void timeoutHandler(const MinetHandle &mux, ConnectionList<TCPState>::iterator cs, ConnectionList<TCPState> &clist)
{

    if ( (*cs).state.ExpireTimerTries() || 
	 (*cs).state.GetState()==SYN_SENT ||
	 (*cs).state.GetState()==TIME_WAIT )   //from diagram
    {
        cout << "Now closing the connection, timeout!!" << endl;
	clist.erase(cs);  //erase the connection
    }
    else if ((*cs).state.GetState()==ESTABLISHED)  //if we've been sending out data! resend!
    {
	(*cs).timeout = Time() + 40;
	ConnectionToStateMapping<TCPState>& constate = (*cs);

	int bufferLen = constate.state.SendBuffer.GetSize();
	const int Size = TCP_MAXIMUM_SEGMENT_SIZE;
	
	char buffer[Size];
	int whereAt = 0;

	//This should be the GoBackN stuff

	while( bufferLen > 0 )  //keep going until we've sent all of the stuff in the buffer
 	{
	    //Redo the whole process as in sending a packet...for each packet in the window that got skipped

	    //Resend the data, get the payload
	    int data = constate.state.SendBuffer.GetData(buffer, Size, whereAt); 
	    Packet out_packet = Packet(buffer, data);
	
	    // IP header stuff
	    IPHeader ih;
	    ih.SetProtocol(IP_PROTO_TCP);
	    ih.SetSourceIP(constate.connection.src);
	    ih.SetDestIP(constate.connection.dest);
	    ih.SetTotalLength(TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH + 20); // push it onto the packet
	    out_packet.PushFrontHeader(ih);
	
	    // TCP header stuff
	    TCPHeader tcpsah;
	    tcpsah.SetDestPort(constate.connection.destport, out_packet);
	    tcpsah.SetSourcePort(constate.connection.srcport, out_packet);
	    tcpsah.SetSeqNum(constate.state.GetLastSent() + whereAt + data, out_packet);
	    tcpsah.RecomputeChecksum(out_packet);
	    out_packet.PushBackHeader(tcpsah);
	
	    //Send the TCP segment out to mux
	    MinetSend(mux, out_packet);

	    bufferLen -= Size;  //shorted the buffer now that we sent out one of the packets
	    whereAt += Size;    //move over where to get data from the buffer

	}
    }
}


