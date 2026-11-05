"""Create 2,000 deterministic, fictional records for comparing manual algorithms."""
from pathlib import Path
import json
import random

randomizer = random.Random(2026)
records = []
for index in range(1, 2001):
    category = randomizer.choice(["General", "Emergency", "Follow-up"])
    records.append({
        "id": f"B{index:05d}",
        "name": f"Demo Patient {randomizer.randrange(100000):05d}",
        "age": randomizer.randrange(121),
        "contact": f"benchmark{index}@example.test",
        "category": category,
        "reason": "Fictional algorithm demonstration",
        "urgency": randomizer.randrange(1, 6),
        "previousVisit": f"DEMO-{index}" if category == "Follow-up" else ""
    })
randomizer.shuffle(records)
data = dict(schemaVersion=2, nextPatientId=1, nextAppointmentId=1, patients=records,
            appointments=[], waitingQueue=[], archivedPatients=[], users=[], audit=[])
target = Path(__file__).resolve().parents[1] / "data" / "benchmark-2000.json"
target.write_text(json.dumps(data, indent=2), encoding="utf-8")
print(target)
