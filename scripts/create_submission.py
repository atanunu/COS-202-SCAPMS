"""Create an editable COS 202 submission using the supplied assignment structure."""
from pathlib import Path
import textwrap
import xml.etree.ElementTree as ET
from docx import Document
from docx.shared import Inches, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn

root=Path(__file__).resolve().parents[1]
out=root/"output/docx"
out.mkdir(parents=True,exist_ok=True)
doc=Document()
section=doc.sections[0]
section.page_width=Inches(8.5);section.page_height=Inches(11)
section.top_margin=Inches(.7);section.bottom_margin=Inches(.65)
section.left_margin=section.right_margin=Inches(.75)
section.footer_distance=Inches(.3)
normal=doc.styles["Normal"]
normal.font.name="Arial";normal.font.size=Pt(11.5);normal.font.color.rgb=RGBColor(0,0,0)
normal.paragraph_format.line_spacing=1.15
normal.paragraph_format.space_after=Pt(8)
normal.paragraph_format.first_line_indent=Inches(.32)
normal.paragraph_format.widow_control=True
for name in ["Title","Heading 1","Heading 2","Heading 3"]:
    st=doc.styles[name];st.font.name="Arial";st.font.color.rgb=RGBColor(0,0,0)
    st.font.size=Pt(13 if name in ["Title","Heading 1"] else 11.5)
    st.font.bold=True
    st.paragraph_format.first_line_indent=Inches(0)
    st.paragraph_format.space_before=Pt(9);st.paragraph_format.space_after=Pt(8)
    st.paragraph_format.keep_with_next=True
    for border in list(st.element.xpath('./w:pPr/w:pBdr')):
        border.getparent().remove(border)
code_style=doc.styles.add_style("SourceCode",1)
code_style.font.name="Consolas";code_style.font.size=Pt(8)
code_style.paragraph_format.first_line_indent=Inches(0)
code_style.paragraph_format.line_spacing=Pt(9.2)
code_style.paragraph_format.space_before=code_style.paragraph_format.space_after=Pt(0)
code_style.paragraph_format.widow_control=False
code_style.paragraph_format.keep_with_next=False
footer=section.footer.paragraphs[0];footer.alignment=WD_ALIGN_PARAGRAPH.CENTER
footer.paragraph_format.first_line_indent=Inches(0)
field=OxmlElement("w:fldSimple");field.set(qn("w:instr"),"PAGE");footer._p.append(field)
doc.core_properties.title="COS 202 Smart Clinic Application Lab Assessment"
doc.core_properties.author=""
doc.core_properties.subject="Editable assessment submission"
def paragraph(text):
    p=doc.add_paragraph(text);p.alignment=WD_ALIGN_PARAGRAPH.JUSTIFY;return p
next_page=False
def heading(text,level=2):
    global next_page
    p=doc.add_paragraph(text,"Heading "+str(level))
    if next_page:
        p.paragraph_format.page_break_before=True
        next_page=False
    return p
def page():
    global next_page
    next_page=True
def identity(label,value):
    p=doc.add_paragraph()
    p.paragraph_format.first_line_indent=Inches(0)
    p.paragraph_format.space_after=Pt(8)
    p.paragraph_format.tab_stops.add_tab_stop(Inches(2.7))
    p.add_run(label).bold=True;p.add_run("\t"+value)
identity("FULL NAME:","[Enter full name]")
identity("DATE:","[Enter submission date]")
identity("REGISTRATION NUMBER:","[Enter registration number]")
identity("COURSE:","COS 202 (Programming II)")
# The supplied reference uses a double rule below its identity block.
p=doc.add_paragraph();p.paragraph_format.space_after=Pt(12)
border=OxmlElement("w:pBdr");bottom=OxmlElement("w:bottom")
for key,value in [("val","double"),("sz","8"),("space","1"),("color","000000")]:
    bottom.set(qn("w:"+key),value)
