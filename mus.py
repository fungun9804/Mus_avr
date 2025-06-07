import serial
import wave
import numpy as np
import time
import sys

def send_audio(wav_path, port):
    try:
        with wave.open(wav_path, 'rb') as wav:
            # Проверка формата
            if wav.getnchannels() != 1:
                raise ValueError("Только монофонический звук!")
            
            # Конвертация данных
            data = np.frombuffer(wav.readframes(wav.getnframes()), 
                   dtype=np.int16 if wav.getsampwidth() == 2 else np.uint8)
            if wav.getsampwidth() == 2:
                data = ((data / 32768) * 127 + 128).astype(np.uint8)

            # Настройка соединения с Arduino
            ser = None
            for attempt in range(5):  # 5 попыток подключения
                try:
                    ser = serial.Serial(port, 115200, timeout=2)
                    print(f"Подключено к {port}")
                    break
                except serial.SerialException as e:
                    print(f"Попытка {attempt+1}/5: {str(e)}")
                    time.sleep(2)
            
            if not ser:
                raise ConnectionError("Не удалось подключиться к Arduino")

            try:
                time.sleep(2)  # Важно для инициализации Arduino
                ser.reset_input_buffer()

                packet_size = 128  # Уменьшенный размер пакета
                total_sent = 0
                
                for i in range(0, len(data), packet_size):
                    chunk = data[i:i+packet_size].tobytes()
                    ser.write(chunk)
                    total_sent += len(chunk)
                    
                    # Ожидание подтверждения с таймаутом
                    start_time = time.time()
                    while time.time() - start_time < 1.0:  # Таймаут 1 секунда
                        if ser.in_waiting:
                            ack = ser.read(1)
                            if ack == b'\x01':
                                print(f"Отправлено: {total_sent}/{len(data)} байт", end='\r')
                                break
                        time.sleep(0.01)
                    else:
                        raise TimeoutError("Таймаут ожидания подтверждения")
                        
            finally:
                ser.close()
                print(f"\nПередача завершена. Всего отправлено: {total_sent} байт")

    except Exception as e:
        print(f"Ошибка: {str(e)}")
        if 'ser' in locals() and ser and ser.is_open:
            ser.close()

if __name__ == "__main__":
    if len(sys.argv) == 3:
        send_audio(sys.argv[1], sys.argv[2])
    else:
        print("Использование: python mus.py audio.wav COM_PORT")
        print("Пример: python mus.py sound.wav COM3")