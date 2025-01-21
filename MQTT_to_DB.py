import pymongo
import paho.mqtt.client as mqtt
from datetime import datetime, timezone
# MongoDB configuration
mongo_client = pymongo.MongoClient("mongodb://localhost:27017/")
db = mongo_client["smarthome"]
collection = db["iot"]
# MQTT configuration
mqtt_broker_address = "34.56.86.175"
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
    # Convert MQTT timestamp to datetime
    timestamp = datetime.now(timezone.utc)
    datetime_obj = timestamp.strftime("%Y-%m-%dT%H:%M:%S.%fZ")
    # Process the payload and insert into MongoDB with proper timestamp
    document = {"timestamp": datetime_obj, "data": payload}
    collection.insert_one(document)
    print("Data ingested into MongoDB")
    
# Create a MQTT client instance
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
# Attach the callbacks using explicit methods
client.on_connect = on_connect
client.on_message = on_message
# Connect to MQTT broker
client.connect(mqtt_broker_address, 1883, 60)
# Start the MQTT loop
client.loop_forever()