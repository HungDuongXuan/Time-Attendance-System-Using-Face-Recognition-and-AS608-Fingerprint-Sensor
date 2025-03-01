#!/usr/bin/env python

import os
import cv2
import onnxruntime
from .scrfd import SCRFD
from .arcface_onnx import ArcFaceONNX

onnxruntime.set_default_logger_severity(3)

assets_dir = os.path.join('.insightface', 'models', 'buffalo_l')

detector = SCRFD(os.path.join(assets_dir, 'det_10g.onnx'))
detector.prepare(0)
model_path = os.path.join(assets_dir, 'w600k_r50.onnx')
rec = ArcFaceONNX(model_path)
rec.prepare(0)


def embedding_face(image):
    if isinstance(image, str):
        image = cv2.imread(image)
    bboxes, kpss = detector.autodetect(image, max_num=1)
    if bboxes.shape[0] == 0:
        return None
    kps = kpss[0]
    feat = rec.get(image, kps)
    return feat.reshape(-1, 512)

