#pragma once
#include <Windows.h>

class CSerialPort
{
public:
    CSerialPort();
    ~CSerialPort();

    BOOL Open(LPCTSTR lpszPort, DWORD dwBaudRate = 9600, BYTE byParity = NOPARITY,
        BYTE byDataBits = 8, BYTE byStopBits = ONESTOPBIT);
    void Close();
    BOOL Write(const BYTE* pData, DWORD dwLen);
    DWORD Read(BYTE* pBuffer, DWORD dwBufferLen, DWORD dwTimeoutMs = 220);
    BOOL Purge();
    BOOL IsOpen() const { return m_hComm != INVALID_HANDLE_VALUE; }
    DWORD GetLastErrorCode() const { return m_dwLastError; }

private:
    BOOL SetTimeouts(DWORD dwReadTimeoutMs, DWORD dwWriteTimeoutMs);

private:
    HANDLE m_hComm;
    DWORD m_dwLastError;
};
