/*@H
* File: win32_ray.cpp
* Author: Jesse Calvert
* Created: October 24, 2017, 17:48
* Last modified: October 24, 2017, 18:10
*/

#include "windows.h"

internal void RenderTile(work_queue *WorkOrder);

internal u32
LockedAddAndReturnPreviousValue(volatile u32 *Value, u32 Add)
{
	u32 Result = InterlockedExchangeAdd((volatile LONG *)Value, Add);
	return Result;
}

DWORD WINAPI ThreadDoWork(void *Param)
{
	work_queue *WorkQueue = (work_queue *)Param;

	while(WorkQueue->TilesCompleted < WorkQueue->TileQueueSize)
	{
		RenderTile(WorkQueue);
	}

	return 0;
}

internal void
ThreadStart(work_queue *WorkQueue, u32 ThreadCount)
{
	for(u32 ThreadIndex = 0;
		ThreadIndex < ThreadCount;
		++ThreadIndex)
	{
		HANDLE Thread = CreateThread(0, 0, ThreadDoWork, WorkQueue, 0, 0);
		CloseHandle(Thread);
	}
}
