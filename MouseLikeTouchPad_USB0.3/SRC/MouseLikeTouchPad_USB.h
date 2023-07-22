#pragma once

#include <ntifs.h>
#include <hidport.h>
#include <Usb.h>
#include <usbioctl.h>
#include <Usbdlib.h>

//
#include <wdf.h>

#include<math.h>

#define FUNCTION_FROM_CTL_CODE(ctrlCode) (((ULONG)((ctrlCode) & 0x3FFC)) >> 2)

#include "hidsdi.h"
#include "hidpi.h" 
#include <initguid.h>
#include <devpkey.h>

//
#define RESHUB_USE_HELPER_ROUTINES
#include "reshub.h"

typedef BYTE* PBYTE;

//#include "pshpack1.h"

#define HID_USB_TAG     'UdiH'
#define HID_RESET_TAG   'tesR'
#define HID_REMLOCK_TAG 'WRUH'


const ULONG DEXT_NO_HID_DESC = 0x1;

#define STABLE_INTERVAL_FingerSeparated_MSEC   20   // ��ָ�ֿ�������������ȶ�ʱ����
#define STABLE_INTERVAL_FingerClosed_MSEC      100   // ��ָ��£������������ȶ�ʱ���� 

#define MouseReport_INTERVAL_MSEC         8   // ��걨����ʱ��ms����Ƶ��125hzΪ��׼
#define ButtonPointer_Interval_MSEC      200   // ��������Ҽ���ָ��������ʱ��ms��

#define Jitter_Offset         10    // ������������΢������λ����ֵ

#define SCROLL_OFFSET_THRESHOLD_X      100   // ����λ����X��ֵ 
#define SCROLL_OFFSET_THRESHOLD_Y      100   // ����λ����Y��ֵ 



CHAR UnitExponent_Table[16] = { 0,1,2,3,4,5,6,7,-8,-7,-6,-5,-4,-3,-2,1 };
double MouseSensitivityTable[3] = { 0.8,1.0,1.25 };

///���״̬����,��Ӧ��HID���ϱߵı���
#pragma pack(1)
struct mouse_report_t
{
    BYTE    report_id;
    BYTE    button; //0 no press, 1 left, 2 right ; 3 ����ͬʱ���£�������һ�㲻����������
    CHAR    dx;
    CHAR    dy;
    CHAR    v_wheel; // ��ֱ
    CHAR    h_wheel; // ˮƽ
};
#pragma pack()


#pragma pack(1)
struct mouse_report5
{
    BYTE    report_id;
    BYTE    button; //0 no press, 1 left, 2 right ; 3 ����ͬʱ���£�������һ�㲻����������
    CHAR    dx;
    CHAR    dy;
    CHAR    v_wheel; // ��ֱ
};
#pragma pack()


#pragma pack(push)
#pragma pack(1)
typedef struct _PTP_CONTACT {
    UCHAR		Confidence : 1;
    UCHAR		TipSwitch : 1;
    UCHAR		ContactID : 3;
    UCHAR		Padding : 3;
    USHORT		X;
    USHORT		Y;
} PTP_CONTACT, * PPTP_CONTACT;
#pragma pack(pop)

typedef struct _PTP_REPORT {
    UCHAR       ReportID;
    PTP_CONTACT Contacts[5];
    USHORT      ScanTime;
    UCHAR       ContactCount;
    UCHAR       IsButtonClicked;
} PTP_REPORT, * PPTP_REPORT;

typedef struct _PTP_DEVICE_FETURE_REPORT {
    UCHAR ReportID;
    UCHAR FeatureValue;
} PTP_DEVICE_FETURE_REPORT, * PPTP_DEVICE_FETURE_REPORT;

typedef struct _PTP_DEVICE_HQA_CERTIFICATION_REPORT {
    UCHAR ReportID;
    UCHAR CertificationBlob[256];
} PTP_DEVICE_HQA_CERTIFICATION_REPORT, * PPTP_DEVICE_HQA_CERTIFICATION_REPORT;


typedef struct _PTP_PARSER {

    //����׷�ٵ���ָ����
    PTP_REPORT lastFinger;
    PTP_REPORT currentFinger;

    char nMouse_Pointer_CurrentIndex; //���嵱ǰ���ָ�봥������������������ţ�-1Ϊδ����
    char nMouse_LButton_CurrentIndex; //���嵱ǰ��������������������������ţ�-1Ϊδ����
    char nMouse_RButton_CurrentIndex; //���嵱ǰ����Ҽ���������������������ţ�-1Ϊδ����
    char nMouse_MButton_CurrentIndex; //���嵱ǰ����м���������������������ţ�-1Ϊδ����
    char nMouse_Wheel_CurrentIndex; //���嵱ǰ�����ָ����ο���ָ��������������������ţ�-1Ϊδ����

    char nMouse_Pointer_LastIndex; //�����ϴ����ָ�봥������������������ţ�-1Ϊδ����
    char nMouse_LButton_LastIndex; //�����ϴ���������������������������ţ�-1Ϊδ����
    char nMouse_RButton_LastIndex; //�����ϴ�����Ҽ���������������������ţ�-1Ϊδ����
    char nMouse_MButton_LastIndex; //�����ϴ�����м���������������������ţ�-1Ϊδ����
    char nMouse_Wheel_LastIndex; //�����ϴ������ָ����ο���ָ��������������������ţ�-1Ϊδ����

    BOOLEAN bMouse_Wheel_Mode; //����������״̬��0Ϊ����δ���1Ϊ���ּ���
    BOOLEAN bMouse_Wheel_Mode_JudgeEnable;//�����Ƿ��������б�

    BOOLEAN bPtpReportCollection;//���Ƽ��ϱ�־
    BOOLEAN bGestureCompleted;//���Ʋ���������־

    LARGE_INTEGER MousePointer_DefineTime;//���ָ�붨��ʱ�䣬���ڼ��㰴�����ʱ���������ж�����м�͹��ֲ���

    LARGE_INTEGER JitterFixStartTime; // ���������㶶������ʱ���ʱ��

    float Scroll_TotalDistanceX; //��������ۼƹ�������X
    float Scroll_TotalDistanceY; //��������ۼƹ�������Y

    ULONG tick_Count;

    LARGE_INTEGER current_Ticktime;//��ǰ�����ʱ

    //
    ULONG physicalMax_X;
    ULONG physicalMax_Y;
    ULONG logicalMax_X;
    ULONG logicalMax_Y;
    CHAR unitExp;
    UCHAR unit;

    DOUBLE physical_Width_mm;
    DOUBLE  physical_Height_mm;

    //������ָͷ�ߴ��С
    float thumb_Width;//��ָͷ���
    float thumb_Height;//��ָͷ�߶�
    float thumb_Scale;//��ָͷ�ߴ����ű���
    float FingerMinDistance;//������Ч��������ָ��С����
    float FingerClosedThresholdDistance;//����������ָ��£ʱ����С����
    float FingerMaxDistance;//������Ч��������ָ������
    float TouchPad_DPMM_x;//���崥����ֱ���
    float TouchPad_DPMM_y;//���崥����ֱ���
    float PointerSensitivity_x;//����ָ�������ȼ�ָ����ƶ������ű���
    float PointerSensitivity_y;//����ָ�������ȼ�ָ����ƶ������ű���


} PTP_PARSER, * PPTP_PARSER;


