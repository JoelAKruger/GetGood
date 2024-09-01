import mss
from PIL import Image, ImageGrab
from ultralytics import YOLO
import pyautogui

import cv2 as cv
import numpy as np
import time

import torch


import sys

if len(sys.argv) < 2:
	print("Please specify .pt model file")
	exit()

#model = torch.hub.load('ultralytics/yolov8', 'custom', path=sys.argv[1], force_reload=True)
model = YOLO(sys.argv[1])
model.conf = 0.05

w, h = pyautogui.size()
print("PIL Screen Capture Speed Test")
print("Screen Resolution: " + str(w) + 'x' + str(h))

img = None
t0 = time.time()
n_frames = 1

while True:
    img = ImageGrab.grab()
    img = np.array(img)                         # Convert to NumPy array
    img = cv.cvtColor(img, cv.COLOR_RGB2BGR)    # Convert RGB to BGR color
    
    small = cv.resize(img, (0, 0), fx=0.5, fy=0.5)
    results = model(small)
    cv.imshow("Computer Vision", np.squeeze(results[0].plot()))

    # Break loop and end test
    key = cv.waitKey(1)
    if key == ord('q'):
        break
    
    elapsed_time = time.time() - t0
    avg_fps = (n_frames / elapsed_time)
    print("Average FPS: " + str(avg_fps))
    n_frames += 1