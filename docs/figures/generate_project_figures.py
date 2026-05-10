from pathlib import Path
from xml.sax.saxutils import escape


OUT_DIR = Path(__file__).resolve().parent
FONT = "Microsoft YaHei, SimHei, Arial, sans-serif"


def tag(name, attrs=None, content="", self_close=False):
    attrs = attrs or {}
    attr_text = "".join(f' {k}="{escape(str(v))}"' for k, v in attrs.items() if v is not None)
    if self_close:
        return f"<{name}{attr_text}/>"
    return f"<{name}{attr_text}>{content}</{name}>"


def text(x, y, value, size=16, weight="400", fill="#172033", anchor="middle", extra=""):
    return (
        f'<text x="{x}" y="{y}" font-family="{escape(FONT)}" font-size="{size}" '
        f'font-weight="{weight}" fill="{fill}" text-anchor="{anchor}" {extra}>{escape(value)}</text>'
    )


def multiline(x, y, lines, size=14, weight="400", fill="#172033", anchor="middle", line_gap=20):
    return "\n".join(text(x, y + i * line_gap, line, size, weight, fill, anchor) for i, line in enumerate(lines))


def rect(x, y, w, h, fill="#FFFFFF", stroke="#6B7280", rx=6, sw=1.4, dashed=False, opacity=1):
    dash = "6 5" if dashed else None
    return tag(
        "rect",
        {
            "x": x,
            "y": y,
            "width": w,
            "height": h,
            "rx": rx,
            "fill": fill,
            "stroke": stroke,
            "stroke-width": sw,
            "stroke-dasharray": dash,
            "opacity": opacity,
        },
        self_close=True,
    )


def line(x1, y1, x2, y2, stroke="#334155", sw=1.8, arrow=True, dashed=False):
    return tag(
        "line",
        {
            "x1": x1,
            "y1": y1,
            "x2": x2,
            "y2": y2,
            "stroke": stroke,
            "stroke-width": sw,
            "marker-end": "url(#arrow)" if arrow else None,
            "stroke-dasharray": "6 5" if dashed else None,
        },
        self_close=True,
    )


def path(d, stroke="#334155", sw=1.8, fill="none", arrow=True, dashed=False):
    return tag(
        "path",
        {
            "d": d,
            "stroke": stroke,
            "stroke-width": sw,
            "fill": fill,
            "marker-end": "url(#arrow)" if arrow else None,
            "stroke-dasharray": "6 5" if dashed else None,
        },
        self_close=True,
    )


def diamond(cx, cy, w, h, fill="#FFFFFF", stroke="#6B7280"):
    points = f"{cx},{cy-h/2} {cx+w/2},{cy} {cx},{cy+h/2} {cx-w/2},{cy}"
    return tag("polygon", {"points": points, "fill": fill, "stroke": stroke, "stroke-width": 1.4}, self_close=True)


def svg_root(width, height, body):
    defs = """
    <defs>
      <marker id="arrow" markerWidth="10" markerHeight="10" refX="9" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L9,3 L0,6 Z" fill="#334155"/>
      </marker>
      <filter id="softShadow" x="-10%" y="-10%" width="120%" height="130%">
        <feDropShadow dx="0" dy="2" stdDeviation="2" flood-color="#475569" flood-opacity="0.16"/>
      </filter>
    </defs>
    """
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" role="img">\n'
        f'{defs}\n{rect(0, 0, width, height, "#F8FAFC", "#F8FAFC", 0)}\n{body}\n</svg>\n'
    )


def labeled_box(x, y, w, h, title, subtitle=None, fill="#FFFFFF", stroke="#64748B"):
    inner = rect(x, y, w, h, fill, stroke, 8, 1.4)
    lines = [title] if subtitle is None else [title, subtitle]
    size = 15 if subtitle is None else 14
    gap = 19
    start_y = y + h / 2 - ((len(lines) - 1) * gap) / 2 + 5
    return inner + "\n" + multiline(x + w / 2, start_y, lines, size=size, weight="600" if subtitle is None else "500", line_gap=gap)