border.append(bottom);p._p.get_or_add_pPr().append(border)
doc.add_paragraph("1.0 SMART CLINIC APPLICATION PROJECT REPORT","Title")
heading("1.1 PROJECT DESCRIPTION AND FEATURES")
paragraph("The Smart Clinic Appointment and Patient Management System (SCAPMS 3.0) is a C++17 desktop application for the COS 202 lab assessment. Its Qt Widgets interface provides patient registration, appointment management, a FIFO queue, searching, sorting, reporting and persistent storage. Eight panels separate overview, patient records, appointments, queue, algorithms, reports, staff activity and resource calendars.")
paragraph("Scheduled and walk-in visits have persistent consultation records. Check-in joins the queue; Call next starts consultation; completion saves notes. Rescheduling checks conflicts, cancellation and no-show require reasons, and corrections retain previous notes. Clinician and room allocations reject overlapping active bookings. History and archiving preserve previous visits. A welcome screen opens recent workspaces or a fictional demonstration copy.")
heading("1.2 CLASS HIERARCHY AND RESPONSIBILITIES")
paragraph("PatientRecord is abstract, with pure virtual category(), consultationSummary() and clone() operations. GeneralPatient, EmergencyPatient and FollowUpPatient provide category-specific behaviour. Appointable defines scheduling and cancellation policies. Processing a PatientRecord reference dispatches to the concrete subtype at runtime, while clone() preserves that subtype in undo snapshots.")
paragraph("ClinicDatabase owns patient and appointment collections. ClinicManager applies validation, permissions and transactions. Separate panels present data; FileHandler and RecoveryService handle persistence. WorkspaceGuard detects competing file access. AccessControl and PortableBackup provide authentication and encryption. ReportService and Benchmark calculate reports and algorithm comparisons. Header/source files and namespaces separate these responsibilities.")
page()
heading("1.3 DATA STRUCTURES",1)
paragraph("A std::vector of unique_ptr<PatientRecord> owns patients without object slicing. Appointments refer to immutable patient IDs and optional clinician/room names. A std::queue of QueueEntry objects preserves arrival order and links waiting patients to visits. A fixed std::array of ten CacheEntry objects tracks frequent access, replacing the least-used entry with an oldest-access tie-break.")
paragraph("A std::stack of UndoSnapshot objects stores up to fifty deep snapshots. Candidate transactions are validated before committing; rejected changes preserve live state and undo history. Undo restores the latest record state while retaining existing audit events and adding an Undo entry. Account changes and switching users clear the undo stack.")
heading("1.4 SEARCHING SORTING AND RECURSION")
paragraph("Linear search scans IDs and supports name/category filtering. Binary search uses an ID-sorted view prepared by the handwritten merge sort. Lookup costs O(log n); preparing the order costs O(n log n), reported separately in the interface.")
paragraph("Insertion sort has O(n) best-case and O(n squared) worst-case time. Recursive merge sort is stable with O(n log n) time and O(n) temporary storage. Ranges smaller than two items form its base case; recursive calls sort both halves before merging. Equal keys preserve their original order. Required searching and sorting algorithms are implemented manually.")
paragraph("Both sorts support name, ID and age. The comparison runs sorted, reversed and seeded shuffled input, identical for each algorithm. One warm-up is excluded; five measured runs provide a median. Comparisons, moves and recursion depth supplement timing. A 2,000-record fictional dataset supports demonstrations. Background calculation keeps the interface responsive.")
heading("1.5 GUI DESIGN AND EVENTS")
paragraph("VBox, HBox, Grid and Form layouts arrange controls. A QAbstractTableModel adapter presents patients. Signals and slots handle actions, selections and timers. Dynamic subtype fields, date/resource filters, duplicate warnings, validation messages, role-aware tooltips, printing and recovery dialogs support daily workflows. Text scaling, visible empty states and keyboard shortcuts improve usability. Search, sort, selection and scrolling survive updates.")
page()
heading("1.6 VALIDATION SECURITY AND PERSISTENCE",1)
paragraph("Custom exceptions reject invalid contacts, IDs, ages, times, overlapping bookings, duplicate queue entries and malformed data. New appointments are future-dated; check-in requires the appointment date; completion requires an active consultation and notes. No-show requires a scheduled visit whose end time has passed. Follow-up selection uses a completed visit from the patient's history.")
paragraph("Administrator, clinician, receptionist and viewer roles govern actions. Clinical forms and tooltips restrict sensitive fields; report exports/printing require clinical or administrator access, and portable backups require an administrator. Staff passwords use salted PBKDF2-HMAC-SHA256. Windows DPAPI protects normal workspaces and recovery copies. Separate portable backups use a passphrase-derived key with AES-256-GCM and authenticated format metadata.")
paragraph("QSaveFile commits writes atomically. WorkspaceGuard reserves a canonical path with QLockFile and checks a SHA-256 fingerprint before saving. Autosave runs every thirty seconds, with unique files for unsaved sessions. Manual saves retain five protected previous versions. Recovery previews show timestamps and record counts; imported or recovered data opens unsaved for Save As.")
heading("1.7 CHALLENGES AND RESOLUTIONS")
paragraph("Polymorphic undo requires deep cloning of unique_ptr records. Views release stale pointers before transactions and restore selection by ID afterward. Walk-ins now create visit records so notes are retained. Resource validation checks patient, clinician and room conflicts together. An empty-state callback initially outlived its table viewport; parent ownership and guarded pointers corrected the teardown failure.")
paragraph("Portable encryption delegates AES to Windows CNG. Authentication, tamper rejection and an independent AES-GCM fixture verify the backup format. Normal DPAPI files remain bound to the Windows account; portable backups require their passphrase and staff administrator credentials. These local controls are an assessment implementation, not an independently evaluated clinical production system.")
heading("1.8 VERIFICATION")
test_file=root/"build/test-results.xml"
test_summary="The recorded automated results are included with the project."
if test_file.exists():
    result=ET.parse(test_file).getroot()
    test_summary=f"The final Qt Test run reports {result.get('tests')} results, {result.get('failures')} failures and {result.get('errors')} errors."
