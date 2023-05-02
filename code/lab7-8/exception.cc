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
//PC自增
void AdvancePC(){
    machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));		// 前一个PC
    machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));		// 当前PC
    machine->WriteRegister(NextPCReg,machine->ReadRegister(NextPCReg)+4);	// 下一个PC
}
//作为新应用程序进程所关联的核心线程的执行代码
void StartProcess(int spaceId) {
    ASSERT(currentThread->userProgramId() == spaceId);
    printf("------------------------------------------------------\n");
    printf("CurrentThreadPID: %d , Change!\n",spaceId);
    currentThread->space->InitRegisters();     // 设置寄存器初值
    currentThread->space->RestoreState();      // 加载页表寄存器
    machine->Run();             // 运行
    ASSERT(FALSE);
}
//作为子进程所关联的核心线程的执行代码
void StartProcessFork(int spaceId){
    printf("------------------------------------------------------\n");
    printf("CurrentThreadPID: %d , Change!\n",spaceId);
    printf("Childthread begin id=%d \n",(currentThread->space)->getSpaceId());
    if(currentThread->space!=NULL){
        currentThread->RestoreUserState();   //恢复寄存器
        currentThread->space->RestoreState();//加载页表寄存器
    }
    else{
        printf("Childthread begin space =NULL\n");
    }
    AdvancePC();
    machine->Run();
}

