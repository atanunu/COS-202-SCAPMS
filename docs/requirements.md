# Assessment coverage and enhancement map

| Requirement | Implementation | Demonstration |
|---|---|---|
| Abstract patient class | PatientRecord pure virtual category, summary and clone | Selected history and queue summary |
| Three subclasses | GeneralPatient, EmergencyPatient, FollowUpPatient | Dynamic registration fields |
| Abstract interface | Appointable scheduling/cancellation policies | Appointment controller dispatch |
| Runtime polymorphism | processPatient(const PatientRecord&) | History and Call next |
| Composition and encapsulation | Database owns unique_ptr patients; manager owns state | UML and model/controller code |
| Vector | vector<unique_ptr<PatientRecord>> | Registered patient storage |
| Queue | queue<QueueEntry> with linked appointment IDs | FIFO waiting and check-in |
| Stack | stack<UndoSnapshot>, bounded to fifty | Descriptive undo, audit retained |
| Fixed array | array<CacheEntry,10> | Frequently accessed dashboard |
| Linear and binary search | Manual record scan and binary lookup | Search algorithm selector |
| Two manual sorts | Insertion and recursive merge sort | Sort and comparison controls |
| Recursion | mergeRange base case and recursive halves | Recursion-depth metric |
| Multiple sort fields | Name, ID, age | Sort field selector |
| Event-driven GUI | Signals/slots, menus, input events, timers | Seven independent panels |
| Multiple layouts | VBox, HBox, Grid, Form | Adaptive pages and dialogs |
| Advanced GUI | Dynamic fields, formatting, validation, file dialogs, tooltips, shortcuts | Actual screenshots |
| Exceptions | ValidationError, FileError, ClinicError | Automated error-path tests |
| Persistence | Versioned JSON; protected DPAPI envelopes | Save/load and recovery |
| Reports | Attendance, pending, categories, status totals | Date ranges, CSV/PDF and print |
| Submission | Editable report, UML, screenshots, source and data | output/docx/SCAPMS-assessment.docx |

## Added improvements

- Rescheduling with overlap checks and undo.
- Appointment check-in, consultation state and linked FIFO queue.
- Full patient history and retained archived records (logical deletion).
- Selection, search and sort preservation after mutations.
- Today's, overdue and waiting-time dashboard lists.
- Elapsed time and identical-input algorithm comparisons with a 2,000-record dataset.
- Date-range reporting, summaries and printable output.
- Protected autosaves, five rolling backups and recovery selection.
- Staff roles, password changes, inactivity locking and local audit history.
- Separate panel classes and consistent C++ formatting.
- Extended workflow, authorization, encryption, backup and GUI tests.

## Version 3 additions

- Permission-aware tooltips, clinical forms, report export/printing and workspace saves.
- Persistent walk-in encounters, strict consultation completion, no-show and correction reasons.
- Canonical file locks, external-change fingerprints and unique unsaved recovery sessions.
- Welcome/recent workspace selection and recovery timestamp/count previews.
- Duplicate warnings, contact validation and completed-visit selection for follow-ups.
- Clinician/room calendar, booking allocations and overlapping resource validation.
- Portable AES-GCM password-protected backups and authorized import.
- Visible empty states, text scaling and keyboard navigation.
- Background benchmarks with medians across three input orders.
