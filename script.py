import mss
import numpy as np
import cv2
import time
import keyboard
import torch

model = torch.hub.load('ultralytics/yolov5', 'custom', path='last.pt', force_reload=True)

with mss.mss() as sct:
	monitor = {"top": 0, "left": 0, "width": 2560, "height": 1440}
	
while True:
	t = time.time()
	img = np.array(sct.grab(monitor))
	results = model(img)
	
	cv2.imshow("s", np.squeeze(results.render()))

	print("FPS: {}".format(1 / (time.time - t)))

	if keyboard.is_pressed("q"):
		break

cv2.destroyAllWindows()