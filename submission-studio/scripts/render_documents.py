#!/usr/bin/env python3
"""Build detailed report and synchronized operator guide from editable Markdown."""
from pathlib import Path
import argparse,re,json,csv,html,textwrap,hashlib,datetime
from reportlab.platypus import SimpleDocTemplate,Paragraph,Spacer,Table,TableStyle,PageBreak,Image,Preformatted,KeepTogether
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
p=argparse.ArgumentParser();p.add_argument('--font',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];studio=root/'submission-studio';content=studio/'content';content.mkdir(exist_ok=True);dest=studio/'report';dest.mkdir(exist_ok=True)
pdfmetrics.registerFont(TTFont('CN',a.font));pdfmetrics.registerFontFamily('CN',normal='CN',bold='CN',italic='CN',boldItalic='CN')
navy=colors.HexColor('#18314A');teal=colors.HexColor('#087E8B');grey=colors.HexColor('#536779');light=colors.HexColor('#EDF4F6')
W,H=A4;cw=W-88
styles={
'body':ParagraphStyle('body',fontName='CN',fontSize=9.5,leading=15,spaceAfter=7,wordWrap='CJK',textColor=navy),
'small':ParagraphStyle('small',fontName='CN',fontSize=8,leading=12,spaceAfter=7,wordWrap='CJK',textColor=grey),
'h1':ParagraphStyle('h1',fontName='CN',fontSize=17,leading=24,spaceBefore=15,spaceAfter=10,wordWrap='CJK',textColor=teal,keepWithNext=True),
'h2':ParagraphStyle('h2',fontName='CN',fontSize=12,leading=19,spaceBefore=11,spaceAfter=7,wordWrap='CJK',textColor=navy,keepWithNext=True),
'h3':ParagraphStyle('h3',fontName='CN',fontSize=10,leading=16,spaceBefore=8,spaceAfter=5,wordWrap='CJK',textColor=navy,keepWithNext=True),
'cell':ParagraphStyle('cell',fontName='CN',fontSize=8,leading=12,wordWrap='CJK',textColor=navy),
'head':ParagraphStyle('head',fontName='CN',fontSize=8,leading=12,wordWrap='CJK',textColor=colors.white),
'code':ParagraphStyle('code',fontName='CN',fontSize=7.8,leading=11,spaceBefore=3,spaceAfter=9,backColor=light,borderPadding=7,wordWrap='CJK'),
'cover':ParagraphStyle('cover',fontName='CN',fontSize=27,leading=38,textColor=navy,spaceAfter=16,wordWrap='CJK')}
def para(t,k='body'):
 t=html.escape(t);t=re.sub(r'!\[([^]]*)\]\([^)]*\)',r'\1',t);t=re.sub(r'\[([^]]*)\]\(([^)]*)\)',r'\1',t);t=re.sub(r'\*\*(.*?)\*\*',r'<b>\1</b>',t);t=re.sub(r'`([^`]*)`',r'<font color="#087E8B">\1</font>',t)
 return Paragraph(t,styles[k])
