#include "pch.h"
#include "Digitaltemperaturesensor.h"
#include "HistData.h"
#include "afxdialogex.h"
#include "resource.h"
#include <atlconv.h>

IMPLEMENT_DYNAMIC(HistData, CDialogEx)

static CString CsvEscape(const CString& value)
{
    CString escaped = value;
    escaped.Replace(_T("\""), _T("\"\""));
    return _T("\"") + escaped + _T("\"");
}

static void WriteUtf8Line(CFile& file, const CString& line)
{
    CW2A utf8(line, CP_UTF8);
    file.Write(static_cast<LPCSTR>(utf8), static_cast<UINT>(strlen(static_cast<LPCSTR>(utf8))));
}

static void BuildTemperatureRecords(const std::vector<TempRecord>& source, std::vector<TempRecord>& temps)
{
    temps.clear();
    for (size_t i = 0; i < source.size(); ++i)
    {
        if (source[i].hasTemperature)
        {
            temps.push_back(source[i]);
        }
    }
}

HistData::HistData(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_HistData, pParent)
{
    m_pRawData = nullptr;
    m_nLastDataCount = 0;
    m_nHoverIndex = -1;
    m_nCurrentPage = 1;
    m_nTotalPages = 1;
    m_nItemsPerPage = 30;
    m_bIsListView = false;
}

HistData::~HistData() {}

void HistData::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_HIST_DATA, m_listData);
}

BEGIN_MESSAGE_MAP(HistData, CDialogEx)
    ON_WM_PAINT()
    ON_WM_TIMER()
    ON_WM_MOUSEMOVE()
    ON_BN_CLICKED(IDC_BUTTON_QUERY, &HistData::OnBnClickedButtonQuery)
    ON_BN_CLICKED(IDC_BUTTON2, &HistData::OnBnClickedExport)
    ON_BN_CLICKED(IDC_BTN_FIRST, &HistData::OnBnClickedBtnFirst)
    ON_BN_CLICKED(IDC_BTN_PREV, &HistData::OnBnClickedBtnPrev)
    ON_BN_CLICKED(IDC_BTN_NEXT, &HistData::OnBnClickedBtnNext)
    ON_BN_CLICKED(IDC_BTN_LAST, &HistData::OnBnClickedBtnLast)
    ON_BN_CLICKED(IDC_BTN_TAB_LIST, &HistData::OnBnClickedBtnTabList)
    ON_BN_CLICKED(IDC_BTN_TAB_CHART, &HistData::OnBnClickedBtnTabChart)
END_MESSAGE_MAP()

BOOL HistData::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    InitListControl();
    m_listData.ShowWindow(SW_HIDE);

    CDateTimeCtrl* pStartCtrl = (CDateTimeCtrl*)GetDlgItem(IDC_DATETIMEPICKER1);
    CDateTimeCtrl* pEndCtrl = (CDateTimeCtrl*)GetDlgItem(IDC_DATETIMEPICKER2);
    if (pStartCtrl && pEndCtrl)
    {
        pStartCtrl->SetFormat(_T("yyyy-MM-dd HH:mm:ss"));
        pEndCtrl->SetFormat(_T("yyyy-MM-dd HH:mm:ss"));

        if (m_pRawData && !m_pRawData->empty())
        {
            pStartCtrl->SetTime(&(m_pRawData->front().time));
            pEndCtrl->SetTime(&(m_pRawData->back().time));
            m_filteredData = *m_pRawData;
            m_nLastDataCount = m_pRawData->size();
        }
    }

    CheckRadioButton(IDC_BTN_TAB_LIST, IDC_BTN_TAB_CHART, IDC_BTN_TAB_CHART);
    SetTimer(2, 1000, NULL);

    UpdatePageData();
    return TRUE;
}

void HistData::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 2 && m_pRawData)
    {
        if (m_pRawData->size() != m_nLastDataCount)
        {
            m_nLastDataCount = m_pRawData->size();
            m_filteredData = *m_pRawData;
            UpdatePageData();
        }
    }
    CDialogEx::OnTimer(nIDEvent);
}