void ExceptionHandler(ExceptionType which) {
    int type = machine->ReadRegister(2);	// 2号寄存器

    if(which == SyscallException){
    	switch(type){
    		case SC_Halt:{
    			printf("CurrentThreadPID: %d , This is SC_Halt\n",(currentThread->space)->getSpaceId());
        
	    		DEBUG('a', "Shutdown, initiated by user program.\n");
			   	interrupt->Halt();
    			break;
    		}
            case SC_Exit:{
                printf("CurrentThreadPID: %d , This is SC_Exit\n",(currentThread->space)->getSpaceId());
        
    			int exitCode = machine->ReadRegister(4);
    			machine->WriteRegister(2,exitCode);
    			currentThread->setExitCode(exitCode); //设置当前进程退出码
    			// 父进程的退出码特殊标记，由 Join 的实现方式决定
    			//if(exitCode == 99)
    				//scheduler->emptyList(scheduler->getTerminatedList()); //清空terminatedList
                //释放该进程的内存空间及其页表、pid
   				delete currentThread->space;
   				currentThread->Finish(); //该进程对应的线程
                AdvancePC();
    			break;
    		}
    		case SC_Exec:{
    			printf("CurrentThreadPID: %d , This is SC_Exec\n",(currentThread->space)->getSpaceId());
        
    			int addr = machine->ReadRegister(4);
                //1 得到文件名
    			char filename[50];  
    			for(int i = 0; ; i++){
    				machine->ReadMem(addr+i,1,(int *)&filename[i]);
    				if(filename[i] == '\0') break;
    			}
    			OpenFile *executable = fileSystem->Open(filename);  //打开文件
    			if(executable == NULL) {
    				printf("Unable to open file %s\n",filename);
    				return;
    			}
                //2 为应用程序分配内存地址空间
    			AddrSpace *space = new AddrSpace(executable);
    			//space->Print();   // 输出新分配的地址空间
    			delete executable;	// 关闭文件 
    			// 建立新核心线程
    			Thread *thread = new Thread(filename);
                printf("NewThreadPID: %d, Name: %s\n",space->getSpaceId(),filename);
    			//3 将用户进程映射到核心线程上
    			thread->space = space;
                //4 构建进程树
                currentThread->ConnectThread(thread);//成为当前进程的子进程
                
    			thread->Fork(StartProcess,(int)space->getSpaceId());
                //5 返回SpaceId号
    			machine->WriteRegister(2,space->getSpaceId());
    			AdvancePC();
    			break;
    		}
    		case SC_Join:{//父进程等待子进程执行完成
                printf("CurrentThreadPID: %d , This is SC_Join\n",(currentThread->space)->getSpaceId());
        
    			int SpaceId = machine->ReadRegister(4);//获取参数
    			currentThread->Join(SpaceId);
    			//返回 Joinee 的退出码 waitProcessExitCode
    			machine->WriteRegister(2, currentThread->waitExitCode());
    			AdvancePC();
    			break;
    		}
            case SC_Yield:{
                printf("CurrentThreadPID: %d , This is SC_Yield\n",(currentThread->space)->getSpaceId());
        
                currentThread->Yield();
                AdvancePC();
                break;
            }
            case SC_Fork:{
                printf("CurrentThreadPID: %d , This is SC_Fork\n",(currentThread->space)->getSpaceId());
        
                Thread *thread = new Thread("new thread");
                //4 构建进程树
                currentThread->ConnectThread(thread);//成为当前进程的子进程
                //复制父进程地址空间
                thread->space = new AddrSpace(thread->GetParentThread()->space);
                printf("My ChildThread PID= %d\n",thread->space->getSpaceId());
                //space->Print();             //输出该作业的页表信息
                //复制父进程所有的寄存器,以便于与父进程在同一地址开始执行
                for(int i=0;i<NumTotalRegs;i++){
                    thread->setuserRegisters(i,machine->ReadRegister(i));
                }
                //2 号寄存器写入 0，作为返回值
                thread->setuserRegisters(2,0);
                thread->Fork(StartProcessFork,thread->space->getSpaceId());//启动子线程
                //返回子进程号
                machine->WriteRegister(2,thread->space->getSpaceId());
                AdvancePC();
                break;
            }
            case SC_Time:{
                printf("CurrentThreadPID: %d , This is SC_Time\n",(currentThread->space)->getSpaceId());
        
                time_t timep;
                time(&timep); //获取从1970至今过了多少秒，存入time_t类型的timep
                printf("--------------------------------------------\n");
                printf("%s", ctime(&timep));//用ctime将秒数转化成字符串格式
                printf("--------------------------------------------\n");
                AdvancePC(); 
                break;
            }
            case SC_Create:{
                printf("CurrentThreadPID: %d , This is SC_Create\n",(currentThread->space)->getSpaceId());
        
                int addr = machine->ReadRegister(4);
                char filename[128];
                for(int i = 0; i < 128; i++){
                    machine->ReadMem(addr+i,1,(int *)&filename[i]);
                    if(filename[i] == '\0') break;
                }
                int fileDescriptor = OpenForWrite(filename);
                if(fileDescriptor == -1) printf("create file %s failed!\n",filename);
                else printf("create file %s succeed, the file id is %d\n",filename,fileDescriptor);
                Close(fileDescriptor);
                AdvancePC();
                break;
            }
            case SC_Open:{
                printf("CurrentThreadPID: %d , This is SC_Open\n",(currentThread->space)->getSpaceId());
        
                int addr = machine->ReadRegister(4);
                char filename[128];
                for(int i = 0; i < 128; i++){
                    machine->ReadMem(addr+i,1,(int *)&filename[i]);
                    if(filename[i] == '\0') break;
                }
                int fileDescriptor = OpenForWrite(filename);
                if(fileDescriptor == -1) printf("Open file %s failed!\n",filename);
                else printf("Open file %s succeed, the file id is %d\n",filename,fileDescriptor);                
                machine->WriteRegister(2,fileDescriptor);
                AdvancePC();
                break;
            }
            case SC_Write:{
                printf("CurrentThreadPID: %d , This is SC_Write\n",(currentThread->space)->getSpaceId());
        
                // 读取寄存器信息
                int addr = machine->ReadRegister(4);
                int size = machine->ReadRegister(5);       // 字节数
                int fileId = machine->ReadRegister(6);      // fd
                
                // 打开文件
                OpenFile *openfile = new OpenFile(fileId);
                ASSERT(openfile != NULL);

                // 读取具体数据
                char buffer[128];
                for(int i = 0; i < size; i++){
                    machine->ReadMem(addr+i,1,(int *)&buffer[i]);
                    if(buffer[i] == '\0') break;
                }
                buffer[size] = '\0';

                // 写入数据
                int writePos;
                if(fileId == 1) writePos = 0;
                else writePos = openfile->Length();
                // 在 writePos 后面进行数据添加
                int writtenBytes = openfile->WriteAt(buffer,size,writePos);
                if(writtenBytes == 0) printf("write file failed!\n");
                else printf("\"%s\" has wrote in file %d succeed!\n",buffer,fileId);
                AdvancePC();
                break;
            }
            case SC_Read:{
                printf("CurrentThreadPID: %d , This is SC_Read\n, ",(currentThread->space)->getSpaceId());
        
                int addr = machine->ReadRegister(4);
                int size = machine->ReadRegister(5);
                int fileId = machine->ReadRegister(6);
                //将具体数据读到buffer中
                char *buffer = new char[size + 1];
                OpenFile *openfile = new OpenFile(fileId);
                ASSERT(openfile != NULL);
                int readnum = openfile->Read(buffer, min(size, openfile->Length()));
                buffer[readnum] = '\0';
                //写到内存指定地址
                for(int i = 0; i <= readnum; ++i) {
                    machine->WriteMem(addr + i, 1, buffer[i]);
                }
                printf("read %d bytes, content: %s\n", readnum, buffer);
                machine->WriteRegister(2, readnum); //返回读取的有效字节数
                AdvancePC();
                break;
            }
            case SC_Close:{
                printf("CurrentThreadPID: %d , This is SC_Close\n, ",(currentThread->space)->getSpaceId());
        
                int fileId = machine->ReadRegister(4);
                Close(fileId);
                printf("File %d closed succeed!\n",fileId);
                AdvancePC();
                break;
            }
        }
	
    }else {
		printf("Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
    }
}
