#!/usr/bin/env python3
"""Render timed real-output replay + evidence charts, without changing audio rate."""
from pathlib import Path
import json,math,re,textwrap,wave,subprocess,hashlib,datetime,argparse
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[2];s=root/'submission-studio';scenes=json.loads((s/'scenes.json').read_text());frames=s/'video/frames';frames.mkdir(exist_ok=True)
parser=argparse.ArgumentParser();parser.add_argument('--font',default='/System/Library/Fonts/Supplemental/Arial Unicode.ttf');parser.add_argument('--mono-font',default='/System/Library/Fonts/Menlo.ttc');args=parser.parse_args()
font_path=args.font;mono_path=args.mono_font
f=lambda z:ImageFont.truetype(font_path,z);mono=lambda z:ImageFont.truetype(mono_path,z)
navy='#132B43';teal='#087E8B';grey='#607184';light='#EDF4F6';W,H=1920,1080
receipts=json.loads((s/'evidence/real-demo/receipts.json').read_text());records={r['id']:r for r in receipts['records']}
def txt(d,xy,t,size=32,fill=navy):d.text(xy,t,font=f(size),fill=fill)
def wrap(text,limit):
 return '\n'.join(textwrap.wrap(text,width=limit,break_long_words=True,replace_whitespace=False,drop_whitespace=False))
