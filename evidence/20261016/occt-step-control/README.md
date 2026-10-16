# Pure OCCT 7.7 STEP export LeakSanitizer control

This experiment contains no BS2Quad source, headers, or linked library. It uses
only the same pinned OCCT 7.7 SDK at `/work/occt770`, a local stack-allocated
STEPControl_Writer, and one rectangular planar TopoDS_Face.

The probe source and scripts were written locally under `/tmp` and synchronized
to the remote task directory. Compilation and all executions occurred remotely
inside `bs2quad_20261016`. The SDK was not replaced or modified.

## Reproduction

From the remote host:

```sh
docker exec bs2quad_20261016 bash -c 'bash /work/evidence/occt-step-control/build.sh > /work/evidence/occt-step-control/build.log 2>&1'
docker exec bs2quad_20261016 bash /work/evidence/occt-step-control/run-controls.sh
docker exec bs2quad_20261016 bash /work/evidence/occt-step-control/collect-environment.sh
```

`build.sh` contains the exact compiler and linker command, including GCC
AddressSanitizer/UndefinedBehaviorSanitizer flags. Each execution explicitly
sets `ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0` and
`LD_LIBRARY_PATH=/work/occt770/lib`. Leak checking remains enabled. No suppression,
ignored-object registration, `_exit`, or sanitizer bypass is used.

## Observed controls

| Control | Completed writes | Exit status | LSAN bytes | Allocations |
| --- | ---: | ---: | ---: | ---: |
| Same linked SDK, no STEP initialization | 0 | 0 | no leak report | no leak report |
| STEPControl_Controller::Init only | 0 | 1 | 2916 | 136 |
| Construct/destroy STEPControl_Writer only | 0 | 1 | 2916 | 136 |
| Normal stack writer per export | 1 | 1 | 2964 | 138 |
| Normal stack writer per export | 10 | 1 | 2964 | 138 |
| Normal stack writer per export | 100 | 1 | 2964 | 138 |
| ClearData(1) + ClearContext before destruction | 1 | 1 | 2964 | 138 |
| ClearData(7) + ClearContext before destruction | 1 | 1 | 2964 | 138 |
| Standard::Purge after all writers are destroyed | 1 | 1 | 2964 | 138 |
| SetAlgoContainer(null) after all writers are destroyed | 1 | 1 | 2964 | 138 |

The STEP transfer and file write return IFSelect_RetDone; each write run prints
`PROBE completed writes=N`. The final process status 1 is the sanitizer exit
failure, not a failed STEP operation. The same 94-entity face file is overwritten
each time, avoiding accumulating output files. The application's 130-entity
export reported the same total 2964 bytes / 138 allocations.

## Provenance and lifecycle audit

The deep allocation stacks in the logs identify:

- MoniTool_TypedValue::AddEnum -> Interface_Static::Init -> XSAlgo::Init or
  STEPControl_Controller construction. These occur even before transferring any
  shape.
- IFSelect_Editor arrays constructed by STEPEdit_EditSDR,
  APIHeaderSection_EditHeader, and STEPEdit_EditContext during controller setup.
- Two 24-byte GeomToStep_MakeDirection arrays through
  STEPConstruct_ContextTool::GetDefaultAxis during the first transfer. Its source
  caches the default axis, consistent with the extra fixed 48 bytes after the
  first export.

The pinned public XSControl_WorkSession destructor already calls ClearBinders;
its implementation clears mapped binders and transfer data. STEPControl_Writer
owns its work session through an OCCT Handle. Explicit documented session cleanup
and allocator purge did not eliminate the report. The inspected public
STEPControl_Controller/XSControl_Controller/Interface_Static interfaces provide
initialization/registration and parameter access, but no verified global STEP
shutdown procedure was identified.

The SDK's NCollection_Array1 uses shifted data pointers for nonzero lower bounds.
That representation can complicate conservative reachability analysis; this
experiment does not establish whether each reported allocation is a true lost
allocation or a retained global allocation misclassified by LSAN. It does
establish that the failure is reproduced by pure OCCT and that its reported
size does not grow across these 1/10/100 identical exports.

## Conclusion and evidence boundary

This is not evidence of a BS2Quad-specific writer/session RAII omission. It is a
reproducible OCCT 7.7 initialization/first-transfer exit-leak report under the
pinned sanitizer setup. No public lifecycle fix was verified. Keep the STEP
LeakSanitizer acceptance check marked unresolved; do not call the full sanitizer
suite leak-free and do not suppress this failure. The repeated-export result is
bounded to the tested identical face and 100 writes; it is not a proof that all
STEP workloads have bounded memory.

`status.csv` and the ten per-control logs are the raw receipts. `environment.txt`,
`sdk-sha256.txt`, and `pinned-source-excerpts.txt` record toolchain/library
provenance and the inspected lifecycle source. `probe.step` is a successful STEP
output artifact. Production source files were not edited by this experiment.
