# image_processing.py

import os
import cv2
import numpy as np
import psycopg2
from keras.models import load_model
from keras.preprocessing.image import img_to_array
from PIL import Image
from dotenv import load_dotenv
import time
import serial

class LeafDiseaseDetector:
    def __init__(self, model_path, db_config, arduino_port):
        self.model = load_model(model_path)
        self.db_config = db_config
        self.class_labels = {0: "Healthy", 1: "Powdery", 2: "Rust"}
        self.arduino = serial.Serial(arduino_port, 115200, timeout=1)

    def move_servo(self, angle):
        if 0 <= angle <= 180:  # Ensure angle is valid
            command = f"{angle}\n"  # Add newline for Arduino parsing
            self.arduino.write(command.encode())  # Send angle to Arduino
            time.sleep(.1)  # Allow time for the servo to move
        else:
            print("Angle out of range! Must be between 0 and 180.")
 
    def send_status_to_arduino(self, status):
        if self.arduino.is_open:
            try:
                if status == "Healthy":
                    self.move_servo(90)  # Move the servo to position 0
                elif status == "Powdery" or status == "Rust":
                    self.move_servo(0)
                    time.sleep(2)
                print(f"Sent to Arduino: {status}")
            except Exception as e:
                print(f"Error sending data to Arduino: {e}")
        else:
            print("Arduino is not connected!")

    

    def capture_image(self, image_path):
        cap = cv2.VideoCapture(0)  # Open the default camera
        if not cap.isOpened():
            print("Error: Could not open camera.")
            return False
        ret, frame = cap.read()  # Capture a single frame
        cap.release()  # Release the camera
        if ret:
            cv2.imwrite(image_path, frame)  # Save the captured frame as an image file
            print(f"Picture taken and saved as '{image_path}'.")
            return True
        else:
            print("Error: Could not read frame.")
            return False

    def preprocess_image(self, image_path):
        test_img = Image.open(image_path).resize((225, 225))  # Load and resize the image using PIL
        test_img_array = img_to_array(test_img)
        test_img_array = np.expand_dims(test_img_array, axis=0)  # Add batch dimension
        test_img_array /= 255.0  # Normalize the image
        return test_img_array

    def predict(self, test_img_array):
        predictions = self.model.predict(test_img_array)
        predicted_class = np.argmax(predictions, axis=1)
        return self.class_labels.get(predicted_class[0], "Unknown")

    def insert_image_to_db(self, image_path, predicted_class):
        try:
            connection = psycopg2.connect(**self.db_config)
            cursor = connection.cursor()
            with open(image_path, 'rb') as image_file:
                image_data = image_file.read()
            insert_query = "INSERT INTO images (images, result) VALUES (%s, %s);"
            cursor.execute(insert_query, (psycopg2.Binary(image_data), predicted_class))
            connection.commit()
            print("Image inserted successfully.")
        except Exception as e:
            print(f"Failed to insert image: {e}")
        finally:
            cursor.close()
            connection.close()
            print("Connection closed.")

def main():
    load_dotenv()  # Load environment variables from .env

    # Database configuration
    db_config = {
        'user': "postgres.zsniufaudrldmnecbupq",
        'password': "CPC357_Project",
        'host': "aws-0-ap-southeast-1.pooler.supabase.com",
        'port': "6543",
        'dbname': "postgres"
    }

    script_dir = os.path.dirname(os.path.abspath(__file__))
    model_path = os.path.join(script_dir, 'leaf_disease_detection_model.h5')
    image_path = os.path.join(script_dir, 'snapshot.jpg')
    arduino_port = "COM5"
    detector = LeafDiseaseDetector(model_path, db_config, arduino_port)

#while True:  # Infinite loop to keep taking pictures every 30 minutes
    if detector.capture_image(image_path):
        test_img_array = detector.preprocess_image(image_path)
        predicted_class = detector.predict(test_img_array)
        print(f'Predicted class: {predicted_class}')
        detector.insert_image_to_db(image_path, predicted_class)
        
        # Send status to Arduino to control the servo
        detector.send_status_to_arduino(predicted_class)
    
    print("Waiting for 30 minutes before the next capture...")
    #time.sleep(5)  # Sleep for 30 minutes (1800 seconds)

if __name__ == "__main__":
    main()