void HistData::UpdatePageData()
{
    if (m_filteredData.empty())
    {
        m_nTotalPages = 1;
        m_nCurrentPage = 1;
        m_pageData.clear();
    }
    else
    {
        m_nTotalPages = (int)((m_filteredData.size() + m_nItemsPerPage - 1) / m_nItemsPerPage);
        if (m_nCurrentPage < 1) m_nCurrentPage = 1;
        if (m_nCurrentPage > m_nTotalPages) m_nCurrentPage = m_nTotalPages;

        int startIndex = (m_nCurrentPage - 1) * m_nItemsPerPage;
        int endIndex = min(startIndex + m_nItemsPerPage, (int)m_filteredData.size());
        m_pageData.assign(m_filteredData.begin() + startIndex, m_filteredData.begin() + endIndex);
    }

    CString strPage;
    strPage.Format(_T("%d / %d"), m_nCurrentPage, m_nTotalPages);
    SetDlgItemText(IDC_STATIC_PAGE, strPage);

    if (GetDlgItem(IDC_BTN_FIRST)) GetDlgItem(IDC_BTN_FIRST)->EnableWindow(m_nCurrentPage > 1);
    if (GetDlgItem(IDC_BTN_PREV))  GetDlgItem(IDC_BTN_PREV)->EnableWindow(m_nCurrentPage > 1);
    if (GetDlgItem(IDC_BTN_NEXT))  GetDlgItem(IDC_BTN_NEXT)->EnableWindow(m_nCurrentPage < m_nTotalPages);
    if (GetDlgItem(IDC_BTN_LAST))  GetDlgItem(IDC_BTN_LAST)->EnableWindow(m_nCurrentPage < m_nTotalPages);

    m_nHoverIndex = -1;
    if (m_bIsListView) UpdateListContent();
    else Invalidate();
}

void HistData::InitListControl()
{
    m_listData.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_listData.InsertColumn(0, _T("序号"), LVCFMT_LEFT, 50);
    m_listData.InsertColumn(1, _T("接收时间"), LVCFMT_LEFT, 150);
    m_listData.InsertColumn(2, _T("设备"), LVCFMT_LEFT, 170);
    m_listData.InsertColumn(3, _T("站号"), LVCFMT_LEFT, 50);
    m_listData.InsertColumn(4, _T("温度"), LVCFMT_LEFT, 70);
    m_listData.InsertColumn(5, _T("单位"), LVCFMT_LEFT, 50);
    m_listData.InsertColumn(6, _T("状态"), LVCFMT_LEFT, 150);
}

void HistData::UpdateListContent()
{
    m_listData.DeleteAllItems();
    for (size_t i = 0; i < m_pageData.size(); i++)
    {
        int globalIdx = (m_nCurrentPage - 1) * m_nItemsPerPage + (int)i + 1;
        CString str;
        str.Format(_T("%d"), globalIdx);
        m_listData.InsertItem((int)i, str);
        m_listData.SetItemText((int)i, 1, m_pageData[i].time.Format(_T("%Y-%m-%d %H:%M:%S")));
        m_listData.SetItemText((int)i, 2, m_pageData[i].deviceName);
        str.Format(_T("%d"), m_pageData[i].deviceAddr);
        m_listData.SetItemText((int)i, 3, str);
        if (m_pageData[i].hasTemperature)
        {
            str.Format(_T("%.1f"), m_pageData[i].temperature);
        }
        else
        {
            str = _T("--");
        }
        m_listData.SetItemText((int)i, 4, str);
        m_listData.SetItemText((int)i, 5, m_pageData[i].hasTemperature ? _T("℃") : _T(""));
        m_listData.SetItemText((int)i, 6, m_pageData[i].status);
    }
}

