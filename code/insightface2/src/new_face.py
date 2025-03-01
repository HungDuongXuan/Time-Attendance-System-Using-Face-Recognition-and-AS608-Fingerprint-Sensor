import numpy as np
import faiss
from .process_name import add_name
from .face_query import embedding_face


def new_face(name, images=None):
    index = faiss.read_index('vector_database/face_embedding.index')
    face_embeddings = []
    print('Đang chụp ảnh gương mặt ...')
    img_list = images
    print('Chụp ảnh gương mặt thành công!')
    print('Đang đăng kí khuôn mặt ...')
    for img in img_list:
        face_embedding = embedding_face(img)
        face_embeddings.append(face_embedding)
    face_embeddings = np.stack(face_embeddings)
    face_embedding = np.mean(face_embeddings, axis=0).reshape(-1, 512)
    faiss.normalize_L2(face_embedding)
    index.add(face_embedding)
    faiss.write_index(index, 'vector_database/face_embedding.index')
    add_name(name=name)
    print('Đăng kí khuôn mặt thành công!')