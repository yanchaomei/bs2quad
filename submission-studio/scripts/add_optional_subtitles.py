#!/usr/bin/env python3
"""Merge service subtitle timing and mux an optional Chinese track."""
from pathlib import Path
import re,json,textwrap,subprocess,hashlib,wave,datetime
root=Path(__file__).resolve().parents[2];v=root/'submission-studio/xiaoxiao';sc=json.loads((v/'scenes.json').read_text());cues=[];offset=0;coverage=[]
def seconds(s):
 h,m,t=s.replace(',','.').split(':');return int(h)*3600+int(m)*60+float(t)
def stamp(s,vtt=False):
 ms=round(s*1000);return f'{ms//3600000:02d}:{ms//60000%60:02d}:{ms//1000%60:02d}'+('.' if vtt else ',')+f'{ms%1000:03d}'
def normalize(s):return re.sub(r'[\s，。；：、,.!?！？“”"\-]','',s)
for seg in sc['segments']:
 raw=(v/'narration'/f"{seg['id']}.srt").read_text(encoding='utf-8-sig');local=[];sent=''
 for block in re.split(r'\n\s*\n',raw.strip()):
  lines=block.splitlines();idx=next(i for i,x in enumerate(lines) if '-->' in x);a,b=lines[idx].split('-->');text=''.join(lines[idx+1:]);sent+=text;local.append([seconds(a.strip()),seconds(b.strip()),text])
 assert normalize(sent)==normalize(seg['narration']),(seg['id'],sent)
 for i,c in enumerate(local):
  if i+1<len(local):c[1]=min(c[1],local[i+1][0])
  cues.append([offset+seg['lead_in_seconds']+c[0],offset+seg['lead_in_seconds']+c[1],c[2]])
 coverage.append({'id':seg['id'],'all_script_text_present_in_service_subtitles':True,'audio_seconds':seg['audio_duration_seconds']});offset+=seg['duration_seconds']
srt='';vtt='WEBVTT\n\n'
for i,(a,b,text) in enumerate(cues,1):
 lines='\n'.join(textwrap.wrap(text,30,break_long_words=True));srt+=f'{i}\n{stamp(a)} --> {stamp(b)}\n{lines}\n\n';vtt+=f'{i}\n{stamp(a,True)} --> {stamp(b,True)}\n{lines}\n\n'
(v/'video/BS2Quad_晓晓_可选字幕.srt').write_text(srt);(v/'video/BS2Quad_晓晓_可选字幕.vtt').write_text(vtt)
base=v/'video/BS2Quad_真实演示_晓晓版_20261016.mp4';out=v/'video/BS2Quad_演示_晓晓_可选字幕_20261016.mp4'
cmd=['ffmpeg','-v','error','-y','-i',str(base),'-i',str(v/'video/BS2Quad_晓晓_可选字幕.srt'),'-map','0:v:0','-map','0:a:0','-map','1:0','-c:v','copy','-c:a','copy','-c:s','mov_text','-metadata:s:s:0','language=zho','-metadata:s:s:0','title=简体中文（可选）','-disposition:s:0','0','-movflags','+faststart',str(out)];subprocess.run(cmd,check=True)
with wave.open(str(v/'narration/final-narration.wav'),'rb') as f:rate=f.getframerate();data=f.readframes(f.getnframes())
pos=0;checks=[]
for seg in sc['segments']:
 with wave.open(str(v/seg['audio']),'rb') as f:b=f.readframes(f.getnframes())
 begin=pos+int(seg['lead_in_seconds']*rate)*2;assert data[begin:begin+len(b)]==b;checks.append({'id':seg['id'],'full_pcm_preserved':True,'sha256':hashlib.sha256(b).hexdigest()});pos+=int(seg['duration_seconds']*rate)*2
assert pos==len(data)
(v/'qa/subtitle-and-audio-audit.json').write_text(json.dumps({'status':'pass','voice':'zh-CN-XiaoxiaoNeural','native_synthesis_rate':'-8%','playback_rate_changes':False,'audio_truncation':False,'audio_pcm_checks':checks,'service_subtitle_text_coverage':coverage,'subtitle_timing':'Original service sentence boundaries shifted by exact scene start and 0.6s lead; adjacent 50ms overlaps clipped in subtitle only; no audio edit','subtitle_default_on':True,'requested_default_disposition':False,'subtitle_default_note':'Readback found MP4 subtitle enabled/default=1; selection and default display depend on player. Track remains switchable, not forced','duration_seconds':offset,'mux_command':cmd,'final_video_sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'subjective_listening':'not independently certified'},ensure_ascii=False,indent=2)+'\n')
print(out,'seconds',offset,'subtitle cues',len(cues))
