# SCAPMS 3.0

A C++17 / Qt Widgets clinic assessment application for COS 202.

## Run

Double-click Launch-SCAPMS.cmd. The welcome screen offers Open workspace,
Recent workspaces, Create new and Try demo. Demo opens a fictional copy without
overwriting the shipped sample. The current Windows package is dist/SCAPMS-v3;
older package folders are retained for any already-running instances.

## Features

- General, Emergency and Follow-up patient records; history and archive/restore.
- Contact validation, possible-duplicate warnings and a completed-visit picker.
- Role-aware clinical fields, tooltips, report exports and printing.
- Scheduled and walk-in visits both have persistent consultation records.
- Schedule/reschedule, check in, FIFO call-next, complete, cancel and no-show.
- Completion requires In consultation. Cancellation and no-show require reasons.
- Corrections preserve previous notes, author, timestamp and correction reason.
- Clinician and room catalogs, a day calendar and resource overlap validation.
- Search/filter/sort/selection preservation, descriptive undo and dashboard tabs.
- Handwritten linear/binary search, insertion sort and recursive merge sort.
- Repeated benchmarks: sorted, reversed and seeded shuffled input; one warm-up
  and five measured runs per algorithm, reporting medians and operation counts.
- Date-range reports, totals, CSV/PDF export and printing.
- Atomic saves, one-writer workspace locks and detection of external file changes.
- Protected autosaves, five rotating backups and recovery metadata previews.
- Staff roles, passphrase authentication, automatic locking and local audit history.
- Password-protected portable backup export/import across Windows computers.
- Text sizes of 10/12/14 pt, visible empty states and keyboard navigation.

## Staff and protection

Demo files are for fictional data. Staff & activity > Protect this workspace
creates the first administrator and saves a protected .clinic file.
There are no default credentials.

Clinicians and administrators may export/print reports; only administrators
may export full portable backups. Receptionists edit demographics and booking
details; clinical fields remain protected. Viewers cannot modify or save records.

Normal .clinic files and automatic recovery copies use Windows-account DPAPI.
File > Export portable encrypted backup creates a separate .scapbackup file
protected by a backup passphrase. Import requires that passphrase and, for a
staff-protected workspace, its administrator credentials. Keep both safely.
See docs/security-and-recovery.md for permissions and recovery details.

## Calendar and visits

Administrators add clinicians and rooms under Calendar & rooms. Choose these
resources while scheduling, or allocate an existing active visit from the calendar.
Overlapping active bookings for a patient, clinician or room are rejected.
Unassigned visits remain supported. Resource names are scheduling labels;
they are separate from staff login accounts.

Add walk-in creates a checked-in visit. Call next starts consultation, then use
Appointments > Complete consultation to enter notes. A scheduled visit can be
marked No-show only after its planned end time. Follow-up registration uses a
completed visit from that patient's history; register first, then change the
patient category after a completed consultation.

## Recovery and concurrent access

A named workspace is locked while open. Another instance must use a different
file or wait for the first to close. An external content change blocks Save;
use Save As to preserve the current in-memory work, or reload deliberately.

Dirty work autosaves every 30 seconds. Unsaved sessions use unique recovery files.
Startup offers available autosaves for review. File > Recover autosave / backup
shows saved times and record counts and restores a copy as an unsaved workspace.
The five newest previous versions are retained after manual saves.

## Build and verify

    ./scripts/build.ps1 -Test -Package

For a fresh machine with Python and internet access:

    ./scripts/setup.ps1 -Python python
    ./scripts/build.ps1 -Test -Package

Qt 6.8.3, MinGW 13.1, CMake/Ninja and C++17 are used.
Qt modules: Core, Widgets, Network, PrintSupport and Test.
Windows system libraries: crypt32 and bcrypt. Qt Creator can use CMakeLists.txt.
Schema versions 1 and 2 remain readable; new saves use schema 3.

## Shortcuts and materials

Ctrl+N New; Ctrl+O Open; Ctrl+S Save; Ctrl+Shift+S Save As; Ctrl+Z Undo;
Ctrl+F Search; Ctrl+Q Exit; Alt+1 through Alt+8 switch panels.
Enter opens details for a selected table row. View changes the text size.

- docs/demonstration-guide.md: reproducible walkthrough.
- docs/requirements.md: assessment and enhancement coverage.
- docs/diagrams/: editable UML source and rendered PNG/PDF diagrams.
- output/screenshots/: twelve GUI evidence images.
- output/docx/SCAPMS-project-report.docx: standalone three-page editable report.
- output/pdf/SCAPMS-project-report.pdf: standalone three-page report for submission.
- output/docx/SCAPMS-assessment.docx: combined submission containing the report, UML, screenshots and source listings.
- output/test-results.txt and .xml: recorded test results.
- output/SCAPMS-Windows.zip and SCAPMS-Source.zip: application and source packages.

Student identity fields in the Word submission remain editable placeholders.
Demo data can be regenerated using scripts/make_sample.py and
scripts/make_benchmark.py; both contain fictional records.
Runtime notices are in licenses and THIRD_PARTY_NOTICES.md.

The project repository is https://github.com/atanunu/COS-202-SCAPMS (private). Grant the assessor access before submission. The supplied brief asks for supporting materials and a 2–3-page report in one PDF. The standalone report supports the requested GitHub-based handoff, while SCAPMS-assessment.pdf retains the combined submission option.
