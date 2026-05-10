#include "pch.h"
#include "framework.h"
#include "Digitaltemperaturesensor.h"
#include "DigitaltemperaturesensorDlg.h"
#include "afxdialogex.h"
#include "HistData.h"
#include "ParaSet.h"
#include "CompSet.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static const double SENSOR_TEMP_MIN = -50.0;
static const double SENSOR_TEMP_MAX = 120.0;
static const int DEFAULT_COLLECT_INTERVAL_MS = 500;
static const int MIN_COLLECT_INTERVAL_MS = 200;
static const int DEFAULT_MAX_HISTORY_RECORDS = 5000;

static CString FormatWin32Error(DWORD dwError)
{
    if (dwError == ERROR_SUCCESS)
    {
        return _T("无系统错误信息");
    }

    LPTSTR pBuffer = nullptr;
    DWORD dwLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, dwError, 0, reinterpret_cast<LPTSTR>(&pBuffer), 0, nullptr);

    CString strMessage;
    if (dwLen > 0 && pBuffer != nullptr)
    {
        strMessage = pBuffer;
        strMessage.Trim();
        LocalFree(pBuffer);
    }
    else
    {
        strMessage.Format(_T("系统错误码 %lu"), dwError);
    }
    return strMessage;
}

static CString ReadIniString(LPCTSTR section, LPCTSTR key, LPCTSTR defaultValue, LPCTSTR path)
{
    TCHAR buffer[512] = { 0 };
    GetPrivateProfileString(section, key, defaultValue, buffer, _countof(buffer), path);
    return CString(buffer);
}

static double ReadIniDouble(LPCTSTR section, LPCTSTR key, double defaultValue, LPCTSTR path)
{
    CString strDefault;
    strDefault.Format(_T("%.1f"), defaultValue);
    CString strValue = ReadIniString(section, key, strDefault, path);
    strValue.Trim();
    if (strValue.IsEmpty())
    {
        return defaultValue;
    }
    return _tstof(strValue);
}

