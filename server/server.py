import json
import os
from datetime import datetime

import requests
from fastapi import FastAPI, HTTPException, Request
from pydantic import BaseModel
from pymongo import MongoClient

# sensitive Info like URI...
import sensitiveInfo

# to initiate subscriber:
# ╰─❯ curl -X POST http://localhost:5089/\~/in-cse/in-name/UltrasoundData \                                                                            ─╯
#   -H "X-M2M-Origin: admin:admin" \
#   -H "Content-Type: application/vnd.onem2m-res+json; ty=23" \
#   -d '{
#     "m2m:sub": {
#       "rn": "subFastAPI",
#       "nu": ["http://127.0.0.1:8000/notify"],
#       "nct": 2
#     }
#   }'

# MongoDB connection
try:
    client = MongoClient(sensitiveInfo.MONGO_URI)
    db = client[sensitiveInfo.DB_NAME]
    collection = db[sensitiveInfo.COLLECTION_NAME]
    print("✅ Connected to MongoDB Atlas")
    print("📁 DB:", db.name)
    print("📂 Collection:", collection.name)
except Exception as e:
    print("❌ MongoDB Connection Error:", e)
    raise

# FastAPI app
app = FastAPI()


# Data Model
class TrackData(BaseModel):
    track_id: str
    time: datetime
    ultrasound_reading: float
    temperature: float
    speed: float
    track_condition: str


# Endpoint
@app.post("/push-data/")
async def push_data(data: TrackData):
    try:
        result = collection.insert_one(data.model_dump())
        print("✅ Inserted Document ID:", result.inserted_id)
        return {"status": "success", "data": data}
    except Exception as e:
        print("❌ Insert Error:", str(e))
        raise HTTPException(status_code=500, detail=str(e))


# updating mongo based on OM2M
@app.post("/notify")
async def notify(request: Request):
    try:
        body = await request.json()
        print("📨 Received body:\n", json.dumps(body, indent=2))

        con_raw = body["m2m:sgn"]["m2m:nev"]["m2m:rep"]["m2m:cin"]["con"]
        sensor_data = json.loads(con_raw)

        if sensor_data["track_condition"] == "Defective":
            notifyDefective("Defective")

        collection.insert_one(sensor_data)

        print("✅ Data inserted:", sensor_data)
        return {"status": "ok", "message": "Data stored"}

    except Exception as e:
        print("❌ Error parsing OM2M notification:", e)
        return {"status": "error", "detail": str(e)}


def notifyDefective(condition: str):
    # the uri
    OM2M_URI = "http://127.0.0.1:5089/~/in-cse/in-name/condition"

    # converting input into a string
    sensor_dict = {"track_condition": condition}

    # the headers
    headers = {
        "X-M2M-Origin": "admin:admin",
        "X-M2M-RI": "FastAPI",
        "Content-Type": "application/vnd.onem2m-res+json; ty=4",
    }

    # the payload
    payload = {"m2m:cin": {"con": json.dumps(sensor_dict)}}

    response = requests.post(OM2M_URI, headers=headers, json=payload)

    print("Status Code:", response.status_code)
    print("Response:", response.text)
