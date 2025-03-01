
# Facial Recognition Attendance System

## Project Description
This project leverages facial recognition technology using deep learning with the InsightFace AI model to mark attendance for university students participating in classes.


## Demo
![Demo Web](demo_web.gif)


## Features
- **Attendance Tracking**: Automatically mark student attendance using facial recognition.
- **New Face Registration**: Register new student faces to the system for future recognition.

## Installation Guide
To set up the project, follow these steps:

1. Install [Anaconda](https://www.anaconda.com/).
2. Create a new conda environment named `insightFace`:
   ```bash
   conda create --name insightFace python=3.10
   ```
3. Activate the environment:
   ```bash
   conda activate insightFace
   ```
4. Install all required libraries:
   ```bash
   pip install -r requirements.txt
   ```

## Usage Guide
To run the project, use the following command to launch the web interface:
```bash
streamlit run web_deploy.py
```


## Author
Nguyen Tien Anh