static int ClampInt(int value, int minValue, int maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

void CDigitaltemperaturesensorDlg::OnBnClickedSerialPort()
{
    if (!m_bSerialOpened)
    {
        int nConfigIndex = GetSelectedDeviceConfigIndex();
        if (nConfigIndex < 0)
        {
            MessageBox(_T("请先选择一个设备。"), _T("提示"), MB_ICONWARNING);
            return;
        }

        DeviceConfig& cfg = m_deviceConfigs[nConfigIndex];
        if (m_serialPort.Open(cfg.portName, cfg.baudRate, static_cast<BYTE>(cfg.parity),
            static_cast<BYTE>(cfg.dataBits), static_cast<BYTE>(cfg.stopBits)))
        {
            m_bSerialOpened = true;
            if (GetDlgItem(IDC_SerialPort)) GetDlgItem(IDC_SerialPort)->SetWindowText(_T("关闭串口"));
            if (GetDlgItem(IDC_Collection)) GetDlgItem(IDC_Collection)->EnableWindow(TRUE);

            CString strMsg;
            strMsg.Format(_T("串口 %s 打开成功，波特率 %d，格式 %dN%d。"),
                (LPCTSTR)cfg.portName, cfg.baudRate, cfg.dataBits, cfg.stopBits == TWOSTOPBITS ? 2 : 1);
            SetStatusText(strMsg);
            AppendAppLog(strMsg);
            MessageBox(strMsg, _T("提示"), MB_ICONINFORMATION);
        }
        else
        {
            CString strErr = BuildSerialErrorMessage(_T("打开串口失败"));
            SetStatusText(strErr);
            AppendAppLog(strErr);
            MessageBox(strErr, _T("错误"), MB_ICONERROR);
        }
    }
    else
    {
        if (m_bCollecting) OnBnClickedCollection();

        m_serialPort.Close();
        m_bSerialOpened = false;

        if (GetDlgItem(IDC_SerialPort)) GetDlgItem(IDC_SerialPort)->SetWindowText(_T("打开串口"));
        if (GetDlgItem(IDC_Collection)) GetDlgItem(IDC_Collection)->EnableWindow(FALSE);
        SetDlgItemText(IDC_EDIT_SHOW, _T(""));
        SetStatusText(_T("串口已关闭。"));
        AppendAppLog(_T("串口已关闭"));
        MessageBox(_T("串口已安全关闭。"), _T("提示"), MB_ICONINFORMATION);
    }
}

void CDigitaltemperaturesensorDlg::OnBnClickedCollection()
{
    if (!m_bCollecting)
    {
        if (!m_bSerialOpened)
        {
            MessageBox(_T("请先打开串口。"), _T("警告"), MB_ICONWARNING);
            return;
        }
        if (GetSelectedDeviceConfigIndex() < 0)
        {
            MessageBox(_T("请先选择一个设备。"), _T("警告"), MB_ICONWARNING);
            return;
        }

        m_bCollecting = true;
        m_bWaitingResponse = false;
        if (GetDlgItem(IDC_Collection)) GetDlgItem(IDC_Collection)->SetWindowText(_T("停止采集"));
        if (GetDlgItem(IDC_SerialPort)) GetDlgItem(IDC_SerialPort)->EnableWindow(FALSE);
        m_nTimerID = SetTimer(1, static_cast<UINT>(m_nCollectIntervalMs), NULL);
        SetStatusText(_T("正在采集温度。"));
        AppendAppLog(_T("开始采集温度"));
    }
    else
    {
        m_bCollecting = false;
        m_bWaitingResponse = false;
        if (GetDlgItem(IDC_Collection)) GetDlgItem(IDC_Collection)->SetWindowText(_T("开始采集"));
        if (GetDlgItem(IDC_SerialPort)) GetDlgItem(IDC_SerialPort)->EnableWindow(TRUE);

        if (m_nTimerID != 0)
        {
            KillTimer(m_nTimerID);
            m_nTimerID = 0;
        }
        SetStatusText(_T("采集已停止。"));
        AppendAppLog(_T("停止采集温度"));
    }
}

void CDigitaltemperaturesensorDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == m_nTimerID && m_bCollecting)
    {
        if (m_bWaitingResponse)
        {
            DeviceConfig* pPendingDevice = nullptr;
            if (m_nPendingConfigIndex >= 0 && m_nPendingConfigIndex < static_cast<int>(m_deviceConfigs.size()))
            {
                pPendingDevice = &m_deviceConfigs[m_nPendingConfigIndex];
            }

            BYTE recvBuf[256] = { 0 };
            DWORD recvLen = m_serialPort.Read(recvBuf, 256, 0);
            if (recvLen > 0 && pPendingDevice != nullptr)
            {
                float temp = 0.0f;
                CString strError;
                if (!ParseTemperatureResponse(recvBuf, recvLen, m_pendingAddr, temp, strError))
                {
                    HandleCommunicationFailure(pPendingDevice, strError);
                }
                else
                {
                    ClearCommunicationAlarm(*pPendingDevice);

                    CString strStatus;
                    if (!IsTemperatureInSupportedRange(temp))
                    {
                        strStatus.Format(_T("温度异常：%.1f℃ 超出文档范围 -50℃~120℃"), temp);
                        if (!pPendingDevice->temperatureAlarmActive)
                        {
                            pPendingDevice->temperatureAlarmActive = true;
                            AppendAppLog(strStatus);
                        }
                    }
                    else
                    {
                        strStatus = UpdateTemperatureAlarm(*pPendingDevice, temp);
                    }

                    AddHistoryRecord(pPendingDevice, true, temp, strStatus);

                    CString strDisplay;
                    strDisplay.Format(_T("[%s] %s 站号%d 温度：%.1f ℃ 状态：%s\r\n"),
                        (LPCTSTR)CTime::GetCurrentTime().Format(_T("%Y-%m-%d %H:%M:%S")),
                        (LPCTSTR)pPendingDevice->deviceName, m_pendingAddr, temp, (LPCTSTR)strStatus);

                    CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_SHOW);
                    if (pEdit)
                    {
                        int nLength = pEdit->GetWindowTextLength();
                        pEdit->SetSel(nLength, nLength);
                        pEdit->ReplaceSel(strDisplay);
                    }
                    SetStatusText(strStatus);
                }
            }
            else if (GetTickCount() - m_dwRequestTick >= static_cast<DWORD>(MIN_COLLECT_INTERVAL_MS))
            {
                HandleCommunicationFailure(pPendingDevice, _T("传感器无响应或读取超时"));
            }
            else
            {
                CDialogEx::OnTimer(nIDEvent);
                return;
            }

            m_bWaitingResponse = false;
            m_pendingAddr = 0;
            m_nPendingConfigIndex = -1;
        }

        int nConfigIndex = GetSelectedDeviceConfigIndex();
        if (nConfigIndex < 0)
        {
            HandleCommunicationFailure(nullptr, _T("未选择设备"));
            CDialogEx::OnTimer(nIDEvent);
            return;
        }

        DeviceConfig& cfg = m_deviceConfigs[nConfigIndex];
        BYTE targetAddr = cfg.slaveAddr;

        BYTE cmd[8] = { targetAddr, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 };
        WORD crc = CRC16(cmd, 6);
        cmd[6] = LOBYTE(crc);
        cmd[7] = HIBYTE(crc);

        m_serialPort.Purge();
        if (!m_serialPort.Write(cmd, 8))
        {
            HandleCommunicationFailure(&cfg, BuildSerialErrorMessage(_T("发送读取命令失败")));
            CDialogEx::OnTimer(nIDEvent);
            return;
        }

        m_bWaitingResponse = true;
        m_pendingAddr = targetAddr;
        m_nPendingConfigIndex = nConfigIndex;
        m_dwRequestTick = GetTickCount();
    }
    CDialogEx::OnTimer(nIDEvent);
}
void CDigitaltemperaturesensorDlg::OnBnClickedhistory()
{
    HistData log;
    log.m_pRawData = &m_historyData;
    log.DoModal();
}

