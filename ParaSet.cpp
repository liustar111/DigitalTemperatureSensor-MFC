#include "pch.h"
#include "Digitaltemperaturesensor.h"
#include "ParaSet.h"
#include "afxdialogex.h"

IMPLEMENT_DYNAMIC(ParaSet, CDialogEx)

ParaSet::ParaSet(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_ParaSet, pParent)
{
    m_strPort = _T("COM4");
    m_nBaud = 9600;
    m_nAddr = 1;
    m_nCalibrationTenths = 0;
}

ParaSet::~ParaSet() {}

void ParaSet::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(ParaSet, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_PARA_SAVE, &ParaSet::OnBnClickedBtnParaSave)
END_MESSAGE_MAP()

BOOL ParaSet::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetDlgItemText(IDC_COMBO_PARA_PORT, m_strPort);

    CString strBaud;
    strBaud.Format(_T("%d"), m_nBaud);
    SetDlgItemText(IDC_COMBO_PARA_BAUD, strBaud);

    SetDlgItemInt(IDC_EDIT_PARA_ADDR, m_nAddr);
    SetDlgItemInt(IDC_EDIT_DEVICE_NAME, m_nCalibrationTenths, TRUE);

    return TRUE;
}

void ParaSet::OnBnClickedBtnParaSave()
{
    GetDlgItemText(IDC_COMBO_PARA_PORT, m_strPort);
    m_strPort.Trim();
    if (m_strPort.IsEmpty())
    {
        MessageBox(_T("串口号不能为空。"), _T("参数错误"), MB_ICONERROR);
        return;
    }

    CString strBaud;
    GetDlgItemText(IDC_COMBO_PARA_BAUD, strBaud);
    m_nBaud = _ttoi(strBaud);
    if (m_nBaud != 2400 && m_nBaud != 9600 && m_nBaud != 38400)
    {
        MessageBox(_T("波特率只支持 2400、9600、38400。"), _T("参数错误"), MB_ICONERROR);
        return;
    }

    m_nAddr = GetDlgItemInt(IDC_EDIT_PARA_ADDR);
    if (m_nAddr < 1 || m_nAddr > 255)
    {
        MessageBox(_T("Modbus站号必须在 1 到 255 之间。"), _T("参数错误"), MB_ICONERROR);
        return;
    }

    BOOL bTranslated = FALSE;
    m_nCalibrationTenths = GetDlgItemInt(IDC_EDIT_DEVICE_NAME, &bTranslated, TRUE);
    if (!bTranslated || m_nCalibrationTenths < -32768 || m_nCalibrationTenths > 32767)
    {
        MessageBox(_T("温度校准系数必须是 -32768 到 32767 之间的整数，单位为 0.1℃。"), _T("参数错误"), MB_ICONERROR);
        return;
    }

    CDialogEx::OnOK();
}
