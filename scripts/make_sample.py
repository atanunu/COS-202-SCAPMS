"""Generate fictional clinic data. Run again to refresh dates for a demonstration."""
import json
from datetime import datetime, timedelta, timezone
from pathlib import Path
root = Path(__file__).resolve().parents[1]
now = datetime.now(timezone.utc)
def iso(value):
    return value.isoformat(timespec="milliseconds").replace("+00:00", "Z")
people = [
    ("Ada Okafor", 23, "General", "Routine health review", 1, ""),
    ("Tunde Bello", 31, "Emergency", "Urgent assessment", 4, ""),
    ("Zainab Musa", 27, "Follow-up", "Review of previous consultation", 1, "VIS-2026-014"),
    ("Chidi Eze", 20, "General", "Wellness screening", 1, ""),
    ("Fatima Ibrahim", 34, "Follow-up", "Scheduled review", 1, "VIS-2026-018"),
    ("Emeka Nwosu", 26, "General", "General consultation", 1, ""),
    ("Amina Yusuf", 22, "Emergency", "Same-day assessment", 3, ""),
    ("Samuel Adeyemi", 29, "General", "Health screening", 1, ""),
]
patients = [dict(id=f"P{i:04}", name=p[0], age=p[1], contact=f"patient{i}@example.test",
    category=p[2], reason=p[3], urgency=p[4], previousVisit=p[5]) for i,p in enumerate(people,1)]
appointments = []
def appointment(patient, offset, status, service):
    appointments.append(dict(id=f"A{len(appointments)+1:04}", patientId=f"P{patient:04}", service=service,
        start=iso(now+timedelta(minutes=offset)), durationMinutes=30, status=status,
        consultationNotes="Sample consultation completed; routine follow-up recorded." if status=="Completed" else ""))
appointment(1,-150,"Completed","General consultation")
appointment(3,-100,"Completed","Follow-up review")
appointment(1,-1440,"Completed","Health screening")
appointment(2,-35,"Scheduled","Emergency review")
appointment(5,20,"Scheduled","Follow-up review")
appointment(4,60,"Scheduled","Health screening")
appointment(6,120,"Cancelled","General consultation")
appointment(8,1440,"Scheduled","Health screening")
data = dict(schemaVersion=3,nextPatientId=9,nextAppointmentId=9,patients=patients,appointments=appointments,
    waitingQueue=[dict(patientId=f"P{i:04}",arrivedAt=iso(now-timedelta(minutes=m))) for i,m in [(2,20),(5,10),(4,5)]])

data.update(clinicians=["Dr Demo A","Dr Demo B"], rooms=["Room 1","Room 2"],
            archivedPatients=[], users=[], audit=[])
for index,visit in enumerate(appointments):
    visit.update(changeReason="Sample cancellation" if visit["status"]=="Cancelled" else "",
                 clinician=data["clinicians"][index%2], room=data["rooms"][index%2],
                 walkIn=False, corrections=[], consultationStarted="", completedAt="")
for entry in data["waitingQueue"]:
    visit=next(a for a in appointments if a["patientId"]==entry["patientId"] and a["status"]=="Scheduled")
    visit["status"]="Checked in"
    entry["appointmentId"]=visit["id"]
(root/"data").mkdir(exist_ok=True)
(root/"data"/"sample-clinic.json").write_text(json.dumps(data,indent=2),encoding="utf-8")
print("Created fictional sample-clinic.json")
