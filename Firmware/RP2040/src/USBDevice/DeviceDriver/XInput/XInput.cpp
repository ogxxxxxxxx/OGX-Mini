import ctypes
import cv2
import math
import mss
import os
import sys
import time
import numpy as np
import onnxruntime as ort
import serial
import struct
from termcolor import colored

# ==============================================================================
# CONFIGURACIÓN: TARGET LOCK + ESTABILIDAD HARDWARE
# ==============================================================================
COM_PORT = 'COM6'        
BAUD_RATE = 115200

fov_default = 750

SCALE_X = 0.10  
SCALE_Y = 0.39   

BASE_SENSITIVITY = 1200 
MIN_FORCE = 3500         
MAX_FORCE = 15000        
RECOIL_STRENGTH = 2.0    

FRENO_DISTANCIA = 30    
POTENCIA_FRENO = 0.75    
SEND_DELAY = 0.05      

VISUAL_SCALE = 0.2      
VISUAL_BOX_SHIFT_Y = 80  
CONFIDENCE_LEVEL = 0.75  
LOCK_FREEZE_RADIUS = 8   # 🔥 radio donde se queda totalmente quieto

LOCK_DISTANCE_THRESHOLD = 60  
TARGET_SWITCH_COOLDOWN = 0.25
   
SWITCH_RATIO = 0.65               

# 🔹 NUEVO: elimina temblor al llegar
DEADZONE_PIXELS = 6

# ==============================================================================

class XINPUT_GAMEPAD(ctypes.Structure):
    _fields_ = [("wButtons", ctypes.c_ushort), ("bLeftTrigger", ctypes.c_ubyte),
                ("bRightTrigger", ctypes.c_ubyte), ("sThumbLX", ctypes.c_short),
                ("sThumbLY", ctypes.c_short), ("sThumbRX", ctypes.c_short),
                ("sThumbRY", ctypes.c_short)]

class XINPUT_STATE(ctypes.Structure):
    _fields_ = [("dwPacketNumber", ctypes.c_ulong), ("Gamepad", XINPUT_GAMEPAD)]

class XInput:
    def __init__(self):
        try: self.xinput = ctypes.windll.xinput1_4
        except OSError: self.xinput = ctypes.windll.xinput1_3
        self.state = XINPUT_STATE()
    def get_l2_pressed(self):
        res = self.xinput.XInputGetState(0, ctypes.byref(self.state))
        return self.state.Gamepad.bLeftTrigger > 20 if res == 0 else False

