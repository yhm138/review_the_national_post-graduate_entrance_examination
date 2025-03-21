/*-----------------------------------------------------------------------
第12章  注入技术
《加密与解密（第四版）》
(c)  看雪学院 www.kanxue.com 2000-2018
-----------------------------------------------------------------------*/

#include "Common.h"
#include "ntifs.h"
#include "dbghelp.h"
#include <ntimage.h>

/********************************************************************
	created:	2008/08/08
	lastupd:	2016/03/08
	filename: 	Common.h
	author:		achillis
	purpose:	作为ntifs.h的补充
*********************************************************************/

//用于映射文件操作
PFN_ZwQueryVirtualMemory pfnZwQueryVirtualMemory;
BYTE *g_ZwFuncStartOffset = 0 ;
ULONG g_ZwFunSize = 0 ;

ULONG HookSSDTServiceByIndex(ULONG ServiceIndex,ULONG_PTR FakeFunAddr )
{
	ULONG OriginalFun=0;
	OriginalFun=(ULONG)(KeServiceDescriptorTable->ServiceTable[ServiceIndex]);
	WPOFF();
	KeServiceDescriptorTable->ServiceTable[ServiceIndex]=(PVOID)FakeFunAddr;
	WPON();
	return OriginalFun;
}

VOID UnhookSSDTServiceByIndex(ULONG ServiceIndex,ULONG_PTR OriginalFunAddr )
{
	WPOFF();
	KeServiceDescriptorTable->ServiceTable[ServiceIndex]=(PVOID)OriginalFunAddr;
	WPON();
}

DWORD GetServiceIndexByName(ULONG_PTR hModule,char *szServiceName)
{
	DWORD ServiceIndex = 0 ;
	BYTE *FnAddr = (BYTE*)KeGetProcAddress(hModule,szServiceName);
	if (FnAddr == NULL || FnAddr[0] != 0xB8)
	{
		dprintf("[GetServiceIndexByName] Get Function Address of %s Failed!\n",szServiceName);
		return 0;
	}
	ServiceIndex= *(ULONG*)(FnAddr+1);
	return ServiceIndex;
}

