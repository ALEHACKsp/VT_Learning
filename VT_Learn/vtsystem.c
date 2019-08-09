#include "vtsystem.h"
#include "vtasm.h"
#include "exithandler.h"

VMX_CPU g_VMXCPU;

static ULONG VmxAdjustControls(ULONG Ctl, ULONG Msr)
{
	LARGE_INTEGER MsrValue;
	MsrValue.QuadPart = Asm_ReadMsr(Msr);
	Ctl &= MsrValue.HighPart;
	Ctl |= MsrValue.LowPart;
	return Ctl;
}

void SetupVMCS()
{
	ULONG GdtBase, IdtBase;
	GdtBase = Asm_GetGdtBase();
	IdtBase = Asm_GetIdtBase();
//1.Guest state fields
//2.host state fields
	Vmx_VmWrite(HOST_CR0, Asm_GetCr0());
	Vmx_VmWrite(HOST_CR3, Asm_GetCr3());
	Vmx_VmWrite(HOST_CR4, Asm_GetCr4());

	Vmx_VmWrite(HOST_ES_SELECTOR, Asm_GetEs() & 0xFFF8);
	Vmx_VmWrite(HOST_CS_SELECTOR, Asm_GetCs() & 0xFFF8);
	Vmx_VmWrite(HOST_DS_SELECTOR, Asm_GetDs() & 0xFFF8);
	Vmx_VmWrite(HOST_FS_SELECTOR, Asm_GetFs() & 0xFFF8);
	Vmx_VmWrite(HOST_GS_SELECTOR, Asm_GetGs() & 0xFFF8);
	Vmx_VmWrite(HOST_SS_SELECTOR, Asm_GetSs() & 0xFFF8);
	Vmx_VmWrite(HOST_TR_SELECTOR, Asm_GetTr() & 0xFFF8);

	Vmx_VmWrite(HOST_TR_BASE, 0x801E3000);


	Vmx_VmWrite(HOST_GDTR_BASE, GdtBase);
	Vmx_VmWrite(HOST_IDTR_BASE, IdtBase);

	Vmx_VmWrite(HOST_IA32_SYSENTER_CS, Asm_ReadMsr(MSR_IA32_SYSENTER_CS) & 0xFFFFFFFF);
	Vmx_VmWrite(HOST_IA32_SYSENTER_ESP, Asm_ReadMsr(MSR_IA32_SYSENTER_ESP) & 0xFFFFFFFF);
	Vmx_VmWrite(HOST_IA32_SYSENTER_EIP, Asm_ReadMsr(MSR_IA32_SYSENTER_EIP) & 0xFFFFFFFF); // KiFastCallEntry

	Vmx_VmWrite(HOST_RSP, ((ULONG)g_VMXCPU.pStack) + 0x1000);     //Host ��ʱջ
	Vmx_VmWrite(HOST_RIP, (ULONG)VMMEntryPoint);                  //���ﶨ�����ǵ�VMM����������
//3.vm-control fields
	//3.1. vm execution contro
	Vmx_VmWrite(PIN_BASED_VM_EXEC_CONTROL, VmxAdjustControls(0, MSR_IA32_VMX_PINBASED_CTLS));
	Vmx_VmWrite(CPU_BASED_VM_EXEC_CONTROL, VmxAdjustControls(0, MSR_IA32_VMX_PROCBASED_CTLS));
	//3.2. vm entry control
	Vmx_VmWrite(VM_ENTRY_CONTROLS, VmxAdjustControls(0, MSR_IA32_VMX_ENTRY_CTLS));
	//3.3. vm exit control
	Vmx_VmWrite(VM_EXIT_CONTROLS, VmxAdjustControls(0, MSR_IA32_VMX_EXIT_CTLS));
	

}