def image_fit(base,path,box):
 im=Image.open(path).convert('RGB');im.thumbnail((box[2]-box[0],box[3]-box[1]),Image.Resampling.LANCZOS);base.paste(im,(box[0]+(box[2]-box[0]-im.width)//2,box[1]+(box[3]-box[1]-im.height)//2))
def base_frame(seg,local,caption):
 im=Image.new('RGB',(W,H),'white');d=ImageDraw.Draw(im);d.rectangle((0,0,W,12),fill=teal);txt(d,(60,38),'BS2QUAD  /  C++ GEOMETRY  /  OCCT 7.7.0',24,teal);txt(d,(60,82),seg['id']+'  '+seg['title'],44)
 d.line((60,150,1860,150),fill='#CBD8DF',width=2)
 sid=seg['id']
 if sid=='S01':
  image_fit(im,s/'assets/mechanism.png',(55,170,1300,905));d=ImageDraw.Draw(im)
  for y,t,z in [(230,'先恢复候选',42),(310,'再检查接受条件',42),(435,'完整域：单向支撑距离界',28),(500,'裁剪 / G1：有限采样验收',28),(610,'五类解析面',36),(680,'源码与失败记录可核查',28)]:txt(d,(1320,y),t,z)
  txt(d,(80,862),'AI 机制示意；非实验截图。G1 仅在提供真实邻面时检查。',22,grey)
 elif sid=='S03':
  image_fit(im,s/'assets/actual-cylinder.png',(50,185,1140,830));d=ImageDraw.Draw(im)
  data=json.loads((s/'evidence/real-demo/converted-cylinder.json').read_text());x=1180
  txt(d,(x,200),'实际转换报告',32,teal)
  values=[('success',str(data['success']).lower()),('type',data['type']),('decision_ms',f"{data['decision_ms']:.6f}"),('conversion_ms',f"{data['conversion_ms']:.6f}"),('support bound (mm)',f"{data['directed_support_bound_mm']:.3e}"),('adjacency_checked',str(data['adjacency_checked']).lower())]
  for i,(key,value) in enumerate(values):txt(d,(x,265+i*83),key,25,grey);txt(d,(x+350,265+i*83),value,29)
  txt(d,(70,850),'真实 BRep 求值预览，仅用于可视化；右侧 JSON 字段重排展示。',23,grey)
 elif sid=='S06':
  image_fit(im,s/'assets/decision-times.png',(60,180,1300,805));d=ImageDraw.Draw(im)
  for y,t,z in [(225,'1000 个合成输入',38),(315,'834 正确接受',34),(385,'166 正确拒绝',34),(465,'错误接受 / 拒绝：0',30),(555,'实际控制点 50-200',30),(655,'判定最高 8.247 ms',36)]:txt(d,(1320,y),t,z)
  txt(d,(80,840),'归档主实验：2026-10-16 / final-release-v3；本场景未重新运行该性能测试。',24,grey)
 elif sid=='S07':
  image_fit(im,s/'assets/cad-reductions.png',(60,180,1240,800));d=ImageDraw.Draw(im)
  for y,t,z in [(210,'9 / 15 组达到 50%',38),(300,'6 组未达到目标',32),(430,'SGK 1.7：尚未接入',31),(520,'边界 / G1：采样检查',31),(610,'STEP 退出泄漏：未解决',29)]:txt(d,(1280,y),t,z)
  txt(d,(75,840),'同域、同查询与操作数；1 次预热 + 5 次正式测量；中位数。',24,grey)
 else:
  d.rounded_rectangle((60,180,1860,840),radius=14,fill='#102235');txt(d,(85,196),'REMOTE CLI EVENT REPLAY  ·  2026-10-16  ·  source a813309',23,'#7BC7D1')
  events=[json.loads(x) for x in (s/'evidence/real-demo'/f'{sid}.cast').read_text().splitlines()[1:]];text=''.join(e[2] for e in events if e[0]<=local)
  text=re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]','',text).replace('\r','');lines=[]
  for line in text.split('\n'):lines+=textwrap.wrap(line,width=123,replace_whitespace=False,drop_whitespace=False) or ['']
  if sid=='S05':
   first=next((i for i,line in enumerate(lines) if line.startswith('[==========] Running')),0);lines=lines[first:]
  lines=lines[-20:]
  for i,line in enumerate(lines):d.text((85,242+i*28),line,font=mono(22),fill='#E2EDF5')
  rec=records[sid];txt(d,(75,858),f"真实进程 {rec['actual_elapsed_seconds']:.3f} s  /  exit {rec['exit_code']}  /  完成后停留讲解；完整事件流另附",23,grey)
 d=ImageDraw.Draw(im);d.rectangle((0,910,W,1030),fill=light)
 if caption:
  lines=wrap(caption,47).split('\n');y=930 if len(lines)>1 else 947
  for line in lines:txt(d,(85,y),line,32);y+=44
 txt(d,(65,1043),'原始命令与输出可核查 · 语音未加速或截断 · 仅作工程原型演示',21,grey)
 return im

def captions(text,duration):
 clauses=[x for x in re.split(r'(?<=[，。；：])',text) if x];chunks=[];buf=''
 for c in clauses:
  if len(buf+c)>44 and buf:chunks.append(buf);buf=''
  buf+=c
 if buf:chunks.append(buf)
 total=sum(map(len,chunks));out=[];t=.6
 for c in chunks:
  length=duration*len(c)/total;out.append((t,t+length,c));t+=length
 return out
frame_list=[];offset=0;srt=[];metadata=[];wav_path=s/'narration/final-narration.wav'
with wave.open(str(wav_path),'wb') as result:
 result.setnchannels(1);result.setsampwidth(2);result.setframerate(24000)
 for seg in scenes['segments']:
  sid=seg['id'];length=seg['duration_seconds'];audio=seg['audio_duration_seconds'];caps=captions(seg['narration'],audio);events=[]
  if sid in ['S02','S04','S05']:events=[json.loads(x)[0] for x in (s/'evidence/real-demo'/f'{sid}.cast').read_text().splitlines()[1:]]
  times=sorted({0.0,float(length),*[round(t,6) for t,_,_ in caps],*[round(t,6) for _,t,_ in caps],*[min(t,float(length)) for t in events]})
  for i,(t,end) in enumerate(zip(times,times[1:])):
   if end<=t:continue
   cap=next((text for start,stop,text in caps if start<=t+.001<stop),'');im=base_frame(seg,t+.001,cap);path=frames/f'{sid}-{i:03d}.png';im.save(path);frame_list.append((path,end-t))
  with wave.open(str(s/seg['audio']),'rb') as source:
   assert source.getframerate()==24000 and source.getnchannels()==1 and source.getsampwidth()==2
   raw=source.readframes(source.getnframes());lead=int(.6*24000);pad=int(length*24000)-lead-source.getnframes();assert pad>=0
   result.writeframes(b'\0\0'*lead+raw+b'\0\0'*pad)
  metadata.append({'id':sid,'video_start_seconds':offset,'scene_seconds':length,'actual_command_seconds':records[sid]['actual_elapsed_seconds'] if sid in records else None,'editing':'Time-faithful event replay then freeze; terminal view redraw/scroll; evidence-derived plot cards; captions timed by text length, not forced alignment'});offset+=length
playlist=s/'video/frames.ffconcat';playlist.write_text('ffconcat version 1.0\n'+''.join("file '"+str(p.resolve()).replace("'","'\\''")+"'\nduration "+str(d)+'\n' for p,d in frame_list)+"file '"+str(frame_list[-1][0].resolve())+"'\n")
output=s/'video/BS2Quad_真实演示_20261016.mp4';cmd=['ffmpeg','-v','error','-y','-f','concat','-safe','0','-i',str(playlist),'-i',str(wav_path),'-t',str(offset),'-vf','fps=24,format=yuv420p','-c:v','libx264','-preset','fast','-crf','23','-c:a','aac','-b:a','128k','-movflags','+faststart',str(output)]
(s/'video/encoding-command.json').write_text(json.dumps(cmd,ensure_ascii=False,indent=2)+'\n');subprocess.run(cmd,check=True)
(s/'video/edit-receipt.json').write_text(json.dumps({'duration_seconds':offset,'scenes':metadata,'source_receipt':'evidence/real-demo/receipts.json','audio_processing':'PCM concatenation with exact silence lead/pad only; no rate change or truncation','caption_alignment':'Proportional text-length segments; subjective synchronization review still required','output_sha256':hashlib.sha256(output.read_bytes()).hexdigest()},ensure_ascii=False,indent=2)+'\n')
print('Rendered',output,'seconds',offset,'frames',len(frame_list))
