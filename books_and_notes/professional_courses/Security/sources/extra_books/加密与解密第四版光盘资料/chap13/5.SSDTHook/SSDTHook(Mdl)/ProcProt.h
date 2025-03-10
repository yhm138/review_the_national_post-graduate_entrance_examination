/*

  ProcProt.H

  Author: <your name>
  Last Updated: 2006-02-12

  This framework is generated by EasySYS 0.3.0
  This template file is copying from QuickSYS 0.3.0 written by Chunhua Liu

*/

#ifndef _PROCPROT_H
#define _PROCPROT_H 1

//
// Define the various device type values.  Note that values used by Microsoft
// Corporation are in the range 0-0x7FFF(32767), and 0x8000(32768)-0xFFFF(65535)
// are reserved for use by customers.
//

#define FILE_DEVICE_PROCPROT	0x8000

//
// Macro definition for defining IOCTL and FSCTL function control codes. Note
// that function codes 0-0x7FF(2047) are reserved for Microsoft Corporation,
// and 0x800(2048)-0xFFF(4095) are reserved for customers.
//

#define PROCPROT_IOCTL_BASE	0x800

//
// The device driver IOCTLs
//

#define CTL_CODE_PROCPROT(i) CTL_CODE(FILE_DEVICE_PROCPROT, PROCPROT_IOCTL_BASE+i, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PROCPROT_HELLO	CTL_CODE_PROCPROT(0)
#define IOCTL_PROCPROT_TEST	CTL_CODE_PROCPROT(1)

//
// Name that Win32 front end will use to open the ProcProt device
//

#define PROCPROT_WIN32_DEVICE_NAME_A	"\\\\.\\ProcProt"
#define PROCPROT_WIN32_DEVICE_NAME_W	L"\\\\.\\ProcProt"
#define PROCPROT_DEVICE_NAME_A			"\\Device\\ProcProt"
#define PROCPROT_DEVICE_NAME_W			L"\\Device\\ProcProt"
#define PROCPROT_DOS_DEVICE_NAME_A		"\\DosDevices\\ProcProt"
#define PROCPROT_DOS_DEVICE_NAME_W		L"\\DosDevices\\ProcProt"

#ifdef _UNICODE
#define PROCPROT_WIN32_DEVICE_NAME PROCPROT_WIN32_DEVICE_NAME_W
#define PROCPROT_DEVICE_NAME		PROCPROT_DEVICE_NAME_W
#define PROCPROT_DOS_DEVICE_NAME	PROCPROT_DOS_DEVICE_NAME_W
#else
#define PROCPROT_WIN32_DEVICE_NAME PROCPROT_WIN32_DEVICE_NAME_A
#define PROCPROT_DEVICE_NAME		PROCPROT_DEVICE_NAME_A
#define PROCPROT_DOS_DEVICE_NAME	PROCPROT_DOS_DEVICE_NAME_A
#endif

#endif