void CDigitaltemperaturesensorDlg::OnBnClickedParameterset()
{
    if (m_bCollecting)
    {
        MessageBox(_T("请先停止采集，再修改通讯或设备参数。"), _T("提示"), MB_ICONWARNING);
        return;
    }

    int nConfigIndex = GetSelectedDeviceConfigIndex();
    if (nConfigIndex < 0)
    {
        MessageBox(_T("请先选中一个设备。"), _T("提示"), MB_ICONWARNING);
        return;
    }

    DeviceConfig oldConfig = m_deviceConfigs[nConfigIndex];
    ParaSet dlg;
    dlg.m_strPort = oldConfig.portName;
    dlg.m_nBaud = oldConfig.baudRate;
    dlg.m_nAddr = oldConfig.slaveAddr;
    dlg.m_nCalibrationTenths = oldConfig.calibrationTenths;

    if (dlg.DoModal() == IDOK)
    {
        DeviceConfig& cfg = m_deviceConfigs[nConfigIndex];
        cfg.portName = dlg.m_strPort;
        cfg.baudRate = dlg.m_nBaud;
        cfg.slaveAddr = static_cast<BYTE>(dlg.m_nAddr);
        cfg.calibrationTenths = dlg.m_nCalibrationTenths;
        SaveConfig();

        CString strInfo = _T("软件本地配置已保存。COM口、软件波特率会在下次打开串口时生效。");
        if (m_bSerialOpened)
        {
            int nRet = MessageBox(_T("软件本地配置已保存。是否同时把站号、波特率寄存器和温度校准系数写入当前传感器？"),
                _T("写入传感器确认"), MB_YESNO | MB_ICONQUESTION);
            if (nRet == IDYES)
            {
                CString strReport;
                bool bWriteOk = WriteSensorSettings(oldConfig, cfg, strReport);
                strInfo += _T("\r\n") + strReport;
                AppendAppLog(strReport);
                MessageBox(strInfo, bWriteOk ? _T("参数保存完成") : _T("参数部分失败"), bWriteOk ? MB_ICONINFORMATION : MB_ICONWARNING);
            }
            else
            {
                strInfo += _T("\r\n未写入传感器寄存器。");
                MessageBox(strInfo, _T("参数保存完成"), MB_ICONINFORMATION);
            }
        }
        else
        {
            strInfo += _T("\r\n串口未打开，未写入传感器寄存器。");
            MessageBox(strInfo, _T("参数保存完成"), MB_ICONINFORMATION);
        }

        RebuildDeviceList(nConfigIndex);
        SetStatusText(_T("参数配置已保存。"));
        AppendAppLog(_T("参数配置已保存"));
    }
}

void CDigitaltemperaturesensorDlg::OnBnClickedBtnAddDevice()
{
    DeviceConfig newCfg;
    newCfg.deviceName.Format(_T("新增设备_%d"), static_cast<int>(m_deviceConfigs.size()) + 1);
    newCfg.portName = _T("COM4");
    newCfg.baudRate = 9600;
    newCfg.slaveAddr = static_cast<BYTE>(ClampInt(static_cast<int>(m_deviceConfigs.size()) + 1, 1, 255));

    m_deviceConfigs.push_back(newCfg);
    RebuildDeviceList(static_cast<int>(m_deviceConfigs.size()) - 1);
    SaveConfig();
    AppendAppLog(_T("新增设备配置"));
}

