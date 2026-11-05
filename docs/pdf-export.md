# Bounded PDF export lifecycle

Task 5a (2026-11-05) adds cancellation and a bounded helper lifecycle to the
existing shared-engine PDF path. It does not close the writing milestone's
import/compatibility or responsiveness gates. Imported writing retains its
existing PDF block; the [export profile](export-profile.md) and independent
fixture requirements are unchanged.

A request captures owned elements, emphasis, dual flags and the title-inclusion
choice on the document thread. The offscreen `pdf_export` helper receives this
fixed snapshot; later writing, autosave, assistance and Undo continue separately.
Busy and successful status identify the capture time, including milliseconds;
success explicitly excludes later edits. Only one PDF request is accepted at a
time. The title checkbox applies to the captured request, including body-page
numbering when the title is omitted.

The helper writes a temporary file on the destination filesystem. Only a clean
helper exit and valid timing response can publish it through atomic rename.
Existing destination permissions are retained. Files inside the active project
folder are refused, including file/parent aliases; Save a Copy updates that
protected folder. Failure or cancellation keeps
existing destination bytes and removes the staged file after the helper exits.
Cancellation also wins if painting has finished but the helper has not exited.

- **Export** stays available while busy. Reopen it to choose **Cancel Export**.
  Cancellation sends a kill to the helper asynchronously, keeps the job busy
  until it is reaped, then reports cancellation separately from export failure.
  A later request can retry normally.
- After 30 seconds, a prompt offers **Cancel Export** or **Keep Waiting**.
  Keep Waiting returns focus to writing and starts another 30-second interval;
  it does not silently cancel or replace the requested snapshot. Tests shorten
  the timer through a C++ timing seam; ordinary application behavior uses 30 s.
- Closing the window during PDF export offers the same choice. Keep Waiting
  retains the pending close. Completion/cancellation retries window close through
  the ordinary autosave guard, including writing entered after Keep Waiting.
- Leaving for Home continues draining saves and the accepted export without
  blocking the GUI. The timeout's cancellation lets Home finish even if the
  helper has stopped. Save/recovery failures still retain their existing choices.
- Teardown without an event loop cancels unfinished PDF work instead of publishing
  it or waiting indefinitely. It kills the helper and waits up to one second
  for process completion. Normal close uses the asynchronous choices above.

`export_job_tests` exercises starting/stopped helpers, cancellation after a
complete staged PDF, retries, exact prior bytes, temporary-file cleanup,
process reaping, bounded destruction, actual QML wait/cancel controls and typing.
Its successful snapshot check extracts the actual PDF through Poppler and
compares it with a hand-written expected line while the live document has
changed and been destroyed. `project_access_flow_tests` checks Home/Close,
newer autosaved writing, unchanged PDF bytes and destination protection after
Save a Copy through real controls.

Run `QT_IM_MODULE=compose tools/inspect-writing-desk.sh --smoke` for the focused
native Wayland path and saved logs. It includes the PDF lifecycle and departure
checks; Qt Test supplies their key/mouse events. This does not measure physical
input, fcitx, sustained typing latency, compositor-input latency or owner comfort.
The task result and full-suite evidence are in
[the writing plan](writing-build-plan.md#task-5a--bounded-pdf-export-lifecycle).

Fountain's in-process worker still waits during teardown and has no cancel or
watchdog. That is the next separate export task, followed by measurements with
all current writing assistance and autosave enabled. This bounded task does not
establish cancellation of a kernel-level filesystem stall during publication.
