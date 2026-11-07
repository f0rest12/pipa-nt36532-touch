#pragma once
#include <ntddk.h>

/*
 * Мінімальний HID report descriptor: тачскрін-дигітайзер, 1 контакт для старту
 * (TipSwitch, ContactId, X, Y, ContactCount). Розширимо до 10 пальців + перо пізніше.
 * ReportId = 1. Логічний діапазон X/Y 0..32767 (Windows масштабує під екран 2880×1800).
 */

static const UCHAR g_NtReportDescriptor[] = {
    0x05, 0x0D,        // Usage Page (Digitizer)
    0x09, 0x04,        // Usage (Touch Screen)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x95, 0x07,        //     Report Count (7) — padding
    0x81, 0x03,        //     Input (Const,Var,Abs)
    0x75, 0x08,        //     Report Size (8)
    0x09, 0x51,        //     Usage (Contact Identifier)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0xC0,              //   End Collection
    0x09, 0x54,        //   Usage (Contact Count)
    0x25, 0x7F,        //   Logical Maximum (127)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0xC0               // End Collection
};

#pragma pack(push,1)
typedef struct _NT_HID_REPORT {
    UCHAR  ReportId;      // = 1
    UCHAR  TipSwitch;     // біт0 = торкання
    UCHAR  ContactId;
    USHORT X;             // 0..32767
    USHORT Y;
    UCHAR  ContactCount;
} NT_HID_REPORT;
#pragma pack(pop)