ULONG_PTR KeGetProcAddress(ULONG_PTR hModule,char *FuncName)
{
	//自己实现GetProcAddress
	ULONG_PTR retAddr=0;
	DWORD *namerav,*funrav;
	DWORD cnt=0;
	DWORD max,min,mid;
	WORD *nameOrdinal;
	WORD nIndex=0;
	int cmpresult=0;
	char *ModuleBase=(char*)hModule;
	char *szMidName = NULL ;
	PIMAGE_DOS_HEADER pDosHeader;
	PIMAGE_FILE_HEADER pFileHeader;
	PIMAGE_OPTIONAL_HEADER pOptHeader;
	PIMAGE_EXPORT_DIRECTORY pExportDir;
	pDosHeader=(PIMAGE_DOS_HEADER)ModuleBase;
	pFileHeader=(PIMAGE_FILE_HEADER)(ModuleBase+pDosHeader->e_lfanew+4);
	pOptHeader=(PIMAGE_OPTIONAL_HEADER)((char*)pFileHeader+sizeof(IMAGE_FILE_HEADER));
	pExportDir=(PIMAGE_EXPORT_DIRECTORY)(ModuleBase+pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	namerav=(DWORD*)(ModuleBase+pExportDir->AddressOfNames);
	funrav=(DWORD*)(ModuleBase+pExportDir->AddressOfFunctions);
	nameOrdinal=(WORD*)(ModuleBase+pExportDir->AddressOfNameOrdinals);
	if ((DWORD)FuncName<0x0000FFFF)
	{
		retAddr=(ULONG_PTR)(ModuleBase+funrav[(WORD)FuncName]);
	}
	else
	{
		//二分法查找
		max = pExportDir->NumberOfNames ;
		min = 0;
		mid=(max+min)/2;
		while (min < max)
		{
			szMidName = ModuleBase+namerav[mid] ;
			cmpresult=strcmp(FuncName,szMidName);
			if (cmpresult < 0)
			{
				//比中值小,则取中值-1为最大值
				max = mid -1 ;
			}
			else if (cmpresult > 0)
			{
				//比中值大,则取中值+1为最小值
				min = mid + 1;
			}
			else
			{
				break;
			}
			mid=(max+min)/2;
			
		}
		
		if (strcmp(FuncName,ModuleBase+namerav[mid]) == 0)
		{
			nIndex=nameOrdinal[mid];
			retAddr=(DWORD)(ModuleBase+funrav[nIndex]);
		}
	}
	return retAddr;
}

NTSTATUS
MapImageFile(
	IN char *szFilePath ,
	OUT MAP_IMAGE_INFO *pImageInfo
	)
{
	PVOID MappedBase=0;
	OBJECT_ATTRIBUTES oba;
	IO_STATUS_BLOCK ioblk;
	char objFilePath[260];
	ANSI_STRING ansiStr;
	UNICODE_STRING uFilePath;
	NTSTATUS status;
	LARGE_INTEGER Offset;
	SIZE_T viewsize=0;

	if (szFilePath == NULL
		|| pImageInfo == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	RtlInitAnsiString(&ansiStr,szFilePath);
	RtlAnsiStringToUnicodeString(&uFilePath,&ansiStr,TRUE);
	InitializeObjectAttributes(&oba ,
        &uFilePath,
        OBJ_CASE_INSENSITIVE |OBJ_KERNEL_HANDLE,
        NULL ,
        NULL
        );

	//Open File
	status=ZwCreateFile(&pImageInfo->hFile,
		SYNCHRONIZE|GENERIC_READ,
		&oba,
		&ioblk,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ,
		FILE_OPEN_IF,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0
		);
	if (!NT_SUCCESS(status))
	{
		dprintf("ZwCreateFile Failed! status=0x%08X\n",status);
		goto __failed;
	}

	InitializeObjectAttributes(&oba,
        NULL,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
        );
	status=ZwCreateSection(&pImageInfo->hMap,
		SECTION_MAP_READ,
		0,
		0,
		PAGE_READONLY,
		SEC_IMAGE,
		pImageInfo->hFile
		);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("ZwCreateSection Failed! status=0x%08X\n",status);
		goto __failed;
	}

	Offset.QuadPart=0;
	status=ZwMapViewOfSection(pImageInfo->hMap,
		NtCurrentProcess(),
		(PVOID*)&MappedBase,
		0,
		0x1000,
		NULL,
		&viewsize,
		ViewShare,
		MEM_TOP_DOWN,
		PAGE_READONLY);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("ZwMapViewOfSection Failed! status=0x%08X\n",status);
		goto __failed;
	}
	
	
	ZwClose(pImageInfo->hFile);
	pImageInfo->hFile = INVALID_HANDLE_VALUE;
	pImageInfo->MappedBase = MappedBase;
	
	
__failed:
	RtlFreeUnicodeString(&uFilePath);

	if (pImageInfo->hFile != INVALID_HANDLE_VALUE)
	{
		ZwClose(pImageInfo->hFile);
		pImageInfo->hFile = INVALID_HANDLE_VALUE;
	}
	if (pImageInfo->hMap != NULL)
	{
		ZwClose(pImageInfo->hMap);
		pImageInfo->hMap = NULL ;
	}
		
	return status;
}

VOID UnMapImageFile(PMAP_IMAGE_INFO pImageInfo)
{
	NTSTATUS status;
	if (pImageInfo != NULL)
	{
		status = ZwUnmapViewOfSection(NtCurrentProcess(),pImageInfo->MappedBase);

		if (pImageInfo->hFile != INVALID_HANDLE_VALUE)
		{
			ZwClose(pImageInfo->hFile);
			pImageInfo->hFile = INVALID_HANDLE_VALUE;
		}

		if (pImageInfo->hMap != NULL)
		{
			ZwClose(pImageInfo->hMap);
			pImageInfo->hMap = NULL ;
		}
	}
	
	
}


