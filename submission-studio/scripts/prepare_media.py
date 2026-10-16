#!/usr/bin/env python3
"""Prepare evidence-derived charts, source-evaluated geometry preview and narration."""
from pathlib import Path
import json,csv,subprocess,math,hashlib,datetime,argparse
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties
root=Path(__file__).resolve().parents[2]; studio=root/'submission-studio';assets=studio/'assets'
parser=argparse.ArgumentParser();parser.add_argument('--font',default='/System/Library/Fonts/Supplemental/Arial Unicode.ttf');args=parser.parse_args()
font=FontProperties(fname=args.font)
plt.rcParams.update({'font.family':font.get_name(),'axes.unicode_minus':False,'font.size':11,'svg.fonttype':'none'})
rows=list(csv.DictReader((root/'evidence/20261016/final-release-v3/surface1000/cases.csv').open()))
fig,ax=plt.subplots(figsize=(10,4));ax.scatter([int(r['id']) for r in rows],[float(r['decision_ms']) for r in rows],s=5,color='#087E8B',alpha=.6);ax.axhline(10,color='#AD6B16',linestyle='--',label='10 ms target');ax.set(xlabel='Case ID',ylabel='Support decision time (ms)',ylim=(0,11));ax.grid(alpha=.15);ax.legend();fig.tight_layout();fig.savefig(assets/'decision-times.png',dpi=180);fig.savefig(assets/'decision-times.svg');plt.close(fig)
cad=json.loads((root/'evidence/20261016/final-release-v3/cad/summary.json').read_text());fig,ax=plt.subplots(figsize=(10,4.5));labels=['Plane','Cylinder','Sphere','Cone','Torus'];x=np.arange(5);colors=['#087E8B','#243D58','#B37B25']
for j,op in enumerate(['projection','intersection','boolean']):
 values=[100*r['reduction_fraction'] for r in cad['results'] if r['operation']==op];bars=ax.bar(x+(j-1)*.24,values,width=.23,color=colors[j],label=op)
 for bar,value in zip(bars,values):ax.text(bar.get_x()+bar.get_width()/2,value+1,f'{value:.1f}',ha='center',fontsize=8)
ax.axhline(50,color='#AA4F40',linestyle='--');ax.set_xticks(x,labels);ax.set(ylabel='Median time reduction (%)',ylim=(0,102));ax.legend(ncol=3,frameon=False);ax.grid(axis='y',alpha=.12);fig.tight_layout();fig.savefig(assets/'cad-reductions.png',dpi=180);fig.savefig(assets/'cad-reductions.svg');plt.close(fig)
paths=[studio/'evidence/real-demo/source-geometry.json',studio/'evidence/real-demo/converted-geometry.json']
if all(p.exists() for p in paths):
 fig=plt.figure(figsize=(10,5.5))
 for i,p in enumerate(paths):
  d=json.loads(p.read_text());a=np.array(d['points']).reshape(d['rows'],d['cols'],3);ax=fig.add_subplot(1,2,i+1,projection='3d');ax.plot_surface(a[:,:,0],a[:,:,1],a[:,:,2],color=['#5BA2BB','#4C9F83'][i],alpha=.55,linewidth=0);ax.plot_wireframe(a[:,:,0],a[:,:,1],a[:,:,2],rstride=5,cstride=5,color='#173B56',linewidth=.5);ax.set_title(['Input NURBS BRep','Recovered analytic BRep'][i]);ax.set_axis_off();ax.view_init(25,-60);mins=a.reshape(-1,3).min(axis=0);maxs=a.reshape(-1,3).max(axis=0);ax.set_box_aspect(maxs-mins)
 fig.tight_layout();fig.savefig(assets/'actual-cylinder.png',dpi=180,transparent=False);plt.close(fig)