void CDigitaltemperaturesensorDlg::OnBnClickedBtnDelDevice()
{
    int nConfigIndex = GetSelectedDeviceConfigIndex();
    if (nConfigIndex < 0)
    {
        MessageBox(_T("请先选择要删除的设备。"), _T("提示"), MB_ICONWARNING);
        return;
    }

    if (MessageBox(_T("确定删除当前设备配置？"), _T("确认"), MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        m_deviceConfigs.erase(m_deviceConfigs.begin() + nConfigIndex);
        if (m_deviceConfigs.empty())
        {
            LoadDefaultDevice();
        }
        RebuildDeviceList(min(nConfigIndex, static_cast<int>(m_deviceConfigs.size()) - 1));
        SaveConfig();
        AppendAppLog(_T("删除设备配置"));
    }
}

void CDigitaltemperaturesensorDlg::OnBnClickedBtnModDevice()
{
    CString strBtn;
    GetDlgItemText(IDC_BTN_MOD_DEVICE, strBtn);
    static int s_nEditConfigIndex = -1;

    if (strBtn == _T("修改"))
    {
        s_nEditConfigIndex = GetSelectedDeviceConfigIndex();
        if (s_nEditConfigIndex < 0)
        {
            MessageBox(_T("请先选择要修改名称的设备。"), _T("提示"), MB_ICONWARNING);
            return;
        }

        SetDlgItemText(IDC_EDIT_DEVICE_NAME, m_deviceConfigs[s_nEditConfigIndex].deviceName);
        if (GetDlgItem(IDC_EDIT_DEVICE_NAME)) GetDlgItem(IDC_EDIT_DEVICE_NAME)->ShowWindow(SW_SHOW);
        if (GetDlgItem(IDC_BTN_MOD_DEVICE)) GetDlgItem(IDC_BTN_MOD_DEVICE)->SetWindowText(_T("保存"));
        m_deviceList.EnableWindow(FALSE);
    }
    else
    {
        CString strNew;
        GetDlgItemText(IDC_EDIT_DEVICE_NAME, strNew);
        strNew.Trim();
        if (strNew.IsEmpty())
        {
            MessageBox(_T("设备名称不能为空。"), _T("提示"), MB_ICONWARNING);
            return;
        }
        if (s_nEditConfigIndex >= 0 && s_nEditConfigIndex < static_cast<int>(m_deviceConfigs.size()))
        {
            m_deviceConfigs[s_nEditConfigIndex].deviceName = strNew;
            RebuildDeviceList(s_nEditConfigIndex);
            SaveConfig();
            AppendAppLog(_T("修改设备名称"));
        }

        if (GetDlgItem(IDC_EDIT_DEVICE_NAME)) GetDlgItem(IDC_EDIT_DEVICE_NAME)->ShowWindow(SW_HIDE);
        m_deviceList.EnableWindow(TRUE);
        if (GetDlgItem(IDC_BTN_MOD_DEVICE)) GetDlgItem(IDC_BTN_MOD_DEVICE)->SetWindowText(_T("修改"));
        s_nEditConfigIndex = -1;
    }
}

WORD CDigitaltemperaturesensorDlg::CRC16(const BYTE* pData, int iLen)
{
    if (pData == nullptr || iLen <= 0) return 0;

    WORD wCRC = 0xFFFF;
    for (int i = 0; i < iLen; i++)
    {
        wCRC ^= pData[i];
        for (int j = 0; j < 8; j++)
        {
            if (wCRC & 0x0001)
            {
                wCRC >>= 1;
                wCRC ^= 0xA001;
            }
            else
            {
                wCRC >>= 1;
            }
        }
    }
    return wCRC;
}

float CDigitaltemperaturesensorDlg::ParseTemperature(BYTE* pRecv, DWORD dwLen)
{
    float temp = 0.0f;
    CString error;
    if (pRecv != nullptr && dwLen > 0 && ParseTemperatureResponse(pRecv, dwLen, pRecv[0], temp, error))
    {
        return temp;
    }
    return -999.0f;
}

bool CDigitaltemperaturesensorDlg::ParseTemperatureResponse(const BYTE* pRecv, DWORD dwLen, BYTE expectedAddr, float& temperature, CString& errorMessage)
{
    if (pRecv == nullptr)
    {
        errorMessage = _T("响应缓冲区为空");
        return false;
    }
    if (dwLen < 7)
    {
        errorMessage.Format(_T("响应长度不足：%lu"), dwLen);
        return false;
    }
    if (pRecv[0] != expectedAddr)
    {
        errorMessage.Format(_T("响应站号不匹配：期望%d，实际%d"), expectedAddr, pRecv[0]);
        return false;
    }
    if (pRecv[1] != 0x03)
    {
        errorMessage.Format(_T("响应功能码错误：0x%02X"), pRecv[1]);
        return false;
    }
    if (pRecv[2] != 0x02)
    {
        errorMessage.Format(_T("温度响应字节数错误：%d"), pRecv[2]);
        return false;
    }

    DWORD frameLen = 3 + pRecv[2] + 2;
    if (dwLen < frameLen)
    {
        errorMessage.Format(_T("响应帧不完整：收到%lu，期望%lu"), dwLen, frameLen);
        return false;
    }

    WORD crcRecv = pRecv[frameLen - 2] | (pRecv[frameLen - 1] << 8);
    WORD crcCalc = CRC16(pRecv, static_cast<int>(frameLen - 2));
    if (crcRecv != crcCalc)
    {
        errorMessage.Format(_T("CRC校验失败：收到0x%04X，计算0x%04X"), crcRecv, crcCalc);
        return false;
    }

    short raw = static_cast<short>((pRecv[3] << 8) | pRecv[4]);
    // TODO: The PDF's negative examples conflict with its 0.1C resolution and positive /10 example; keep /10 until hardware confirms. 
    temperature = raw / 10.0f;
    return true;
}

bool CDigitaltemperaturesensorDlg::WriteSingleRegister(BYTE deviceAddr, WORD registerAddr, short value, CString& errorMessage)
{
    if (!m_bSerialOpened || !m_serialPort.IsOpen())
    {
        errorMessage = _T("串口未打开，无法写入传感器寄存器");
        return false;
    }

    WORD rawValue = static_cast<WORD>(value);
    BYTE cmd[8] = {
        deviceAddr,
        0x06,
        HIBYTE(registerAddr),
        LOBYTE(registerAddr),
        HIBYTE(rawValue),
        LOBYTE(rawValue),
        0x00,
        0x00
    };
    WORD crc = CRC16(cmd, 6);
    cmd[6] = LOBYTE(crc);
    cmd[7] = HIBYTE(crc);

    m_serialPort.Purge();
    if (!m_serialPort.Write(cmd, 8))
    {
        errorMessage = BuildSerialErrorMessage(_T("发送写寄存器命令失败"));
        return false;
    }

    BYTE recvBuf[16] = { 0 };
    DWORD recvLen = m_serialPort.Read(recvBuf, 8, 220);
    if (recvLen < 8)
    {
        errorMessage.Format(_T("写寄存器无完整应答：收到%lu字节"), recvLen);
        return false;
    }

    WORD respCrc = recvBuf[6] | (recvBuf[7] << 8);
    WORD calcCrc = CRC16(recvBuf, 6);
    if (respCrc != calcCrc)
    {
        errorMessage.Format(_T("写寄存器应答CRC失败：收到0x%04X，计算0x%04X"), respCrc, calcCrc);
        return false;
    }
    if (recvBuf[0] != deviceAddr || recvBuf[1] != 0x06 || recvBuf[2] != HIBYTE(registerAddr) ||
        recvBuf[3] != LOBYTE(registerAddr) || recvBuf[4] != HIBYTE(rawValue) || recvBuf[5] != LOBYTE(rawValue))
    {
        errorMessage = _T("写寄存器应答内容与请求不一致");
        return false;
    }

    return true;
}

BEGIN_MESSAGE_MAP(CDigitaltemperaturesensorDlg, CDialogEx)
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_history, &CDigitaltemperaturesensorDlg::OnBnClickedhistory)
    ON_BN_CLICKED(IDC_ParameterSet, &CDigitaltemperaturesensorDlg::OnBnClickedParameterset)
    ON_BN_CLICKED(IDC_CompanyInfo, &CDigitaltemperaturesensorDlg::OnBnClickedCompanyinfo)
    ON_BN_CLICKED(IDC_Collection, &CDigitaltemperaturesensorDlg::OnBnClickedCollection)
    ON_BN_CLICKED(IDC_SerialPort, &CDigitaltemperaturesensorDlg::OnBnClickedSerialPort)
    ON_BN_CLICKED(IDC_BTN_ADD_DEVICE, &CDigitaltemperaturesensorDlg::OnBnClickedBtnAddDevice)
    ON_BN_CLICKED(IDC_BTN_MOD_DEVICE, &CDigitaltemperaturesensorDlg::OnBnClickedBtnModDevice)
    ON_BN_CLICKED(IDC_BTN_DEL_DEVICE, &CDigitaltemperaturesensorDlg::OnBnClickedBtnDelDevice)
