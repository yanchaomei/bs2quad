#!/usr/bin/env python3
"""Capture actual command output with wall-clock receipts and asciicast v2 events."""
import argparse,datetime,hashlib,json,os,pty,select,shlex,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--binary-dir',required=True);p.add_argument('--archive-dir',required=True);p.add_argument('--container');p.add_argument('--container-output');p.add_argument('--host-root');a=p.parse_args()
out=Path(a.output);out.mkdir(parents=True,exist_ok=False)
bin=Path(a.binary_dir);arc=Path(a.archive_dir);q=lambda x:shlex.quote(str(x))
input_dir=arc/'surface1000/inputs'
output_in_command=Path(a.container_output) if a.container_output else out
def host(path):
 return Path(a.host_root)/Path(path).relative_to('/work') if a.host_root else Path(path)
filter='ValidationRegression.FullDomainCertificateRejectsNarrowBumpBetweenAllSamplingColumns:ValidationRegression.FaceOrientationReversalIsNotNormalPreservation:ValidationRegression.ReverseBoundaryDirectionDetectsAnExtraInteriorHole:ValidationRegression.ConversionWithActualNeighborsAcceptsSmoothAndRejectsCreasedFaces'
steps=[('S02',q(bin/'bs2quad_main')+' --help',0),('S03',q(bin/'bs2quad_main')+' --input '+q(input_dir/'case_1.brep')+' --output '+q(output_in_command/'converted-cylinder.brep')+' --report '+q(output_in_command/'converted-cylinder.json'),0),('S04',q(bin/'bs2quad_main')+' --input '+q(input_dir/'case_5.brep')+' --output '+q(output_in_command/'rejected-freeform.brep')+' --report '+q(output_in_command/'rejected-freeform.json'),1),('S05',q(bin/'unit_tests')+' --gtest_color=no --gtest_filter='+q(filter),0),('S06','cat '+q(arc/'surface1000/summary.json'),0)]
records=[]
for sid,cmd,expected in steps:
 start=time.monotonic();wall=datetime.datetime.now(datetime.timezone.utc).isoformat();cast=out/(sid+'.cast');raw=out/(sid+'.txt')
 with cast.open('w') as stream,raw.open('wb') as text:
  stream.write(json.dumps({'version':2,'width':112,'height':32,'timestamp':int(time.time()),'env':{'TERM':'xterm-256color'},'title':sid+' real BS2Quad command'})+'\n')
  def event(t):
   stream.write(json.dumps([round(time.monotonic()-start,6),'o',t],ensure_ascii=False)+'\n');stream.flush();text.write(t.encode());text.flush()
  event('$ '+cmd+'\r\n');master,slave=pty.openpty();env=os.environ.copy();env['TERM']='dumb';env['GTEST_COLOR']='no'
  actual=['docker','exec','-e','LD_LIBRARY_PATH=/work/occt770/lib','-e','OMP_NUM_THREADS=1',a.container,'bash','-c',cmd] if a.container else ['/bin/bash','-c',cmd]
  child=subprocess.Popen(actual,stdin=subprocess.DEVNULL,stdout=slave,stderr=slave,env=env);os.close(slave)
  while True:
   ready,_,_=select.select([master],[],[],.1)
   if ready:
    try:data=os.read(master,65536)
    except OSError:data=b''
    if not data:break
    event(data.decode('utf-8',errors='replace'))
   elif child.poll() is not None:break
  os.close(master);rc=child.wait();event('\r\n[process exit='+str(rc)+']\r\n')
 duration=time.monotonic()-start
 records.append({'id':sid,'command':cmd,'actual_launcher':actual,'started_utc':wall,'ended_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'actual_elapsed_seconds':duration,'exit_code':rc,'expected_exit_code':expected,'expected_exit_observed':rc==expected,'cast':cast.name,'raw':raw.name})
receipts={'scope':'Actual remote execution capture, no generated or post-hoc typed output','records':records,'binary_sha256':{x.name:hashlib.sha256(host(x).read_bytes()).hexdigest() for x in [bin/'bs2quad_main',bin/'unit_tests',bin/'bs2quad_evidence',bin/'bs2quad_cad_benchmark']},'archive_summary_sha256':hashlib.sha256(host(arc/'surface1000/summary.json').read_bytes()).hexdigest(),'input_sha256':{x.name:hashlib.sha256(host(x).read_bytes()).hexdigest() for x in [input_dir/'case_1.brep',input_dir/'case_5.brep']}}
(out/'receipts.json').write_text(json.dumps(receipts,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'all_expected_exit_codes_observed':all(x['expected_exit_observed'] for x in records),'records':records},ensure_ascii=False,indent=2))
raise SystemExit(0 if all(x['expected_exit_observed'] for x in records) else 1)
