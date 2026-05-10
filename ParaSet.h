#pragma once

class ParaSet : public CDialogEx
{
    DECLARE_DYNAMIC(ParaSet)

public:
    ParaSet(CWnd* pParent = nullptr);
    virtual ~ParaSet();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ParaSet };
#endif

public:
    CString m_strPort;
    int m_nBaud;
    int m_nAddr;
    int m_nCalibrationTenths;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBnClickedBtnParaSave();
};
