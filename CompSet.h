#pragma once


// CompSet 对话框

class CompSet : public CDialogEx
{
	DECLARE_DYNAMIC(CompSet)

public:
	CompSet(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CompSet();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CompSet };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
};