END_MESSAGE_MAP()

BOOL CDigitaltemperaturesensorDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    if (GetDlgItem(IDC_Collection)) GetDlgItem(IDC_Collection)->EnableWindow(FALSE);

    LoadConfig();
    RebuildDeviceList(0);
    SetStatusText(_T("软件已启动，配置已加载。"));
    AppendAppLog(_T("主窗口初始化完成"));

    return TRUE;
}

CDigitaltemperaturesensorDlg::CDigitaltemperaturesensorDlg(CWnd* pParent)
    : CDialogEx(IDD_DIGITALTEMPERATURESENSOR_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_bSerialOpened = false;
    m_bCollecting = false;
    m_nTimerID = 0;
    m_bWaitingResponse = false;
    m_pendingAddr = 0;
    m_nPendingConfigIndex = -1;
    m_dwRequestTick = 0;
    m_nCurrentDeviceIndex = -1;
    m_nCollectIntervalMs = DEFAULT_COLLECT_INTERVAL_MS;
    m_nMaxHistoryRecords = DEFAULT_MAX_HISTORY_RECORDS;
}

void CDigitaltemperaturesensorDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_DEVICES, m_deviceList);
}

void CDigitaltemperaturesensorDlg::OnDestroy()
{
    SaveConfig();
    if (m_nTimerID != 0)
    {
        KillTimer(m_nTimerID);
        m_nTimerID = 0;
    }
    if (m_serialPort.IsOpen())
    {
        m_serialPort.Close();
        AppendAppLog(_T("程序退出时关闭串口"));
    }
    CDialogEx::OnDestroy();
}

void CDigitaltemperaturesensorDlg::OnBnClickedCompanyinfo()
{
    AppendAppLog(_T("打开公司信息管理窗口"));
    CompSet dlg;
    dlg.DoModal();
}

void CDigitaltemperaturesensorDlg::LoadDefaultDevice()
{
    DeviceConfig cfg;
    cfg.deviceName = _T("HSTL-200R/HSTL-200UR 数字量温度传感器 (-50℃~120℃)");
    cfg.portName = _T("COM4");
    cfg.baudRate = 9600;
    cfg.slaveAddr = 1;
    cfg.parity = NOPARITY;
    cfg.dataBits = 8;
    cfg.stopBits = ONESTOPBIT;
    cfg.calibrationTenths = 0;
    cfg.alarmLow = SENSOR_TEMP_MIN;
    cfg.alarmHigh = SENSOR_TEMP_MAX;
    m_deviceConfigs.push_back(cfg);
}