_mss_tmp = mss.mss()
monitor = _mss_tmp.monitors[1] if len(_mss_tmp.monitors) > 1 else _mss_tmp.monitors[0]
screen_center_x = monitor['left'] + (monitor['width'] // 2)
screen_center_y = monitor['top'] + (monitor['height'] // 2)
_mss_tmp.close()

def xywh2xyxy_arr(x):
    y = np.copy(x)
    y[:, 0] = x[:, 0] - x[:, 2] / 2
    y[:, 1] = x[:, 1] - x[:, 3] / 2
    y[:, 2] = x[:, 0] + x[:, 2] / 2
    y[:, 3] = x[:, 1] + x[:, 3] / 2
    return y

class Aimbot:
    screen = mss.mss()
    
    def __init__(self, **kwargs):
        self.box_constant = int(fov_default)
        self.last_target_pos = None
        self.current_target_center_dist = None
        
        try:
            self.session = ort.InferenceSession(
                'lib/best.onnx',
                providers=['DmlExecutionProvider', 'CPUExecutionProvider']
            )
            print(colored(f"✅ MOTOR CON TARGET LOCK ESTABLE", 'green'))
        except Exception:
            sys.exit()

        self.input_name = self.session.get_inputs()[0].name
        
        try:
            self.serial = serial.Serial(port=COM_PORT, baudrate=BAUD_RATE, timeout=0)
            self.serial.dtr = True
            self.serial.rts = True
        except:
            self.serial = None

        self.controller = XInput()
        self.last_packet_time = 0
        self.last_switch_time = 0
        self.last_sent_x = None
        self.last_sent_y = None


        self.detection_box = {
            'left': int(screen_center_x - self.box_constant // 2), 
            'top': int(screen_center_y - self.box_constant // 2), 
            'width': int(self.box_constant), 
            'height': int(self.box_constant)
        }

    def send_packet(self, joy_x, joy_y):
        if self.serial is None:
            return

        # 🔹 NO enviar si es igual al anterior
        if joy_x == self.last_sent_x and joy_y == self.last_sent_y:
            return

        t = time.time()
        if t - self.last_packet_time < SEND_DELAY:
            return

        self.last_packet_time = t
        self.last_sent_x = joy_x
        self.last_sent_y = joy_y

        try:
            self.serial.write(struct.pack('>Bhh', 0xA5, int(joy_x), int(joy_y)))
        except:
            pass


        # 🔹 NO enviar si es igual al anterior
        if joy_x == self.last_sent_x and joy_y == self.last_sent_y:
            return

        t = time.time()
        if t - self.last_packet_time < SEND_DELAY:
            return

        self.last_packet_time = t
        self.last_sent_x = joy_x
        self.last_sent_y = joy_y

        try:
            self.serial.write(struct.pack('>Bhh', 0xA5, int(joy_x), int(joy_y)))
        except:
            pass

    def move_to_target(self, rel_x, rel_y, frame_w, frame_h):
        cv_x, cv_y = frame_w // 2, frame_h // 2
        dx = rel_x - cv_x
        dy = rel_y - cv_y
        dist = math.hypot(dx, dy)

        # 🔥 FREEZE TOTAL CUANDO LLEGA
        if dist <= LOCK_FREEZE_RADIUS:
            self.send_packet(0, 0)
            return
        

        current_sens = BASE_SENSITIVITY
        if dist < FRENO_DISTANCIA:
            current_sens *= POTENCIA_FRENO

        force_x = dx * current_sens
        force_y = dy * current_sens

        if dy > 0:
            force_y *= RECOIL_STRENGTH

        # MIN_FORCE solo cuando está lejos
        if dist > FRENO_DISTANCIA:
            if 0 < abs(force_x) < MIN_FORCE:
                force_x = MIN_FORCE if force_x > 0 else -MIN_FORCE
            if 0 < abs(force_y) < MIN_FORCE:
                force_y = MIN_FORCE if force_y > 0 else -MIN_FORCE

        force_x = max(-MAX_FORCE, min(MAX_FORCE, force_x))
        force_y = max(-MAX_FORCE, min(MAX_FORCE, force_y))

        force_y *= -1
        self.send_packet(force_x, force_y)

    def start(self):
        while True:
            screenshot = Aimbot.screen.grab(self.detection_box)
            frame = cv2.cvtColor(np.array(screenshot), cv2.COLOR_BGRA2BGR)
            f_h, f_w = frame.shape[:2]
            cv_x, cv_y = f_w // 2, f_h // 2

            img = cv2.resize(frame, (640, 640))
            blob = img.astype(np.float32).transpose(2, 0, 1) / 255.0
            blob = np.expand_dims(blob, axis=0)

            outputs = self.session.run(None, {self.input_name: blob})
            pred = outputs[0][0]
            if pred.shape[0] < pred.shape[1]:
                pred = pred.transpose(1, 0)

            scores = np.max(pred[:, 4:], axis=1)
            mask = scores > CONFIDENCE_LEVEL

            if np.any(mask):
                boxes = xywh2xyxy_arr(pred[mask, :4]) * (self.box_constant / 640)
                indices = cv2.dnn.NMSBoxes(
                    boxes.tolist(),
                    scores[mask].tolist(),
                    CONFIDENCE_LEVEL,
                    0.45
                )

                if len(indices) > 0:
                    candidates = []

                    for i in indices:
                        idx = i if isinstance(i, (int, np.integer)) else i[0]
                        b = boxes[idx]
                        cx = b[0] + (b[2]-b[0]) * 0.5
                        cy = b[1] + (b[3]-b[1]) * 0.5
                        dist_center = math.hypot(cx - cv_x, cy - cv_y)
                        candidates.append((idx, b, cx, cy, dist_center))

                    best_candidate = None

                    if self.last_target_pos is not None:
                        min_lock_dist = float('inf')
                        lock_candidate = None

                        for c in candidates:
                            lock_dist = math.hypot(
                                c[2] - self.last_target_pos[0],
                                c[3] - self.last_target_pos[1]
                            )
                            if lock_dist < min_lock_dist:
                                min_lock_dist = lock_dist
                                lock_candidate = c

                        if min_lock_dist < LOCK_DISTANCE_THRESHOLD:
                            best_candidate = lock_candidate

                    if best_candidate is None:
                        current_time = time.time()
                        if current_time - self.last_switch_time > TARGET_SWITCH_COOLDOWN:
                            best_candidate = min(candidates, key=lambda x: x[4])
                            self.last_switch_time = current_time
            

                    if best_candidate is not None:
                        idx, b, cx, cy, center_dist = best_candidate
                        self.last_target_pos = (cx, cy)

                        w, h = b[2]-b[0], b[3]-b[1]
                        tx = b[0] + (w * 0.5) + (w * SCALE_X)
                        ty = b[1] + (h * SCALE_Y) + VISUAL_BOX_SHIFT_Y

                        vx1 = int(b[0] + (w * (1 - VISUAL_SCALE) / 2))
                        vy1 = int(b[1] + (h * (1 - VISUAL_SCALE) / 2) + VISUAL_BOX_SHIFT_Y)
                        cv2.rectangle(frame, (vx1, vy1),
                                      (vx1+int(w*VISUAL_SCALE), vy1+int(h*VISUAL_SCALE)),
                                      (0, 255, 0), 2)

                        cv2.line(frame, (int(tx), int(ty)), (cv_x, cv_y),
                                 (244, 242, 113), 2)

                        if self.controller.get_l2_pressed():
                            self.move_to_target(tx, ty, f_w, f_h)

            else:
                self.last_target_pos = None

            cv2.imshow("Vision Sincronizada", frame)
            if cv2.waitKey(1) & 0xFF == ord('0'):
                break

if __name__ == "__main__":
    app = Aimbot()
    app.start()
