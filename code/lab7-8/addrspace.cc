// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------
static void
SwapHeader(NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    ASSERT(pidMap->NumClear() >= 1); // 确定有空闲的线程号
    spaceId = pidMap->Find() + 100;  // 0-99留给内核线程

    // 读出noff文件头
    NoffHeader noffH;
    unsigned int i, size;
    // 读出noff文件
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) &&
        (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    // 计算地址空间大小 代码 + 初始化 + 未初始化数据 + 用户栈
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize; // we need to increase the size
                                                                                          // to leave room for the stack
    numPages = divRoundUp(size, PageSize);                                                // 确定页数
    size = numPages * PageSize;                                                           // 计算真实占用大小

    ASSERT(numPages <= NumPhysPages); // check we're not trying
                                      // to run anything too big --
                                      // at least until we have
                                      // virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
          numPages, size);

    /**************************** 创建页表 **********************************/
    // 首先确定有足够的空闲帧
    ASSERT(numPages <= freeMap->NumClear());
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++)
    {                                                // 虚页实页映射
        pageTable[i].virtualPage = i;                // for now, virtual page # = phys page #
        pageTable[i].physicalPage = freeMap->Find(); // 寻找空闲页进行分配
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE; // if the code segment was entirely on
                                       // a separate page, we could set its
                                       // pages to be read-only
    }

    // 将noff文件代码和初始化数据段复制到物理内存中
    // 通过虚拟地址和页大小计算出所在页和页内偏移量，然后得出所在页对应的物理帧，最后加上页内偏移量
    if (noffH.code.size > 0)
    {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
              noffH.code.virtualAddr, noffH.code.size);
        unsigned int vpn, offset, ppn;              // virtualPageNumber/offset/ physicalPageNumber
        vpn = noffH.code.virtualAddr / PageSize;    // 虚页号
        offset = noffH.code.virtualAddr % PageSize; // 页内偏移量
        ppn = pageTable[vpn].physicalPage * PageSize + offset;  // 实页号
        executable->ReadAt(&(machine->mainMemory[ppn]),
                           noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0)
    {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
              noffH.initData.virtualAddr, noffH.initData.size);
        unsigned int vpn, offset, ppn;                  // virtualPageNumber/offset/ physicalPageNumber
        vpn = noffH.initData.virtualAddr / PageSize;    // 虚页号
        offset = noffH.initData.virtualAddr % PageSize; // 页内偏移量
        ppn = pageTable[vpn].physicalPage * PageSize + offset;
        executable->ReadAt(&(machine->mainMemory[ppn]),
                           noffH.initData.size, noffH.initData.inFileAddr);
    }

    // 初始化打开文件的文件描述符
    for (int i=3;i<10;i++)    //up to open 10 file for each process
       fileDescriptor[i] = NULL; 
       
    OpenFile *StdinFile = new OpenFile("stdin");
    OpenFile *StdoutFile = new OpenFile("stdout");
    OpenFile *StderrFile = new OpenFile("stderr");
    fileDescriptor[0] =  StdinFile;
    fileDescriptor[1] =  StdoutFile;
    fileDescriptor[2] =  StderrFile;

}

AddrSpace::AddrSpace(AddrSpace *space)
{
    ASSERT(pidMap->NumClear() >= 1); // 确定有空闲的线程号
    spaceId = pidMap->Find() + 100;  // 0-99留给内核线程
    numPages = space->getnumPages();
    // 第一步，创建页表
    // 首先确定有足够的空闲帧
    ASSERT(numPages <= freeMap->NumClear());
    pageTable = new TranslationEntry[numPages];
    for (int i = 0; i < numPages; i++)
    {                                                // 虚页实页映射
        pageTable[i].virtualPage = i;                // for now, virtual page # = phys page #
        pageTable[i].physicalPage = freeMap->Find(); // 寻找空闲页进行分配
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
    }
    unsigned int i, size;
    // 复制父进程数据
    size = numPages * PageSize; // 计算真实占用大小
    if (size > 0)
    {
        int physicalPagefirst = space->getphysicalPagefirst();
        unsigned int ppa;
        ppa = pageTable[0].physicalPage * PageSize;
        for (i = 0; i < size; i++)
        {
            machine->mainMemory[ppa + i] = machine->mainMemory[physicalPagefirst * PageSize + i];
        }
    }

    // 初始化打开文件的文件描述符
    for (int i=3;i<10;i++)    //up to open 10 file for each process
       fileDescriptor[i] = NULL; 
       
    OpenFile *StdinFile = new OpenFile("stdin");
    OpenFile *StdoutFile = new OpenFile("stdout");
    OpenFile *StderrFile = new OpenFile("stderr");
    fileDescriptor[0] =  StdinFile;
    fileDescriptor[1] =  StdoutFile;
    fileDescriptor[2] =  StderrFile;

}
//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    for (int i = 0; i < numPages; i++) // 释放物理空间
        freeMap->Clear(pageTable[i].physicalPage);
    pidMap->Clear(spaceId - 100); // 释放进程号
    delete[] pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void AddrSpace::InitRegisters() // 初始化寄存器
{
    int i;

    for (i = 0; i < NumTotalRegs; i++) // 将寄存器register[i]初值置0
        machine->WriteRegister(i, 0);

    machine->WriteRegister(PCReg, 0); // PC寄存器初值置0

    machine->WriteRegister(NextPCReg, 4); // 存储下一个PC值的寄存器置4

    // 将栈顶指针初始化为应用程序空间的尾部,减16避免越界
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState()
{
    pageTable = machine->pageTable;
    numPages = machine->pageTableSize;
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------
// 将用户进程的页表传递给 Machine 类
void AddrSpace::RestoreState()
{
    machine->pageTable = pageTable;    // 页表项
    machine->pageTableSize = numPages; // 页表大小
}


void AddrSpace::Print()
{
    printf("page table dump: %d pages in total\n", numPages);
    printf("=======================================================\n");
    printf("VirtPage : ");
    for (int i = 0; i < numPages; i++)
        printf("%2d ", pageTable[i].virtualPage);
    printf("\nPhysPage : ");
    for (int i = 0; i < numPages; i++)
        printf("%2d ", pageTable[i].physicalPage);
    printf("\n=======================================================\n\n");
}


int AddrSpace::getFileDescriptor(OpenFile * openfile)
{
    for (int i=3;i<10;i++)
    {
      if (fileDescriptor[i] == NULL)
        {
           fileDescriptor[i] = openfile;
           return i;
        }  //if      
    }  //for
    return -1;
 }


OpenFile* AddrSpace::getFileId(int fd)
{
   return fileDescriptor[fd]; 
}

void AddrSpace::releaseFileDescriptor(int fd)
{
   fileDescriptor[fd] = NULL;
}
