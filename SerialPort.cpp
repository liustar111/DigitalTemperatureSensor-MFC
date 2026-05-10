#include "pch.h"
#include "SerialPort.h"

CSerialPort::CSerialPort()
    : m_hComm(INVALID_HANDLE_VALUE)
    , m_dwLastError(ERROR_SUCCESS)
{
}

CSerialPort::~CSerialPort()
{
    Close();
}

BOOL CSerialPort::Open(LPCTSTR lpszPort, DWORD dwBaudRate, BYTE byParity,
    BYTE byDataBits, BYTE byStopBits)
{
    if (IsOpen()) return TRUE;

    CString strPort(lpszPort == nullptr ? _T("") : lpszPort);
    strPort.Trim();
    if (strPort.IsEmpty())
    {
        m_dwLastError = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    CString strDevice = strPort;
    if (strDevice.Left(4).CompareNoCase(_T("\\\\.\\")) != 0)
    {
        strDevice = _T("\\\\.\\") + strDevice;
    }

    m_hComm = CreateFile(strDevice, GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);
    if (m_hComm == INVALID_HANDLE_VALUE)
    {
        m_dwLastError = GetLastError();
        return FALSE;
    }

    SetupComm(m_hComm, 4096, 4096);

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(m_hComm, &dcb))
    {
        m_dwLastError = GetLastError();
        Close();
        return FALSE;
    }

    dcb.BaudRate = dwBaudRate;
    dcb.Parity = byParity;
    dcb.ByteSize = byDataBits;
    dcb.StopBits = byStopBits;
    dcb.fBinary = TRUE;

    if (!SetCommState(m_hComm, &dcb))
    {
        m_dwLastError = GetLastError();
        Close();
        return FALSE;
    }

    if (!SetTimeouts(220, 220))
    {
        Close();
        return FALSE;
    }

    Purge();
    m_dwLastError = ERROR_SUCCESS;
    return TRUE;
}

void CSerialPort::Close()
{
    if (m_hComm != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hComm);
        m_hComm = INVALID_HANDLE_VALUE;
    }
}

BOOL CSerialPort::SetTimeouts(DWORD dwReadTimeoutMs, DWORD dwWriteTimeoutMs)
{
    if (!IsOpen())
    {
        m_dwLastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }

    COMMTIMEOUTS timeouts = { 0 };
    if (dwReadTimeoutMs == 0)
    {
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
    }
    else
    {
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = dwReadTimeoutMs;
        timeouts.ReadTotalTimeoutMultiplier = 0;
    }
    timeouts.WriteTotalTimeoutConstant = dwWriteTimeoutMs;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    if (!SetCommTimeouts(m_hComm, &timeouts))
    {
        m_dwLastError = GetLastError();
        return FALSE;
    }
    return TRUE;
}

BOOL CSerialPort::Write(const BYTE* pData, DWORD dwLen)
{
    if (!IsOpen())
    {
        m_dwLastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    if (pData == nullptr || dwLen == 0)
    {
        m_dwLastError = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    DWORD dwWritten = 0;
    if (!WriteFile(m_hComm, pData, dwLen, &dwWritten, NULL) || dwWritten != dwLen)
    {
        m_dwLastError = GetLastError();
        return FALSE;
    }

    m_dwLastError = ERROR_SUCCESS;
    return TRUE;
}

DWORD CSerialPort::Read(BYTE* pBuffer, DWORD dwBufferLen, DWORD dwTimeoutMs)
{
    if (!IsOpen())
    {
        m_dwLastError = ERROR_INVALID_HANDLE;
        return 0;
    }
    if (pBuffer == nullptr || dwBufferLen == 0)
    {
        m_dwLastError = ERROR_INVALID_PARAMETER;
        return 0;
    }

    SetTimeouts(dwTimeoutMs, 220);

    DWORD dwRead = 0;
    if (!ReadFile(m_hComm, pBuffer, dwBufferLen, &dwRead, NULL))
    {
        m_dwLastError = GetLastError();
        return 0;
    }

    m_dwLastError = ERROR_SUCCESS;
    return dwRead;
}

BOOL CSerialPort::Purge()
{
    if (!IsOpen())
    {
        m_dwLastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }

    if (!PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT))
    {
        m_dwLastError = GetLastError();
        return FALSE;
    }
    return TRUE;
}
