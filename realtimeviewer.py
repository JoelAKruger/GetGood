import mss
from PIL import Image, ImageGrab
import pyautogui

import cv2 as cv
import numpy as np
import time

import torch

model = torch.hub.load('ultralytics/yolov5', 'custom', path='last.pt', force_reload=True)
model.conf = 0.02

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
    cv.imshow("Computer Vision", np.squeeze(results.render()))

    # Break loop and end test
    key = cv.waitKey(1)
    if key == ord('q'):
        break
    
    elapsed_time = time.time() - t0
    avg_fps = (n_frames / elapsed_time)
    print("Average FPS: " + str(avg_fps))
    n_frames += 1