/*
0: kd> dt _PEB
ntdll!_PEB
+0x000 InheritedAddressSpace : UChar
+0x001 ReadImageFileExecOptions : UChar
+0x002 BeingDebugged    : UChar
+0x003 SpareBool        : UChar
+0x004 Mutant           : Ptr32 Void
+0x008 ImageBaseAddress : Ptr32 Void
+0x00c Ldr              : Ptr32 _PEB_LDR_DATA

  0: kd> dt _PEB_LDR_DATA
  ntdll!_PEB_LDR_DATA
  +0x000 Length           : Uint4B
  +0x004 Initialized      : UChar
  +0x008 SsHandle         : Ptr32 Void
  +0x00c InLoadOrderModuleList : _LIST_ENTRY
  +0x014 InMemoryOrderModuleList : _LIST_ENTRY
  +0x01c InInitializationOrderModuleList : _LIST_ENTRY
  +0x024 EntryInProgress  : Ptr32 Void

  0:000> dt _LDR_DATA_TABLE_ENTRY 0x241f48 
  ntdll!_LDR_DATA_TABLE_ENTRY
  +0x000 InLoadOrderLinks : _LIST_ENTRY [ 0x242010 - 0x241ee0 ]
  +0x008 InMemoryOrderLinks : _LIST_ENTRY [ 0x242018 - 0x241ee8 ]
  +0x010 InInitializationOrderLinks : _LIST_ENTRY [ 0x242020 - 0x241ebc ]
  +0x018 DllBase          : 0x7c920000 
  +0x01c EntryPoint       : 0x7c9320f8 
  +0x020 SizeOfImage      : 0x96000
  +0x024 FullDllName      : _UNICODE_STRING "C:\windows\system32\ntdll.dll"
  +0x02c BaseDllName      : _UNICODE_STRING "ntdll.dll"
  +0x034 Flags            : 0x85004
  +0x038 LoadCount        : 0xffff
  +0x03a TlsIndex         : 0
  +0x03c HashLinks        : _LIST_ENTRY [ 0x7c99e2e8 - 0x7c99e2e8 ]
  +0x03c SectionPointer   : 0x7c99e2e8 
  +0x040 CheckSum         : 0x7c99e2e8
  +0x044 TimeDateStamp    : 0x4d00f280
  +0x044 LoadedImports    : 0x4d00f280 
  +0x048 EntryPointActivationContext : (null) 
   +0x04c PatchInformation : (null) 
*/