paragraph(test_summary+" Tests cover validation, failed-operation atomicity, visit transitions, permission enforcement, file locking, recovery, encryption, resource conflicts, repeated benchmarks and GUI state. Actual application captures and editable source listings follow.")
page()
heading("2.0 UML AND SYSTEM DESIGN",1)
paragraph("The diagram shows inheritance, interface realization, composition and the main service dependencies. Each patient can be associated with zero or more appointments through a stable patient ID.")
diagram=root/"tmp/docx-assets/classes.png"
if not diagram.exists():
    raise RuntimeError("Render scripts/draw_uml.py output to tmp/docx-assets/classes.png first.")
doc.add_picture(str(diagram),width=Inches(7))
doc.paragraphs[-1].paragraph_format.first_line_indent=Inches(0)
paragraph("General, emergency and follow-up classes share an abstract contract. The database owns records and resource catalogs. Controllers coordinate validated state changes; panels use controllers and services for persistence, authorization and backup protection.")
page()
heading("3.0 GRAPHICAL USER INTERFACE EVIDENCE",1)
shots=sorted((root/"output/screenshots").glob("*.png"))
if len(shots)<11:
    raise RuntimeError("Capture the updated application screenshots before creating the submission.")
for index,image in enumerate(shots,1):
    if index>1:page()
    title=image.stem.split("-",1)[-1].replace("-"," ").upper()
    heading(f"3.{index} {title}")
    from PIL import Image
    with Image.open(image) as im:
        width,height=im.size
    image_width=min(7.0,7.6*width/height)
    doc.add_picture(str(image),width=Inches(image_width))
    doc.paragraphs[-1].paragraph_format.first_line_indent=Inches(0)
    caption="Figure "+str(index)+". "+title.capitalize()+" in the compiled SCAPMS application."
    if image.stem.startswith("10-"):
        caption+=" Both algorithms process the same 2,000 fictional records in three orders, with five measured runs per case."
    paragraph(caption)