void HistData::OnBnClickedBtnTabList()
{
    if (!m_bIsListView)
    {
        m_bIsListView = true;
        m_listData.ShowWindow(SW_SHOW);
        if (GetDlgItem(IDC_STATIC_CHART)) GetDlgItem(IDC_STATIC_CHART)->ShowWindow(SW_HIDE);
        UpdateListContent();
    }
}

void HistData::OnBnClickedBtnTabChart()
{
    if (m_bIsListView)
    {
        m_bIsListView = false;
        m_listData.ShowWindow(SW_HIDE);
        if (GetDlgItem(IDC_STATIC_CHART)) GetDlgItem(IDC_STATIC_CHART)->ShowWindow(SW_SHOW);
        Invalidate();
    }
}

void HistData::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);
        return;
    }

    CDialogEx::OnPaint();
    if (m_bIsListView) return;

    CClientDC dc(this);
    CWnd* pChartWnd = GetDlgItem(IDC_STATIC_CHART);
    if (pChartWnd == nullptr) return;

    CRect rect;
    pChartWnd->GetWindowRect(&rect);
    ScreenToClient(&rect);
    if (rect.Width() <= 0 || rect.Height() <= 0) return;

    dc.FillSolidRect(&rect, RGB(255, 255, 255));

    int leftMargin = 45, bottomMargin = 30, topMargin = 20, rightMargin = 35;
    CRect chartRect(rect.left + leftMargin, rect.top + topMargin, rect.right - rightMargin, rect.bottom - bottomMargin);
    if (chartRect.Width() <= 0 || chartRect.Height() <= 0) return;

    std::vector<TempRecord> tempData;
    BuildTemperatureRecords(m_pageData, tempData);

    double yMin = -50.0;
    double yMax = 120.0;
    for (size_t i = 0; i < tempData.size(); ++i)
    {
        if (tempData[i].temperature < yMin) yMin = tempData[i].temperature - 5.0;
        if (tempData[i].temperature > yMax) yMax = tempData[i].temperature + 5.0;
    }
    if (yMax <= yMin) yMax = yMin + 10.0;

    CPen gridPen(PS_SOLID, 1, RGB(220, 220, 220));
    CPen axisPen(PS_SOLID, 2, RGB(100, 100, 100));
    CPen linePen(PS_SOLID, 2, RGB(40, 120, 210));

    dc.SelectObject(&gridPen);
    dc.SetBkMode(TRANSPARENT);
    int gridCount = 10;
    for (int i = 0; i <= gridCount; ++i)
    {
        double value = yMin + (yMax - yMin) * i / gridCount;
        int y = chartRect.bottom - (int)((value - yMin) / (yMax - yMin) * chartRect.Height());
        dc.MoveTo(chartRect.left, y);
        dc.LineTo(chartRect.right, y);
        CString str;
        str.Format(_T("%.1f"), value);
        dc.TextOut(chartRect.left - 42, y - 8, str);
    }

    dc.SelectObject(&axisPen);
    dc.MoveTo(chartRect.left, chartRect.top);
    dc.LineTo(chartRect.left, chartRect.bottom);
    dc.MoveTo(chartRect.left, chartRect.bottom);
    dc.LineTo(chartRect.right, chartRect.bottom);

    if (tempData.empty())
    {
        dc.TextOut(chartRect.left + 10, chartRect.top + 10, _T("暂无温度数据"));
        return;
    }

    dc.SelectObject(&linePen);
    double xStep = (tempData.size() > 1) ? (double)chartRect.Width() / (tempData.size() - 1) : 0;
    for (size_t i = 0; i < tempData.size(); i++)
    {
        int x = chartRect.left + (int)(i * xStep);
        int y = chartRect.bottom - (int)((tempData[i].temperature - yMin) / (yMax - yMin) * chartRect.Height());
        if (i == 0) dc.MoveTo(x, y);
        else dc.LineTo(x, y);
    }

    CBrush dotBrush(RGB(40, 120, 210));
    dc.SelectObject(&dotBrush);
    for (size_t i = 0; i < tempData.size(); i++)
    {
        int x = chartRect.left + (int)(i * xStep);
        int y = chartRect.bottom - (int)((tempData[i].temperature - yMin) / (yMax - yMin) * chartRect.Height());
        dc.Ellipse(x - 3, y - 3, x + 3, y + 3);
    }

    if (m_nHoverIndex >= 0 && m_nHoverIndex < (int)tempData.size())
    {
        int x = chartRect.left + (int)(m_nHoverIndex * xStep);
        int y = chartRect.bottom - (int)((tempData[m_nHoverIndex].temperature - yMin) / (yMax - yMin) * chartRect.Height());

        CString strTip;
        strTip.Format(_T("时间: %s\n设备: %s\n站号: %d\n温度: %.1f ℃\n状态: %s"),
            (LPCTSTR)tempData[m_nHoverIndex].time.Format(_T("%Y-%m-%d %H:%M:%S")),
            (LPCTSTR)tempData[m_nHoverIndex].deviceName,
            tempData[m_nHoverIndex].deviceAddr,
            tempData[m_nHoverIndex].temperature,
            (LPCTSTR)tempData[m_nHoverIndex].status);

        CRect textRect(0, 0, 0, 0);
        dc.DrawText(strTip, &textRect, DT_CALCRECT);
        CRect tipRect(x + 15, y - textRect.Height() - 22, x + 15 + textRect.Width() + 18, y + 4);
        if (tipRect.right > rect.right) tipRect.OffsetRect(-tipRect.Width() - 30, 0);
        if (tipRect.top < rect.top) tipRect.OffsetRect(0, rect.top - tipRect.top + 4);

        CBrush tipBrush(RGB(255, 255, 225));
        dc.SelectObject(&tipBrush);
        dc.Rectangle(&tipRect);
        CRect drawRect = tipRect;
        drawRect.DeflateRect(8, 6);
        dc.DrawText(strTip, &drawRect, DT_LEFT);

        CPen highlightPen(PS_SOLID, 2, RGB(220, 40, 40));
        dc.SelectObject(&highlightPen);
        dc.SelectStockObject(NULL_BRUSH);
        dc.Ellipse(x - 6, y - 6, x + 6, y + 6);
    }
}

