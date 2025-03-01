import faiss
from .embedding_face import embedding_face
from .process_name import get_name
import os

os.environ["KMP_DUPLICATE_LIB_OK"] = "TRUE"


def query_face(img):
    index = faiss.read_index('vector_database/face_embedding.index')
    face_embedding = embedding_face(img)
    faiss.normalize_L2(face_embedding)
    D, I = index.search(face_embedding, k=1)
    name = get_name(index=I[0][0])
    return name
