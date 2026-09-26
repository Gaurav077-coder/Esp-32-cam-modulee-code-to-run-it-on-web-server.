
import cv2
from ultralytics import YOLO

# Your trained model
MODEL_PATH = "best.pt"

# Replace this with the IP printed by ESP32-CAM
ESP32_STREAM = ""

print("Loading YOLO model...")
model = YOLO(MODEL_PATH)

print("Opening ESP32-CAM stream...")
camera = cv2.VideoCapture(ESP32_STREAM)

if not camera.isOpened():
    print("ERROR: ESP32-CAM stream could not be opened.")
    print("Check the IP address and Wi-Fi connection.")
    raise SystemExit

while True:
    success, frame = camera.read()

    if not success:
        print("Could not receive camera frame.")
        break

    # Run YOLO on the ESP32-CAM frame
    results = model.predict(
        source=frame,
        conf=0.60,
        verbose=False
    )

    # Draw YOLO boxes and names
    annotated_frame = results[0].plot()

    cv2.imshow(
        "WTE ESP32-CAM YOLO Detection",
        annotated_frame
    )

    # Press Q to close
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

camera.release()
cv2.destroyAllWindows()
