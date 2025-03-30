from flask import Flask, request, render_template, jsonify, send_file
from pymongo import MongoClient
import gridfs
import certifi
import io
from bson import ObjectId
import password

app = Flask(__name__)

# MongoDB Atlas Connection
MONGO_URI = "mongodb+srv://yadalak1:" + password.password + "@iotproject.tb44wol.mongodb.net/?retryWrites=true&w=majority&appName=IoTProject"
client = MongoClient(MONGO_URI, tlsCAFile=certifi.where())
db = client['image_database']
fs = gridfs.GridFS(db)
metadata_collection = db["metadata"]

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/get_data', methods=['GET'])
def get_data():
    data = []
    for doc in metadata_collection.find():
        entry = {
            "id": str(doc["image_id"]),
            "Route": doc["Route"],
            "TimeTake": doc["TimeTake"],
            "Accelerometer": doc["Accelerometer"],
            "Temperature": doc["Temperature"]
        }
        data.append(entry)
    return jsonify(data)

@app.route('/upload', methods=['POST'])
def upload_image():
    if 'image' not in request.files:
        return jsonify({"error": "No image found"}), 400
    
    image = request.files['image']
    route = request.form.get("Route", " ")
    time_take = int(request.form.get("TimeTake", 0))
    accelerometer = float(request.form.get("Accelerometer", 0.00))
    temperature = int(request.form.get("Temperature", 0))

    # Store image in GridFS
    file_id = fs.put(image, filename=image.filename)

    # Store metadata
    metadata = {
        "image_id": file_id,
        "Route": route,
        "TimeTake": time_take,
        "Accelerometer": accelerometer,
        "Temperature": temperature
    }
    metadata_collection.insert_one(metadata)

    return jsonify({"message": "Upload successful", "id": str(file_id)}), 200

@app.route('/image/<file_id>', methods=['GET'])
def get_image(file_id):
    try:
        file_id = ObjectId(file_id)
        image = fs.get(file_id)
        return send_file(io.BytesIO(image.read()), mimetype="image/jpeg")
    except:
        return jsonify({"error": "Image not found"}), 404

@app.route('/metadata/<file_id>', methods=['GET'])
def get_metadata(file_id):
    try:
        metadata = metadata_collection.find_one({"image_id": ObjectId(file_id)}, {"_id": 0})
        if metadata:
            return jsonify(metadata), 200
        else:
            return jsonify({"error": "Metadata not found"}), 404
    except:
        return jsonify({"error": "Invalid ID"}), 400

if __name__ == '__main__':
    app.run(debug=True)
