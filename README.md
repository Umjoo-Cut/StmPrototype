
# AI 기반 차량 음주측정 및 시동 제어 시스템

## 1. 프로젝트 개요

### 프로젝트명

AI 기반 차량 음주측정 및 시동 제어 시스템

### 프로젝트 목적

운전자의 음주 여부를 차량 시동 전에 판별하여 음주 운전을 예방하는 시스템
구현

------------------------------------------------------------------------

## 2. 시스템 구성(HW)

-   STM32F411RE
-   Raspberry Pi 5
-   MQ3 알코올 센서
-   DHT11 온습도 센서
-   압력 센서
-   LED
-   Buzzer
-   Engine 제어

------------------------------------------------------------------------

## 3. 하드웨어 핀맵

### 하드웨어 구성


<img width="776" height="548" alt="image" src="https://github.com/user-attachments/assets/b716a0ee-7971-4da8-b329-50076599b5a4" />


### STM 핀맵


<img width="1014" height="775" alt="image" src="https://github.com/user-attachments/assets/2de658c1-b7a6-4cef-a738-ff93c89e1bbf" />


### 라즈베리파이 핀맵

<img width="767" height="478" alt="image" src="https://github.com/user-attachments/assets/b17c0158-de62-4a47-bf36-4937f400d29b" />


------------------------------------------------------------------------

## 4. 시스템 역할 분리

### STM32 역할

-   MQ3 센서 측정
-   DHT11 측정
-   상태 머신 제어
-   LED / 부저 / 엔진 제어
-   UART 통신

### Raspberry Pi 역할

-   데이터 수신
-   데이터 전처리
-   AI 모델 추론
-   PASS / FAIL / RETRY / OTHER 전송

------------------------------------------------------------------------

## 5. UART 통신 프로토콜

### STM → Pi

-   SYSTEM_START
-   SEAT_ON
-   SEAT_OFF
-   BLOW_START
-   MEASURE_BEGIN
-   MQ3:value
-   HUM:value
-   MEASURE_END
-   DHT_ERROR

### Pi → STM

-   PASS
-   FAIL
-   RETRY
-   OTHER

------------------------------------------------------------------------

## 6. 상태 머신

IDLE → WAIT_SEAT → WAIT_BLOW → MEASURING → WAIT_RESULT → PASS / FAIL /
RETRY

------------------------------------------------------------------------

## 7. 상태별 동작

### STATE_IDLE

-   모든 LED OFF
-   Engine OFF
-   START 버튼 대기

### STATE_WAIT_SEAT

-   Yellow LED ON
-   착석 대기

### STATE_WAIT_BLOW

-   Yellow LED Blink
-   숨 불기 대기

### STATE_MEASURING

-   Yellow LED 빠른 Blink
-   MQ3 측정
-   DHT11 측정
-   5초 데이터 수집

### STATE_WAIT_RESULT

-   Raspberry Pi 결과 대기
-   Timeout: 10초

### STATE_PASS

-   Green LED ON
-   Engine ON

### STATE_FAIL

-   Red LED ON
-   Engine OFF
-   부저 동작

### STATE_RETRY_WAIT

-   Green + Yellow + Red ON
-   5초 안정화

------------------------------------------------------------------------

## 8. 예외 처리

### 자리 이탈

MEASURING → SEAT_LOST_WAIT → 10초 복귀 대기

복귀 성공: - WAIT_BLOW

시간 초과: - FAIL

### 결과 수신 Timeout

WAIT_RESULT → ERROR → IDLE

------------------------------------------------------------------------

## 9. MQ3 / HUM 판정 로직

### Baseline 측정

초기 센서 평균값 계산

### Peak 측정

측정 중 최대값 저장

### Delta 계산

MQ3 Delta = Peak - Baseline

HUM Delta = Peak - Baseline

입력: - MQ3 Baseline - MQ3 Peak - MQ3 Delta - HUM Baseline - HUM Peak -
HUM Delta

출력: - PASS - FAIL - RETRY

------------------------------------------------------------------------

## 10. 재측정(RETRY) 로직

### 목적

-   센서 안정화
-   잘못된 측정 보완
-   측정 품질 향상

### 조건

-   MQ3 변화량 부족
-   HUM 변화량 부족
-   비정상 패턴

### 흐름

RETRY → RETRY_WAIT → 5초 안정화 → WAIT_BLOW

------------------------------------------------------------------------

## 11. 최종 시스템 시퀀스

START → 착석 → 숨 불기 → 측정 → AI 판정

결과: - PASS → 시동 허용 - FAIL → 시동 차단 - RETRY → 재측정