def make_table(rows):
 n=len(rows[0]);weights={2:[.32,.68],3:[.20,.39,.41],4:[.17,.22,.30,.31],5:[.23,.19,.19,.19,.20],6:[.16,.16,.17,.17,.17,.17],7:[.13,.18,.10,.18,.10,.19,.12]}.get(n,[1/n]*n)
 cells=[[para(x,'head' if i==0 else 'cell') for x in row] for i,row in enumerate(rows)]
 t=Table(cells,colWidths=[cw*x/sum(weights) for x in weights],repeatRows=1,hAlign='LEFT')
 t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),navy),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white,light]),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),7),('RIGHTPADDING',(0,0),(-1,-1),7),('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6)]));return t
class Doc(SimpleDocTemplate):
 def afterFlowable(self,f):
  if isinstance(f,Paragraph) and hasattr(f,'bookmark'):
   self.canv.bookmarkPage(f.bookmark);self.canv.addOutlineEntry(f.getPlainText(),f.bookmark,level=f.level,closed=False);self.notify('TOCEntry',(f.level,f.getPlainText(),self.page,f.bookmark))
def parse(md):
 lines=md.splitlines();items=[];i=0;hcount=0
 while i<len(lines):
  line=lines[i].strip()
  if not line or line=='---':i+=1;continue
  if line.startswith('```'):
   lang=line[3:];code=[];i+=1
   while i<len(lines) and not lines[i].startswith('```'):code.append(lines[i]);i+=1
   i+=1
   if lang=='mermaid':items.append(para('处理顺序：输入检查 → 候选恢复 → 独立全域距离界 → 裁剪重建 → 面/邻接验收 → 发布或拒绝。','small'));continue
   wrapped=[]
   for x in code:wrapped+=textwrap.wrap(x,width=93,replace_whitespace=False,drop_whitespace=False) or ['']
   items.append(Preformatted('\n'.join(wrapped),styles['code']));continue
  if line.startswith('|'):
   rows=[]
   while i<len(lines) and lines[i].strip().startswith('|'):
    raw=lines[i].strip().strip('|');row=[];cell='';quoted=False
    for char in raw:
     if char=='`':quoted=not quoted
     if char=='|' and not quoted:row.append(cell.strip());cell=''
     else:cell+=char
    row.append(cell.strip());i+=1
    if all(re.fullmatch(r'[:\- ]+',x) for x in row):continue
    rows.append(row)
   if rows:
    n=len(rows[0]);assert all(len(r)==n for r in rows),(n,rows);items.append(make_table(rows));items.append(Spacer(1,8))
   continue
  m=re.match(r'^(#{2,4}) (.*)',line)
  if m:
   level=min(len(m[1])-2,2);f=para(m[2],['h1','h2','h3'][level]);hcount+=1
   if level<2:f.bookmark='heading-'+str(hcount);f.level=level
   items.append(f);i+=1;continue
  if line.startswith('!['):
   m=re.match(r'!\[([^]]*)\]\(([^)]*)\)',line)
   if m:
    path=(content/m[2]).resolve();im=Image(str(path));ratio=min(cw/im.imageWidth,220/im.imageHeight);im.drawWidth=im.imageWidth*ratio;im.drawHeight=im.imageHeight*ratio;items.append(im);items.append(para(m[1],'small'))
   i+=1;continue
  if line.startswith('# '):i+=1;continue
  if re.match(r'^[-*] |^\d+\. ',line):items.append(para(line));i+=1;continue
  block=[line];i+=1
  while i<len(lines) and lines[i].strip() and not re.match(r'^(#{1,4} |\||```|!\[|[-*] |\d+\. )',lines[i].strip()):block.append(lines[i].strip());i+=1
  items.append(para(' '.join(block)))
 return items

def footer(c,d):
 c.saveState();c.setStrokeColor(teal);c.line(44,34,W-44,34);c.setFillColor(grey);c.setFont('Helvetica',7.5);c.drawString(44,22,'BS2QUAD 1.1.0 / source a813309 / 2026-10-16');c.drawRightString(W-44,22,str(d.page));c.restoreState()
def build(name,subtitle,md,cover_image=None,toc=True):
 out=dest/name;doc=Doc(str(out),pagesize=A4,leftMargin=44,rightMargin=44,topMargin=42,bottomMargin=48,title='BS2Quad '+subtitle,author='BS2Quad',pageCompression=1)
 flow=[para('BS2QUAD 1.1.0 · 中船海舟工业软件赛题','small'),Spacer(1,18),para('样条面到二次曲面\n智能转换算法'.replace('\n',' '),'cover'),para(subtitle,'h1'),para('材料依据：已验证源码 a813309；主实验 final-release-v3；制作基线 6172362。当前后端 OCCT 7.7.0，SGK 1.7 接入尚未完成。')]
 if cover_image:
  im=Image(str(cover_image),width=cw,height=cw*2/3);flow+=[Spacer(1,8),im,para('AI 生成的机制示意，非实验结果；G1 仅在提供真实邻面时执行。具体模型版本未由工具回执确认。','small')]
 flow+=[para('本材料分别记录实现、实测、验证范围与未完成条件。64 项单元测试通过；1000 例合成支撑面判定最高 8.247364 ms；CAD 9/15 组达到耗时降低 50%。STEP 导出进程退出的 LSAN 报告仍未解决。','small'),PageBreak() if toc else Spacer(1,16)]
 if toc:
  flow+=[para('目录','h1')];t=TableOfContents();t.levelStyles=[ParagraphStyle('toc1',fontName='CN',fontSize=10,leading=18,leftIndent=0,spaceBefore=5),ParagraphStyle('toc2',fontName='CN',fontSize=8.5,leading=14,leftIndent=12)];flow+=[t,PageBreak()]
 if not toc:flow=[para('BS2QUAD 1.1.0 · source a813309 · 2026-10-16','small'),para(subtitle,'h1'),para('当前后端 OCCT 7.7.0；本指南与 152 秒演示使用同一场景顺序。','small')]
 flow+=parse(md);doc.multiBuild(flow,onFirstPage=footer,onLaterPages=footer);return out

def nested(path):
 s=path.read_text();s=re.sub(r'^# .*\n','',s,count=1);s=re.sub(r'^## (?:\d+\.\s*)?',r'### ',s,flags=re.M);return s
intro='''# BS2Quad 详细作品说明

## 一、任务、使用场景与交付状态

船舶设计人员在大型装配中反复进行投影、求交和布尔操作。输入虽然以 NURBS 保存，其中仍可能包含圆柱、球面等解析结构。本项目先恢复解析候选，再检查该输入是否满足替换条件，避免只凭拟合样点的小误差作接受决定。

本轮目标是提供可复现的 C++ 实现、测试与原始证据。在线路径是确定性几何计算；没有在运行时接入大模型。成熟候选恢复来自 OCCT，本项目工程增量是独立距离界、裁剪与方向验收、成本控制和可追溯结果。尚未完成系统文献查新，不宣称方法首创。

| 状态 | 实际情况 |
|---|---|
| 已实现 | 五类解析支撑面、单裁剪面、指定邻面、有限点拟合、文件与批处理入口 |
| 已运行 | Linux / GCC 11.4 / OCCT 7.7.0；64 单元测试及原始数据回执 |
| 已测量 | 1000 例合成支撑面，15 组同域 CAD 测量；未将其扩张为真实船舶全量验证 |
| 尚未通过 | SGK 1.7、全部 CAD 50%、完整 STEP 出口无泄漏、连续边界与 G1 证明 |
| 本次材料状态 | 生成作品说明、操作指南、真实 CLI 回放和中文旁白；不代表邮件或门户已提交 |

## 二、算法与工程设计

'''
report=intro+nested(root/'docs/design.md')+'\n## 三、验证设计、原始结果与误差分析\n\n'
exp=(root/'docs/experiment_report.md').read_text();ix=exp.index('## 2. 环境');exp=exp[ix:];exp=re.sub(r'^## \d+\. ',r'### ',exp,flags=re.M)
report+=exp.replace('## 7.','### 7.')
report+='''
## 四、真实演示与证据来源

视频展示的是本次远程真实命令的事件回放，原始时间戳、命令、退出码和 stdout/stderr 合并 PTY 流保存为 asciicast v2。终端画面由原始事件重新排版绘制；命令输出顺序与关键字段未改写。执行完成后停留画面讲解，不将展示时长称为计算耗时。几何预览来自实际 BRep 支撑面求值，仅用于可视化。

一千例主统计以归档结果读取展示；录制没有重新跑该性能集合。新录制的圆柱面操作包含实际拓扑重建，其 conversion_ms 不与原主统计的 decision_ms 混用。自由曲面明确拒绝，退出码 1 是预期业务结果；四项针对性回归均返回成功测试结果。

![实际输入输出 BRep 求值预览；采样网格仅作可视化，不构成误差验收依据。](../assets/actual-cylinder.png)

![主实验的 1000 条判定耗时，直接由 cases.csv 绘制。](../assets/decision-times.png)

![15 组同域 CAD 中位耗时降低比例；虚线为 50% 门槛，数据包含未达标项。](../assets/cad-reductions.png)

### 原始依据索引

| 编号 | 依据 | 支持的主张及限制 |
|---|---|---|
| E1 | evidence/20261016/source-final.json、source-readback.json | 51 个源文件与验证配方同源；不包含生成文档 |
| E2 | evidence/20261016/unit-final.xml、final-release-v3/correctness.log | 64 单元测试、5 CTest 组通过；不代表全赛题通过 |
| E3 | final-release-v3/surface1000/cases.csv、inputs.csv、summary.json | 合成数据分类、实际控制点与判定时延，1000 BRep 输入保留 |
| E4 | final-release-v3/cad/raw.csv、summary.json、输入及结果 BRep | 15 组测量有效，9 达标；有限采样结果检查 |
| E5 | asan-ctest.log、asan1000.log、valgrind1000.log | 已运行范围与失败；STEP LSAN 未解决 |
| E6 | evidence/20261016/occt-step-control/ | 不含 BS2Quad 的 STEP 泄漏复现，固定有限对照 |
| E7 | submission-studio/evidence/real-demo/ | 新录制命令、时间、退出码、实际输出与二进制哈希 |
| E8 | submission-studio/assets/*.provenance.json、*.py 生成入口 | AI 示意与数据图来源分离；不能把示意图当实验截图 |

## 五、集成工作与提交前缺口

最高优先级是取得真实 SGK 1.7 SDK 并完成目标环境适配，然后用相同反例、原始输入与真实船舶裁剪面复验。未经实际初始化、编译链接和运算回执，不修改兼容性标志。当前 C++ 原型的可复现性不替代赛题指定内核要求。

其后分别优化未达到 50% 的投影和布尔工作负载，保留相同查询、操作数与有效输出条件。次数缓存的最终版本严格单因素消融尚未执行；早期跨版本优化记录只作为研发观测。边界和法向的连续证书、装配级事务替换与真实工业数据回放仍需独立完成。

官方活动页 https://www.oschina.net/os2026/ 的搜索索引与用户提供文本均给出 10 月 16 日 24:00 及 oscc@oschina.cn；本次页面直读未取得完整正文，规则具体版本以组委会通知为准。材料采用 Asia/Shanghai 时区。本次没有发送邮件、配置外部评委访问、上传门户或取得提交回执。

### 复现入口

安装见 INSTALL.md，完整验收使用 scripts/verify.sh。脚本拒绝覆盖非空证据目录，任何硬指标失败都会保留非零状态。当前完整验收返回 1，原因是 SGK 兼容门槛与 CAD 50% 门槛未通过。操作指南与视频使用相同场景编号，旁白、事件流及包清单另附。
'''
(content/'detailed-report.md').write_text(report)
scenes=json.loads((studio/'scenes.json').read_text())
guide='''# BS2Quad 演示与操作指南

## 一、前提与范围

本指南与演示视频使用相同 S01-S07 场景编号。当前支持单个 NURBS 裁剪面文件，输入输出可用 BRep/STEP。多面装配替换需要上层提供邻接上下文；本指南不把孤立面转换当作装配级替换。

在远程 Linux 安装 OCCT 7.7.0、C++17 编译器、CMake 与 Google Test，具体命令见工程根目录 INSTALL.md。设置 OpenCASCADE_DIR 和 LD_LIBRARY_PATH 后构建。原始核心代码不因本轮材料制作发生改变。

```bash
export OpenCASCADE_DIR=/path/to/occt770/lib/cmake/opencascade
export LD_LIBRARY_PATH=/path/to/occt770/lib
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure --no-tests=error
```

## 二、逐场景操作与观察

'''
cmds={
'S01':'查看 assets/mechanism.png 及 provenance，理解候选与独立验收的边界。',
'S02':'./build/bs2quad_main --help',
'S03':'./build/bs2quad_main --input evidence/20261016/final-release-v3/surface1000/inputs/case_1.brep --output converted-cylinder.brep --report cylinder.json',
'S04':'./build/bs2quad_main --input evidence/20261016/final-release-v3/surface1000/inputs/case_5.brep --output rejected.brep --report rejected.json',
'S05':"./build/unit_tests --gtest_filter='ValidationRegression.FullDomainCertificateRejectsNarrowBumpBetweenAllSamplingColumns:ValidationRegression.FaceOrientationReversalIsNotNormalPreservation:ValidationRegression.ReverseBoundaryDirectionDetectsAnExtraInteriorHole:ValidationRegression.ConversionWithActualNeighborsAcceptsSmoothAndRejectsCreasedFaces'",
'S06':'cat evidence/20261016/final-release-v3/surface1000/summary.json',
'S07':'cat evidence/20261016/final-release-v3/cad/summary.json'}
observes={'S01':'AI 机制示意不构成实验结果；G1 需要实际邻面输入。','S02':'版本 1.1.0，OCCT 后端；帮助文本明确 SGK 尚未接入。','S03':'退出 0；success=true，type=Cylinder，保存新几何与报告。判定与完整转换耗时分别记录。','S04':'退出 1；success=false，拒绝自由曲面并保存原因；没有可发布的新几何。','S05':'四项固定回归通过。覆盖窄凸起、反向法向、多孔边界与平滑/折角邻面。','S06':'这是 2026-10-16 主实验的归档读取；834/166、0 FP/FN、控制点50-200、max8.247364ms。','S07':'15 项有效、9 项达到50%；SGK、连续边界/G1及STEP出口仍有缺口。'}
for s in scenes['segments']:
 guide+='### '+s['id']+' '+s['title']+'\n\n'+('操作：'+cmds[s['id']]+'\n\n' if s['id']=='S01' else '```bash\n'+cmds[s['id']]+'\n```\n\n')+'预期观察：'+observes[s['id']]+'\n\n'
guide+='''
## 三、结果字段与错误处理

| 字段 / 状态 | 含义与处理 |
|---|---|
| success / error / message | 先检查成功状态和错误原因，再使用输出几何 |
| directed_support_bound_mm | 原样条完整参数域到解析支撑面的单向上界 |
| sampled_boundary_hausdorff_mm | 双向边界并集的有限采样诊断；不是连续域证书 |
| normal_deviation_degrees | 原面与转换面的有向法向保持，不代替相邻面 G1 |
| adjacency_checked | 只有提供并检查真实邻面才为真 |
| null 数值 | 未执行或无法可靠得到的量，不应解释为零误差 |
| 退出码 0 / 1 / 2 | 成功 / 明确拒绝转换 / 参数、文件或内核错误 |
| 输入输出冲突 | 输入、输出与报告应为不同文件；符号链接别名也会拒绝 |
| SGK 配置失败 | 当前不支持真实 SGK 构建；不要建立假接口绕过失败 |

## 四、复现与保留回执

```bash
./build/bs2quad_evidence --count 1000 --output-dir evidence/a-new-run
./build/bs2quad_cad_benchmark --output-dir evidence/a-new-cad --iterations 5
bash scripts/verify.sh build evidence/a-new-verification
```

使用新的输出目录。返回码为 0 的 CAD 测量只表示有效执行；全部达到 50% 需另查 all_operation_targets_pass。完整脚本会检查该门槛及 SGK 标志。内存工具下的耗时不用于十毫秒指标。

本视频保留真实远程执行事件，执行完成后停留画面讲解；未缩短等待、未加速或截断旁白。终端视窗会滚动，完整输出另附。几何图来自实际 BRep 求值，统计图来自主实验 CSV。中文声音来自本机 macOS Tingting；主观自然度与逐字发音仍需人工试听确认。

## 五、验收与提交状态

查看 qa/ 下的 PDF、旁白、视频和包审计记录。字节校验通过不代表几何硬指标通过、主观听审通过或主办方接受。此材料包尚未发送邮件或上传门户。
'''
(content/'operator-guide.md').write_text(guide)
outputs=[build('BS2Quad_详细作品说明_20261016.pdf','详细作品说明与实测证据',report,studio/'assets/mechanism.png'),build('BS2Quad_演示操作指南_20261016.pdf','演示与复现操作指南',guide,toc=False)]
print('\n'.join(map(str,outputs)))