page()
heading("4.0 DEMONSTRATION AND REQUIREMENT COVERAGE",1)
steps=[
("Patient records","Register each category; show dynamic fields, edit a record and open its history."),
("Appointments and queue","Schedule and reschedule; reject an overlap; check in today's visit, call next, complete with notes and verify history."),
("Undo and archive","Archive a patient, show retained historical visits, undo the archive and inspect the additional audit entry."),
("Algorithms","Select manual searches and sorts. Compare five-run median timings and operation counts across three input orders using benchmark-2000.json."),
("Reports","Select a date range, review totals and export CSV/PDF or open the print dialog."),
("Access and recovery","Protect a workspace and demonstrate staff restrictions, file locks and recovery previews. Export/import a portable backup and allocate clinicians/rooms in the calendar.")
]
for title,body in steps:
    heading("4."+str(steps.index((title,body))+1)+" "+title.upper())
    paragraph(body)
paragraph("Shortcuts: Ctrl+N New, Ctrl+O Load, Ctrl+S Save, Ctrl+Shift+S Save As, Ctrl+Z Undo, Ctrl+F Search, Ctrl+Q Exit and Alt+1 through Alt+8 for panels.")
page()
heading("5.0 SOURCE CODE AND RESOURCES",1)
paragraph("The following editable listings contain the complete application source, tests, build scripts and required demonstration data. Long lines are wrapped with a continuation marker. Original compilable files are also provided in the source archive. The large benchmark dataset is reproducible from its included generator.")
sources=[root/"CMakeLists.txt"]
sources+=sorted((root/"src").rglob("*.h"))+sorted((root/"src").rglob("*.cpp"))
sources+=sorted((root/"tests").glob("*.cpp"))
sources += [root/"scripts/build.ps1",root/"scripts/setup.ps1",root/"scripts/make_sample.py",root/"scripts/make_benchmark.py",
            root/"Launch-SCAPMS.cmd",root/"data/sample-clinic.json"]
for index,source in enumerate(sources,1):
    if index>1 and source.suffix==".cpp": page()
    heading(f"5.{index} "+("HEADER FILE" if source.suffix==".h" else "SOURCE OR RESOURCE FILE"))
    p=doc.add_paragraph(source.relative_to(root).as_posix())
    p.paragraph_format.first_line_indent=Inches(0);p.paragraph_format.keep_with_next=True
    for number,line in enumerate(source.read_text(encoding="utf-8-sig").splitlines(),1):
        line=line.expandtabs(4)
        chunks=[line[i:i+99] for i in range(0,len(line),99)] or [""]
        for continuation,chunk in enumerate(chunks):
            prefix=f"{number:4} " if not continuation else "   > "
            p=doc.add_paragraph(prefix+chunk,"SourceCode")
            if source.name in {"ReportsPanel.cpp", "PatientDialog.cpp"}:
                p.paragraph_format.line_spacing=Pt(9)
page()
heading("6.0 REFERENCES",1)
references=[
"The Qt Company. (n.d.). Model/view programming. https://doc.qt.io/qt-6/model-view-programming.html",
"The Qt Company. (n.d.). QPasswordDigestor namespace. https://doc.qt.io/qt-6/qpassworddigestor.html",
"The Qt Company. (n.d.). QSaveFile class. https://doc.qt.io/qt-6/qsavefile.html",
"Microsoft. (n.d.). CryptProtectData function. https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata",
"Microsoft. (n.d.). BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO. https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_authenticated_cipher_mode_info",
"The Qt Company. (n.d.). QLockFile class. https://doc.qt.io/qt-6/qlockfile.html",
"Miva Open University. (n.d.). COS 202 Programming II lab assessment project: Smart Clinic Appointment and Patient Management System. Supplied assessment brief."
]
for ref in references:
    p=doc.add_paragraph(ref);p.paragraph_format.first_line_indent=Inches(-.25);p.paragraph_format.left_indent=Inches(.25)
# Avoid hidden document metadata and ensure pictures have meaningful alternate text.
for shape in doc.inline_shapes:
    shape._inline.docPr.set("descr","SCAPMS class diagram or application interface evidence")
target=out/"SCAPMS-assessment.docx"
doc.save(target)
print(target)
print(f"{len(sources)} editable source/resource listings; {len(shots)} screenshots.")
