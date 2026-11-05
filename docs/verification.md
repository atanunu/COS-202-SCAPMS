# Verification record — SCAPMS 3.0

Verified on Windows on 4–5 November 2026.

## Build and automated tests

C++17, Qt 6.8.3 and MinGW 13.1.0. Final Release build passed. Qt Test: **31 passed, 0 failed, 0 skipped** (29 functional methods plus initialization and cleanup). CTest: **1/1 suite passed**. Raw results are in output/test-results.txt and output/test-results.xml.

Coverage includes patient validation, transactional changes, archive/restore, undo, searches, manual sorting, persistence, roles, authentication, clinical privacy, report permissions, walk-in visits, strict consultation transitions, no-show and corrections, clinician/room conflicts, workspace locks and external modifications, recovery, contact validation, previous-visit selection, repeated benchmarks, welcome and calendar UI. Portable encrypted backups passed round-trip, tamper and wrong-password checks plus an independently generated Python AES-GCM interoperability fixture.

## Packaging and UI

The standalone package was exercised with development paths removed, using both sample data and the 2,000-record benchmark. Both runs exited successfully with empty stderr and generated 12 application captures. Native Windows startup reached the welcome-dialog event loop, but the automation tool could not expose a targetable window. Startup now shows the owner window before opening modal dialogs. The build and tests passed after that change; the native welcome window was subsequently verified visually. Its recent-list background was explicitly styled to fix Windows dark-theme contrast. All eight panels were visually checked at 14 pt in the native application. Keyboard panel switching worked. The 12 pt size was checked and the original 10 pt setting restored. Low-contrast menus, tabs and calendar controls were corrected and checked in the rebuilt package.

## Submission

The editable Word submission follows the supplied reference structure and keeps student-data placeholders. It includes a three-page report, UML, 12 application captures, demonstration guidance and editable source listings. All 117 pages of the initial Word render were visually reviewed. Two isolated closing lines were fitted onto their preceding pages. The final render contains 115 pages. Of those, 97 page bodies matched previously reviewed pages pixel for pixel. The 18 changed page bodies were reviewed, including refreshed screenshots with readable fonts. The final document retains 64 editable source and resource listings.

## Limits

This is a local assessment application using fictional demonstration data. Normal protected files use Windows-account-bound DPAPI; portable backups use a passphrase and AES-256-GCM. Transfer to a second physical computer has not been tested. Queue order remains FIFO. Undo and cache are session-only. Local roles and audit are not an independently secured clinical service.