void CDigitaltemperaturesensorDlg::LoadConfig()
{
    m_deviceConfigs.clear();

    CString path = GetAppConfigPath();
    m_nCollectIntervalMs = GetPrivateProfileInt(_T("General"), _T("CollectIntervalMs"), DEFAULT_COLLECT_INTERVAL_MS, path);
    if (m_nCollectIntervalMs < MIN_COLLECT_INTERVAL_MS)
    {
        m_nCollectIntervalMs = DEFAULT_COLLECT_INTERVAL_MS;
    }
    m_nMaxHistoryRecords = GetPrivateProfileInt(_T("General"), _T("MaxHistoryRecords"), DEFAULT_MAX_HISTORY_RECORDS, path);
    if (m_nMaxHistoryRecords < 100)
    {
        m_nMaxHistoryRecords = DEFAULT_MAX_HISTORY_RECORDS;
    }

    int nDeviceCount = GetPrivateProfileInt(_T("General"), _T("DeviceCount"), 0, path);
    for (int i = 0; i < nDeviceCount; ++i)
    {
        CString section;
        section.Format(_T("Device%d"), i);

        DeviceConfig cfg;
        cfg.deviceName = ReadIniString(section, _T("Name"), _T(""), path);
        cfg.deviceName.Trim();
        if (cfg.deviceName.IsEmpty())
        {
            continue;
        }
        cfg.portName = ReadIniString(section, _T("Port"), _T("COM4"), path);
        cfg.portName.Trim();
        if (cfg.portName.IsEmpty()) cfg.portName = _T("COM4");
        cfg.baudRate = GetPrivateProfileInt(section, _T("BaudRate"), 9600, path);
        if (cfg.baudRate != 2400 && cfg.baudRate != 9600 && cfg.baudRate != 38400) cfg.baudRate = 9600;
        cfg.slaveAddr = static_cast<BYTE>(ClampInt(GetPrivateProfileInt(section, _T("SlaveAddr"), 1, path), 1, 255));
        cfg.parity = GetPrivateProfileInt(section, _T("Parity"), NOPARITY, path);
        cfg.dataBits = ClampInt(GetPrivateProfileInt(section, _T("DataBits"), 8, path), 5, 8);
        cfg.stopBits = GetPrivateProfileInt(section, _T("StopBits"), ONESTOPBIT, path);
        if (cfg.stopBits != ONESTOPBIT && cfg.stopBits != TWOSTOPBITS) cfg.stopBits = ONESTOPBIT;
        cfg.calibrationTenths = ClampInt(GetPrivateProfileInt(section, _T("CalibrationTenths"), 0, path), -32768, 32767);
        cfg.alarmLow = ReadIniDouble(section, _T("AlarmLow"), SENSOR_TEMP_MIN, path);
        cfg.alarmHigh = ReadIniDouble(section, _T("AlarmHigh"), SENSOR_TEMP_MAX, path);
        if (cfg.alarmLow >= cfg.alarmHigh)
        {
            cfg.alarmLow = SENSOR_TEMP_MIN;
            cfg.alarmHigh = SENSOR_TEMP_MAX;
        }
        m_deviceConfigs.push_back(cfg);
    }

    if (m_deviceConfigs.empty())
    {
        LoadDefaultDevice();
        SaveConfig();
    }
}

void CDigitaltemperaturesensorDlg::SaveConfig() const
{
    CString path = GetAppConfigPath();
    CString strValue;

    strValue.Format(_T("%d"), static_cast<int>(m_deviceConfigs.size()));
    WritePrivateProfileString(_T("General"), _T("DeviceCount"), strValue, path);
    strValue.Format(_T("%d"), m_nCollectIntervalMs);
    WritePrivateProfileString(_T("General"), _T("CollectIntervalMs"), strValue, path);
    strValue.Format(_T("%d"), m_nMaxHistoryRecords);
    WritePrivateProfileString(_T("General"), _T("MaxHistoryRecords"), strValue, path);

    for (size_t i = 0; i < m_deviceConfigs.size(); ++i)
    {
        CString section;
        section.Format(_T("Device%d"), static_cast<int>(i));
        const DeviceConfig& cfg = m_deviceConfigs[i];

        WritePrivateProfileString(section, _T("Name"), cfg.deviceName, path);
        WritePrivateProfileString(section, _T("Port"), cfg.portName, path);
        strValue.Format(_T("%d"), cfg.baudRate);
        WritePrivateProfileString(section, _T("BaudRate"), strValue, path);
        strValue.Format(_T("%d"), cfg.slaveAddr);
        WritePrivateProfileString(section, _T("SlaveAddr"), strValue, path);
        strValue.Format(_T("%d"), cfg.parity);
        WritePrivateProfileString(section, _T("Parity"), strValue, path);
        strValue.Format(_T("%d"), cfg.dataBits);
        WritePrivateProfileString(section, _T("DataBits"), strValue, path);
        strValue.Format(_T("%d"), cfg.stopBits);
        WritePrivateProfileString(section, _T("StopBits"), strValue, path);
        strValue.Format(_T("%d"), cfg.calibrationTenths);
        WritePrivateProfileString(section, _T("CalibrationTenths"), strValue, path);
        strValue.Format(_T("%.1f"), cfg.alarmLow);
        WritePrivateProfileString(section, _T("AlarmLow"), strValue, path);
        strValue.Format(_T("%.1f"), cfg.alarmHigh);
        WritePrivateProfileString(section, _T("AlarmHigh"), strValue, path);
    }
}

