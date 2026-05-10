// CompSet.cpp: 实现文件
//

#include "pch.h"
#include "Digitaltemperaturesensor.h"
#include "CompSet.h"
#include "afxdialogex.h"


// CompSet 对话框

IMPLEMENT_DYNAMIC(CompSet, CDialogEx)

CompSet::CompSet(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CompSet, pParent)
{	

}

CompSet::~CompSet()
{
}

void CompSet::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CompSet, CDialogEx)
END_MESSAGE_MAP()


// CompSet 消息处理程序
