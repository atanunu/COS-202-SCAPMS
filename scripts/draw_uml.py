from pathlib import Path
from reportlab.pdfgen import canvas
from reportlab.lib.colors import black, white, HexColor

root = Path(__file__).resolve().parents[1]
out = root / 'tmp/docx-assets'
out.mkdir(parents=True, exist_ok=True)
c = canvas.Canvas(str(out / 'classes.pdf'), pagesize=(600, 590))

def box(x, y, w, h, title, lines):
    c.setFillColor(HexColor('#f3f5f6'))
    c.setStrokeColor(black)
    c.rect(x, y, w, h, fill=1)
    c.setFillColor(black)
    c.setFont('Helvetica-Bold', 11)
    c.drawCentredString(x+w/2, y+h-17, title)
    c.line(x, y+h-25, x+w, y+h-25)
    c.setFont('Helvetica', 10)
    for i, line in enumerate(lines):
        c.drawString(x+8, y+h-40-i*13, line)

def triangle(x, y):
    p=c.beginPath(); p.moveTo(x,y); p.lineTo(x-6,y-10); p.lineTo(x+6,y-10); p.close()
    c.setFillColor(white); c.drawPath(p, stroke=1, fill=1)

def diamond(x, y):
    p=c.beginPath(); p.moveTo(x,y); p.lineTo(x+6,y+4); p.lineTo(x+12,y); p.lineTo(x+6,y-4); p.close()
    c.setFillColor(black); c.drawPath(p, stroke=1, fill=1)

box(30,465,245,110,'PatientRecord <<abstract>>',[
    '- details: PatientDetails', '+ category()', '+ consultationSummary()', '+ clone(): unique_ptr<PatientRecord>'])
box(325,465,245,110,'Appointable <<interface>>',[
    '+ validateScheduling(Appointment)', '+ validateCancellation(Appointment)'])
for x, title, lines in [
    (10,'GeneralPatient',['General visit reason','Clones and overrides policies']),
    (210,'EmergencyPatient',['Urgency-specific summary','Clones and overrides policies']),
    (410,'FollowUpPatient',['Previous-visit reference','Clones and overrides policies'])]:
    box(x,335,180,80,title,lines)
c.line(70,415,70,433); c.line(270,415,270,433); c.line(470,415,470,433)
c.line(70,433,470,433); c.line(150,433,150,455); triangle(150,465)
c.setDash(4,3)
c.line(130,415,130,448); c.line(330,415,330,448); c.line(530,415,530,448)
c.line(130,448,530,448); c.line(450,448,450,455); c.setDash(); triangle(450,465)
c.setFont('Helvetica',10)
c.setFillColor(black)
c.drawString(20,311,'PatientRecord 1 ---- 0..* Appointment, associated by immutable patientId.')
box(10,165,180,110,'ClinicDatabase',[
    'Owns patients and appointments','FIFO queue and fixed cache','Resources, archives, users, audit'])
box(210,165,180,110,'ClinicManager',[
    'Owns database and undo stack','Validates transactions','Applies roles and records audit'])
box(410,165,180,110,'Appointment',[
    'ID, patientId, time, duration','Service, status and notes','Six workflow statuses'])
c.line(10,220,2,220); c.line(2,220,2,520); c.line(2,520,30,520); diamond(0,220)
c.line(190,220,210,220); diamond(198,220)
c.line(100,165,100,145); c.line(100,145,500,145); c.line(500,145,500,165); diamond(100,145)
box(10,15,280,105,'MainWindow and eight panel classes',[
    'Dashboard, patients, appointments, queue,', 'algorithms, reports, staff, resource calendars.', 'Signals and slots call ClinicManager.'])
box(310,15,280,105,'Services used by the application',[
    'FileHandler / RecoveryService: storage, backups', 'AccessControl / PortableBackup: encryption', 'WorkspaceGuard: locks and fingerprints'])
c.setDash(4,3); c.line(150,120,150,133); c.line(150,133,300,133); c.line(300,133,300,165); c.setDash()
c.save()