void CDigitaltemperaturesensorDlg::RebuildDeviceList(int nSelectConfigIndex)
{
    m_deviceList.ResetContent();
    int nListSelection = LB_ERR;

    for (size_t i = 0; i < m_deviceConfigs.size(); ++i)
    {
        const DeviceConfig& cfg = m_deviceConfigs[i];
        CString strDisplay;
        strDisplay.Format(_T("%s (站号%d)"), (LPCTSTR)cfg.deviceName, cfg.slaveAddr);
        int nItem = m_deviceList.AddString(strDisplay);
        if (nItem != LB_ERR)
        {
            m_deviceList.SetItemData(nItem, static_cast<DWORD_PTR>(i));
            if (static_cast<int>(i) == nSelectConfigIndex)
            {
                nListSelection = nItem;
            }
        }
    }

    if (nListSelection == LB_ERR && m_deviceList.GetCount() > 0)
    {
        nListSelection = 0;
    }
    if (nListSelection != LB_ERR)
    {
        m_deviceList.SetCurSel(nListSelection);
        m_nCurrentDeviceIndex = GetSelectedDeviceConfigIndex();
    }
    else
    {
        m_nCurrentDeviceIndex = -1;
    }
}

int CDigitaltemperaturesensorDlg::GetSelectedDeviceConfigIndex()
{
    int nSel = m_deviceList.GetCurSel();
    if (nSel == LB_ERR)
    {
        return -1;
    }

    DWORD_PTR data = m_deviceList.GetItemData(nSel);
    if (data == static_cast<DWORD_PTR>(LB_ERR) || data >= m_deviceConfigs.size())
    {
        return -1;
    }
    return static_cast<int>(data);
}

DeviceConfig* CDigitaltemperaturesensorDlg::GetSelectedDeviceConfig()
{
    int nConfigIndex = GetSelectedDeviceConfigIndex();
    if (nConfigIndex < 0 || nConfigIndex >= static_cast<int>(m_deviceConfigs.size()))
    {
        return nullptr;
    }
    return &m_deviceConfigs[nConfigIndex];
}

CString CDigitaltemperaturesensorDlg::BuildSerialErrorMessage(LPCTSTR action) const
{
    CString strMsg;
    strMsg.Format(_T("%s：%s"), action, (LPCTSTR)FormatWin32Error(m_serialPort.GetLastErrorCode()));
    return strMsg;
}

void CDigitaltemperaturesensorDlg::SetStatusText(LPCTSTR text)
{
    if (GetDlgItem(IDC_EDIT3))
    {
        SetDlgItemText(IDC_EDIT3, text);
    }
}

void CDigitaltemperaturesensorDlg::AddHistoryRecord(const DeviceConfig* pDevice, bool hasTemperature, double temperature, LPCTSTR status)
{
    TempRecord record;
    record.time = CTime::GetCurrentTime();
    record.timeStr = record.time.Format(_T("%H:%M:%S"));
    record.hasTemperature = hasTemperature;
    record.temperature = temperature;
    record.status = status;
    if (pDevice != nullptr)
    {
        record.deviceName = pDevice->deviceName;
        record.deviceAddr = pDevice->slaveAddr;
    }
    else
    {
        record.deviceName = _T("未选择设备");
        record.deviceAddr = 0;
    }

    m_historyData.push_back(record);
    TrimHistory();
}

void CDigitaltemperaturesensorDlg::TrimHistory()
{
    if (m_nMaxHistoryRecords <= 0) return;
    if (m_historyData.size() > static_cast<size_t>(m_nMaxHistoryRecords))
    {
        size_t nRemove = m_historyData.size() - static_cast<size_t>(m_nMaxHistoryRecords);
        m_historyData.erase(m_historyData.begin(), m_historyData.begin() + nRemove);
    }
}

bool CDigitaltemperaturesensorDlg::IsTemperatureInSupportedRange(double temperature) const
{
    return temperature >= SENSOR_TEMP_MIN && temperature <= SENSOR_TEMP_MAX;
}

bool CDigitaltemperaturesensorDlg::TryBaudRateToRegister(int baudRate, WORD& registerValue) const
{
    switch (baudRate)
    {
    case 2400:
        registerValue = 0;
        return true;
    case 9600:
        registerValue = 1;
        return true;
    case 38400:
        registerValue = 2;
        return true;
    default:
        return false;
    }
}

