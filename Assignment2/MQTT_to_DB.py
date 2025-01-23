import pymongo
import paho.mqtt.client as mqtt
from datetime import datetime, timezone
import json

# MongoDB configuration
mongo_client = pymongo.MongoClient("mongodb://localhost:27017/")
db = mongo_client["smartalgriculture"]
collection = db["iot"]

# MQTT configuration
mqtt_broker_address = "34.68.255.77"
mqtt_topic = "iot"

# Define the callback function for connection
def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print(f"Successfully connected")
        client.subscribe(mqtt_topic)

# Define the callback function for ingesting data into MongoDB
def on_message(client, userdata, message):
    payload = message.payload.decode("utf-8")
    print(f"Received message: {payload}")
    
    try:
        # Parse the payload as JSON (it's a string, so it needs to be converted to a dictionary)
        payload_data = json.loads(payload)
        
        # Convert MQTT timestamp to current UTC time
        timestamp = datetime.now(timezone.utc)
        datetime_obj = timestamp.strftime("%Y-%m-%dT%H:%M:%S.%fZ")
        
        # Create the document to insert into MongoDB
        document = {"timestamp": datetime_obj, "data": payload_data}
        
        # Insert the document into MongoDB
        collection.insert_one(document)
        print("Data ingested into MongoDB")
    except json.JSONDecodeError:
        print("Failed to decode payload to JSON")
    except Exception as e:
        print(f"Error: {e}")

# Create a MQTT client instance
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

# Attach the callbacks using explicit methods
client.on_connect = on_connect
client.on_message = on_message

# Connect to MQTT broker
client.connect(mqtt_broker_address, 1883, 60)

# Start the MQTT loop
client.loop_forever()