#include "device.h"
#include "hidreport.h"

// VHF: читаємо дескриптор у Virtual HID Framework, далі шлемо HID-звіти знизу вгору.

NTSTATUS
NtHidStart(_In_ PDEVICE_CONTEXT ctx)
{
    if (ctx->VhfStarted) return STATUS_SUCCESS;

    VHF_CONFIG cfg;
    VHF_CONFIG_INIT(&cfg, WdfDeviceWdmGetDeviceObject(ctx->Device),
                    sizeof(g_NtReportDescriptor), (PUCHAR)g_NtReportDescriptor);
    cfg.VhfClientContext = ctx;

    NTSTATUS status = VhfCreate(&cfg, &ctx->VhfHandle);
    if (!NT_SUCCESS(status)) return status;

    status = VhfStart(ctx->VhfHandle);
    if (!NT_SUCCESS(status)) {
        VhfDelete(ctx->VhfHandle, TRUE);
        ctx->VhfHandle = NULL;
        return status;
    }
    ctx->VhfStarted = TRUE;
    return STATUS_SUCCESS;
}

VOID
NtHidStop(_In_ PDEVICE_CONTEXT ctx)
{
    if (ctx->VhfHandle) {
        VhfDelete(ctx->VhfHandle, TRUE);
        ctx->VhfHandle = NULL;
        ctx->VhfStarted = FALSE;
    }
}

// Поки шлемо лише перший контакт (дескриптор на 1 палець). Розширимо під 10 пальців.
VOID
NtHidReportTouch(_In_ PDEVICE_CONTEXT ctx,
                 _In_ ULONG contactCount,
                 _In_reads_(contactCount) const NT_CONTACT* contacts)
{
    if (!ctx->VhfStarted || contactCount == 0) return;

    NT_HID_REPORT rep;
    RtlZeroMemory(&rep, sizeof(rep));
    rep.ReportId     = 1;
    rep.TipSwitch    = contacts[0].tip ? 1 : 0;
    rep.ContactId    = contacts[0].id;
    rep.X            = contacts[0].x;
    rep.Y            = contacts[0].y;
    rep.ContactCount = (UCHAR)contactCount;

    HID_XFER_PACKET pkt;
    RtlZeroMemory(&pkt, sizeof(pkt));
    pkt.reportId        = rep.ReportId;
    pkt.reportBuffer    = (PUCHAR)&rep;
    pkt.reportBufferLen = sizeof(rep);

    (void)VhfReadReportSubmit(ctx->VhfHandle, &pkt);
}