def figure_flowchart():
    width, height = 1280, 900
    parts = [
        text(60, 52, "数字温度传感器软件流程图", 26, "700", "#0F172A", "start"),
        text(60, 82, "依据 CDigitaltemperaturesensorDlg、CSerialPort 与 HistData 的实际调用链绘制", 14, "400", "#475569", "start"),
    ]

    parts.append(labeled_box(500, 115, 280, 62, "程序启动", "OnInitDialog"))
    parts.append(labeled_box(500, 205, 280, 72, "读取配置", "LoadConfig / RebuildDeviceList", "#EEF6FF", "#2563EB"))
    parts.append(labeled_box(500, 315, 280, 78, "选择设备并打开串口", "CreateFile + DCB + timeout", "#F0FDF4", "#16A34A"))
    parts.append(labeled_box(500, 445, 280, 76, "开始采集", "SetTimer(1, interval)", "#FFF7ED", "#EA580C"))
    parts.append(labeled_box(500, 575, 280, 80, "发送 Modbus 读取命令", "03H, 寄存器 0000H, CRC16", "#FFFFFF", "#64748B"))
    parts.append(labeled_box(500, 710, 280, 82, "解析响应并更新界面", "CRC / raw÷10 / 状态文本", "#F5F3FF", "#7C3AED"))

    for y1, y2 in [(177, 205), (277, 315), (393, 445), (521, 575), (655, 710)]:
        parts.append(line(640, y1, 640, y2))

    parts.append(diamond(230, 355, 180, 92, "#FFFFFF", "#64748B"))
    parts.append(multiline(230, 350, ["串口打开", "成功？"], 14, "600", line_gap=18))
    parts.append(path("M500 354 C430 354, 360 354, 321 354"))
    parts.append(path("M230 401 C230 445, 335 477, 500 477"))
    parts.append(text(375, 337, "成功", 13, "500", "#15803D"))
    parts.append(text(282, 440, "否：显示错误并写日志", 13, "500", "#B91C1C", "start"))

    parts.append(diamond(960, 615, 190, 96, "#FFFFFF", "#64748B"))
    parts.append(multiline(960, 610, ["收到有效", "响应？"], 14, "600", line_gap=18))
    parts.append(path("M780 615 C840 615, 870 615, 865 615"))
    parts.append(path("M960 663 C960 710, 890 746, 780 746"))
    parts.append(text(833, 597, "读取", 13, "500", "#475569"))
    parts.append(text(995, 694, "是", 13, "500", "#15803D"))

    parts.append(labeled_box(895, 735, 250, 76, "通信异常处理", "超时 / 长度 / 站号 / 功能码 / CRC", "#FEF2F2", "#DC2626"))
    parts.append(path("M960 663 C960 690, 1020 703, 1020 735"))
    parts.append(text(1028, 700, "否", 13, "500", "#B91C1C", "start"))

    parts.append(labeled_box(120, 555, 250, 74, "历史数据", "AddHistoryRecord / TrimHistory", "#ECFEFF", "#0891B2"))
    parts.append(path("M500 748 C420 748, 370 610, 370 594"))
    parts.append(line(370, 594, 370, 592, arrow=False))
    parts.append(line(370, 592, 370, 592, arrow=False))
    parts.append(path("M500 748 C410 748, 370 626, 370 592"))
    parts.append(line(370, 592, 370, 592, arrow=False))
    parts.append(path("M500 748 C425 748, 370 620, 370 592"))
    parts.append(path("M500 748 C410 748, 365 617, 370 592"))
    parts.append(path("M500 748 C405 748, 366 628, 370 629", arrow=False))
    parts.append(line(370, 592, 370, 592, arrow=True))
    parts.append(path("M500 748 C420 748, 365 610, 370 592", arrow=True))

    parts.append(labeled_box(120, 680, 250, 74, "历史查询窗口", "筛选 / 分页 / 列表 / 曲线 / 导出 CSV", "#ECFEFF", "#0891B2"))
    parts.append(line(245, 629, 245, 680))

    parts.append(labeled_box(895, 205, 250, 76, "参数设置", "COM / 波特率 / 站号 / 校准", "#FAF5FF", "#9333EA"))
    parts.append(labeled_box(895, 315, 250, 76, "写传感器寄存器", "06H: 0013H / 0014H / 0015H", "#FAF5FF", "#9333EA"))
    parts.append(line(1020, 281, 1020, 315))
    parts.append(path("M895 353 C845 353, 815 353, 780 353", dashed=True))
    parts.append(text(1019, 426, "参数改动时可写入硬件；波特率变化后要求重新开串口", 13, "400", "#475569"))

    parts.append(path("M640 792 C640 842, 940 840, 940 811", dashed=True))
    parts.append(text(735, 850, "定时器继续下一轮轮询，直到点击“停止采集”", 13, "500", "#475569"))
    return svg_root(width, height, "\n".join(parts))


