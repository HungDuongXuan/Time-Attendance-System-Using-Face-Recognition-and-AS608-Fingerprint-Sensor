import faiss

dimension = 512
index = faiss.IndexFlatIP(dimension)

file_path = "face_embedding.index"

faiss.write_index(index, file_path)