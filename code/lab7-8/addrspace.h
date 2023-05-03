// addrspace.h
//	Data structures to keep track of executing user programs
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "bitmap.h"
#include "machine.h"

#define UserStackSize 1024                         // increase this as necessary!
static BitMap *pidMap = new BitMap(128);           // 管理 pid 的分配
static BitMap *freeMap = new BitMap(NumPhysPages); // 管理空闲帧

class AddrSpace
{
public:
  AddrSpace(OpenFile *executable); // Create an address space,
                                   // initializing it with the program
                                   // stored in the file "executable"
  ~AddrSpace();                    // De-allocate an address space

  void InitRegisters(); // 初始化寄存器 Initialize user-level CPU registers,
                        // before jumping to user code

  void SaveState();    // Save/restore address space-specific
  void RestoreState(); // 将用户进程的页表传递给 Machine 类
  void Print();        // 输出页表信息
  unsigned int getSpaceId() { return spaceId; }

  // 用于复制父进程地址空间
  AddrSpace(AddrSpace *space);                                     // 重构构造函数
  unsigned int getnumPages() { return numPages; }                  // 获得页数
  int getphysicalPagefirst() { return pageTable[0].physicalPage; } // 获得首物理页号

private:
  TranslationEntry *pageTable; // 页表
  unsigned int numPages;       // 页数
  unsigned int spaceId;        // 进程相应的 pid

  // 文件描述符，0,1,2分别为stdin,stdout与stderr
  OpenFile *fileDescriptor[10];

public:
  int getFileDescriptor(OpenFile *openfile);
  OpenFile *getFileId(int fd);
  void releaseFileDescriptor(int fd);
};

#endif // ADDRSPACE_H
