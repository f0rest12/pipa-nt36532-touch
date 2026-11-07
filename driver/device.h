/*
 * nt36532 — KMDF SPB-клієнт → HID (VHF) для тачскріна NovaTek NT36532 на Xiaomi Pad 6 (pipa).
 * Скелет: вантажиться, біндиться на ACPI\PIPA3653, відкриває SPB (SE4/SPI4), вішає GPIO-переривання,
 * реєструє HID-digitizer через VHF. Логіка протоколу NT36xxx — у TODO (брати з Linux drivers/input/touchscreen/nt36xxx).
 */
#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <SPBCx.h>
#include <vhf.h>

#define NT_POOL_TAG '2563'   // 'nt36' навпаки, для WdfMemory/ExAllocate

// Параметри панелі (з live.dts: xiaomi,pipa-nt36532, 19.2 МГц, IRQ TLMM GPIO39)
#define NT_SPI_FREQ_HZ      19200000
#define NT_MAX_CONTACTS     10

typedef struct _DEVICE_CONTEXT {
    WDFDEVICE            Device;

    // SPB (контролер SPI4/SE4 через qcspi8250 + SpbCx)
    WDFIOTARGET          SpbTarget;
    LARGE_INTEGER        SpbConnectionId;   // з CmResourceTypeConnection у _CRS
    BOOLEAN              SpbOpened;

    // GPIO-переривання тача (GpioInt 39 на \_SB.GIO0)
    WDFINTERRUPT         Interrupt;

    // HID через Virtual HID Framework
    VHFHANDLE            VhfHandle;
    BOOLEAN              VhfStarted;

    // робочий буфер читання пакета тача
    WDFMEMORY            RxMemory;
    PVOID                RxBuffer;
    size_t               RxLength;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

// driver.c
DRIVER_INITIALIZE                   DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD           NtEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE     NtEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE     NtEvtReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY             NtEvtD0Entry;
EVT_WDF_DEVICE_D0_EXIT              NtEvtD0Exit;
EVT_WDF_INTERRUPT_ISR               NtEvtIsr;
EVT_WDF_INTERRUPT_DPC               NtEvtDpc;

// spb.c
NTSTATUS NtSpbOpen(_In_ PDEVICE_CONTEXT ctx);
VOID     NtSpbClose(_In_ PDEVICE_CONTEXT ctx);
NTSTATUS NtSpbWriteRead(_In_ PDEVICE_CONTEXT ctx,
                        _In_reads_(wlen) PUCHAR wbuf, _In_ ULONG wlen,
                        _Out_writes_(rlen) PUCHAR rbuf, _In_ ULONG rlen);

// hid.c
NTSTATUS NtHidStart(_In_ PDEVICE_CONTEXT ctx);
VOID     NtHidStop(_In_ PDEVICE_CONTEXT ctx);
VOID     NtHidReportTouch(_In_ PDEVICE_CONTEXT ctx,
                          _In_ ULONG contactCount,
                          _In_reads_(contactCount) const struct _NT_CONTACT* contacts);

typedef struct _NT_CONTACT {
    UCHAR  id;
    BOOLEAN tip;        // торкання
    USHORT x;
    USHORT y;
} NT_CONTACT;
