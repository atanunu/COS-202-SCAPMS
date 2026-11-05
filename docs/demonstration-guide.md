# COS 202 demonstration guide

Use fictional records throughout. Launch-SCAPMS.cmd opens the welcome screen.

1. Select Try demo. Show the eight panels and the date/queue dashboard.
2. Register a General or Emergency patient. Demonstrate contact validation and
   the duplicate warning. Edit demographic details and view patient history.
3. Schedule and reschedule a visit dated today. Reject an overlapping booking.
   Check in, Call next, then Complete consultation with notes.
4. Add a walk-in. Verify its linked visit ID, call it, complete it and find its
   notes in history. Correct the completed notes with a reason and show history.
5. Cancel a future visit with a reason. Mark an overdue, unstarted visit No-show.
6. Edit a patient with a completed visit; choose Follow-up and select that visit.
   Previously imported external references remain available as existing values.
7. In Calendar & rooms add two clinicians and two rooms. Schedule overlapping
   visits for different patients with different resources. Attempt to reuse a
   busy clinician or room and show the rejection. Allocate an unassigned visit.
8. Archive a patient, show retained historical visits, and undo. Inspect the
   separate audit entry. Restore does not reactivate cancelled visits.
9. Demonstrate linear/binary ID searches and name/category filters. Sort using
   insertion and recursive merge sort. Show retained search and selection after editing.
10. Load benchmark-2000.json. Compare sorted, reversed and seeded shuffled input.
    Explain one excluded warm-up, five measured runs, medians and operation counts.
11. Generate date-range reports; export CSV/PDF or print as a clinician/admin.
12. Protect a separate workspace and create clinician, receptionist and viewer
    accounts. Verify restricted tooltips/forms and disabled report-export controls.
13. Open the same saved file in a second v3 instance and show the lock rejection.
    Close the first instance to release it. Demonstrate Save As after an external
    file change using a disposable fictional workspace only.
14. Make a change and wait for autosave. Review timestamps and counts in Recovery,
    restore a copy, and Save As. Show recent workspaces on the welcome screen.
15. Export a portable backup as administrator with a separate passphrase. Import
    it and authenticate using the contained administrator. Save to a new .clinic.
16. Select View > Text size; use Alt+1 through Alt+8, Ctrl+F and Enter on a selected
    record. Clear a filter to show helpful empty-state text.
17. Run scripts/build.ps1 -Test and show recorded results, UML and manual algorithms.

The Word submission retains the previous assignment's identity block, numbered
headings and page numbers. Fill in the bracketed student details, then export
your final submission PDF from Word.
