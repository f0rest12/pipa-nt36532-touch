#include "device.h"

// ---- Driver / Device add -------------------------------------------------

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, NtEvtDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}

NTSTATUS
NtEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    UNREFERENCED_PARAMETER(Driver);
    NTSTATUS status;
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES attrs;
    WDFDEVICE device;
    PDEVICE_CONTEXT ctx;

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = NtEvtPrepareHardware;
    pnp.EvtDeviceReleaseHardware = NtEvtReleaseHardware;
    pnp.EvtDeviceD0Entry         = NtEvtD0Entry;
    pnp.EvtDeviceD0Exit          = NtEvtD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnp);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs, DEVICE_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &attrs, &device);
    if (!NT_SUCCESS(status)) return status;

    ctx = GetDeviceContext(device);
    RtlZeroMemory(ctx, sizeof(*ctx));
    ctx->Device = device;

    // Переривання тача (GpioInt з _CRS). Рівень/ISR/DPC.
    WDF_INTERRUPT_CONFIG irq;
    WDF_INTERRUPT_CONFIG_INIT(&irq, NtEvtIsr, NtEvtDpc);
    status = WdfInterruptCreate(device, &irq, WDF_NO_OBJECT_ATTRIBUTES, &ctx->Interrupt);
    if (!NT_SUCCESS(status)) return status;

    // буфер читання
    WDF_OBJECT_ATTRIBUTES_INIT(&attrs);
    attrs.ParentObject = device;
    status = WdfMemoryCreate(&attrs, NonPagedPoolNx, NT_POOL_TAG, 256, &ctx->RxMemory, &ctx->RxBuffer);
    if (!NT_SUCCESS(status)) return status;
    ctx->RxLength = 256;

    return STATUS_SUCCESS;
}

// ---- PnP / Power ---------------------------------------------------------

NTSTATUS
NtEvtPrepareHardware(_In_ WDFDEVICE Device,
                     _In_ WDFCMRESLIST ResourcesRaw,
                     _In_ WDFCMRESLIST ResourcesTranslated)
{
    UNREFERENCED_PARAMETER(ResourcesRaw);
    PDEVICE_CONTEXT ctx = GetDeviceContext(Device);
    ULONG i, n = WdfCmResourceListGetCount(ResourcesTranslated);

    // Знайти Connection-ресурс SPB (SERIAL/SPI) у _CRS → ConnectionId для відкриття SpbCx-таргета.
    for (i = 0; i < n; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (d->Type == CmResourceTypeConnection &&
            d->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL &&
            d->u.Connection.Type  == CM_RESOURCE_CONNECTION_TYPE_SERIAL_SPI) {
            ctx->SpbConnectionId.LowPart  = d->u.Connection.IdLowPart;
            ctx->SpbConnectionId.HighPart = d->u.Connection.IdHighPart;
            break;
        }
    }
    if (ctx->SpbConnectionId.QuadPart == 0)
        return STATUS_DEVICE_CONFIGURATION_ERROR;   // немає SPB-ресурсу в _CRS

    return NtSpbOpen(ctx);
}

NTSTATUS
NtEvtReleaseHardware(_In_ WDFDEVICE Device, _In_ WDFCMRESLIST ResourcesTranslated)
{
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    NtSpbClose(GetDeviceContext(Device));
    return STATUS_SUCCESS;
}

NTSTATUS
NtEvtD0Entry(_In_ WDFDEVICE Device, _In_ WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(PreviousState);
    PDEVICE_CONTEXT ctx = GetDeviceContext(Device);

    // TODO: NT36532 bring-up — reset через SPI-команду (swrst-n8-addr 0x3f0fe),
    // завантажити прошивку nt36532_tianma.bin / nt36532_csot.bin у чип (хост сам заливає),
    // прочитати chip-id для перевірки. Поки лише піднімаємо HID.
    return NtHidStart(ctx);
}

NTSTATUS
NtEvtD0Exit(_In_ WDFDEVICE Device, _In_ WDF_POWER_DEVICE_STATE TargetState)
{
    UNREFERENCED_PARAMETER(TargetState);
    NtHidStop(GetDeviceContext(Device));
    return STATUS_SUCCESS;
}

// ---- Interrupt -----------------------------------------------------------

BOOLEAN
NtEvtIsr(_In_ WDFINTERRUPT Interrupt, _In_ ULONG MessageID)
{
    UNREFERENCED_PARAMETER(MessageID);
    // GPIO-переривання edge/oneshot — читати не можна на DIRQL, відкладаємо у DPC.
    WdfInterruptQueueDpcForIsr(Interrupt);
    return TRUE;
}

VOID
NtEvtDpc(_In_ WDFINTERRUPT Interrupt, _In_ WDFOBJECT AssociatedObject)
{
    UNREFERENCED_PARAMETER(AssociatedObject);
    PDEVICE_CONTEXT ctx = GetDeviceContext(WdfInterruptGetDevice(Interrupt));

    // TODO: прочитати пакет точок по SPB і розпарсити формат NT36532.
    // Нижче — кістяк: читаємо, парсимо у NT_CONTACT[], віддаємо у HID.
    UCHAR cmd = 0x00;  // TODO: реальна адреса/команда читання event-буфера NT36xxx
    NTSTATUS s = NtSpbWriteRead(ctx, &cmd, 1, (PUCHAR)ctx->RxBuffer, (ULONG)ctx->RxLength);
    if (!NT_SUCCESS(s)) return;

    NT_CONTACT contacts[NT_MAX_CONTACTS] = {0};
    ULONG count = 0;
    // TODO: розпарсити ctx->RxBuffer → contacts/count за протоколом nt36xxx.
    // (поки count=0 — HID-звітів нема, але шлях робочий)

    if (count) NtHidReportTouch(ctx, count, contacts);
}
