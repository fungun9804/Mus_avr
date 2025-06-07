#include <avr/io.h>
#include <avr/interrupt.h>

#define BUFFER_SIZE 128
#define SAMPLE_RATE 8000
#define SILENCE 128

volatile uint8_t buffer[BUFFER_SIZE];
volatile bool buffer_ready = false;
volatile uint16_t play_pos = 0;

void setup() {
  Serial.begin(115200);  // Увеличена скорость передачи
  
  // Настройка PWM на пине 11 (Timer2)
  DDRB |= (1 << PB3);    // OC2A (Pin 11)
  TCCR2A = (1 << COM2A1) | (1 << WGM21) | (1 << WGM20); // Fast PWM
  TCCR2B = (1 << CS20);  // Без предделителя
  OCR2A = SILENCE;       // Начальное значение
  
  // Настройка Timer1 для 8kHz
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS10); // CTC mode, предделитель 1
  OCR1A = (F_CPU / SAMPLE_RATE) - 1;   // Установка частоты
  TIMSK1 = (1 << OCIE1A);              // Разрешение прерывания
  
  // Светодиод для диагностики
  pinMode(LED_BUILTIN, OUTPUT);
  sei(); // Глобальное разрешение прерываний
}

ISR(TIMER1_COMPA_vect) {
  if (buffer_ready) {
    OCR2A = buffer[play_pos++];
    if (play_pos >= BUFFER_SIZE) {
      buffer_ready = false;
      digitalWrite(LED_BUILTIN, LOW);
    }
  } else {
    OCR2A = SILENCE;
  }
}

void loop() {
  static uint8_t rx_buffer[BUFFER_SIZE];
  static uint16_t rx_index = 0;
  static uint32_t last_packet_time = 0;

  // Постепенное чтение данных
  while (Serial.available() > 0 && rx_index < BUFFER_SIZE) {
    rx_buffer[rx_index++] = Serial.read();
    last_packet_time = millis();
  }

  // Проверка заполнения буфера
  if (rx_index == BUFFER_SIZE) {
    noInterrupts(); // Блокировка прерываний
    memcpy((void*)buffer, rx_buffer, BUFFER_SIZE);
    interrupts();
    
    Serial.write(0x01); // Подтверждение приема
    play_pos = 0;
    buffer_ready = true;
    digitalWrite(LED_BUILTIN, HIGH);
    rx_index = 0; // Сброс индекса
  }

  // Таймаут приема пакета (100ms)
  if (rx_index > 0 && millis() - last_packet_time > 100) {
    rx_index = 0; // Сброс при неполучении полного пакета
  }

  // Автоотключение при простое
  if (buffer_ready && millis() - last_packet_time > 2000) {
    buffer_ready = false;
    digitalWrite(LED_BUILTIN, LOW);
  }
}