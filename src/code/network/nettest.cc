// nettest.cc 
//	Test out message delivery between two "Nachos" machines,
//	using the Post Office to coordinate delivery.
//
//	Two caveats:
//	  1. Two copies of Nachos must be running, with machine ID's 0 and 1:
//		./nachos -m 0 -o 1 &
//		./nachos -m 1 -o 0 &
//
//	  2. You need an implementation of condition variables,
//	     which is *not* provided as part of the baseline threads 
//	     implementation.  The Post Office won't work without
//	     a correct implementation of condition variables.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "network.h"
#include "post.h"
#include "interrupt.h"

#define NBB_MSG	1

// Test out message delivery, by doing the following:
//	1. send a message to the machine with ID "farAddr", at mail box #0
//	2. wait for the other machine's message to arrive (in our mailbox #0)
//	3. send an acknowledgment for the other machine's message
//	4. wait for an acknowledgment from the other machine to our original message

void MailTest(int farAddr)
{
	PacketHeader outPktHdr, inPktHdr;
	MailHeader outMailHdr, inMailHdr;
	const char *data = "Hello there!";
	const char *response = "Got it!";
	char buffer[MaxMailSize];
	bool test;

	for (int i=0; i<NBB_MSG; i++)
	{
		outPktHdr.to		= farAddr;								// construct packet, mail header for original message To: destination machine, mailbox 0
		outMailHdr.to		= 0;									//		From: our machine, reply to: mailbox 1
		outMailHdr.from		= 1;
		outMailHdr.length	= strlen(data) + 1;
		test = postOffice->Send(outPktHdr, outMailHdr, data);		// Send the first message
		ASSERT(test);
		postOffice->Receive(0, &inPktHdr, &inMailHdr, buffer);		// Wait for the first message from the other machine
		printf("Got \"%s\" from %d, box %d, isACK = %d, ACK %d\n",
				buffer, inPktHdr.from, inMailHdr.from, inMailHdr.isAck, inMailHdr.ackId);
		fflush(stdout);

		outPktHdr.to		= inPktHdr.from;						// Send acknowledgement to the other machine (using "reply to" mailbox in the message that just arrived
		outMailHdr.to		= inMailHdr.from;
		outMailHdr.length	= strlen(response) + 1;
		test = postOffice->Send(outPktHdr, outMailHdr, response);
		ASSERT(test);
		postOffice->Receive(1, &inPktHdr, &inMailHdr, buffer);		// Wait for the ack from the other machine to the first message we sent.
		printf("Got \"%s\" from %d, box %d, isACK %d, ACK %d\n",buffer,inPktHdr.from,inMailHdr.from, inMailHdr.isAck, inMailHdr.ackId);
		fflush(stdout);
	}



	interrupt->Halt();											// Then we're done!
}
