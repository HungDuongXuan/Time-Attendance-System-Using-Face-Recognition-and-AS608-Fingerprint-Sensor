from flask import Flask, request, jsonify
import requests  # Import thư viện requests để gửi HTTP request

app = Flask(__name__)

# Endpoint trả về thông điệp với phương thức GET
@app.route('/message', methods=['GET'])
def send_message():
    message = "Hello from the server!"  # Nội dung chuỗi văn bản cần gửi
    return message, 200  # Trả về chuỗi văn bản cùng mã trạng thái HTTP 200

# Endpoint nhận thông điệp với phương thức POST
@app.route('/receiveMessage', methods=['POST'])
def receive_message():
    print("Headers:", request.headers)
    print("Data:", request.data)
    print("JSON:", request.get_json())

    # Lấy dữ liệu JSON từ body của yêu cầu POST
    data = request.get_json()

    # Kiểm tra nếu không có dữ liệu trong body hoặc không có key 'message'
    if not data or 'message' not in data:
        return jsonify({"error": "No message found in the request"}), 400

    # Lấy nội dung thông điệp từ dữ liệu JSON
    message = data['message']

    # Kiểm tra nếu thông điệp là "Authentication by fingerprint"
    if message == "Authentication by fingerprint":
        try:
            string = "192.168.1.4"
            # Gửi yêu cầu GET đến địa chỉ http://192.168.1.5/verify
            response = requests.get("http://" + string +"/verify")
            # Lấy nội dung từ phản hồi
            verify_message = response.text

            # In ra thông tin yêu cầu GET gửi lên
            print("Request to http://192.168.239.142/verify sent. Response:", verify_message)

            # Trả về chuỗi thông báo xác thực và phản hồi từ server
            return jsonify({"message": f"Message received: {message}", "verify": verify_message}), 200
        except requests.RequestException as e:
            # Xử lý lỗi khi không thể kết nối đến địa chỉ
            return jsonify({"error": "Unable to connect to verification server", "details": str(e)}), 500
    elif message == "Authentication by face":
        try:
            # Send a request to the Gradio app's custom endpoint
            gradio_app_url = 'http://localhost:7860/receiveMessage'  # Adjust the port if necessary
            gradio_response = requests.post(gradio_app_url, json={'message': 'Authentication by face'})
            if gradio_response.status_code == 200:
                return jsonify({"message": "Attendance triggered in Gradio app"}), 200
            else:
                return jsonify({"error": "Failed to trigger attendance in Gradio app"}), 500
        except requests.RequestException as e:
            return jsonify({"error": "Unable to connect to Gradio app", "details": str(e)}), 500
    else:
        # Nếu thông điệp không phải là "Authentication by fingerprint", in ra message
        print(f"Message received: {message}")

        # Trả về thông điệp nhận được cùng mã trạng thái 200
        return jsonify({"message": f"Message received: {message}"}), 200

if __name__ == '__main__':
    # Chạy server tại địa chỉ IP của máy tính và port 5000
    app.run(host='0.0.0.0', port=5000)

