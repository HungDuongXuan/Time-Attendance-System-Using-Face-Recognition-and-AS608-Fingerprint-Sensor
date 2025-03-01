import gradio as gr
from src.new_face import new_face
from src.face_query import query_face
import pandas as pd
from datetime import datetime
from check_spoofing import check_spoofing
import cv2
import requests

from fastapi import FastAPI, Request
import uvicorn


css = """
:root {
    --primary-color: #2563eb;
    --secondary-color: #1e40af;
}

body {
    font-family: 'Segoe UI', Arial, sans-serif;
    background-color: #f3f4f6;
}

.gradio-container {
    max-width: 1200px !important;
    margin: 0 auto;
}

.main-header {
    text-align: center;
    color: var(--primary-color);
    font-size: 2.5rem;
    margin: 2rem 0;
    font-weight: bold;
}

.tab-nav {
    background-color: white;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.input-container {
    background-color: white;
    padding: 1.5rem;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.custom-button {
    background-color: var(--primary-color) !important;
    color: white !important;
    padding: 0.75rem 1.5rem !important;
    border-radius: 6px !important;
    font-weight: 600 !important;
    transition: background-color 0.3s ease !important;
}

.custom-button:hover {
    background-color: var(--secondary-color) !important;
}

.output-box {
    background-color: #f8fafc;
    border: 1px solid #e2e8f0;
    border-radius: 6px;
    padding: 1rem;
}

.gradio-container [data-testid="dataframe"] {
    font-family: 'Segoe UI', Arial, sans-serif;
    margin-top: 1rem;
}

.gradio-container [data-testid="dataframe"] th {
    background-color: var(--primary-color);
    color: white;
    padding: 12px;
    font-weight: 600;
}

.gradio-container [data-testid="dataframe"] td {
    padding: 10px;
    border-bottom: 1px solid #e2e8f0;
}

.gradio-container [data-testid="dataframe"] tr:nth-child(even) {
    background-color: #f8fafc;
}

.gradio-container [data-testid="dataframe"] tr:hover {
    background-color: #e2e8f0;
}

.date-input input {
    padding: 8px 12px;
    border: 1px solid #e2e8f0;
    border-radius: 6px;
    font-size: 14px;
}

.date-input input:focus {
    border-color: var(--primary-color);
    outline: none;
    box-shadow: 0 0 0 2px rgba(37, 99, 235, 0.2);
}
"""

model_dir = "./resources/anti_spoof_models"
device_id = 0
esp32_cam_url = "http://192.168.1.5/stream"


class AttendanceManager:
    def __init__(self):
        self.attendance_list = []
        self.current_index = 1

    def add_attendance(self, student_id, name, status):
        attendance_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.attendance_list.append({
            'STT': self.current_index,
            'ID': student_id,
            'Họ và tên': name,
            'Thời gian vào': attendance_time,
            'Trạng thái': status
        })
        self.current_index += 1

    def get_attendance_by_date(self, selected_date):
        if not selected_date:
            return pd.DataFrame(self.attendance_list)

        filtered_list = [
            record for record in self.attendance_list
            if record['Thời gian vào'].startswith(selected_date)
        ]
        return pd.DataFrame(filtered_list) if filtered_list else None

    def export_to_csv(self, selected_date=None):
        if not self.attendance_list:
            return "Chưa có dữ liệu"

        if selected_date:
            df = self.get_attendance_by_date(selected_date)
            if df is None or df.empty:
                return f"Không có dữ liệu cho ngày {selected_date}"
            filename = f"attendance_{selected_date}.csv"
        else:
            df = pd.DataFrame(self.attendance_list)
            filename = f"attendance_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

        df.to_csv(filename, index=False, encoding='utf-8-sig')
        return f"✅ Đã xuất file: {filename}"


