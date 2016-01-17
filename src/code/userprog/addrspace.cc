// addrspace.cc 
//      Routines to manage address spaces (executing user programs).
//
//      In order to run a user program, you must:
//
//      1. link with the -N -T 0 option 
//      2. run coff2noff to convert the object file to Nachos format
//              (Nachos object code format is essentially just a simpler
//              version of the UNIX executable object code format)
//      3. load the NOFF file into the Nachos file system
//              (if you haven't implemented the file system yet, you
//              don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

#include <strings.h>		/* for bzero */


// +b simbadSid 15.01.2015



//----------------------------------------------------------------------
// SwapHeader
//      Do little endian to big endian conversion on the bytes in the
//      object file header, in case the file was generated on a little
//      endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void
SwapHeader (NoffHeader * noffH)
{
    noffH->noffMagic = WordToHost (noffH->noffMagic);
    noffH->code.size = WordToHost (noffH->code.size);
    noffH->code.virtualAddr = WordToHost (noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost (noffH->code.inFileAddr);
    noffH->initData.size = WordToHost (noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost (noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost (noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost (noffH->uninitData.size);
    noffH->uninitData.virtualAddr =
	WordToHost (noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost (noffH->uninitData.inFileAddr);
}

// +b simbadSid 15.01.2015
// --------------------------------------------------------------------
// Reads in the input file numBytes starting at the given position.
// The read bytes are stored at the given virtual address in the
// memory described by (pageTable, numPages)
// --------------------------------------------------------------------
static void ReadAtVirtual(OpenFile *executable, int virtualaddr, int numBytes, int position, TranslationEntry *pageTable, unsigned numPages)
{
	IntStatus oldLevel		= interrupt->SetLevel(IntOff);
	machine->pageTable		= pageTable;
	machine->pageTableSize	= numPages;
	interrupt->SetLevel(oldLevel);

	char buffer[numBytes];
	int nbrRedBytes	= executable->ReadAt(buffer, numBytes, position);
	int virtualByteShift;

    oldLevel = interrupt->SetLevel(IntOff);
    for(virtualByteShift=0; virtualByteShift<nbrRedBytes; virtualByteShift++)
        machine->WriteMem(virtualaddr+virtualByteShift, 1, buffer[virtualByteShift]);
    interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
//      Create an address space to run a user program.
//      Load the program from a file "executable", and set everything
//      up so that we can start executing user instructions.
//
//      Assumes that the object code file is in NOFF format.
//
//      First, set up the translation from program memory to physical 
//      memory.  For now, this is really simple (1:1), since we are
//      only uniprogramming, and we have a single unsegmented page table
//
//      "executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace (OpenFile * executable)
{
	NoffHeader noffH;
	unsigned int i, size;

	executable->ReadAt ((char *) &noffH, sizeof (noffH), 0);								// Reads the header of the executable file
	if ((noffH.noffMagic != NOFFMAGIC) && (WordToHost (noffH.noffMagic) == NOFFMAGIC))		// Convert the header to the host endian
		SwapHeader (&noffH);

//TODO ask why
	ASSERT (noffH.noffMagic == NOFFMAGIC);													// Ensure that the executable has the good file type
//TODO ask why

// how big is address space?
	size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;	// Read the expected addrspace size expected by the executable file
																							// we need to increase the size to leave room for the stack
	numPages	= divRoundUp (size, PageSize);
	pageBitmap	= new BitMap(numPages);
	size		= numPages * PageSize;
	ASSERT (numPages <= NumPhysPages);														// check we're not trying to run anything too big --
																							// TODO at least until we have virtual memory

	DEBUG ('a', "Initializing address space, num pages %d, size %d\n", numPages, size);
// first, set up the translation
	pageTable = new TranslationEntry[numPages];												// Initialize the page table of the addrSpace
	for (i = 0; i < numPages; i++)
	{
		pageTable[i].virtualPage	= i;													//		TODO for now, virtual page # = phys page #
		pageTable[i].physicalPage	= i;
		pageTable[i].valid			= TRUE;
		pageTable[i].use			= FALSE;
		pageTable[i].dirty			= FALSE;
		pageTable[i].readOnly		= FALSE;												//		if the code segment was entirely on
																							//		a separate page, we could set its pages to be read-only
	}

	bzero (machine->mainMemory, size);														// zero out the entire address space

	unsigned int nbrCodePages = divRoundUp (noffH.code.size, PageSize);
	unsigned int nbrDataPages = divRoundUp (noffH.initData.size, PageSize);
	if (noffH.code.size > 0)																// Copy code and segments of the executable into addrSpace
	{
		DEBUG ('a', "Initializing code segment, at 0x%x, size %d\n",
				noffH.code.virtualAddr, noffH.code.size);
// TODO test for the step 4 question I.3
//		executable->ReadAt (&(machine->mainMemory[noffH.code.virtualAddr]),					//		Writes the file code section in memory
//				noffH.code.size, noffH.code.inFileAddr);
		ReadAtVirtual(executable, noffH.code.virtualAddr, noffH.code.size,					//		Writes the file code section in memory
				noffH.code.inFileAddr, pageTable, numPages);
// TODO end test for the step 4 question I.3
		for (i=0; i<nbrCodePages; i++)
		{
			this->pageBitmap->Mark(i);														//		Notify the code pages as used
			pageTable[i].readOnly	= FALSE;												//		Ensure that the code will not be corrupted
		}
	}
	if (noffH.initData.size > 0)															// Copy data segments of the executable into addrSpace
	{
		DEBUG ('a', "Initializing data segment, at 0x%x, size %d\n",
				noffH.initData.virtualAddr, noffH.initData.size);
// TODO test for the step 4 question I.3
//		executable->ReadAt (&(machine->mainMemory[noffH.initData.virtualAddr]),				//		Writes the file datd section in memory
//				noffH.initData.size, noffH.initData.inFileAddr);
		ReadAtVirtual(executable, noffH.initData.virtualAddr, noffH.initData.size,			//		Writes the file data section in memory
				noffH.initData.inFileAddr, pageTable, numPages);
// TODO end test for the step 4 question I.3

		for (i=0; i<nbrDataPages; i++)	this->pageBitmap->Mark(i+nbrCodePages);				//		Notify the data pages as used
	}
	int remainingCode	= nbrCodePages % PageSize;
	int remainingData	= nbrDataPages % PageSize;
	if ((remainingCode != 0) && (remainingData != 0) && ((remainingCode + remainingData) > PageSize))
		 this->pageBitmap->Mark(nbrCodePages + nbrDataPages - 1);
}
// +e simbadSid 15.01.2015

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
//      Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace ()
{
  // LB: Missing [] for delete
  // delete pageTable;
  delete [] pageTable;
  // End of modification
  delete pageBitmap;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
//      Set the initial values for the user-level register set.
//
//      We write these directly into the "machine" registers, so
//      that we can immediately jump to user code.  Note that these
//      will be saved/restored into the currentThread->userRegisters
//      when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters ()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
    	machine->WriteRegister (i, 0);

    machine->WriteRegister (PCReg, 0);			// Initial program counter -- must be location of "Start"
    machine->WriteRegister (NextPCReg, 4);		// Need to also tell MIPS where next instruction is, because
    											// of branch delay possibility
    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we don't
    // accidentally reference off the end!
    machine->WriteRegister (StackReg, numPages * PageSize - 16);
    DEBUG ('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
//      On a context switch, save any machine state, specific
//      to this address space, that needs saving.
//
//      For now, nothing!
//----------------------------------------------------------------------

void
AddrSpace::SaveState ()
{
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
//      On a context switch, restore the machine state so that
//      this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void
AddrSpace::RestoreState ()
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

// +b simbadSid 15.01.2016

//----------------------------------------------------------------------
// Allocate memory in the userStack for a new thread.
// Marks all the memory as allocated in the address space bitmap.
//----------------------------------------------------------------------
int AddrSpace::AllocateThreadStack(int nbrStackPages)
{
// TODO begin critical section
	int highestPage	= this->pageBitmap->FindLast(nbrStackPages);				// Get the index of the lowest free page
	int lowestPage	= highestPage - nbrStackPages;
	int page;
	void *physicalPageAddress;

	if(highestPage == -1)														// Case full memory:
	{
// TODO replace this by a page creation
		DEBUG('t', "\t*** AllocateThreadStack: full memory***\n");
		return -1;
	}
	for(page=highestPage; page>lowestPage; page--)								// For each page of the allocated stack
	{
		this->pageBitmap->Mark(page);											//		Mark the page as allocated
		physicalPageAddress = &machine->mainMemory[pageTable[page].physicalPage];
		bzero(physicalPageAddress, PageSize);									//		Initialize the corresponding physical memory with zeros
	}
// TODO replace the alignment address 16 by a macros
	int stackPointer = (highestPage * PageSize) - 16;							// Compute the virtual address of the new stack pointer
// TODO stop critical section
// TODO return the physical translation of the virtual address stackPointer
	return stackPointer;
}
// --------------------------------------------------------------------
// Return the address space size in bytes
// --------------------------------------------------------------------
int AddrSpace::GetSize()
{
	return this->numPages*PageSize;
}

