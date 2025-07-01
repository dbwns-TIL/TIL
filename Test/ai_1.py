import speech_recognition as sr
import paho.mqtt.client as mqtt
import re
from typing import Optional, Tuple

TOPIC_IOT_ACTION_LED_ALL = "dsm/em/led/00"
TOPIC_IOT_ACTION_LED_11 = "dsm/em/led.11"

def parse_light_command(text: str) -> Tuple[Optional[str], Optional[int]]:
    light_keywords = ['조명', '등', '형광등', '밝기', '밝은', '어두운', '밝게', '어둡게']

    # 숫자 추출 (0~999 범위), 단어 경계 제거
    number_match = re.search(r'(\d{1,3})', text)
    try:
        option = int(number_match.group(1)) if number_match else None
    except:
        option = None

    # 조명 관련 키워드 확인
    if any(keyword in text for keyword in light_keywords):
        command = 'led'
    else:
        command = None

    return command, option


def on_connect(client, userdata, flags, reason_code):
    if reason_code != 0:
        raise SyntaxError("브로커에 접속할 수 없습니다.")

r = sr.Recognizer()
cmqtt = mqtt.Client()
cmqtt.on_connect = on_connect
cmqtt.connect("mqtt.eclipseprojects.io")


while True:
    input("음성 인식을 시작할까요?")
    with sr.Microphone() as source:
        r.adjust_for_ambient_noise(source, 2)
        r.pause_threshold = 1.2
        print("말씀하세요...")
        audio = r.listen(source, 5, 15)

    text = r.recognize_google(audio, language='ko-KR')
    print(text)
    command, option = parse_light_command(text)
    if command == "led":
        topic = TOPIC_IOT_ACTION_LED_ALL
        cmqtt.publish(topic, option)
    print(f"Command: {command}, Option: {option}")