typedef struct _HIDDESC_SETTING {
    UCHAR CONTACT_COUNT_MAXIMUM;
    UCHAR PAD_TYPE;
    UCHAR INPUT_MODE;
    UCHAR FUNCTION_SWITCH;

    UCHAR REPORTID_MULTITOUCH_COLLECTION;
    UCHAR REPORTID_MOUSE_COLLECTION;

    UCHAR REPORTID_DEVICE_CAPS;
    UCHAR REPORTSIZE_DEVICE_CAPS;

    UCHAR REPORTID_PTPHQA;
    USHORT REPORTSIZE_PTPHQA;

    UCHAR REPORTID_INPUT_MODE;
    UCHAR REPORTSIZE_INPUT_MODE;

    UCHAR REPORTID_FUNCTION_SWITCH;
    UCHAR REPORTSIZE_FUNCTION_SWITCH;

    UCHAR REPORTID_LATENCY;//�ӳ�ģʽ
    UCHAR REPORTID_CONFIG_PTP_HAPTICS_ID;//���ط�������SimpleHapticsController

    BOOLEAN bHybrid_ReportingMode;//��ϱ���ģʽ״̬,TRUEΪSingle finger hybrid reporting mode��ָ���ģʽ��FALSEΪParallel mode���б���ģʽ
    UCHAR DeviceDescriptorFingerCount;

}HIDDESC_SETTING, * PHIDDESC_SETTING;


typedef
struct _HID_MINI_DEV_EXTENSION
{
    ULONG PnpState;
    PUSB_DEVICE_DESCRIPTOR pDevDesc;
    PUSBD_INTERFACE_INFORMATION pInterfaceInfo;
    USBD_CONFIGURATION_HANDLE UsbdConfigurationHandle;
    LONG PendingRequestsCount;
    KEVENT Event;
    ULONG Flags;
    PIO_WORKITEM pWorkItem;
    HID_DESCRIPTOR HidDesc;
    //3 bytes align
    PDEVICE_OBJECT pFdo;
    IO_REMOVE_LOCK RemoveLock;

    PDEVICE_OBJECT PDO;

    PUSB_DEVICE_DESCRIPTOR pUsbDeviceDescriptorGlobal;
    PUSB_CONFIGURATION_DESCRIPTOR pConfigDescGlobal;
    PHID_DESCRIPTOR pHidDescGlobal;

    PUCHAR pReportDesciptorData;
    USHORT ReportDescriptorLength;
    PPTP_DEVICE_HQA_CERTIFICATION_REPORT pCertReport;
    PWCHAR pUsbStrData;

    // Windows PTP context
    BOOLEAN  SetFeatureReady;
    BOOLEAN  SetInputModeOK;
    BOOLEAN  SetFunSwicthOK;
    UCHAR   GetStringStep;

    BOOLEAN PtpInputModeOn;


    //MouseLikeTouchpad Paser context
    PTP_PARSER  tp_settings;  //PTP_PARSER����
    HIDDESC_SETTING desc_settings;

    BOOLEAN bWheelDisabled;//��ǰ���ֹ��ܿ����ر�״̬

    BOOLEAN bMouseLikeTouchPad_Mode;//�л������ʽ��������windowsԭ���PTP��ȷʽ�����������ʽ

    UCHAR MouseSensitivity_Index;
    double MouseSensitivity_Value;

    BOOLEAN SensitivityChanged;
    BOOLEAN ButtonDown;

} HID_MINI_DEV_EXTENSION, * PHID_MINI_DEV_EXTENSION;


typedef
struct _WRKITM_RESET_CONTEXT
{
    ULONG Tag;
    PIO_WORKITEM pWorkItem;
    PDEVICE_OBJECT pDeviceObject;
    PIRP pIrp;
} WRKITM_RESET_CONTEXT, * PWRKITM_RESET_CONTEXT;