def figure_principle():
    width, height = 1280, 760
    parts = [
        text(60, 52, "核心原理图：PC 端 MFC + 串口 Modbus RTU + 数字温度传感器", 25, "700", "#0F172A", "start"),
        text(60, 82, "核心链路是“配置参数 → 打开串口 → 周期读寄存器 → 校验解析 → 告警与历史记录”。", 14, "400", "#475569", "start"),
    ]

    parts.append(rect(70, 130, 340, 500, "#FFFFFF", "#2563EB", 10, 1.8))
    parts.append(text(240, 165, "PC 上位机 / MFC 程序", 19, "700", "#1D4ED8"))
    parts.append(labeled_box(105, 200, 270, 70, "主对话框", "CDigitaltemperaturesensorDlg", "#EFF6FF", "#60A5FA"))
    parts.append(labeled_box(105, 300, 270, 70, "串口封装", "CSerialPort: Open / Read / Write", "#EFF6FF", "#60A5FA"))
    parts.append(labeled_box(105, 400, 270, 70, "配置与历史缓存", "INI + vector<TempRecord>", "#EFF6FF", "#60A5FA"))
    parts.append(labeled_box(105, 500, 270, 70, "历史窗口", "HistData: List / Chart / CSV", "#EFF6FF", "#60A5FA"))

    parts.append(rect(505, 130, 270, 500, "#FFFFFF", "#0F766E", 10, 1.8))
    parts.append(text(640, 165, "通信协议", 19, "700", "#0F766E"))
    parts.append(labeled_box(535, 210, 210, 74, "读温度命令", "Addr 03 0000 0001 CRC", "#F0FDFA", "#14B8A6"))
    parts.append(labeled_box(535, 335, 210, 74, "传感器响应", "Addr 03 02 Hi Lo CRC", "#F0FDFA", "#14B8A6"))
    parts.append(labeled_box(535, 465, 210, 92, "解析规则", "CRC16 通过后 raw / 10 = ℃", "#F0FDFA", "#14B8A6"))

    parts.append(rect(880, 130, 330, 500, "#FFFFFF", "#EA580C", 10, 1.8))
    parts.append(text(1045, 165, "数字温度传感器", 19, "700", "#C2410C"))
    parts.append(labeled_box(920, 215, 250, 74, "温度采样单元", "输出 0.1℃ 分辨率数据", "#FFF7ED", "#FB923C"))
    parts.append(labeled_box(920, 345, 250, 74, "Modbus 从站寄存器", "0000H 温度值", "#FFF7ED", "#FB923C"))
    parts.append(labeled_box(920, 475, 250, 74, "可写参数寄存器", "0013H 地址 / 0014H 波特率 / 0015H 校准", "#FFF7ED", "#FB923C"))

    parts.append(path("M375 335 C440 335, 475 248, 535 248"))
    parts.append(path("M745 248 C805 248, 850 248, 920 248"))
    parts.append(text(640, 245, "请求帧", 13, "700", "#0F766E"))
    parts.append(path("M920 382 C850 382, 805 372, 745 372"))
    parts.append(path("M535 372 C475 372, 440 335, 375 335"))
    parts.append(text(640, 398, "响应帧", 13, "700", "#0F766E"))
    parts.append(line(640, 409, 640, 465))
    parts.append(path("M535 512 C470 512, 430 435, 375 435"))
    parts.append(text(452, 490, "温度与状态", 13, "500", "#475569"))

    parts.append(line(240, 270, 240, 300))
    parts.append(line(240, 370, 240, 400))
    parts.append(line(240, 470, 240, 500))
    parts.append(path("M375 535 C430 535, 455 610, 535 610"))
    parts.append(labeled_box(535, 590, 210, 58, "输出", "实时文本 / 状态栏 / 日志 / CSV", "#FFFFFF", "#64748B"))
    parts.append(path("M920 512 C845 512, 815 512, 745 512", dashed=True))
    parts.append(text(815, 500, "参数写入 06H", 13, "500", "#9333EA"))

    parts.append(rect(70, 665, 1140, 54, "#F1F5F9", "#CBD5E1", 8, 1))
    parts.append(text(90, 697, "关键保护：串口未打开不可采集；采集中禁止改通信参数；响应需检查长度、站号、功能码、字节数和 CRC；温度超界或通信失败写入状态与日志。", 14, "500", "#334155", "start"))
    return svg_root(width, height, "\n".join(parts))