def main():
    attendance_manager = AttendanceManager()
    app = FastAPI()

    def capture_image(url=0):
        cap = cv2.VideoCapture(url)
        if not cap.isOpened():
            return "Không thể mở webcam."
        ret, frame = cap.read()
        cap.release()
        if not ret:
            return "Không thể chụp ảnh."
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        return frame

    def register_interface(image_input=None, student_id='', name=''):
        image_input = capture_image(esp32_cam_url)
        # image_input = capture_image()
        is_spoofing, score = check_spoofing(image_input, model_dir, device_id)
        if is_spoofing:
            return f"WARNING: Phát hiện giả mạo!!!"

        new_face(f"{student_id} - {name}", [image_input])
        return f"✅ Đăng kí thành công: {name} - ID: {student_id}", image_input

    def attendance_interface(attendance_image=None):
        attendance_image = capture_image(esp32_cam_url)
        # attendance_image = capture_image()
        if attendance_image is None:
            return "⚠️ Vui lòng tải lên ảnh để điểm danh"
        is_spoofing, score = check_spoofing(attendance_image, model_dir, device_id)
        if is_spoofing:
            return f"WARNING: Phát hiện giả mạo!!!", attendance_image
        result = query_face(attendance_image)
        if not result:
            attendance_manager.add_attendance("N/A", "N/A", "Không nhận diện được")
            return "❌ Không tìm thấy khuôn mặt hoặc không nhận diện được nhân viên", attendance_image

        # Parse student_id and name from result
        try:
            # try:
            #     response = requests.post(
            #         'http://192.168.239.131:5000/receiveMessage',
            #         json={'message': result}  # Gửi result dưới dạng JSON
            #     )
            #     if response.status_code == 200:
            #         print(f"Đã gửi thông điệp thành công: {result}")
            #     else:
            #         print(f"Lỗi khi gửi thông điệp: {response.status_code}")
            # except Exception as e:
            #     print(f"Lỗi khi kết nối đến server: {str(e)}")
            student_id, name = result.split(" - ", 1)
            attendance_manager.add_attendance(student_id, name, "Có mặt")
        except:
            attendance_manager.add_attendance("Error", result, "Lỗi định dạng")

        return f"✅ Chấm công thành công - ID: {result}", attendance_image

    def get_attendance_dataframe(selected_date=None):
        return attendance_manager.get_attendance_by_date(selected_date)

    def export_attendance(selected_date=None):
        return attendance_manager.export_to_csv(selected_date)

    with gr.Blocks(title="Facial Recognition Attendance System", css=css) as interface:

        gr.Markdown(
            """
            <div class="main-header">
                Hệ Thống chấm công bằng nhận diện gương mặt
            </div>
            """
        )

        with gr.Tabs(elem_classes="tab-nav") as tabs:
            with gr.Tab("📝 Đăng Ký Nhân Viên Mới"):
                with gr.Column(elem_classes="input-container"):
                    with gr.Row():
                        image_input = gr.Image()
                        with gr.Column():
                            student_id = gr.Textbox(
                                label="ID",
                                placeholder="Nhập ID...",
                            )
                            name = gr.Textbox(
                                label="Họ và tên",
                                placeholder="Nhập họ tên nhân viên...",
                            )
                            register_btn = gr.Button(
                                "🔒 Đăng Ký",
                                elem_classes="custom-button"
                            )
                    register_output = gr.Textbox(
                        label="Trạng thái đăng ký",
                        elem_classes="output-box"
                    )
                    register_btn.click(
                        fn=register_interface,
                        inputs=[image_input, student_id, name],
                        outputs=[register_output, image_input]
                    )

            with gr.Tab("📸 Chấm công"):
                with gr.Column(elem_classes="input-container"):
                    with gr.Row():
                        attendance_image = gr.Image()
                        with gr.Column():
                            attendance_btn = gr.Button(
                                "✔️ Chấm công",
                                elem_classes="custom-button"
                            )
                    attendance_output = gr.Textbox(
                        label="Kết quả",
                        elem_classes="output-box"
                    )
                    attendance_btn.click(
                        fn=attendance_interface,
                        inputs=attendance_image,
                        outputs=[attendance_output, attendance_image]
                    )

            with gr.Tab("📋 Danh Sách Chấm Công"):
                with gr.Column(elem_classes="input-container"):
                    date_select = gr.Textbox(
                        label="Chọn ngày (định dạng YYYY-MM-DD)",
                        placeholder="VD: 2024-03-20",
                        elem_classes="date-input"
                    )
                    attendance_dataframe = gr.Dataframe(
                        headers=["STT", "ID", "Họ và tên", "Thời gian vào", "Trạng thái"],
                        datatype=["number", "str", "str", "str", "str"],
                        label="Danh sách nhân viên",
                        interactive=False,
                    )
                    with gr.Row():
                        refresh_btn = gr.Button(
                            "🔄 Làm mới danh sách",
                            elem_classes="custom-button"
                        )
                        export_btn = gr.Button(
                            "📊 Xuất File",
                            elem_classes="custom-button"
                        )
                    export_output = gr.Textbox(
                        label="Trạng thái xuất file",
                        elem_classes="output-box"
                    )

                    # Cập nhật các event handlers
                    refresh_btn.click(
                        get_attendance_dataframe,
                        inputs=[date_select],
                        outputs=attendance_dataframe
                    )
                    export_btn.click(
                        export_attendance,
                        inputs=[date_select],
                        outputs=export_output
                    )

    # Mount the Gradio app onto the FastAPI app
    gr.mount_gradio_app(app, interface, path="/gradio")

    # Define the custom endpoint
    @app.post("/receiveMessage")
    async def receive_message(request: Request):
        data = await request.json()
        if not data or 'message' not in data:
            return {"error": "No message found in the request"}
        message = data['message']
        if message == "Authentication by face":
            result, image = attendance_interface()
            print("result", result)
            try:
                response = requests.post(
                    'http://localhost:5000/receiveMessage',
                    json={'message': result}  # Gửi result dưới dạng JSON
                )
                if response.status_code == 200:
                    print(f"Đã gửi thông điệp thành công: {result}")
                else:
                    print(f"Lỗi khi gửi thông điệp: {response.status_code}")
            except Exception as e:
                print(f"Lỗi khi kết nối đến server: {str(e)}")
            return {"message": "Attendance triggered", "result": result}
        else:
            return {"message": f"Message received: {message}"}

    # Run the app using uvicorn
    uvicorn.run(app, host="0.0.0.0", port=7860)

    # interface.launch()


if __name__ == "__main__":
    main()
