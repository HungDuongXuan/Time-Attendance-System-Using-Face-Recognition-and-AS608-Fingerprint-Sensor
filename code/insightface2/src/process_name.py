
def add_name(name):
    with open("name_database/face_name.txt", "a", encoding="utf-8") as file:
        file.write(name + '\n')


def get_name(index):
    with open("name_database/face_name.txt", "r") as file:
        data = file.readlines()
        return data[index]
