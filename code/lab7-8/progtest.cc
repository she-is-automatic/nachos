// progtest.cc 
//	Test routines for demonstrating that Nachos can load
//	a user program and execute it.  
//
//	Also, routines for testing the Console hardware device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "console.h"
#include "addrspace.h"
#include "synch.h"

//----------------------------------------------------------------------
// StartProcess
// 	Run a user program.  Open the executable, load it into
//	memory, and jump to it.
//----------------------------------------------------------------------
// 为用户程序 filename 创建相应的进程，并启动该进程的执行
void
StartProcess(char *filename)
{
    OpenFile *executable = fileSystem->Open(filename); // 打开文件
    AddrSpace *space;                                 // 定义地址空间

    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    space = new AddrSpace(executable); // 为应用程序分配内存地址空间
    currentThread->space = space;   //将该进程映射到一个核心线程
    // space->Print();             //输出该作业的页表信息
    printf("------------------------------------------------------\n");
    printf("MainThreadPID: %d, fileName: %s\n",space->getSpaceId(),filename);
    delete executable;		 	// 关闭文件

    space->InitRegisters();		// 初始化 CPU 的寄存器，包括数据寄存器、PC 以及栈指针 set the initial register values
    space->RestoreState();		// 是将用户进程的页表传递给系统核心（Machine 类） load page table register

    machine->Run();			// 从程序入口开始，完成取指令、译码、执行的过程 jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}

// Data structures needed for the console test.  Threads making
// I/O requests wait on a Semaphore to delay until the I/O completes.

static Console *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

//----------------------------------------------------------------------
// ConsoleInterruptHandlers
// 	Wake up the thread that requested the I/O.
//----------------------------------------------------------------------

static void ReadAvail(_int arg) { readAvail->V(); }
static void WriteDone(_int arg) { writeDone->V(); }

//----------------------------------------------------------------------
// ConsoleTest
// 	Test the console by echoing characters typed at the input onto
//	the output.  Stop when the user types a 'q'.
//----------------------------------------------------------------------

void 
ConsoleTest (char *in, char *out)
{
    char ch;

    console = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    
    for (;;) {
	readAvail->P();		// wait for character to arrive
	ch = console->GetChar();
	console->PutChar(ch);	// echo it!
	writeDone->P() ;        // wait for write to finish
	if (ch == 'q') return;  // if q, quit
    }
}