void HistData::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bIsListView)
    {
        CDialogEx::OnMouseMove(nFlags, point);
        return;
    }

    CWnd* pChartWnd = GetDlgItem(IDC_STATIC_CHART);
    if (pChartWnd == nullptr)
    {
        CDialogEx::OnMouseMove(nFlags, point);
        return;
    }

    std::vector<TempRecord> tempData;
    BuildTemperatureRecords(m_pageData, tempData);
    if (tempData.empty())
    {
        CDialogEx::OnMouseMove(nFlags, point);
        return;
    }

    CRect rect;
    pChartWnd->GetWindowRect(&rect);
    ScreenToClient(&rect);
    int leftMargin = 45, bottomMargin = 30, topMargin = 20, rightMargin = 35;
    CRect chartRect(rect.left + leftMargin, rect.top + topMargin, rect.right - rightMargin, rect.bottom - bottomMargin);

    double yMin = -50.0;
    double yMax = 120.0;
    for (size_t i = 0; i < tempData.size(); ++i)
    {
        if (tempData[i].temperature < yMin) yMin = tempData[i].temperature - 5.0;
        if (tempData[i].temperature > yMax) yMax = tempData[i].temperature + 5.0;
    }
    if (yMax <= yMin) yMax = yMin + 10.0;

    double xStep = (tempData.size() > 1) ? (double)chartRect.Width() / (tempData.size() - 1) : 0;
    int hitIndex = -1;
    for (size_t i = 0; i < tempData.size(); i++)
    {
        int x = chartRect.left + (int)(i * xStep);
        int y = chartRect.bottom - (int)((tempData[i].temperature - yMin) / (yMax - yMin) * chartRect.Height());
        if (abs(point.x - x) <= 5 && abs(point.y - y) <= 5)
        {
            hitIndex = (int)i;
            break;
        }
    }

    if (hitIndex != m_nHoverIndex)
    {
        m_nHoverIndex = hitIndex;
        Invalidate();
    }
    CDialogEx::OnMouseMove(nFlags, point);
}