bool CDigitaltemperaturesensorDlg::WriteSensorSettings(const DeviceConfig& oldConfig, const DeviceConfig& newConfig, CString& report)
{
    bool bAllOk = true;
    BYTE currentAddr = oldConfig.slaveAddr;
    CString line;
    CString error;

    report = _T("传感器寄存器写入结果：");

    if (oldConfig.slaveAddr != newConfig.slaveAddr)
    {
        if (WriteSingleRegister(currentAddr, 0x0013, static_cast<short>(newConfig.slaveAddr), error))
        {
            line.Format(_T("\r\n- 0013H 设备地址：成功，%d -> %d"), oldConfig.slaveAddr, newConfig.slaveAddr);
            currentAddr = newConfig.slaveAddr;
        }
        else
        {
            line.Format(_T("\r\n- 0013H 设备地址：失败，%s"), (LPCTSTR)error);
            bAllOk = false;
        }
        report += line;
    }

    if (oldConfig.baudRate != newConfig.baudRate)
    {
        WORD baudReg = 0;
        if (!TryBaudRateToRegister(newConfig.baudRate, baudReg))
        {
            line.Format(_T("\r\n- 0014H 波特率：失败，不支持的波特率 %d"), newConfig.baudRate);
            bAllOk = false;
        }
        else if (WriteSingleRegister(currentAddr, 0x0014, static_cast<short>(baudReg), error))
        {
            line.Format(_T("\r\n- 0014H 波特率：成功，%d -> %d。请重新打开串口。"), oldConfig.baudRate, newConfig.baudRate);
            m_serialPort.Close();
            m_bSerialOpened = false;
            if (GetDlgItem(IDC_SerialPort)) GetDlgItem(IDC_SerialPort)->SetWindowText(_T("打开串口"));
            if (GetDlgItem(IDC_Collection)) GetDlgItem(IDC_Collection)->EnableWindow(FALSE);
        }
        else
        {
            line.Format(_T("\r\n- 0014H 波特率：失败，%s"), (LPCTSTR)error);
            bAllOk = false;
        }
        report += line;
    }

    if (oldConfig.calibrationTenths != newConfig.calibrationTenths)
    {
        if (WriteSingleRegister(currentAddr, 0x0015, static_cast<short>(newConfig.calibrationTenths), error))
        {
            line.Format(_T("\r\n- 0015H 温度校准系数：成功，%d -> %d"), oldConfig.calibrationTenths, newConfig.calibrationTenths);
        }
        else
        {
            line.Format(_T("\r\n- 0015H 温度校准系数：失败，%s"), (LPCTSTR)error);
            bAllOk = false;
        }
        report += line;
    }

    if (oldConfig.slaveAddr == newConfig.slaveAddr && oldConfig.baudRate == newConfig.baudRate &&
        oldConfig.calibrationTenths == newConfig.calibrationTenths)
    {
        report += _T("\r\n- 没有需要写入传感器寄存器的变更。");
    }

    return bAllOk;
}

void CDigitaltemperaturesensorDlg::HandleCommunicationFailure(DeviceConfig* pDevice, LPCTSTR reason)
{
    CString strStatus;
    strStatus.Format(_T("通信异常：%s"), reason);
    SetStatusText(strStatus);

    if (pDevice != nullptr)
    {
        pDevice->commFailCount++;
        if (!pDevice->commAlarmActive)
        {
            pDevice->commAlarmActive = true;
            AddHistoryRecord(pDevice, false, 0.0, strStatus);
            AppendAppLog(strStatus);
        }
    }
    else
    {
        AppendAppLog(strStatus);
    }
}

void CDigitaltemperaturesensorDlg::ClearCommunicationAlarm(DeviceConfig& device)
{
    if (device.commAlarmActive)
    {
        CString strLog;
        strLog.Format(_T("通信恢复：%s 站号%d"), (LPCTSTR)device.deviceName, device.slaveAddr);
        AppendAppLog(strLog);
    }
    device.commAlarmActive = false;
    device.commFailCount = 0;
}

CString CDigitaltemperaturesensorDlg::UpdateTemperatureAlarm(DeviceConfig& device, double temperature)
{
    CString status = _T("正常");
    if (temperature < device.alarmLow)
    {
        status.Format(_T("低温报警：%.1f℃ < %.1f℃"), temperature, device.alarmLow);
    }
    else if (temperature > device.alarmHigh)
    {
        status.Format(_T("高温报警：%.1f℃ > %.1f℃"), temperature, device.alarmHigh);
    }

    if (status != _T("正常"))
    {
        if (!device.temperatureAlarmActive)
        {
            device.temperatureAlarmActive = true;
            CString strLog;
            strLog.Format(_T("%s，设备：%s，站号%d"), (LPCTSTR)status, (LPCTSTR)device.deviceName, device.slaveAddr);
            AppendAppLog(strLog);
        }
    }
    else if (device.temperatureAlarmActive)
    {
        device.temperatureAlarmActive = false;
        CString strLog;
        strLog.Format(_T("温度报警恢复：%s，站号%d，当前%.1f℃"), (LPCTSTR)device.deviceName, device.slaveAddr, temperature);
        AppendAppLog(strLog);
    }

    return status;
}
