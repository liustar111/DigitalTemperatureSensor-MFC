#pragma once
#include <vector>

struct TempRecord {
    CTime time;
    CString timeStr;
    CString deviceName;
    BYTE deviceAddr = 0;
    double temperature = 0.0;
    bool hasTemperature = true;
    CString status;
};

class HistData : public CDialogEx
{
    DECLARE_DYNAMIC(HistData)

public:
    HistData(CWnd* pParent = nullptr);
    virtual ~HistData();

    std::vector<TempRecord>* m_pRawData;
    std::vector<TempRecord> m_filteredData;
    std::vector<TempRecord> m_pageData;

    size_t m_nLastDataCount;
    int m_nCurrentPage;
    int m_nTotalPages;
    int m_nItemsPerPage;
    int m_nHoverIndex;

    CListCtrl m_listData;
    bool m_bIsListView;

    void UpdatePageData();
    void InitListControl();
    void UpdateListContent();

    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedBtnFirst();
    afx_msg void OnBnClickedBtnPrev();
    afx_msg void OnBnClickedBtnNext();
    afx_msg void OnBnClickedBtnLast();
    afx_msg void OnBnClickedBtnTabList();
    afx_msg void OnBnClickedBtnTabChart();
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnBnClickedExport();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_HistData };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

public:
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg void OnBnClickedButtonQuery();
};
