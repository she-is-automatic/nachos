// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "time.h"


#define NONE         "\033[m"
#define RED          "\033[0;32;31m"
#define LIGHT_RED    "\033[1;31m"
#define GREEN        "\033[0;32;32m"
#define LIGHT_GREEN  "\033[1;32m"
#define BLUE         "\033[0;32;34m"
#define LIGHT_BLUE   "\033[1;34m"
#define DARY_GRAY    "\033[1;30m"
#define CYAN         "\033[0;36m"
#define LIGHT_CYAN   "\033[1;36m"
#define PURPLE       "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN        "\033[0;33m"
#define YELLOW       "\033[1;33m"
#define LIGHT_GRAY   "\033[0;37m"
#define WHITE        "\033[1;37m"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------
// PC自增
void AdvancePC()
{
    machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));         // 前一个PC
    machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));         // 当前PC
    machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4); // 下一个PC
}
// 作为新应用程序进程所关联的核心线程的执行代码
void StartProcess(int spaceId)
{
    ASSERT(currentThread->userProgramId() == spaceId);
    printf("------------------------------------------------------\n");
    printf("CurrentThreadPID: %d , StartProcess!\n", spaceId);

    currentThread->space->InitRegisters(); // 初始化寄存器
    currentThread->space->RestoreState();  // 加载页表到Machine中
    machine->Run();                        // 执行应用程序

    ASSERT(FALSE);
}
// 作为子进程所关联的核心线程的执行代码
void StartProcessFork(int spaceId)
{
    printf("------------------------------------------------------\n");
    printf("CurrentThreadPID: %d , StartForkProcess!\n", spaceId);
    printf("Childthread begin id=%d \n", (currentThread->space)->getSpaceId());
    if (currentThread->space != NULL)
    {
        currentThread->RestoreUserState();    // 恢复寄存器
        currentThread->space->RestoreState(); // 加载页表寄存器
    }
    else
    {
        printf("Childthread begin space =NULL\n");
    }
    AdvancePC();
    machine->Run();
}

void ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2); // 从2号寄存器取得系统调用号

    if (which == SyscallException)
    {
        switch (type)
        {
        case SC_Halt:   // 停机指令
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Halt\n" NONE, (currentThread->space)->getSpaceId());

            DEBUG('a', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;
        }
        case SC_Exit:   // 退出指令，返回退出码
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Exit\n" NONE, (currentThread->space)->getSpaceId());

            int exitCode = machine->ReadRegister(4);    // 从4号寄存器取出退出码
            machine->WriteRegister(2, exitCode);        // 返回值为退出码

            currentThread->setExitCode(exitCode); // 设置当前进程退出码

            // 释放该进程的内存空间及其页表，释放分配给该进程的实页（帧）
            delete currentThread->space;
            // 结束该进程对应的线程
            currentThread->Finish();     
            AdvancePC();
            break;
        }
        case SC_Exec:   // 执行程序，返回pid
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Exec\n" NONE, (currentThread->space)->getSpaceId());

            // 打开可执行文件.noff
            int addr = machine->ReadRegister(4); // 从4号寄存器得到文件名的地址
                                                 // 获取文件名filename
            char filename[50];
            for (int i = 0;; i++)
            {
                machine->ReadMem(addr + i, 1, (int *)&filename[i]);
                if (filename[i] == '\0')
                    break;
            }

            // 根据文件名打开文件
            OpenFile *executable = fileSystem->Open(filename); // 打开文件
            if (executable == NULL)
            {
                printf("Unable to open file %s\n", filename);
                AdvancePC();
                return;
            }

            // 为应用程序分配地址空间
            AddrSpace *space = new AddrSpace(executable);
            printf("PageTable of Exec NewThread: %s\n", filename);
            space->Print();    // 输出新分配的地址空间
            delete executable; // 关闭文件

            // 创建新的核心线程，并将用户线程映射到核心线程
            Thread *thread = new Thread(filename);
            printf("NewThreadPID: %d, Name: %s\n", space->getSpaceId(), filename);
            thread->space = space;

            
            // 将新线程设置为当前线程的子线程，加入子线程列表；当前线程设置为新线程的父线程
            currentThread->ConnectThread(thread); // 成为当前进程的子进程
            // 设置thread线程执行的服务例程，即startProcess
            thread->Fork(StartProcess, (int)space->getSpaceId());
            // 返回进程的SpaceId，将spaceId写入2号寄存器
            machine->WriteRegister(2, space->getSpaceId());
            AdvancePC();
            break;
        }
        case SC_Join:   // 父进程等待子进程，返回等待的子进程的退出码
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Join\n" NONE, (currentThread->space)->getSpaceId());
            int SpaceId = machine->ReadRegister(4); // 获取参数spaceId
            currentThread->Join(SpaceId);

            // Joinee 的退出码 waitProcessExitCode 写入2号寄存器
            machine->WriteRegister(2, currentThread->waitExitCode());
            AdvancePC();
            break;
        }
        case SC_Yield:
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Yield\n" NONE, (currentThread->space)->getSpaceId());

            currentThread->Yield();
            AdvancePC();
            break;
        }
        case SC_Fork:   // 创建子进程，子进程返回值为0，父进程返回值为子进程pid
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Fork\n" NONE, (currentThread->space)->getSpaceId());
            
            // 创建子进程，构建进程树
            Thread *thread = new Thread("new thread");
            currentThread->ConnectThread(thread);

            // 复制父进程地址空间
            thread->space = new AddrSpace(thread->GetParentThread()->space);
            printf("My ChildThread PID= %d\n", thread->space->getSpaceId());
            printf("PageTable of FatherThread:\n");
            currentThread->space->Print();             //输出该作业的页表信息
            printf("PageTable of ChildThread:\n");
            thread->space->Print();             //输出子线程的页表信息

            // 复制父进程所有的寄存器,以便于与父进程在同一地址开始执行
            for (int i = 0; i < NumTotalRegs; i++)
                thread->setuserRegisters(i, machine->ReadRegister(i));

            // 子进程fork的返回值为0
            thread->setuserRegisters(2, 0);
            // 启动子进程
            thread->Fork(StartProcessFork, thread->space->getSpaceId()); 

            // 父进程fork返回值为子进程号
            machine->WriteRegister(2, thread->space->getSpaceId());
            AdvancePC();
            break;
        }
        case SC_Time:   // 系统时间
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Time\n" NONE, (currentThread->space)->getSpaceId());

            time_t timep;
            time(&timep); // 获取从1970至今过了多少秒，存入time_t类型的timep
            printf("--------------------------------------------\n");
            printf("%s", ctime(&timep)); // 用ctime将秒数转化成字符串格式
            printf("--------------------------------------------\n");
            AdvancePC();
            break;
        }
        case SC_Create: // 创建文件
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Create\n" NONE, (currentThread->space)->getSpaceId());

            // 从4号寄存器读入参数文件名
            int addr = machine->ReadRegister(4);
            char filename[128];
            for (int i = 0; i < 128; i++)
            {
                machine->ReadMem(addr + i, 1, (int *)&filename[i]);
                if (filename[i] == '\0')
                    break;
            }

            // 打开文件，没有则创建
            int fileDescriptor = OpenForWrite(filename);
            if (fileDescriptor == -1)
                printf("Create file %s Failed!\n", filename);
            else
                printf("Create file %s Succeed, File id: %d\n", filename, fileDescriptor);
            
            // 关闭文件，PC+1
            Close(fileDescriptor);
            AdvancePC();
            break;
        }
        case SC_Open:   // 打开文件，返回文件描述符fd
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Open\n" NONE, (currentThread->space)->getSpaceId());

            // 从4号寄存器读入参数文件名
            int addr = machine->ReadRegister(4);
            char filename[128];
            for (int i = 0; i < 128; i++)
            {
                machine->ReadMem(addr + i, 1, (int *)&filename[i]);
                if (filename[i] == '\0')
                    break;
            }

            // 打开文件
            int fileDescriptor = OpenForWrite(filename);
            if (fileDescriptor == -1)
                printf("Open file %s Failed!\n", filename);
            else
                printf("Open file %s Succeed, File id: %d\n", filename, fileDescriptor);
            
            // 返回文件描述符
            machine->WriteRegister(2, fileDescriptor);
            AdvancePC();
            break;
        }
        case SC_Write:  // 将内容写入文件，返回写入的字节数
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Write\n" NONE, (currentThread->space)->getSpaceId());

            // 读取寄存器信息
            int addr = machine->ReadRegister(4);   // 要写入的字符串的首地址
            int size = machine->ReadRegister(5);   // 字节数
            int fileId = machine->ReadRegister(6); // fd

            // 打开文件
            OpenFile *openfile = new OpenFile(fileId);
            ASSERT(openfile != NULL);

            // 从内存中读出要写入的字符串到buffer
            char buffer[128];
            for (int i = 0; i < size; i++)
            {
                machine->ReadMem(addr + i, 1, (int *)&buffer[i]);
                if (buffer[i] == '\0')
                    break;
            }
            buffer[size] = '\0';

            // 写入的位置
            int writePos;
            if (fileId == 1) writePos = 0;
            else writePos = openfile->Length();
            // 在 writePos 后面进行数据添加
            int writtenBytes = openfile->WriteAt(buffer, size, writePos);
            if (writtenBytes == 0)
                printf("Write file Failed!\n");
            else
                printf("\"%s\" has written in File id: %d!\n", buffer, fileId);
            AdvancePC();
            break;
        }
        case SC_Read:   // 从文件中读取数据，返回读出的字节数
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Read\n" NONE, (currentThread->space)->getSpaceId());
            
            // 读取寄存器信息
            int addr = machine->ReadRegister(4);   // 从文件中读取的内容要放到哪里
            int size = machine->ReadRegister(5);   // 字节数
            int fileId = machine->ReadRegister(6); // fd

            // 将文件数据读到buffer中
            char *buffer = new char[size + 1];
            OpenFile *openfile = new OpenFile(fileId);
            ASSERT(openfile != NULL);
            int readnum = openfile->Read(buffer, min(size, openfile->Length()));
            buffer[readnum] = '\0';
            
            // 写到内存指定地址addr
            for (int i = 0; i <= readnum; ++i)
                if(!machine->WriteMem(addr + i, 1, buffer[i]))
                    printf("Read Error Occured!\n");

            printf("Read %d bytes, content: %s\n", readnum, buffer);
            
            // 返回读取的有效字节数
            machine->WriteRegister(2, readnum); 
            AdvancePC();
            break;
        }
        case SC_Close:  // 关闭文件
        {
            printf(BLUE "\nCurrentThreadPID: %d , SC_Close\n" NONE, (currentThread->space)->getSpaceId());

            int fileId = machine->ReadRegister(4);  // 文件描述符
            Close(fileId);
            printf("File %d Closed Succeed!\n", fileId);
            AdvancePC();
            break;
        }
        }
    }
    else
    {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
