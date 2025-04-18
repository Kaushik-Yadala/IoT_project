import base64
import json
import os
from datetime import datetime
from io import BytesIO

import httpx
import matplotlib.pyplot as plt
import pandas as pd
import requests
import seaborn as sns
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

OM2M_URL = "http://localhost:5089/~/in-cse/in-name/UltrasoundData"
NOTIFY_URL = "http://localhost:8000/notify"
SUBSCRIPTION_NAME = "subFastAPI"


@app.on_event("startup")
async def subscribe_to_om2m():
    headers = {
        "X-M2M-Origin": "admin:admin",
        "Content-Type": "application/vnd.onem2m-res+json; ty=23",  # <-- match curl
        "Accept": "application/json",
    }

    payload = {"m2m:sub": {"rn": SUBSCRIPTION_NAME, "nu": [NOTIFY_URL], "nct": 2}}

    async with httpx.AsyncClient() as client:
        try:
            response = await client.post(OM2M_URL, headers=headers, json=payload)
            print("[SUBSCRIBE] Status:", response.status_code)
            print("[SUBSCRIBE] Response:", response.text)
        except Exception as e:
            print("[ERROR] Subscription failed:", str(e))


# @app.on_event("startup")
# def subscribe_to_om2m():
#     url = "http://localhost:5089/~/in-cse/in-name/UltrasoundData"
#     headers = {
#         "X-M2M-Origin": "admin:admin",
#         "Content-Type": "application/vnd.onem2m-res+json; ty=23",
#     }
#     payload = {
#         "m2m:sub": {
#             "rn": "subFastAPI",
#             "nu": ["http://localhost:8000/notify"],
#             "nct": 2,
#         }
#     }
#
#     try:
#         response = requests.post(url, headers=headers, data=json.dumps(payload))
#         print("OM2M Subscription Response:", response.status_code)
#         print("Response Body:", response.text)
#     except Exception as e:
#         print("Failed to subscribe to OM2M:", e)


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
        encoded = sensor_data["image_encoding"]
        image_data = base64.b64decode(encoded)

        # create a Pillow image
        image = Image.open(BytesIO(image_data))

        # api call to landing lens to get final verdict
        predictor = Predictor(sensitiveInfo.ENDPOINT_ID, api_key=sensitiveInfo.API_KEY)
        predictions = predictor.predict(image)
        print(predictions)
        # ouput format: LIST: [ClassificationPrediction(score=0.9980272650718689, label_name='Defective', label_index=0)]
        condition = predictions[0].label_name
        score = round(predictions[0].score, 2)
        # then determine and modify the dict s.t. it includes everything
        sensor_data["condition"] = condition
        sensor_data["Image_score"] = score
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
    query = {}

    # Date Filter
    date_str = request.query_params.get("date")
    if date_str:
        try:
            query["date"] = datetime.strptime(date_str, "%Y-%m-%d").strftime("%Y-%m-%d")
        except ValueError:
            pass

    # Condition Filter
    condition = request.query_params.get("condition")
    if condition:
        query["condition"] = condition

    # Alignment Filter
    alignment = request.query_params.get("alignment")
    if alignment:
        query["alignment"] = alignment

    # Ultrasound Min Filter
    ultrasound_min = request.query_params.get("ultrasound_min")
    if ultrasound_min:
        try:
            query["ultrasound_reading_1"] = {
                "$gte": float(ultrasound_min)
            }  # Filter by minimum value
        except ValueError:
            pass

    # Score Min Filter
    score_min = request.query_params.get("score_min")
    if score_min:
        try:
            query["Image_score"] = {"$gte": float(score_min)}  # Filter by minimum score
        except ValueError:
            pass

    entries = list(collection.find(query))

    grouped_data = {}
    for entry in entries:
        track_id = entry.get("track_id", "Unknown")
        if track_id not in grouped_data:
            grouped_data[track_id] = []
        grouped_data[track_id].append(entry)

    return templates.TemplateResponse(
        "modified.html", {"request": request, "data": grouped_data}
    )


@app.get("/analytics", response_class=HTMLResponse)
async def analytics(request: Request):
    entries = list(collection.find({}))

    # Convert MongoDB entries to DataFrame
    df = pd.DataFrame(entries)
    df["ultrasound_reading_1"] = pd.to_numeric(
        df["ultrasound_reading_1"], errors="coerce"
    )
    df["Image_score"] = pd.to_numeric(df["Image_score"], errors="coerce")
    df.dropna(subset=["ultrasound_reading_1", "Image_score"], inplace=True)

    # Generate plots
    plots = []

    def plot_to_base64(fig):
        buf = BytesIO()
        fig.savefig(buf, format="png", bbox_inches="tight")
        buf.seek(0)
        encoded = base64.b64encode(buf.read()).decode("utf-8")
        plt.close(fig)
        return encoded

    # 1. Condition vs Score
    fig1, ax1 = plt.subplots()
    sns.barplot(data=df, x="condition", y="Image_score", ax=ax1)
    ax1.set_title("Condition vs. Image Score")
    plots.append(plot_to_base64(fig1))

    # 2. Condition vs Ultrasound
    fig2, ax2 = plt.subplots()
    sns.barplot(data=df, x="condition", y="ultrasound_reading_1", ax=ax2)
    ax2.set_title("Condition vs. Ultrasound")
    plots.append(plot_to_base64(fig2))

    # 3. Alignment vs Ultrasound
    fig3, ax3 = plt.subplots()
    sns.barplot(data=df, x="alignment", y="ultrasound_reading_1", ax=ax3)
    ax3.set_title("Alignment vs. Ultrasound")
    plots.append(plot_to_base64(fig3))

    # 4. Extra: Image Score vs Ultrasound
    fig4, ax4 = plt.subplots()
    sns.regplot(data=df, x="Image_score", y="ultrasound_reading_1", ax=ax4)
    ax4.set_title("Score vs. Ultrasound")
    plots.append(plot_to_base64(fig4))

    return templates.TemplateResponse(
        "analytics.html", {"request": request, "plots": plots}
    )