//第一个线程尚未创建时，Ldr还没有初始化,所以在这种情况下不能使用以下代码
ULONG_PTR KeGetUserModuleHandle(PEPROCESS Process, WCHAR *wModuleName)
{
	PPEB Peb;
	PPEB_LDR_DATA pLdrData;
	PLDR_DATA_TABLE_ENTRY pLdrDataEntry;
	PLIST_ENTRY pListHead,pListNext;
	ULONG_PTR uModuleBase = 0x7C920000 ;
	UNICODE_STRING usModuleName ;
	
	//测试，直接返回硬编码
	return uModuleBase;

	RtlInitUnicodeString(&usModuleName,wModuleName);
	Peb = PsGetProcessPeb(Process);
	dprintf("PEB = 0x%08X\n",Peb);
	__try
	{
		pLdrData=(PPEB_LDR_DATA)*(ULONG*)((BYTE*)Peb+0xC); //Ldr
		pListHead=&(pLdrData->InLoadOrderModuleList);
		pListNext=pListHead->Flink;
		for (pListHead;pListNext!=pListHead;pListNext=pListNext->Flink)
		{
			pLdrDataEntry=(PLDR_DATA_TABLE_ENTRY)pListNext;
			if (RtlEqualUnicodeString(&usModuleName,&pLdrDataEntry->BaseDllName,TRUE))
			{
				uModuleBase = (ULONG)pLdrDataEntry->DllBase ;
			}
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		DbgPrint("Error Occured while Gettting Module Handle.\n");
	}

	return uModuleBase;
}

VOID InitZwFunBaseInfo()
{
	BYTE *pClose = NULL, *pOpenProcess = NULL;
	LONG IndexOfClose = 0 ;
	LONG IndexOfOpenProcess = 0 ;
	LONG FunSize = 0 ;
	pClose = (BYTE*)ZwClose;
	pOpenProcess = (BYTE*)ZwOpenProcess;

	IndexOfClose = *(LONG*)(pClose + 1) ;
	IndexOfOpenProcess = *(LONG*)(pOpenProcess + 1) ;

	//计算差值
	FunSize = ((LONG)(pOpenProcess - pClose)) / (IndexOfOpenProcess - IndexOfClose) ;
	if (FunSize < 0)
	{
		FunSize = - FunSize;
	}

	g_ZwFunSize = FunSize;
	g_ZwFuncStartOffset = pClose - IndexOfClose * g_ZwFunSize ;

}

ULONG_PTR GetZwServiceAddr(DWORD ServiceIndex)
{
	ULONG_PTR FunAddr = 0;
	
	if (g_ZwFunSize ==0
		|| g_ZwFuncStartOffset == NULL)
	{
		InitZwFunBaseInfo();
	}

	FunAddr = (ULONG_PTR)g_ZwFuncStartOffset + ServiceIndex * g_ZwFunSize;
	return FunAddr;
}

ULONG_PTR GetZwQueryVirtualMemoryAddr()
{
	NTSTATUS status;
	MAP_IMAGE_INFO NtdllInfo = {0};
	ULONG ServiceIndex=0;
	ULONG_PTR FnAddr=0;
	ULONG_PTR MapedNtdllBase=0;
	ULONG IndexOfNtQueryVirtualMemory = 0 ;
	
	char szNtdllPath[MAX_PATH] = "\\SystemRoot\\system32\\ntdll.dll";
	status = MapImageFile(szNtdllPath,&NtdllInfo);
	
	if (!NT_SUCCESS(status))
	{
		dprintf("[GetServiceIndex] Map Ntdll.dll Failed!\n");
		return 0;
	}
	
	MapedNtdllBase = (ULONG_PTR)NtdllInfo.MappedBase;
	IndexOfNtQueryVirtualMemory = GetServiceIndexByName(MapedNtdllBase,"NtQueryVirtualMemory");
	FnAddr = GetZwServiceAddr(IndexOfNtQueryVirtualMemory);
	UnMapImageFile(&NtdllInfo);
	return FnAddr;
}

ULONG_PTR GetZwProtectVirtualMemoryAddr()
{
	NTSTATUS status;
	MAP_IMAGE_INFO NtdllInfo = {0};
	ULONG ServiceIndex=0;
	ULONG_PTR FnAddr=0;
	ULONG_PTR MapedNtdllBase=0;
	ULONG IndexOfNtQueryVirtualMemory = 0 ;
	
	char szNtdllPath[MAX_PATH] = "\\SystemRoot\\system32\\ntdll.dll";
	status = MapImageFile(szNtdllPath,&NtdllInfo);
	
	if (!NT_SUCCESS(status))
	{
		dprintf("[GetServiceIndex] Map Ntdll.dll Failed!\n");
		return 0;
	}
	
	MapedNtdllBase = (ULONG_PTR)NtdllInfo.MappedBase;
	IndexOfNtQueryVirtualMemory = GetServiceIndexByName(MapedNtdllBase,"NtProtectVirtualMemory");
	FnAddr = GetZwServiceAddr(IndexOfNtQueryVirtualMemory);
	UnMapImageFile(&NtdllInfo);
	return FnAddr;
}

ULONG_PTR FindImageBase(HANDLE hProc , WCHAR *wFileName)
{
	NTSTATUS status ;
	ULONG_PTR uResult = 0 ;
	TCHAR szBuf[1024]={0};
	DWORD dwSize = 0 ;
	PBYTE pAddress = (PBYTE)0 ;
	SYSTEM_BASIC_INFORMATION SysBasicInfo = {0};
	MEMORY_BASIC_INFORMATION mbi = {0};
	WCHAR szImageFilePath[MAX_PATH]={0};
	ULONG ReturnLength = 0 ;
	BOOL bFoundImage = FALSE ;
	PWCHAR pFileName = NULL ;
	
	//获取函数地址
	if (pfnZwQueryVirtualMemory == NULL)
	{
		pfnZwQueryVirtualMemory = (PFN_ZwQueryVirtualMemory)GetZwQueryVirtualMemoryAddr();
	}
	//获取页的大小
	status = ZwQuerySystemInformation(SystemBasicInformation,&SysBasicInfo,sizeof(SYSTEM_BASIC_INFORMATION),&ReturnLength);
	if (!NT_SUCCESS(status))
	{
		return 0 ;
	}
	
	//Found First MEM_IMAGE Page
	//printf("   Address  :   Size      State     Type     Image\n");
	while (pAddress < (PBYTE)SysBasicInfo.HighestUserAddress)
	{
		ZeroMemory(&mbi,sizeof(MEMORY_BASIC_INFORMATION));
		pfnZwQueryVirtualMemory(hProc, pAddress, MemoryBasicInformation, &mbi, sizeof(MEMORY_BASIC_INFORMATION), &ReturnLength);
		if (ReturnLength == 0)
		{
			pAddress += SysBasicInfo.PhysicalPageSize ;
			continue;
		}
		
		switch(mbi.State)
		{
		case MEM_FREE:
		case MEM_RESERVE:
			pAddress = (PBYTE)mbi.BaseAddress + mbi.RegionSize;
			break;
		case MEM_COMMIT:
			if (mbi.Type == MEM_IMAGE)
			{
				KeGetMappedFileName(hProc,pAddress,szImageFilePath,MAX_PATH , &ReturnLength) ;
				if (ReturnLength != 0)
				{
					pFileName = wcsrchr(szImageFilePath,'\\');
					pFileName = (pFileName == NULL) ? szImageFilePath : (pFileName + 1);
					//printf("Image = %ws\n",pFileName);
					if (_wcsicmp(pFileName,wFileName) == 0)
					{
						uResult = (ULONG_PTR)pAddress;
						bFoundImage = TRUE;
					}
				}
			}
			pAddress = (PBYTE)mbi.BaseAddress + mbi.RegionSize;
			break;
		default:
			break;
		}
		
		if (bFoundImage)
		{
			break;
		}
	}
	
	return uResult;
}

NTSTATUS
KeGetMappedFileName(
	HANDLE hProcess, 
	LPVOID lpv, 
	LPWSTR lpFilename, 
	DWORD nSize , 
	DWORD *ResultLength
	)
{
	NTSTATUS status; 
	DWORD result = 0; 
	USHORT MaxLength; 
	DWORD SizeToCopy; 
	ULONG ReturnLength; 
	char Buffer[528]={0}; // 528 = 260*2 + 8
	PUNICODE_STRING pusFilePath;
	
	//获取函数地址
	if (pfnZwQueryVirtualMemory == NULL)
	{
		pfnZwQueryVirtualMemory = (PFN_ZwQueryVirtualMemory)GetZwQueryVirtualMemoryAddr();
	}

	pusFilePath = (PUNICODE_STRING)Buffer;
	status = pfnZwQueryVirtualMemory(hProcess, lpv, MemorySectionName, pusFilePath, 528, &ReturnLength);
	if (NT_SUCCESS(status))
	{
		MaxLength = pusFilePath->MaximumLength;
		SizeToCopy = pusFilePath->MaximumLength;
		if ( sizeof(WCHAR) * nSize < pusFilePath->MaximumLength )
			SizeToCopy = sizeof(WCHAR) * nSize;
		memcpy(lpFilename, pusFilePath->Buffer, SizeToCopy);
		if ( SizeToCopy == MaxLength )
			SizeToCopy -= sizeof(WCHAR);
		result = SizeToCopy / sizeof(WCHAR) ;
	}
	
	if (ResultLength != NULL)
	{
		*ResultLength = result;
	}
	return status;
}

PMDL MapUserModeAddrWritable(PVOID BaseAddr,ULONG Length, PVOID *pNewAddr)
{
	PVOID pMapedAddr = NULL ;
	//创建一个MDL
	PMDL pMdl = IoAllocateMdl(BaseAddr,Length,FALSE,FALSE,NULL);
	if (pMdl == NULL)
	{
		dprintf("pMDL == NULL\n");
		return NULL;
	}
	
	__try
	{
		MmProbeAndLockPages(pMdl,UserMode,IoReadAccess);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		dprintf("MmProbeAndLockPages exception. Code  = 0x%08X\n",GetExceptionCode());
		IoFreeMdl(pMdl);
		return NULL;
	}
	
	
	pMapedAddr = MmMapLockedPagesSpecifyCache(pMdl,KernelMode,MmCached,NULL,FALSE,NormalPagePriority);
	if (!pMapedAddr)
	{
		dprintf("pMapedAdd == NULL\n");
		MmUnlockPages(pMdl);
		IoFreeMdl(pMdl);
		return NULL;
	}
	
	*pNewAddr = pMapedAddr ;
	return pMdl;
	
}

VOID UnmapMemory(PVOID pAddr, PMDL pMdl)
{
	if (pAddr != NULL)
	{
		MmUnmapLockedPages(pAddr,pMdl);
	}
	
	if (pMdl != NULL)
	{
		MmUnlockPages(pMdl);
		IoFreeMdl(pMdl);
	}
	
}