NTSTATUS HumGetStringDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetHidDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetReportDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp, PBOOLEAN pNeedToCompleteIrp);
NTSTATUS HumGetDeviceAttributes(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetPhysicalDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetMsGenreDescriptor(PDEVICE_OBJECT pDevObj, PIRP pIrp);
PUSBD_PIPE_INFORMATION GetInterruptInputPipeForDevice(PHID_MINI_DEV_EXTENSION pMiniDevExt);
LONG HumDecrementPendingRequestCount(PHID_MINI_DEV_EXTENSION pMiniDevExt);
NTSTATUS HumReadCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
NTSTATUS HumPowerCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
VOID HumSetIdleWorker(PDEVICE_OBJECT DeviceObject, PVOID Context);
NTSTATUS HumSetIdle(PDEVICE_OBJECT pDevObj);
NTSTATUS HumCallUSB(PDEVICE_OBJECT pDevObj, PURB pUrb);
NTSTATUS HumGetDescriptorRequest(PDEVICE_OBJECT pDevObj, USHORT Function, CHAR DescriptorType, PVOID* pDescBuffer, PULONG pDescBuffLen, int Unused, CHAR Index, SHORT LangId);
NTSTATUS HumGetDeviceDescriptor(PDEVICE_OBJECT pDevObj, PHID_MINI_DEV_EXTENSION pMiniDevExt);
NTSTATUS HumGetConfigDescriptor(PDEVICE_OBJECT pDevObj, PUSB_CONFIGURATION_DESCRIPTOR* ppConfigDesc, PULONG pConfigDescLen);
NTSTATUS HumGetHidInfo(PDEVICE_OBJECT pDevObj, UINT_PTR pConfigDescr, UINT_PTR TransferrBufferLen);
NTSTATUS HumSelectConfiguration(PDEVICE_OBJECT pDevObj, PUSB_CONFIGURATION_DESCRIPTOR pConfigDesc);
NTSTATUS HumParseHidInterface(HID_MINI_DEV_EXTENSION* pMiniDevExt, PUSB_INTERFACE_DESCRIPTOR pInterface, LONG TotalLength, PUSB_INTERFACE_DESCRIPTOR* ppHidDesc);
NTSTATUS HumCreateClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumPnP(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumGetSetReport(PDEVICE_OBJECT pDevObj, PIRP pIrp, PBOOLEAN pNeedToCompleteIrp);
NTSTATUS HumAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pFdo);
VOID HumUnload(PDRIVER_OBJECT pDrvObj);
NTSTATUS HumGetSetReportCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
VOID HumResetWorkItem(PDEVICE_OBJECT DeviceObject, PWRKITM_RESET_CONTEXT pContext);
NTSTATUS HumAbortPendingRequests(PDEVICE_OBJECT pDevObj);
NTSTATUS HumRemoveDevice(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS HumStopDevice(PDEVICE_OBJECT pDevObj);
NTSTATUS HumWriteCompletion(PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
NTSTATUS HumIncrementPendingRequestCount(PHID_MINI_DEV_EXTENSION pMiniDevExt);
NTSTATUS HumQueueResetWorkItem(PDEVICE_OBJECT pDevObj, PIRP pIrp);

LARGE_INTEGER UrbTimeout = { 0xFD050F80, 0xFFFFFFFF };




//�󲿷�usage�Ѿ���hidusage.h������,hidsdi.h������hidusage.h
// HID report descriptor constants
#define HID_TYPE_BEGIN_COLLECTION 0xA1
#define HID_TYPE_END_COLLECTION 0xC0
#define HID_TYPE_USAGE_PAGE 0x05
#define HID_TYPE_USAGE_PAGE_2 0x06
#define HID_TYPE_USAGE 0x09
#define HID_TYPE_REPORT_ID 0x85
#define HID_TYPE_REPORT_SIZE 0x75
#define HID_TYPE_REPORT_COUNT 0x95
#define HID_TYPE_REPORT_COUNT_2	0x96
#define HID_TYPE_INPUT   0x81
#define HID_TYPE_FEATURE 0xB1

#define HID_TYPE_USAGE_MINIMUM 0x19
#define HID_TYPE_USAGE_MAXIMUM 0x29
#define HID_TYPE_LOGICAL_MINIMUM 0x15
#define HID_TYPE_LOGICAL_MAXIMUM 0x25
#define HID_TYPE_LOGICAL_MAXIMUM_2 0x26
#define HID_TYPE_LOGICAL_MAXIMUM_3 0x27
#define HID_TYPE_PHYSICAL_MINIMUM 0x35
#define HID_TYPE_PHYSICAL_MAXIMUM 0x45
#define HID_TYPE_PHYSICAL_MAXIMUM_2 0x46
#define HID_TYPE_PHYSICAL_MAXIMUM_3 0x47
#define HID_TYPE_UNIT_EXPONENT 0x55
#define HID_TYPE_UNIT 0x65
#define HID_TYPE_UNIT_2 0x66

#define HID_USAGE_PAGE_VENDOR_DEFINED_DEVICE_CERTIFICATION 0xC5
#define HID_USAGE_CONFIGURATION 0x0E

#define HID_USAGE_BUTTON_STATE 0x01
#define HID_USAGE_X 0x30
#define HID_USAGE_Y 0x31
#define HID_USAGE_TIP 0x42
#define HID_USAGE_CONFIDENCE 0x47
#define HID_USAGE_WIDTH 0x48
#define HID_USAGE_HEIGHT 0x49

#define HID_USAGE_INPUT_MODE 0x52
#define HID_USAGE_CONTACT_COUNT	0x54
#define HID_USAGE_CONTACT_COUNT_MAXIMUM	0x55
#define HID_USAGE_SCAN_TIME	0x56
#define HID_USAGE_BUTTON_SWITCH 0x57
#define HID_USAGE_SURFACE_SWITCH 0x58
#define HID_USAGE_PAD_TYPE	0x59

#define HID_USAGE_LATENCY_MODE 0x60
//#define HID_USAGE_HAPTIC_INTENSITY 0x23

#define HID_COLLECTION_APPLICATION 0x01
#define HID_COLLECTION_LOGICAL 0x02


//PTP Setting Values
#define PTP_MAX_CONTACT_POINTS 5
#define PTP_BUTTON_TYPE_CLICK_PAD 0
#define PTP_BUTTON_TYPE_PRESSURE_PAD 1

#define PTP_COLLECTION_MOUSE 0
#define PTP_COLLECTION_WINDOWS 3

#define PTP_FEATURE_INPUT_COLLECTION   0
#define PTP_FEATURE_SELECTIVE_REPORTING   1
#define PTP_SELECTIVE_REPORT_Button_Surface_ON 3

#define PTP_CONTACT_CONFIDENCE_BIT   1
#define PTP_CONTACT_TIPSWITCH_BIT    2


#define DEFAULT_PTP_HQA_BLOB \
	0xfc, 0x28, 0xfe, 0x84, 0x40, 0xcb, 0x9a, 0x87, \
	0x0d, 0xbe, 0x57, 0x3c, 0xb6, 0x70, 0x09, 0x88, \
	0x07, 0x97, 0x2d, 0x2b, 0xe3, 0x38, 0x34, 0xb6, \
	0x6c, 0xed, 0xb0, 0xf7, 0xe5, 0x9c, 0xf6, 0xc2, \
	0x2e, 0x84, 0x1b, 0xe8, 0xb4, 0x51, 0x78, 0x43, \
	0x1f, 0x28, 0x4b, 0x7c, 0x2d, 0x53, 0xaf, 0xfc, \
	0x47, 0x70, 0x1b, 0x59, 0x6f, 0x74, 0x43, 0xc4, \
	0xf3, 0x47, 0x18, 0x53, 0x1a, 0xa2, 0xa1, 0x71, \
	0xc7, 0x95, 0x0e, 0x31, 0x55, 0x21, 0xd3, 0xb5, \
	0x1e, 0xe9, 0x0c, 0xba, 0xec, 0xb8, 0x89, 0x19, \
	0x3e, 0xb3, 0xaf, 0x75, 0x81, 0x9d, 0x53, 0xb9, \
	0x41, 0x57, 0xf4, 0x6d, 0x39, 0x25, 0x29, 0x7c, \
	0x87, 0xd9, 0xb4, 0x98, 0x45, 0x7d, 0xa7, 0x26, \
	0x9c, 0x65, 0x3b, 0x85, 0x68, 0x89, 0xd7, 0x3b, \
	0xbd, 0xff, 0x14, 0x67, 0xf2, 0x2b, 0xf0, 0x2a, \
	0x41, 0x54, 0xf0, 0xfd, 0x2c, 0x66, 0x7c, 0xf8, \
	0xc0, 0x8f, 0x33, 0x13, 0x03, 0xf1, 0xd3, 0xc1, \
	0x0b, 0x89, 0xd9, 0x1b, 0x62, 0xcd, 0x51, 0xb7, \
	0x80, 0xb8, 0xaf, 0x3a, 0x10, 0xc1, 0x8a, 0x5b, \
	0xe8, 0x8a, 0x56, 0xf0, 0x8c, 0xaa, 0xfa, 0x35, \
	0xe9, 0x42, 0xc4, 0xd8, 0x55, 0xc3, 0x38, 0xcc, \
	0x2b, 0x53, 0x5c, 0x69, 0x52, 0xd5, 0xc8, 0x73, \
	0x02, 0x38, 0x7c, 0x73, 0xb6, 0x41, 0xe7, 0xff, \
	0x05, 0xd8, 0x2b, 0x79, 0x9a, 0xe2, 0x34, 0x60, \
	0x8f, 0xa3, 0x32, 0x1f, 0x09, 0x78, 0x62, 0xbc, \
	0x80, 0xe3, 0x0f, 0xbd, 0x65, 0x20, 0x08, 0x13, \
	0xc1, 0xe2, 0xee, 0x53, 0x2d, 0x86, 0x7e, 0xa7, \
	0x5a, 0xc5, 0xd3, 0x7d, 0x98, 0xbe, 0x31, 0x48, \
	0x1f, 0xfb, 0xda, 0xaf, 0xa2, 0xa8, 0x6a, 0x89, \
	0xd6, 0xbf, 0xf2, 0xd3, 0x32, 0x2a, 0x9a, 0xe4, \
	0xcf, 0x17, 0xb7, 0xb8, 0xf4, 0xe1, 0x33, 0x08, \
	0x24, 0x8b, 0xc4, 0x43, 0xa5, 0xe5, 0x24, 0xc2



//ReportIDΪ������Ϊ�ϲ��������ṩ�ı���id���ϲ��෢�͸��²�������report����������ʱ��Ҫ�滻��ʵ�ʵ�ReportID���ұ������ݸ�ʽҲҪ����ʵ�ʵ��²����������·�װ,��������Ҫ��ǰ��ȡhid������������ȷ����ȷ����ֵ
//ÿ��REPORTID_���벻ͬ�����ֱ�����𣬲���ֵ��1 - 255֮��
#define FAKE_REPORTID_MOUSE 0x02
#define FAKE_REPORTID_MULTITOUCH 0x05
#define FAKE_REPORTID_DEVICE_CAPS 0x05
#define FAKE_REPORTID_INPUTMODE 0x03
#define FAKE_REPORTID_FUNCTION_SWITCH 0x06   
#define FAKE_REPORTID_PTPHQA 0x08

#define FAKE_REPORTID_VendorDefined_9 0x09
#define FAKE_REPORTID_VendorDefined_A 0x0a
#define FAKE_REPORTID_VendorDefined_B 0x0b
#define FAKE_REPORTID_VendorDefined_C 0x0c
#define FAKE_REPORTID_VendorDefined_F 0x0f
#define FAKE_REPORTID_VendorDefined_E 0x0e


#define PTP_FINGER_COLLECTION_2 \
    0xa1, 0x02,                         /*   COLLECTION (Logical)     */ \
    0x15, 0x00,                         /*       LOGICAL_MINIMUM (0)     */ \
    0x25, 0x01,                         /*       LOGICAL_MAXIMUM (1)     */ \
    0x09, 0x47,                         /*       USAGE (Confidence)     */ \
    0x09, 0x42,                         /*       USAGE (Tip switch)     */ \
    0x95, 0x02,                         /*       REPORT_COUNT (2)     */ \
    0x75, 0x01,                         /*       REPORT_SIZE (1)     */ \
    0x81, 0x02,                         /*       INPUT (Data,Var,Abs)     */ \
    0x95, 0x01,                         /*       REPORT_COUNT (1)     */ \
    0x75, 0x02,                         /*       REPORT_SIZE (2)     */ \
    0x25, 0x03,                         /*       LOGICAL_MAXIMUM (3)     */ \
    0x09, 0x51,                         /*       USAGE (Contact Identifier)     */ \
    0x81, 0x03,                         /*       INPUT (Constant,Var)     */ \
    0x75, 0x04,                         /*       REPORT_SIZE (4)     */ \
    0x95, 0x01,                         /*       REPORT_COUNT (1)     */ \
    0x81, 0x02,                         /*       INPUT (Data,Var,Abs)     */ \
\
    0x05, 0x01,                         /* USAGE_PAGE (Generic Desktop)     */ \
    0x15, 0x00,                         /*       LOGICAL_MINIMUM (0)     */ \
    0x26, 0x7c, 0x05,                   /*       LOGICAL_MAXIMUM (1404)     */ \
    0x75, 0x10,                         /*       REPORT_SIZE (16)     */ \
    0x55, 0x0e,                         /*       UNIT_EXPONENT (-2)     */ \
    0x65, 0x11,                         /*       UNIT(cm����)     */ \
    0x09, 0x30,                         /*     USAGE (X)     */ \
    0x35, 0x00,                         /*       PHYSICAL_MINIMUM (0)     */ \
    0x46, 0x90, 0x04,                   /*       PHYSICAL_MAXIMUM (1168)     */ \
    0x95, 0x01,                         /*       REPORT_COUNT (1)     */ \
    0x81, 0x02,                         /*       INPUT (Data,Var,Abs)     */ \
    0x46, 0xD0, 0x02,                   /*       PHYSICAL_MAXIMUM (720)     */ \
    0x26, 0x60, 0x03,                   /*       LOGICAL_MAXIMUM (864)     */ \
    0x09, 0x31,                         /*     USAGE (Y)     */ \
    0x81, 0x02,                         /*       INPUT (Data,Var,Abs)     */ \
    0xc0                               /*   END_COLLECTION     ע�ⲻ��Ҫ���Ž�β*/ \
\

//touchpad������
const unsigned char SingleFingerHybridMode_PtpReportDescriptor[] = {//��������ֻ��Ϊ�ϲ�ͻ�������ʹ�ã�ʵ��hid������Ӧ�����²�������ȡ������Ϊ׼
    //MOUSE TLC
    0x05, 0x01, // USAGE_PAGE(Generic Desktop)
    0x09, 0x02, //   USAGE(Mouse)
    0xA1, 0x01, //   COLLECTION(APPlication)
        0x85, FAKE_REPORTID_MOUSE, //     ReportID(Mouse ReportID)  //��ʱռλ��;��ʵ��ʹ�ö�дReportʱ��Ҫ��ǰ��ȡhid������������ȷ����ȷ����ֵ
        0x09, 0x01, //   USAGE(Pointer)
        0xA1, 0x00, //     COLLECTION(Physical)
            0x05, 0x09, //     USAGE_PAGE(Button)
            0x19, 0x01, //     USAGE_MINIMUM(button 1)   Button ������ λ 0 ����� λ1 �Ҽ��� λ2 �м�
            0x29, 0x07, //     USAGE_MAXMUM(button 5)  //0x05����������갴������
            0x15, 0x00, //     LOGICAL_MINIMUM(0)
            0x25, 0x01, //     LOGICAL_MAXIMUM(1)
            0x75, 0x01, //     REPORT_SIZE(1)
            0x95, 0x07, //     REPORT_COUNT(3)  //0x05��갴������,����4��Back/5��Forward����ǰ�����ܼ�
            0x81, 0x02, //     INPUT(Data,Var,Abs)
            0x95, 0x01, //     REPORT_COUNT(3) //��Ҫ������ٸ�bitʹ�ü�����갴��������n��bitλ��1���ֽ�8bit
            0x81, 0x03, //     INPUT (Cnst,Var,Abs)////һ��pending��λ��input��Cnst����0x03
            0x05, 0x01, //     USAGE_PAGE(Generic Desktop)
            0x09, 0x30, //     USAGE(X)       X�ƶ�
            0x09, 0x31, //     USAGE(Y)       Y�ƶ�
            0x09, 0x38, //     USAGE(Wheel)   ��ֱ����
            0x15, 0x81, //     LOGICAL_MINIMUM(-127)
            0x25, 0x7F, //     LOGICAL_MAXIMUM(127)
            0x75, 0x08, //     REPORT_SIZE(8)
            0x95, 0x03, //     REPORT_COUNT(3)
            0x81, 0x06, //     INPUT(Data,Var, Rel) //X,Y,��ֱ�������������� ���ֵ

            //�±�ˮƽ����
            0x05, 0x0C, //     USAGE_PAGE (Consumer Devices)
            0x0A, 0x38, 0x02, // USAGE(AC Pan)
            0x15, 0x81, //       LOGICAL_MINIMUM(-127)
            0x25, 0x7F, //       LOGICAL_MAXIMUM(127)
            0x75, 0x08, //       REPORT_SIZE(8)
            0x95, 0x01, //       REPORT_COUNT(1)
            0x81, 0x06, //       INPUT(data,Var, Rel) //ˮƽ���֣����ֵ
        0xC0,       //       End Connection(PhySical)
    0xC0,       //     End Connection


    //TOUCH PAD input TLC
    0x05, 0x0d,                         // USAGE_PAGE (Digitizers)          
    0x09, 0x05,                         // USAGE (Touch Pad)    
    0xa1, 0x01,                         // COLLECTION (Application)         
        0x85, FAKE_REPORTID_MULTITOUCH,     /*  REPORT_ID (Touch pad)  REPORTID_MULTITOUCH  */ \

        0x09, 0x22,                        /* Usage: Finger */ \
        PTP_FINGER_COLLECTION_2, \

        0x05, 0x0d,                         // USAGE_PAGE (Digitizers) 
        0x55, 0x0C,                         //    UNIT_EXPONENT (-4) 
        0x66, 0x01, 0x10,                   //    UNIT (Seconds)        
        0x47, 0xff, 0xff, 0x00, 0x00,      //     PHYSICAL_MAXIMUM (65535)
        0x27, 0xff, 0xff, 0x00, 0x00,         //  LOGICAL_MAXIMUM (65535) 
        0x75, 0x10,                           //  REPORT_SIZE (16)             
        0x95, 0x01,                           //  REPORT_COUNT (1) 

        0x09, 0x56,                         //    USAGE (Scan Time)    
        0x81, 0x02,                           //  INPUT (Data,Var,Abs)         
        0x09, 0x54,                         //    USAGE (Contact count)
        0x25, 0x7f,                           //  LOGICAL_MAXIMUM (127) 
        0x95, 0x01,                         //    REPORT_COUNT (1)
        0x75, 0x08,                         //    REPORT_SIZE (8)    
        0x81, 0x02,                         //    INPUT (Data,Var,Abs)

        0x05, 0x09,                         //    USAGE_PAGE (Button)         
        0x09, 0x01,                         //    USAGE_(Button 1)     
        0x25, 0x01,                         //    LOGICAL_MAXIMUM (1)          
        0x75, 0x01,                         //    REPORT_SIZE (1)              
        0x95, 0x01,                         //    REPORT_COUNT (1)             
        0x81, 0x02,                         //    INPUT (Data,Var,Abs)
        0x95, 0x07,                          //   REPORT_COUNT (7)                 
        0x81, 0x03,                         //    INPUT (Constant,Var)

        0x09, 0xC5,                         //    USAGE (��֤״̬Blob)
        0x75, 0x08,                         //    REPORT_SIZE (8)              
        0x95, 0x02,                         //    REPORT_COUNT (2)             
        0x81, 0x03,                         //    INPUT (Constant,Var)

        0x05, 0x0d,                         // USAGE_PAGE (Digitizers) 
        0x85, FAKE_REPORTID_DEVICE_CAPS,    // REPORT_ID (Feature) Ӳ������                  
        0x09, 0x55,                         //    USAGE (Contact Count Maximum) Ӳ��֧�ֵ��� REPORTID_MAX_COUNT
        0x09, 0x59,                         //    USAGE (Pad TYpe) ����������
        0x75, 0x04,                         //    REPORT_SIZE (4) 
        0x95, 0x02,                         //    REPORT_COUNT (2)
        0x25, 0x0f,                         //    LOGICAL_MAXIMUM (15)
        0xb1, 0x02,                         //    FEATURE (Data,Var,Abs)

        //0x85, FAKE_REPORTID_Latency,   //    REPORT_ID   Latency mode feature report id
        //0x09, 0x60,                         //    USAGE (  Latency mode feature report �ӳ�ģʽ���ܱ���Ŀ�ѡ֧��) 
        //0x75, 0x01,                         //    REPORT_SIZE (1)              
        //0x95, 0x01,                         //    REPORT_COUNT (1)    
        //0x15, 0x00,                         //       LOGICAL_MINIMUM (0) 
        //0x25, 0x01,                         //     LOGICAL_MAXIMUM (1)
        //0xb1, 0x02,                         //    FEATURE (Data,Var,Abs)
        //0x95, 0x0F,                          //   REPORT_COUNT (15)  
        //0xb1, 0x03,                         //    FEATURE (Constant,Var)  

        0x06, 0x00, 0xff,                   //    USAGE_PAGE (Vendor Defined)
        0x85, FAKE_REPORTID_PTPHQA, //   REPORT_ID (PTPHQA) 
        0x09, 0xC5,                         //    USAGE (Vendor Usage 0xC5 ��������֤״̬Blob)
        0x15, 0x00,                         //    LOGICAL_MINIMUM (0)          
        0x26, 0xff, 0x00,                   //    LOGICAL_MAXIMUM (0xff) 
        0x75, 0x08,                         //    REPORT_SIZE (8)             
        0x96, 0x00, 0x01,                   //    REPORT_COUNT (0x100 (256))     
        0xb1, 0x02,                         //    FEATURE (Data,Var,Abs)

    0xc0,                               // END_COLLECTION


    //CONFIG TLC
    0x05, 0x0d,                         //    USAGE_PAGE (Digitizer)
    0x09, 0x0E,                         //    USAGE (Configuration)
    0xa1, 0x01,                         //   COLLECTION (Application)
        0x85, FAKE_REPORTID_INPUTMODE,   //   REPORT_ID (Feature)      REPORTID_FEATURE       
        0x09, 0x22,                         //   USAGE (Finger)              
        0xa1, 0x02,                         //   COLLECTION (logical)     
            0x09, 0x52,                         //    USAGE (Input Mode)         
            0x15, 0x00,                         //    LOGICAL_MINIMUM (0)      
            0x25, 0x0a,                         //    LOGICAL_MAXIMUM (10)
            0x75, 0x10,                         //    REPORT_SIZE (16)         
            0x95, 0x01,                         //    REPORT_COUNT (1)         
            0xb1, 0x02,                         //    FEATURE (Data,Var,Abs    
            0xc0,                               //   END_COLLECTION

            0x09, 0x22,                         //   USAGE (Finger)              
            0xa1, 0x00,                         //   COLLECTION (physical)     
            0x85, FAKE_REPORTID_FUNCTION_SWITCH,  //     REPORT_ID (Feature)              
            0x09, 0x57,                         //     USAGE(Surface switch)
            0x09, 0x58,                         //     USAGE(Button switch)
            0x75, 0x01,                         //     REPORT_SIZE (1)
            0x95, 0x02,                         //     REPORT_COUNT (2)
            0x25, 0x01,                         //     LOGICAL_MAXIMUM (1)
            0xb1, 0x02,                         //     FEATURE (Data,Var,Abs)
            0x95, 0x0E,                         //     REPORT_COUNT (14)             
            0xb1, 0x03,                         //     FEATURE (Cnst,Var,Abs)
        0xc0,                               //   END_COLLECTION
    0xc0,                               // END_COLLECTION

};



#define PTP_FINGER_COLLECTION \
    0xa1, 0x02,                         /*   COLLECTION (Logical)     */ \
    0x15, 0x00,                         /*       LOGICAL_MINIMUM (0)     */ \
    0x25, 0x01,                         /*       LOGICAL_MAXIMUM (1)     */ \
    0x09, 0x47,                         /*       USAGE (Confidence)     */ \
    0x09, 0x42,                         /*       USAGE (Tip switch)     */ \
    0x95, 0x02,                         /*       REPORT_COUNT (2)     */ \
    0x75, 0x01,                         /*       REPORT_SIZE (1)     */ \
    0x81, 0x02,                         /*       INPUT (Data,Var,Abs)     */ \
    0x95, 0x01,                         /*       REPORT_COUNT (1)     */ \
    0x75, 0x03,                         /*       REPORT_SIZE (3)     */ \
    0x25, 0x05,                         /*       LOGICAL_MAXIMUM (5)     */ \
    0x09, 0x51,                         /*       USAGE (Contact Identifier)     */ \
    0x81, 0x02,                         /*       INPUT (Data,Var,Abs)     */ \
    0x75, 0x01,                         /*       REPORT_SIZE (1)     */ \
    0x95, 0x03,                         /*       REPORT_COUNT (3)     */ \
    0x81, 0x03,                         /*       INPUT (Constant,Var)     */ \
\
    0x05, 0x01,                         /* USAGE_PAGE (Generic Desktop)     */ \
    0x15, 0x00,                         /*       LOGICAL_MINIMUM (0)     */ \
    0x26, 0x7c, 0x05,                   /*       LOGICAL_MAXIMUM (1404)     */ \
    0x75, 0x10,                         /*       REPORT_SIZE (16)     */ \
    0x55, 0x0e,                         /*       UNIT_EXPONENT (-2)     */ \
    0x65, 0x11,                         /*       UNIT(cm����)     */ \
    0x09, 0x30,                         /*     USAGE (X)     */ \
    0x35, 0x00,                         /*       PHYSICAL_MINIMUM (0)     */ \
    0x46, 0x90, 0x04,                   /*       PHYSICAL_MAXIMUM (1168)     */ \
    0x95, 0x01,                         /*       REPORT_COUNT (1)     */ \
    0x81, 0x02,                         /*       INPUT (Data,Var,Abs)     */ \
    0x46, 0xD0, 0x02,                   /*       PHYSICAL_MAXIMUM (720)     */ \
    0x26, 0x60, 0x03,                   /*       LOGICAL_MAXIMUM (864)     */ \
    0x09, 0x31,                         /*     USAGE (Y)     */ \
    0x81, 0x02,                         /*       INPUT (Data,Var,Abs)     */ \
    0xc0                               /*   END_COLLECTION     ע�ⲻ��Ҫ���Ž�β*/ \
\

const unsigned char ParallelMode_PtpReportDescriptor[] = {
    //MOUSE TLC
    0x05, 0x01, // USAGE_PAGE(Generic Desktop)
    0x09, 0x02, //   USAGE(Mouse)
    0xA1, 0x01, //   COLLECTION(APPlication)
        0x85, FAKE_REPORTID_MOUSE, //     ReportID(Mouse ReportID)  //�����ID���ڿͻ���ͨѶ��;��ʵ��ʹ�ö�дReportʱ��Ҫ��ǰ��ȡhid������������ȷ����ȷ����ֵ
        0x09, 0x01, //   USAGE(Pointer)
        0xA1, 0x00, //     COLLECTION(Physical)
            0x05, 0x09, //     USAGE_PAGE(Button)
            0x19, 0x01, //     USAGE_MINIMUM(button 1)   Button ������ λ 0 ����� λ1 �Ҽ��� λ2 �м�
            0x29, 0x07, //     USAGE_MAXMUM(button 5)  //0x05����������갴������
            0x15, 0x00, //     LOGICAL_MINIMUM(0)
            0x25, 0x01, //     LOGICAL_MAXIMUM(1)
            0x75, 0x01, //     REPORT_SIZE(1)
            0x95, 0x07, //     REPORT_COUNT(3)  //0x05��갴������,����4��Back/5��Forward����ǰ�����ܼ�
            0x81, 0x02, //     INPUT(Data,Var,Abs)
            0x95, 0x01, //     REPORT_COUNT(3) //��Ҫ������ٸ�bitʹ�ü�����갴��������n��bitλ��1���ֽ�8bit
            0x81, 0x03, //     INPUT (Cnst,Var,Abs)////һ��pending��λ��input��Cnst����0x03
            0x05, 0x01, //     USAGE_PAGE(Generic Desktop)
            0x09, 0x30, //     USAGE(X)       X�ƶ�
            0x09, 0x31, //     USAGE(Y)       Y�ƶ�
            0x09, 0x38, //     USAGE(Wheel)   ��ֱ����
            0x15, 0x81, //     LOGICAL_MINIMUM(-127)
            0x25, 0x7F, //     LOGICAL_MAXIMUM(127)
            0x75, 0x08, //     REPORT_SIZE(8)
            0x95, 0x03, //     REPORT_COUNT(3)
            0x81, 0x06, //     INPUT(Data,Var, Rel) //X,Y,��ֱ�������������� ���ֵ

            //�±�ˮƽ����
            0x05, 0x0C, //     USAGE_PAGE (Consumer Devices)
            0x0A, 0x38, 0x02, // USAGE(AC Pan)
            0x15, 0x81, //       LOGICAL_MINIMUM(-127)
            0x25, 0x7F, //       LOGICAL_MAXIMUM(127)
            0x75, 0x08, //       REPORT_SIZE(8)
            0x95, 0x01, //       REPORT_COUNT(1)
            0x81, 0x06, //       INPUT(data,Var, Rel) //ˮƽ���֣����ֵ
        0xC0,       //       End Connection(PhySical)
    0xC0,       //     End Connection


    //TOUCH PAD input TLC
    0x05, 0x0d,                         // USAGE_PAGE (Digitizers)          
    0x09, 0x05,                         // USAGE (Touch Pad)    
    0xa1, 0x01,                         // COLLECTION (Application)         
        0x85, FAKE_REPORTID_MULTITOUCH,     /*  REPORT_ID (Touch pad)  REPORTID_MULTITOUCH  */ \

        0x05, 0x0d,                         /* USAGE_PAGE (Digitizers)  */ \
        0x09, 0x22,                        /* Usage: Finger */ \
        PTP_FINGER_COLLECTION, \

        0x05, 0x0d,                         /* USAGE_PAGE (Digitizers)  */ \
        0x09, 0x22,                        /* Usage: Finger */ \
        PTP_FINGER_COLLECTION, \

        0x05, 0x0d,                         /* USAGE_PAGE (Digitizers)  */ \
        0x09, 0x22,                        /* Usage: Finger */ \
        PTP_FINGER_COLLECTION, \

        0x05, 0x0d,                         /* USAGE_PAGE (Digitizers)  */ \
        0x09, 0x22,                        /* Usage: Finger */ \
        PTP_FINGER_COLLECTION, \

        0x05, 0x0d,                         /* USAGE_PAGE (Digitizers)  */ \
        0x09, 0x22,                        /* Usage: Finger */ \
        PTP_FINGER_COLLECTION, \

        0x05, 0x0d,                         // USAGE_PAGE (Digitizers) 
        0x55, 0x0C,                         //    UNIT_EXPONENT (-4) 
        0x66, 0x01, 0x10,                   //    UNIT (Seconds)        
        0x47, 0xff, 0xff, 0x00, 0x00,      //     PHYSICAL_MAXIMUM (65535)
        0x27, 0xff, 0xff, 0x00, 0x00,         //  LOGICAL_MAXIMUM (65535) 
        0x75, 0x10,                           //  REPORT_SIZE (16)             
        0x95, 0x01,                           //  REPORT_COUNT (1) 

        0x09, 0x56,                         //    USAGE (Scan Time)    
        0x81, 0x02,                           //  INPUT (Data,Var,Abs)         
        0x09, 0x54,                         //    USAGE (Contact count)
        0x25, 0x7f,                           //  LOGICAL_MAXIMUM (127) 
        0x95, 0x01,                         //    REPORT_COUNT (1)
        0x75, 0x08,                         //    REPORT_SIZE (8)    
        0x81, 0x02,                         //    INPUT (Data,Var,Abs)
        0x05, 0x09,                         //    USAGE_PAGE (Button)         
        0x09, 0x01,                         //    USAGE_(Button 1)     
        0x25, 0x01,                         //    LOGICAL_MAXIMUM (1)          
        0x75, 0x01,                         //    REPORT_SIZE (1)              
        0x95, 0x01,                         //    REPORT_COUNT (1)             
        0x81, 0x02,                         //    INPUT (Data,Var,Abs)
        0x95, 0x07,                          //   REPORT_COUNT (7)                 
        0x81, 0x03,                         //    INPUT (Constant,Var)

        0x05, 0x0d,                         //    USAGE_PAGE (Digitizer)
        0x85, FAKE_REPORTID_DEVICE_CAPS,    // REPORT_ID (Feature) Ӳ������                  
        0x09, 0x55,                         //    USAGE (Contact Count Maximum) Ӳ��֧�ֵ��� REPORTID_MAX_COUNT
        0x09, 0x59,                         //    USAGE (Pad TYpe) ����������
        0x75, 0x04,                         //    REPORT_SIZE (4) 
        0x95, 0x02,                         //    REPORT_COUNT (2)
        0x25, 0x0f,                         //    LOGICAL_MAXIMUM (15)
        0xb1, 0x02,                         //    FEATURE (Data,Var,Abs)

        //0x85, FAKE_REPORTID_Latency,   //    REPORT_ID   Latency mode feature report id
        //0x09, 0x60,                         //    USAGE (  Latency mode feature report �ӳ�ģʽ���ܱ���Ŀ�ѡ֧��) 
        //0x75, 0x01,                         //    REPORT_SIZE (1)              
        //0x95, 0x01,                         //    REPORT_COUNT (1)    
        //0x15, 0x00,                         //       LOGICAL_MINIMUM (0) 
        //0x25, 0x01,                         //     LOGICAL_MAXIMUM (1)
        //0xb1, 0x02,                         //    FEATURE (Data,Var,Abs)
        //0x95, 0x07,                          //   REPORT_COUNT (7)  
        //0xb1, 0x03,                         //    FEATURE (Constant,Var)  

        0x85, FAKE_REPORTID_PTPHQA, //   REPORT_ID (PTPHQA) 
        0x06, 0x00, 0xff,                   //    USAGE_PAGE (Vendor Defined)
        0x09, 0xC5,                         //    USAGE (Vendor Usage 0xC5 ��������֤״̬Blob)
        0x15, 0x00,                         //    LOGICAL_MINIMUM (0)          
        0x26, 0xff, 0x00,                   //    LOGICAL_MAXIMUM (0xff) 
        0x75, 0x08,                         //    REPORT_SIZE (8)             
        0x96, 0x00, 0x01,                   //    REPORT_COUNT (0x100 (256))     
        0xb1, 0x02,                         //    FEATURE (Data,Var,Abs)

        ////����ժ¼ժ���豸��֤״̬���ܱ����Windows���� Touchpad �������ϵ������� ��ѡ֧�֡� 
        ////��������֤״̬ Blob ���Ϊ 8 �� 32 �ֽڶΣ������ǵ��� 256 �ֽڶ�,C6�÷�ָʾ�ֶ��������壬C7�÷�ָʾÿ�εĳ����ֽڶ���
        ////����ָʾ�� SET FEATURE �з��صĶ�#���豸Ӧ���� GET FEATURE �еĶ� #�͹����ĶΡ�
        //0x06, 0x00, 0xff,                   //     USAGE_PAGE (Vendor Defined)  
        //0x85, FAKE_REPORTID_PTPHQA,    //     REPORT_ID (PTPHQA)              
        //0x09, 0xC6,                         //     USAGE (Vendor usage for segment #) 
        //0x25, 0x08,                         //     LOGICAL_MAXIMUM (8)
        //0x75, 0x08,                         //     REPORT_SIZE (8)
        //0x95, 0x01,                         //     REPORT_COUNT (1) 
        //0xb1, 0x02,                         //     FEATURE (Data,Var,Abs) 
        //0x09, 0xC7,                         //     USAGE (Vendor Usage) 
        //0x26, 0xff, 0x00,                   //     LOGICAL_MAXIMUM (0xff)                 
        //0x95, 0x20,                         //     REPORT_COUNT (32)             
        //0xb1, 0x02,                         //     FEATURE (Data,Var,Abs)

    0xc0,                               // END_COLLECTION


    //CONFIG TLC
    0x05, 0x0d,                         //    USAGE_PAGE (Digitizer)
    0x09, 0x0E,                         //    USAGE (Configuration)
    0xa1, 0x01,                         //   COLLECTION (Application)
        0x85, FAKE_REPORTID_INPUTMODE,   //   REPORT_ID (Feature)      REPORTID_FEATURE       
        0x09, 0x22,                         //   USAGE (Finger)              
        0xa1, 0x02,                         //   COLLECTION (logical)     
        0x09, 0x52,                         //    USAGE (Input Mode)         
        0x15, 0x00,                         //    LOGICAL_MINIMUM (0)      
        0x25, 0x0a,                         //    LOGICAL_MAXIMUM (10)
        0x75, 0x08,                         //    REPORT_SIZE (8)         
        0x95, 0x01,                         //    REPORT_COUNT (1)         
        0xb1, 0x02,                         //    FEATURE (Data,Var,Abs    
    0xc0,                               //   END_COLLECTION

    0x09, 0x22,                         //   USAGE (Finger)              
    0xa1, 0x00,                         //   COLLECTION (physical)     
        0x85, FAKE_REPORTID_FUNCTION_SWITCH,  //     REPORT_ID (Feature)              
        0x09, 0x57,                         //     USAGE(Surface switch)
        0x09, 0x58,                         //     USAGE(Button switch)
        0x75, 0x01,                         //     REPORT_SIZE (1)
        0x95, 0x02,                         //     REPORT_COUNT (2)
        0x25, 0x01,                         //     LOGICAL_MAXIMUM (1)
        0xb1, 0x02,                         //     FEATURE (Data,Var,Abs)
        0x95, 0x06,                         //     REPORT_COUNT (6)             
        0xb1, 0x03,                         //     FEATURE (Cnst,Var,Abs)
        0xc0,                               //   END_COLLECTION
    0xc0,                               // END_COLLECTION

    ////Vendor Defined
    //0x06, 0x00, 0xff,                   //     USAGE_PAGE (Vendor Defined) ��0x06 = HID_TYPE_USAGE_PAGE_2
    //0x09, 0x01,                         //   USAGE(vendor defined�÷�Usage_x01)   User-mode Application configuration
    //0xa1, 0x01,                         //   COLLECTION (Application)
    //    0x85, FAKE_REPORTID_VendorDefined_9,              //     REPORT_ID ( ) 
    //    0x09, 0x02,                         // USAGE (Vendor Defined)
    //    0x15, 0x00,                         //    LOGICAL_MINIMUM (0)
    //    0x26, 0xff, 0x00,                   //     LOGICAL_MAXIMUM (0xff)  
    //    0x75, 0x08,                         //     REPORT_SIZE (8)
    //    0x95, 0x14,                         //     REPORT_COUNT (20) 
    //    0x91, 0x02,                         //     OUTPUT (Data,Var,Abs)�������

    //    0x85, FAKE_REPORTID_VendorDefined_A,   //     REPORT_ID ( )
    //    0x09, 0x03,                         //   USAGE (Vendor Defined)
    //    0x15, 0x00,                         //    LOGICAL_MINIMUM (0)
    //    0x26, 0xff, 0x00,                   //     LOGICAL_MAXIMUM (0xff)  
    //    0x75, 0x08,                         //     REPORT_SIZE (8)
    //    0x95, 0x14,                         //     REPORT_COUNT (20)  
    //    0x91, 0x02,                         //     OUTPUT (Data,Var,Abs)�������

    //    0x85, FAKE_REPORTID_VendorDefined_B,     //     REPORT_ID ( )
    //    0x09, 0x04,                        //   USAGE (Vendor Defined)
    //    0x15, 0x00,                         //    LOGICAL_MINIMUM (0)
    //    0x26, 0xff, 0x00,                   //     LOGICAL_MAXIMUM (0xff)  
    //    0x75, 0x08,                         //     REPORT_SIZE (8)
    //    0x95, 0x3d,                         //     REPORT_COUNT (61)  
    //    0x81, 0x02,                         //       INPUT (Data,Var,Abs)

    //    0x85, FAKE_REPORTID_VendorDefined_C,     //     REPORT_ID ( ) 
    //    0x09, 0x05,                        //   USAGE (Vendor Defined)
    //    0x15, 0x00,                         //    LOGICAL_MINIMUM (0)
    //    0x26, 0xff, 0x00,                   //     LOGICAL_MAXIMUM (0xff)  
    //    0x75, 0x08,                         //     REPORT_SIZE (8)
    //    0x95, 0x3d,                         //     REPORT_COUNT (61)  
    //    0x81, 0x02,                         //     INPUT (Data,Var,Abs)

    //    0x85, FAKE_REPORTID_VendorDefined_F,     //     REPORT_ID ( )
    //    0x09, 0x06,                        //   USAGE (Vendor usage for segment #6) 
    //    0x15, 0x00,                         //    LOGICAL_MINIMUM (0)
    //    0x26, 0xff, 0x00,                   //     LOGICAL_MAXIMUM (0xff)  
    //    0x75, 0x08,                         //     REPORT_SIZE (8)
    //    0x95, 0x03,                         //     REPORT_COUNT (3)  
    //    0xb1, 0x02,                         //     FEATURE (Data,Var,Abs)

    //    0x85, FAKE_REPORTID_VendorDefined_E,     //     REPORT_ID ( )
    //    0x09, 0x07,                        //   USAGE (Vendor usage for segment #7) 
    //    0x15, 0x00,                         //    LOGICAL_MINIMUM (0)
    //    0x26, 0xff, 0x00,                   //     LOGICAL_MAXIMUM (0xff)  
    //    0x75, 0x08,                         //     REPORT_SIZE (8)
    //    0x95, 0x01,                         //     REPORT_COUNT (1)  
    //    0xb1, 0x02,                         //     FEATURE (Data,Var,Abs)

    //    ////����֧��HID_USAGE_HAPTIC_INTENSITYѹ��ǿ����������Ŀ�ѡ֧�֡�
    //    //0x05, 0x0E,                       //   Usage Page (Haptics)
    //    //0x09, 0x01,                       //   Usage (Simple Haptics Controller)
    //    //0xA1, 0x02,                       //   Collection (Logical)
    //    //0x09, 0x23,                       //     Usage (Intensity)
    //    //0x85, FAKE_REPORTID_CONFIG_PTP_HAPTICS_ID,      //     Report ID (9)
    //    //0x15, 0x00,                       //     Logical Minimum (0)
    //    //0x25, 0x64,                       //     Logical Maximum (100)
    //    //0x75, 0x08,                       //     Report Size (8)
    //    //0x95, 0x01,                       //     Report Count (1)
    //    //0xB1, 0x02,                       //     Feature (Data,Var,Abs)
    //    //0xC0,                             //   End Collection ()
    //0xc0,                               // END_COLLECTION
};


/////////���HID������, �������������ң��� ���֣�ˮƽ�����ʹ�ֱ������, X,Y�������ֵ
const unsigned char MouseReportDescriptor[] = {//��������ֻ��Ϊ�ϲ�ͻ�������ʹ�ã�ʵ��hid������Ӧ�����²�������ȡ������Ϊ׼
    ///
    0x05, 0x01, // USAGE_PAGE(Generic Desktop)
    0x09, 0x02, //   USAGE(Mouse)
    0xA1, 0x01, //   COLLECTION(APPlication)
    0x85, FAKE_REPORTID_MOUSE, //     ReportID(Mouse ReportID)  //��ʱռλ��;��ʵ��ʹ�ö�дReportʱ��Ҫ��ǰ��ȡhid������������ȷ����ȷ����ֵ
    0x09, 0x01, //   USAGE(Pointer)
        0xA1, 0x00, //     COLLECTION(Physical)
        0x05, 0x09, //     USAGE_PAGE(Button)
        0x19, 0x01, //     USAGE_MINIMUM(button 1)   Button ������ λ 0 ����� λ1 �Ҽ��� λ2 �м�
        0x29, 0x03, //     USAGE_MAXMUM(button 3)  //0x03����������갴������
        0x15, 0x00, //     LOGICAL_MINIMUM(0)
        0x25, 0x01, //     LOGICAL_MAXIMUM(1)
        0x75, 0x01, //     REPORT_SIZE(1)
        0x95, 0x03, //     REPORT_COUNT(3)  //0x03��갴������
        0x81, 0x02, //     INPUT(Data,Var,Abs)
        0x95, 0x05, //     REPORT_COUNT(5) //��Ҫ������ٸ�bitʹ�ü�����갴��������3��bitλ��1���ֽ�8bit
        0x81, 0x03, //     INPUT (Cnst,Var,Abs)////һ��pending��λ��input��Cnst����0x03
        0x05, 0x01, //     USAGE_PAGE(Generic Desktop)
        0x09, 0x30, //     USAGE(X)       X�ƶ�
        0x09, 0x31, //     USAGE(Y)       Y�ƶ�
        0x09, 0x38, //     USAGE(Wheel)   ��ֱ����
        0x15, 0x81, //     LOGICAL_MINIMUM(-127)
        0x25, 0x7F, //     LOGICAL_MAXIMUM(127)
        0x75, 0x08, //     REPORT_SIZE(8)
        0x95, 0x03, //     REPORT_COUNT(3)
        0x81, 0x06, //     INPUT(Data,Var, Rel) //X,Y,��ֱ�������������� ���ֵ

        //�±�ˮƽ����
        0x05, 0x0C, //     USAGE_PAGE (Consumer Devices)
        0x0A, 0x38, 0x02, // USAGE(AC Pan)
        0x15, 0x81, //       LOGICAL_MINIMUM(-127)
        0x25, 0x7F, //       LOGICAL_MAXIMUM(127)
        0x75, 0x08, //       REPORT_SIZE(8)
        0x95, 0x01, //       REPORT_COUNT(1)
        0x81, 0x06, //       INPUT(data,Var, Rel) //ˮƽ���֣����ֵ
        0xC0,       //       End Connection(PhySical)
    0xC0,       //     End Connection

};


CONST HID_DESCRIPTOR DefaultHidDescriptor = {
    0x09,   // length of HID descriptor
    0x21,   // descriptor type == HID  0x21
    0x0100, // hid spec release
    0x00,   // country code == Not Specified
    0x01,   // number of HID class descriptors
    { 0x22,   // descriptor type 
    sizeof(ParallelMode_PtpReportDescriptor) }  // MouseReportDescriptor//ParallelMode_PtpReportDescriptor/SingleFingerHybridMode_PtpReportDescriptor
};




NTSTATUS
AnalyzeHidReportDescriptor(PHID_MINI_DEV_EXTENSION pDevContext);

VOID MouseLikeTouchPad_parse_init(PHID_MINI_DEV_EXTENSION pDevContext);
VOID MouseLikeTouchPad_parse(PHID_MINI_DEV_EXTENSION pDevContext, PBYTE pReportBuffer, PULONG pReportLength);

ULONG runtimes_IOCTL_HID_READ_REPORT;
ULONG runtimes_IOCTL_IOCTL;
ULONG runtimes_ioReadCompletion;


VOID KdPrintDataFun(CHAR* pChars, PUCHAR DataBuffer, ULONG DataSize);


NTSTATUS SetRegisterMouseSensitivity(PHID_MINI_DEV_EXTENSION pMiniDevExt, ULONG ms_idx);

NTSTATUS GetRegisterMouseSensitivity(PHID_MINI_DEV_EXTENSION pMiniDevExt, ULONG* ms_idx);

void SetNextSensitivity(PHID_MINI_DEV_EXTENSION pDevContext);