texts=[
('S01','项目与核心矛盾','在船舶设计中，圆柱和球面常被统一保存为复杂的样条。恢复解析表达可以减少后续几何计算，但采样点很近，并不代表整张面都能接受。我们的方案先恢复候选，再独立检查接受条件。'),
('S02','接口与处理流程','当前工程使用真实的开源几何内核。命令行支持演示、生成数据，以及单个裁剪面的文件转换。候选恢复之后，还要检查完整参数域的距离上界；裁剪边界和法向另行验收。'),
('S03','真实圆柱面转换','这里运行一张真实输入文件的圆柱面转换。成功后生成新的几何文件和报告。画面中的几何预览来自实际输入输出的求值。报告分别记录判定时间与完整转换时间，没有把拓扑重建混入十毫秒判定指标。'),
('S04','自由曲面明确拒绝','接着输入自由曲面。程序明确返回拒绝状态，退出码为一，并保存原因。没有有效候选时，不会发布一个看似成功的解析结果。未知的误差字段保持未知，不把它们写成零。'),
('S05','针对性反例回归','现在重放四个真实回归：采样之间的窄凸起、反向法向、额外内孔，以及真实邻面的平滑与折角。测试通过，说明这些固定反例获得了预期处理。它们不等于所有连续边界和法向都已得到数学证明。'),
('S06','已保存的一千例实验','下面读取十月十六日保存的主实验，不是重新运行性能测试。一千个合成输入中，八百三十四个正例正确接受，一百六十六个反例正确拒绝。实际控制点为五十到二百；这组支撑面判定的最高耗时为八点二五毫秒。'),
('S07','性能结果与集成边界','同一有限裁剪域的十五组操作中，九组达到耗时降低百分之五十，六组没有达到。海星几何内核一点七版本尚未接入，完整的文件导出路径仍有内存退出检查失败。代码、原始数据和失败记录均随材料提供。')]
receipts=json.loads((studio/'evidence/real-demo/receipts.json').read_text());byid={x['id']:x for x in receipts['records']};segments=[]
for sid,title,text in texts:
 t=studio/'narration'/f'{sid}.txt';t.write_text(text);aiff=studio/'narration'/f'{sid}.aiff';wav=studio/'narration'/f'{sid}.wav'
 subprocess.run(['say','-v','Tingting','-r','175','-f',str(t),'-o',str(aiff)],check=True)
 subprocess.run(['ffmpeg','-v','error','-y','-i',str(aiff),'-ar','24000','-ac','1',str(wav)],check=True)
 probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries','format=duration','-of','json',str(wav)]));length=float(probe['format']['duration']);duration=math.ceil(length+1.8)
 evidence=[{'path':'evidence/real-demo/'+sid+'.cast','date':'2026-10-16'}] if sid in byid else [{'path':'assets/mechanism.provenance.json' if sid=='S01' else '../evidence/20261016/final-release-v3/cad/summary.json','date':'2026-10-16'}]
 segments.append({'id':sid,'title':title,'duration_seconds':duration,'lead_in_seconds':.6,'kind':'mechanism' if sid=='S01' else 'terminal' if sid in ['S02','S04','S05'] else 'data','audio':f'narration/{sid}.wav','narration':text,'audio_duration_seconds':length,'action':byid[sid]['command'] if sid in byid else 'Explain evidence-derived mechanism or CAD results','observe':'Verify actual source fields and retain explicitly unmet requirements','evidence':evidence,'edit_note':'Remote output replay from original timed events; after real execution, hold result for narration. Geometry preview and plots are evidence-derived. Archived benchmark readback labelled; no wait shortening or narration acceleration.'})
scenes={'schema_version':1,'duration_seconds':sum(s['duration_seconds'] for s in segments),'segments':segments};(studio/'scenes.json').write_text(json.dumps(scenes,ensure_ascii=False,indent=2)+'\n')
(studio/'narration/voice-config.json').write_text(json.dumps({'provider':'Local macOS say','voice':'Tingting','locale':'zh_CN','rate':175,'selected_by':'agent judgment, no prior user voice preference','sample':'voice-sample.wav','sample_text':'voice-sample.txt','online_service_used':False,'paid_service_used':False,'playback_rate_changes':False,'listening_status':'Sample emitted in conversation for listening; subjective naturalness not independently verified','generation_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()},ensure_ascii=False,indent=2)+'\n')
print('Narration generated:',scenes['duration_seconds'],'seconds',[(s['id'],s['audio_duration_seconds'],s['duration_seconds']) for s in segments])
