#include <ntifs.h>
#include "vtasm.h"
#include "vtsystem.h"

void DriverUnLoad(PDRIVER_OBJECT pDriverObj)
{
	StopVirtualTechnology();
	KdPrint(("����ж�سɹ���"));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObj, PUNICODE_STRING usRegPath)
{
	NTSTATUS Status = STATUS_SUCCESS;
	pDriverObj->DriverUnload = DriverUnLoad;
	KdPrint(("������װ�ɹ���"));
	Status = StartVirtualTechnology();
	return Status;
}