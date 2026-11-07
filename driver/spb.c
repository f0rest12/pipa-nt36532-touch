#include "device.h"

// Відкриваємо SpbCx-таргет контролера SPI4 за ConnectionId з _CRS.
// Шлях ресурс-хаба: \\DosDevices\\RESOURCE_HUB\\<id як 16 hex>.
NTSTATUS
NtSpbOpen(_In_ PDEVICE_CONTEXT ctx)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attrs;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    DECLARE_UNICODE_STRING_SIZE(path, 128);

    status = RtlUnicodeStringPrintf(&path,
        L"\\DosDevices\\RESOURCE_HUB\\%0*I64x",
        (int)(sizeof(LARGE_INTEGER) * 2), ctx->SpbConnectionId.QuadPart);
    if (!NT_SUCCESS(status)) return status;

    WDF_OBJECT_ATTRIBUTES_INIT(&attrs);
    attrs.ParentObject = ctx->Device;
    status = WdfIoTargetCreate(ctx->Device, &attrs, &ctx->SpbTarget);
    if (!NT_SUCCESS(status)) return status;

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &path,
        GENERIC_READ | GENERIC_WRITE);
    openParams.ShareAccess = 0;

    status = WdfIoTargetOpen(ctx->SpbTarget, &openParams);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(ctx->SpbTarget);
        ctx->SpbTarget = NULL;
        return status;   // якщо контролер (qcspi8250) не стартував без PEP — побачимо саме тут
    }
    ctx->SpbOpened = TRUE;
    return STATUS_SUCCESS;
}

VOID
NtSpbClose(_In_ PDEVICE_CONTEXT ctx)
{
    if (ctx->SpbTarget) {
        WdfIoTargetClose(ctx->SpbTarget);
        WdfObjectDelete(ctx->SpbTarget);
        ctx->SpbTarget = NULL;
        ctx->SpbOpened = FALSE;
    }
}

// Повний дуплекс: пишемо wlen, читаємо rlen. Для NT36xxx зазвичай: адреса → читання.
NTSTATUS
NtSpbWriteRead(_In_ PDEVICE_CONTEXT ctx,
               _In_reads_(wlen) PUCHAR wbuf, _In_ ULONG wlen,
               _Out_writes_(rlen) PUCHAR rbuf, _In_ ULONG rlen)
{
    if (!ctx->SpbOpened) return STATUS_DEVICE_NOT_READY;

    NTSTATUS status;
    WDFMEMORY wmem = NULL, rmem = NULL;
    WDF_OBJECT_ATTRIBUTES a;

    WDF_OBJECT_ATTRIBUTES_INIT(&a); a.ParentObject = ctx->Device;
    status = WdfMemoryCreatePreallocated(&a, wbuf, wlen, &wmem);
    if (!NT_SUCCESS(status)) return status;
    WDF_OBJECT_ATTRIBUTES_INIT(&a); a.ParentObject = ctx->Device;
    status = WdfMemoryCreatePreallocated(&a, rbuf, rlen, &rmem);
    if (!NT_SUCCESS(status)) { WdfObjectDelete(wmem); return status; }

    // SPB sequence: Write(wbuf) потім Read(rbuf) однією транзакцією.
    SPB_TRANSFER_LIST_AND_ENTRIES(2) seq;
    SPB_TRANSFER_LIST_INIT(&(seq.List), 2);
    seq.List.Transfers[0] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice,   0, wbuf, wlen);
    seq.List.Transfers[1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice, 0, rbuf, rlen);

    WDF_MEMORY_DESCRIPTOR inDesc;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inDesc, &seq, sizeof(seq));

    status = WdfIoTargetSendIoctlSynchronously(ctx->SpbTarget, NULL,
        IOCTL_SPB_EXECUTE_SEQUENCE, &inDesc, NULL, NULL, NULL);

    WdfObjectDelete(wmem);
    WdfObjectDelete(rmem);
    return status;
}