NTSTATUS StartVirtualTechnology()
{
	_CR4 uCr4;
	_EFLAGS uEflags;
	//�жϼ����Ƿ���VT
	if (!IsVTEnabled())
	{
		return STATUS_UNSUCCESSFUL;
	}

	//����CR4��14λΪ1
	*((ULONG*)&uCr4) = Asm_GetCr4();
	uCr4.VMXE = 1;
	Asm_SetCr4(*((ULONG*)&uCr4));

	//����һ��4k�ڴ棬���������ַ��ΪVmx_VmxOn����
	g_VMXCPU.pVMXONRegion = ExAllocatePoolWithTag(NonPagedPool, 0x1000, 'vmx');
	RtlZeroMemory(g_VMXCPU.pVMXONRegion, 0x1000);
	*(ULONG*)g_VMXCPU.pVMXONRegion = 1;
	g_VMXCPU.pVMXONRegion_PA = MmGetPhysicalAddress(g_VMXCPU.pVMXONRegion);

	//����VMX
	Vmx_VmxOn(g_VMXCPU.pVMXONRegion_PA.LowPart, g_VMXCPU.pVMXONRegion_PA.HighPart);
	//�жϿ����Ƿ�ɹ�
	*((ULONG*)&uEflags) = Asm_GetEflags();
	if (uEflags.CF != 0)
	{
		Log("ERROR:VMXONָ�����ʧ�ܣ�", 0);
		ExFreePool(g_VMXCPU.pVMXONRegion);
		return STATUS_UNSUCCESSFUL;
	}
	Log("SUCCESS:VMXONָ����óɹ���", 1);

	//����һ��4k�ڴ棬���������ַ��ΪVmx_clear����
	g_VMXCPU.pVMCSRegion = ExAllocatePoolWithTag(NonPagedPool, 0x1000, 'vmcs');
	RtlZeroMemory(g_VMXCPU.pVMCSRegion, 0x1000);
	*(ULONG*)g_VMXCPU.pVMCSRegion = 1;
	g_VMXCPU.pVMCSRegion_PA = MmGetPhysicalAddress(g_VMXCPU.pVMCSRegion);
	//����ջ�ռ�
	g_VMXCPU.pStack = ExAllocatePoolWithTag(NonPagedPool, 0x1000, 'stck');
	RtlZeroMemory(g_VMXCPU.pStack, 0x1000);

	Vmx_VmClear(g_VMXCPU.pVMCSRegion_PA.LowPart, g_VMXCPU.pVMCSRegion_PA.HighPart);
	Log("SUCCESS:VMCLEARָ����óɹ�!", 0)
	//ѡ�����
	Vmx_VmPtrld(g_VMXCPU.pVMCSRegion_PA.LowPart, g_VMXCPU.pVMCSRegion_PA.HighPart);



	void SetupVMCS();

	Vmx_VmLaunch();
	//=======================================================
	Log("ERROR:Vmx_VmLaunchָ�����ʧ��!", Vmx_VmRead(VM_INSTRUCTION_ERROR));
	return STATUS_SUCCESS;
}

NTSTATUS StopVirtualTechnology()
{
	_CR4 uCr4;
	//�ر�VMX
	Vmx_VmxOff();
	//��Cr4 VMXEλΪ0
	*((ULONG*)&uCr4) = Asm_GetCr4();
	uCr4.VMXE = 0;
	Asm_SetCr4(*((ULONG*)&uCr4));
	//�ͷ��ڴ�
	ExFreePool(g_VMXCPU.pVMXONRegion);
	ExFreePool(g_VMXCPU.pVMCSRegion);
	Log("SUCCESS:VMXOFFָ����óɹ���", 1);
	return STATUS_SUCCESS;
}

static BOOLEAN IsVTEnabled()
{
	ULONG       uRet_EAX, uRet_ECX, uRet_EDX, uRet_EBX;
	_CPUID_ECX  uCPUID;
	_CR0        uCr0;
	_CR4    uCr4;
	IA32_FEATURE_CONTROL_MSR msr;
	//1. CPUID
	Asm_CPUID(1, &uRet_EAX, &uRet_EBX, &uRet_ECX, &uRet_EDX);
	*((PULONG)&uCPUID) = uRet_ECX;

	if (uCPUID.VMX != 1)
	{
		Log("ERROR: ���CPU��֧��VT!", 0);
		return FALSE;
	}

	// 2. MSR
	*((PULONG)&msr) = (ULONG)Asm_ReadMsr(MSR_IA32_FEATURE_CONTROL);
	if (msr.Lock != 1)
	{
		Log("ERROR:VTָ��δ������!", 0);
		return FALSE;
	}

	// 3. CR0 CR4
	*((PULONG)&uCr0) = Asm_GetCr0();
	*((PULONG)&uCr4) = Asm_GetCr4();

	if (uCr0.PE != 1 || uCr0.PG != 1 || uCr0.NE != 1)
	{
		Log("ERROR:���CPUû�п���VT!", 0);
		return FALSE;
	}

	if (uCr4.VMXE == 1)
	{
		Log("ERROR:���CPU�Ѿ�������VT!", 0);
		Log("�����Ǳ�������Ѿ�ռ����VT�������ر�������ܿ�����", 0);
		return FALSE;
	}


	Log("SUCCESS:���CPU֧��VT!", 0);
	return TRUE;
}