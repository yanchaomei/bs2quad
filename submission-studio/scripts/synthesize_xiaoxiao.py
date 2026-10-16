#!/usr/bin/env python3
"""Microsoft Xiaoxiao narration with native slower prosody and service subtitles."""
import argparse,subprocess,json,hashlib,math,re,datetime,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[2];old=root/'submission-studio';v=old/'xiaoxiao';p=argparse.ArgumentParser();p.add_argument('--client',required=True);a=p.parse_args();sc=json.loads((old/'scenes.json').read_text());receipts=[]
replace={'复杂的样条':'非均匀有理样条曲面','新的几何文件和报告':'新的边界表示模型文件和报告','完整的文件导出路径':'通用三维模型格式的完整导出路径'}
for seg in sc['segments']:
 sid=seg['id'];text=seg['narration']
 for x,y in replace.items():text=text.replace(x,y)
 txt=v/'narration'/f'{sid}.txt';txt.write_text(text);mp3=v/'narration'/f'{sid}.mp3';srt=v/'narration'/f'{sid}.srt';wav=v/'narration'/f'{sid}.wav'
 cmd=[a.client,'--voice','zh-CN-XiaoxiaoNeural','--rate=-8%','--file',str(txt),'--write-media',str(mp3),'--write-subtitles',str(srt)]
 proc=subprocess.run(cmd,capture_output=True,text=True,timeout=50);log=re.sub(r'(TrustedClientToken|ConnectionId|Sec-MS-GEC)=[^&\s\x27\x22]+',r'\1=[REDACTED]',proc.stdout+proc.stderr);(v/'qa'/f'{sid}-synthesis.log').write_text(log)
 if proc.returncode:raise RuntimeError(f'{sid}: synthesis failed, exit {proc.returncode}')
 subprocess.run(['ffmpeg','-v','error','-y','-i',str(mp3),'-ar','24000','-ac','1','-c:a','pcm_s16le',str(wav)],check=True)
 length=float(json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries','format=duration','-of','json',str(wav)]))['format']['duration']);seg.update({'narration':text,'audio_duration_seconds':length,'duration_seconds':math.ceil(length+2),'audio':f'narration/{sid}.wav','lead_in_seconds':.6,'edit_note':'Microsoft Xiaoxiao native prosody rate -8%; no playback rate alteration or truncation. Original real-demo record reused; scene holds extended to fit full narration; subtitles are optional.'})
 receipts.append({'id':sid,'command':cmd,'exit_code':proc.returncode,'voice':'zh-CN-XiaoxiaoNeural','rate':'-8%','mp3_sha256':hashlib.sha256(mp3.read_bytes()).hexdigest(),'wav_sha256':hashlib.sha256(wav.read_bytes()).hexdigest(),'text_sha256':hashlib.sha256(text.encode()).hexdigest(),'subtitle_sha256':hashlib.sha256(srt.read_bytes()).hexdigest(),'audio_seconds':length});print(sid,length,flush=True)
sc['duration_seconds']=sum(x['duration_seconds'] for x in sc['segments']);(v/'scenes.json').write_text(json.dumps(sc,ensure_ascii=False,indent=2)+'\n')
config={'provider':'Microsoft Edge online TTS via edge-tts','voice':'zh-CN-XiaoxiaoNeural','display_name':'微软晓晓普通话神经女声','locale':'zh-CN','rate':'-8%','client_version':subprocess.check_output([a.client,'--version'],text=True).strip(),'selected_by':'explicit user request','sample_actual_success':'voice-sample-v3.mp3','online_service_used':True,'external_paid_api_used':False,'audio_playback_rate_changes':False,'audio_truncation':False,'subtitle_mode':'service boundary times; optional SRT/VTT and switchable MP4 track','voice_source':'https://learn.microsoft.com/en-us/azure/ai-services/speech-service/language-support?tabs=tts','client_source':'https://github.com/rany2/edge-tts','pronunciation_glossary':{'NURBS':'非均匀有理样条曲面','BRep':'边界表示模型','CAD':'计算机辅助设计','SGK':'海星几何内核','G1':'切向连续性','SDK':'开发工具包','STEP':'通用三维模型交换格式'},'listening_scope':'Sample supplied for user listening; independent subjective listening not certified','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
(v/'narration/voice-config.json').write_text(json.dumps(config,ensure_ascii=False,indent=2)+'\n');(v/'qa/synthesis-receipts.json').write_text(json.dumps(receipts,ensure_ascii=False,indent=2)+'\n');print('TOTAL',sc['duration_seconds'],flush=True)
