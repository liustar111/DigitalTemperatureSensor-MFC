// DigitaltemperaturesensorDlg.h: header
//

#pragma once
#include "SerialPort.h"
#include <vector>
#include "HistData.h"

struct DeviceConfig {
    CString deviceName;
    CString portName;
    int baudRate = 9600;
    BYTE slaveAddr = 1;
    int parity = NOPARITY;
    int dataBits = 8;
    int stopBits = ONESTOPBIT;
    int calibrationTenths = 0;
    // TODO: The sensor document does not define alarm registers; these limits are software-only.
    double alarmLow = -50.0;
    double alarmHigh = 120.0;
    bool temperatureAlarmActive = false;
    bool commAlarmActive = false;
    int commFailCount = 0;
};

class CDigitaltemperaturesensorDlg : public CDialogEx
{
public:
    CDigitaltemperaturesensorDlg(CWnd* pParent = nullptr);
    std::vector<TempRecord> m_historyData;
    std::vector<DeviceConfig> m_deviceConfigs;
    int m_nCurrentDeviceIndex;

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIGITALTEMPERATURESENSOR_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

protected:
    HICON m_hIcon;

    virtual BOOL OnInitDialog();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
    afx_msg void OnBnClickedhistory();
    afx_msg void OnBnClickedParameterset();
    afx_msg void OnBnClickedCompanyinfo();
    DECLARE_MESSAGE_MAP()

public:
    CSerialPort m_serialPort;
    bool m_bSerialOpened;
    bool m_bCollecting;
    UINT_PTR m_nTimerID;
    bool m_bWaitingResponse;
    BYTE m_pendingAddr;
    int m_nPendingConfigIndex;
    DWORD m_dwRequestTick;

    afx_msg void OnBnClickedSerialPort();
    afx_msg void OnBnClickedCollection();
    afx_msg void OnBnClickedBtnAddDevice();
    afx_msg void OnBnClickedBtnModDevice();
    afx_msg void OnBnClickedBtnDelDevice();

    WORD CRC16(const BYTE* pData, int iLen);
    float ParseTemperature(BYTE* pRecvData, DWORD dwLen);
    bool ParseTemperatureResponse(const BYTE* pRecvData, DWORD dwLen, BYTE expectedAddr, float& temperature, CString& errorMessage);
    bool WriteSingleRegister(BYTE deviceAddr, WORD registerAddr, short value, CString& errorMessage);

    CListBox m_deviceList;

private:
    void LoadConfig();
    void SaveConfig() const;
    void LoadDefaultDevice();
    void RebuildDeviceList(int nSelectConfigIndex = -1);
    int GetSelectedDeviceConfigIndex();
    DeviceConfig* GetSelectedDeviceConfig();
    CString BuildSerialErrorMessage(LPCTSTR action) const;
    void SetStatusText(LPCTSTR text);
    void AddHistoryRecord(const DeviceConfig* pDevice, bool hasTemperature, double temperature, LPCTSTR status);
    void TrimHistory();
    bool IsTemperatureInSupportedRange(double temperature) const;
    bool TryBaudRateToRegister(int baudRate, WORD& registerValue) const;
    bool WriteSensorSettings(const DeviceConfig& oldConfig, const DeviceConfig& newConfig, CString& report);
    void HandleCommunicationFailure(DeviceConfig* pDevice, LPCTSTR reason);
    void ClearCommunicationAlarm(DeviceConfig& device);
    CString UpdateTemperatureAlarm(DeviceConfig& device, double temperature);

private:
    int m_nCollectIntervalMs;
    int m_nMaxHistoryRecords;
};