void HistData::OnBnClickedButtonQuery()
{
    if (m_pRawData == nullptr)
    {
        MessageBox(_T("没有可查询的历史数据。"), _T("提示"), MB_ICONINFORMATION);
        return;
    }

    CDateTimeCtrl* pStart = (CDateTimeCtrl*)GetDlgItem(IDC_DATETIMEPICKER1);
    CDateTimeCtrl* pEnd = (CDateTimeCtrl*)GetDlgItem(IDC_DATETIMEPICKER2);
    if (!pStart || !pEnd) return;

    CTime startTime, endTime;
    pStart->GetTime(startTime);
    pEnd->GetTime(endTime);

    m_filteredData.clear();
    for (size_t i = 0; i < m_pRawData->size(); i++)
    {
        if ((*m_pRawData)[i].time >= startTime && (*m_pRawData)[i].time <= endTime)
        {
            m_filteredData.push_back((*m_pRawData)[i]);
        }
    }
    m_nCurrentPage = 1;
    UpdatePageData();
}

void HistData::OnBnClickedExport()
{
    std::vector<TempRecord> exportData;
    if (!m_filteredData.empty())
    {
        exportData = m_filteredData;
    }
    else if (m_pRawData != nullptr)
    {
        exportData = *m_pRawData;
    }

    if (exportData.empty())
    {
        MessageBox(_T("没有可导出的历史数据。"), _T("提示"), MB_ICONINFORMATION);
        return;
    }

    CString defaultName;
    defaultName.Format(_T("TemperatureHistory_%s.csv"), (LPCTSTR)CTime::GetCurrentTime().Format(_T("%Y%m%d_%H%M%S")));
    CFileDialog dlg(FALSE, _T("csv"), defaultName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("CSV 文件 (*.csv)|*.csv|所有文件 (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK)
    {
        return;
    }

    CFile file;
    CFileException ex;
    if (!file.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite, &ex))
    {
        CString msg;
        msg.Format(_T("导出失败，无法创建文件。错误码：%u"), ex.m_cause);
        MessageBox(msg, _T("导出失败"), MB_ICONERROR);
        AppendAppLog(msg);
        return;
    }

    BYTE bom[] = { 0xEF, 0xBB, 0xBF };
    file.Write(bom, sizeof(bom));
    WriteUtf8Line(file, _T("时间,设备名,设备地址,温度,状态\r\n"));

    for (size_t i = 0; i < exportData.size(); ++i)
    {
        const TempRecord& record = exportData[i];
        CString tempText;
        if (record.hasTemperature)
        {
            tempText.Format(_T("%.1f"), record.temperature);
        }

        CString line;
        line.Format(_T("%s,%s,%d,%s,%s\r\n"),
            (LPCTSTR)CsvEscape(record.time.Format(_T("%Y-%m-%d %H:%M:%S"))),
            (LPCTSTR)CsvEscape(record.deviceName),
            record.deviceAddr,
            (LPCTSTR)tempText,
            (LPCTSTR)CsvEscape(record.status));
        WriteUtf8Line(file, line);
    }

    file.Close();
    CString msg;
    msg.Format(_T("历史数据已导出：%s"), (LPCTSTR)dlg.GetPathName());
    AppendAppLog(msg);
    MessageBox(msg, _T("导出完成"), MB_ICONINFORMATION);
}

void HistData::OnBnClickedBtnFirst() { m_nCurrentPage = 1; UpdatePageData(); }
void HistData::OnBnClickedBtnPrev() { m_nCurrentPage--; UpdatePageData(); }
void HistData::OnBnClickedBtnNext() { m_nCurrentPage++; UpdatePageData(); }
void HistData::OnBnClickedBtnLast() { m_nCurrentPage = m_nTotalPages; UpdatePageData(); }