def draw_button(x, y, w, h, label):
    return rect(x, y, w, h, "#F8FAFC", "#94A3B8", 4, 1) + text(x + w / 2, y + h / 2 + 5, label, 13, "500", "#172033")


def figure_ui():
    width, height = 1280, 860
    parts = [
        text(60, 52, "MFC 界面 UI 图", 26, "700", "#0F172A", "start"),
        text(60, 82, "主界面按 Digitaltemperaturesensor.rc 的控件布局重绘，并补充历史数据与参数设置弹窗。", 14, "400", "#475569", "start"),
    ]

    # Main dialog
    parts.append(rect(65, 120, 720, 475, "#FFFFFF", "#1E293B", 8, 1.8))
    parts.append(rect(65, 120, 720, 34, "#E2E8F0", "#1E293B", 8, 1.8))
    parts.append(text(85, 143, "Digitaltemperaturesensor 主窗口", 15, "700", "#0F172A", "start"))
    parts.append(draw_button(85, 168, 150, 28, "参数设置管理"))
    parts.append(draw_button(245, 168, 150, 28, "公司信息管理"))
    parts.append(draw_button(635, 208, 120, 34, "查询历史"))

    parts.append(rect(85, 218, 175, 118, "#FFFFFF", "#CBD5E1", 6, 1.2))
    parts.append(text(105, 242, "操作按钮", 14, "700", "#334155", "start"))
    parts.append(draw_button(112, 258, 105, 28, "打开串口"))
    parts.append(draw_button(112, 294, 105, 28, "开始采集"))

    parts.append(rect(85, 358, 175, 190, "#FFFFFF", "#CBD5E1", 6, 1.2))
    parts.append(text(105, 382, "设备列表", 14, "700", "#334155", "start"))
    parts.append(draw_button(105, 396, 44, 28, "增加"))
    parts.append(draw_button(155, 396, 44, 28, "修改"))
    parts.append(draw_button(205, 396, 44, 28, "删除"))
    parts.append(rect(105, 448, 135, 78, "#F8FAFC", "#94A3B8", 4, 1))
    parts.append(multiline(172, 475, ["设备A (站号1)", "设备B (站号2)"], 12, "400", "#334155", line_gap=22))

    parts.append(rect(280, 245, 475, 303, "#F8FAFC", "#94A3B8", 4, 1.2))
    parts.append(text(300, 272, "实时采集显示区 IDC_EDIT_SHOW", 14, "700", "#334155", "start"))
    sample = [
        "[2026-05-08 21:00:00] 设备A 站号1 温度：25.6 ℃ 状态：正常",
        "[2026-05-08 21:00:01] 设备A 站号1 温度：25.7 ℃ 状态：正常",
        "[2026-05-08 21:00:02] 设备A 站号1 温度：26.0 ℃ 状态：高温报警..."
    ]
    parts.append(multiline(304, 310, sample, 12, "400", "#475569", "start", 24))
    parts.append(rect(75, 558, 690, 24, "#F1F5F9", "#CBD5E1", 3, 1))
    parts.append(text(88, 575, "状态栏 IDC_EDIT3：正在采集温度 / 通信异常 / 串口已关闭", 12, "500", "#334155", "start"))

    # History dialog
    parts.append(rect(835, 120, 380, 315, "#FFFFFF", "#0F766E", 8, 1.8))
    parts.append(rect(835, 120, 380, 32, "#CCFBF1", "#0F766E", 8, 1.8))
    parts.append(text(855, 142, "历史数据窗口 HistData", 15, "700", "#0F766E", "start"))
    parts.append(rect(855, 170, 120, 22, "#F8FAFC", "#94A3B8", 3, 1))
    parts.append(text(862, 186, "起始时间", 11, "500", "#334155", "start"))
    parts.append(rect(990, 170, 120, 22, "#F8FAFC", "#94A3B8", 3, 1))
    parts.append(text(997, 186, "截止时间", 11, "500", "#334155", "start"))
    parts.append(draw_button(1125, 168, 38, 24, "查询"))
    parts.append(draw_button(1168, 168, 38, 24, "导出"))
    parts.append(draw_button(855, 206, 48, 24, "列表"))
    parts.append(draw_button(910, 206, 48, 24, "曲线"))
    parts.append(rect(855, 240, 335, 145, "#F8FAFC", "#94A3B8", 4, 1))
    parts.append(path("M875 355 L930 315 L985 330 L1040 290 L1095 305 L1165 265", "#2563EB", 2.2, arrow=False))
    for x, y in [(875, 355), (930, 315), (985, 330), (1040, 290), (1095, 305), (1165, 265)]:
        parts.append(tag("circle", {"cx": x, "cy": y, "r": 4, "fill": "#2563EB"}, self_close=True))
    parts.append(text(870, 260, "曲线视图 / 列表视图共用区域", 12, "500", "#334155", "start"))
    parts.append(draw_button(905, 397, 58, 24, "首页"))
    parts.append(draw_button(982, 397, 58, 24, "上一页"))
    parts.append(text(1062, 414, "1 / 1", 12, "600", "#334155"))
    parts.append(draw_button(1090, 397, 58, 24, "下一页"))
    parts.append(draw_button(1155, 397, 58, 24, "末页"))

    # Parameter dialog
    parts.append(rect(835, 485, 380, 255, "#FFFFFF", "#7C3AED", 8, 1.8))
    parts.append(rect(835, 485, 380, 32, "#EDE9FE", "#7C3AED", 8, 1.8))
    parts.append(text(855, 507, "参数设置窗口 ParaSet", 15, "700", "#6D28D9", "start"))
    parts.append(rect(865, 540, 310, 135, "#FFFFFF", "#DDD6FE", 6, 1.2))
    parts.append(text(885, 564, "通信参数", 14, "700", "#334155", "start"))
    rows = [("串口号", "COM4"), ("波特率", "9600"), ("Modbus站号", "1"), ("温度补偿", "0 (0.1℃)")] 
    for i, (label, value) in enumerate(rows):
        y = 585 + i * 28
        parts.append(text(895, y + 16, label, 12, "500", "#334155", "start"))
        parts.append(rect(1000, y, 130, 22, "#F8FAFC", "#94A3B8", 3, 1))
        parts.append(text(1010, y + 16, value, 12, "400", "#475569", "start"))
    parts.append(text(865, 705, "格式：8 位数据、无校验、1 位停止", 12, "500", "#64748B", "start"))
    parts.append(draw_button(1040, 700, 72, 28, "保存设置"))
    parts.append(draw_button(1120, 700, 60, 28, "取消"))

    # Relations
    parts.append(path("M755 225 C795 225, 810 245, 835 270", dashed=True))
    parts.append(text(790, 214, "查询历史", 12, "500", "#0F766E", "start"))
    parts.append(path("M235 182 C500 100, 740 535, 835 565", dashed=True))
    parts.append(text(565, 151, "参数设置", 12, "500", "#7C3AED"))
    parts.append(path("M217 274 C410 680, 835 715, 1040 714", dashed=True))
    parts.append(text(475, 720, "保存后影响串口打开、采集和寄存器写入", 12, "500", "#64748B", "start"))

    return svg_root(width, height, "\n".join(parts))


def main():
    outputs = {
        "project_flowchart.svg": figure_flowchart(),
        "core_principle.svg": figure_principle(),
        "mfc_ui_wireframe.svg": figure_ui(),
    }
    for name, content in outputs.items():
        (OUT_DIR / name).write_text(content, encoding="utf-8")
        print(OUT_DIR / name)


if __name__ == "__main__":
    main()
