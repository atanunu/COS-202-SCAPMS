# Staff access, backup protection and recovery

## Permissions

| Role | Allowed actions |
|---|---|
| Administrator | All workflows, staff/resource management, archive/restore, report export/print, portable backup export |
| Clinician | Patient/clinical editing, booking and consultation workflows, report export/print |
| Receptionist | Demographic registration/editing, scheduling, check-in, cancellations, no-show and queue administration |
| Viewer | Read demographic records and reports; no modifications, file saves, exports or printing |

Clinical summaries, reasons, urgency, previous-visit references, consultation notes
and correction history are hidden from receptionists and viewers. Receptionist
edits preserve existing clinical fields in the controller. New receptionist
registrations use General with an Awaiting clinician assessment reason.
Reports do not include consultation-note text.

Create the first administrator through Staff & activity > Protect this workspace.
Usernames are 3-40 letters, numbers, dots, hyphens or underscores; passphrases
are 12-256 characters. There are no shipped administrator credentials.
Five failed unlock attempts introduce a 30-second delay. Opening a protected
workspace is cancelled after five failed attempts. Idle sessions lock after
five minutes without keyboard, mouse-button or wheel activity.

## Normal workspaces

Staff passwords use independent 16-byte random salts, PBKDF2-HMAC-SHA256 with
210,000 iterations and 32-byte results. Staff-protected workspaces cannot be saved
as plaintext JSON. Windows DPAPI protects the complete .clinic payload and
authenticates it against modification. Normal workspaces, backups and autosaves
normally require the saving Windows account and computer.

## Portable backups

File > Export portable encrypted backup is an administrator action. A separate
12-256 character passphrase protects a versioned .scapbackup file using
AES-256-GCM, a random 16-byte salt, random 12-byte nonce and 16-byte authentication
tag. PBKDF2-HMAC-SHA256 with 600,000 iterations derives the 32-byte key.
The format identifier, salt and nonce are authenticated as additional data.
Encryption uses Windows CNG; the application does not implement AES itself.

Import on another Windows computer requires the backup passphrase. If the
contained workspace has staff accounts, an administrator from that workspace
must also sign in. Imported records open unsaved; Save As creates a fresh
Windows-account-protected workspace. Import never replaces the current workspace
until password, format, record validation and staff authorization succeed.
Losing the backup passphrase prevents restoration; it is not recoverable by the app.
Backups are limited to 20 MB of plaintext workspace data.

## Locks and recovery

QLockFile reserves each canonical named workspace for one application writer.
SHA-256 fingerprints detect deletion, replacement or content changes before
saving or autosaving. These checks protect cooperating application instances and
detect prior changes from other tools; they are not a shared database server.

Every 30 seconds dirty work is written to a protected recovery file.
Named files use <workspace>.recovery.clinic. Unsaved work uses a unique session
file under the application's local recovery directory, avoiding cross-session
overwrites. Explicit Discard removes the current session's autosave.

Each manual save first protects the previous valid file in <workspace>.backups,
then atomically writes the new version and retains the newest five backups.
Startup and File > Recover autosave / backup offer a preview of timestamps and
patient/visit counts. Damaged or inaccessible copies are labeled unavailable.
Restore opens an unsaved copy and retains the source for another attempt.

## Audit and scope

Audit records actor, time and successful state changes, exports and authentication
activity. Undo retains prior audit events and adds an Undo entry. Completed-note
corrections retain earlier notes and their correction reasons in visit history.
These are local application controls, not a remote immutable audit service.

The Windows account can access process memory and application files. Staff roles
do not isolate mutually untrusted users sharing that OS account. This assessment
implementation has not been evaluated as a clinical production system.
Portable backup import/export has been tested locally, including an independently
generated AES-GCM interoperability vector; a second physical computer was not used.

## Primary references

- https://doc.qt.io/qt-6/qpassworddigestor.html
- https://doc.qt.io/qt-6/qlockfile.html
- https://doc.qt.io/qt-6/qsavefile.html
- https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata
- https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_authenticated_cipher_mode_info
