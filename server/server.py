import base64
import json
import os
from datetime import datetime
from io import BytesIO

import requests
from bson import json_util
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from landingai.predict import Predictor
from PIL import Image
from pydantic import BaseModel
from pymongo import MongoClient
from starlette.types import Lifespan

# sensitive Info like URI...
import sensitiveInfo

# manullay type this in do not copy
# curl -X POST \
#   http://localhost:5089/\~/in-cse/in-name/UltrasoundData \
#   -H "X-M2M-Origin: admin:admin" \
#   -H "Content-Type: application/vnd.onem2m-res+json; ty=23" \
#   -d '{
#     "m2m:sub": {
#       "rn": "subFastAPI",
#       "nu": ["http://localhost:8000/notify"],
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
templates = Jinja2Templates(directory="templates")


# updating mongo based on OM2M
@app.post("/notify")
async def notify(request: Request):
    try:
        body = await request.json()
        print("📨 Received body:\n", json.dumps(body, indent=2))

        con_raw = body["m2m:sgn"]["m2m:nev"]["m2m:rep"]["m2m:cin"]["con"]
        sensor_data = json.loads(con_raw)

        # ----------- JSON LOOK ----------------
        # track_id
        # time
        # ultrasound_reading_1
        # ultrasound_reading_2
        # image_encoding
        # alignment
        # temperature

        # decode the image from the encoding
        # encoded = sensor_data["image_encoding"]
        # image_data = base64.b64decode(encoded)

        # create a Pillow image
        # image = Image.open(BytesIO(image_data))

        # api call to landing lens to get final verdict
        # predictor = Predictor(sensitiveInfo.ENDPOINT_ID, api_key=sensitiveInfo.API_KEY)
        # predictions = predictor.predict(image)
        # print(predictions)
        # ouput format: LIST: [ClassificationPrediction(score=0.9980272650718689, label_name='Defective', label_index=0)]
        # condition = predictions[0].label_name
        condition = "Defective"
        # then determine and modify the dict s.t. it includes everything
        sensor_data["condition"] = condition
        # check if defective then inform the respective container
        if condition == "Defective":
            notifyDefective(condition)

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


@app.get("/", response_class=HTMLResponse)
async def read_entries(request: Request):
    # Fetch all entries
    entries = list(collection.find())

    # Group entries by track_id
    grouped_data = {}
    for entry in entries:
        track_id = entry.get("track_id", "Unknown")
        if track_id not in grouped_data:
            grouped_data[track_id] = []
        grouped_data[track_id].append(entry)

    return templates.TemplateResponse(
        "index.html", {"request": request, "data": grouped_data}
    )
