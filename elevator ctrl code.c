/* Atmega2560 마이크로 컨트롤러는 8비트이므로 기본 데이터 타입은 최적화를 위해 바이트(unsigned char)로 함. */

/*
아두이노 메가 핀 번호
인터럽트 핀(순서대로): 2, 3, 21, 20, 19, 18
PWM 핀: 2~13, 44~46
디지털 핀: 0 ~ 53
아날로그 입력: A0~A15
I2C 통신 핀: 20(SCA), 21(SCL)
직렬통신 핀: 0, 1, 14~19
*/

/* 호출 버튼과 목적지 버튼은 인터럽트 구현을 위해 모두 인터럽트 핀에 연결한다. 다이오드를 사용한다(오류 방지) */

/* 코드 시작 부분 */

/* 헤더 불러오기 */
#include <HCMotor.h>
/* 라이브러리 설명: https://forum.hobbycomponents.com/viewtopic.php?f=58&t=1870 */
/* TB6560 제어 http://scipia.co.kr/cms/blog/225 */

/* 참과 거짓 전처리 */
#define FALSE 0
#define TRUE 1

/* 명령어 정의(전처리) */
#define STOP 0
#define UP 1
#define UP_SLOW 2
#define UP_ACCEL 3
#define DN 4
#define DN_SLOW 5
#define DN_ACCEL 6
#define UP_RECY 8
#define DN_RECY 9
#define CLOSE 0
#define OPEN 1
#define GO 10

/* 디지털 핀 번호 정의 */
#define BTN_CALL_INTR 2 /* 호출 버튼 인터럽트 */
#define BTN_DEST_INTR 3 /* 목적지 버튼 인터럽트 */

/* TB6560 드라이버의 CW-, CLK- 단자는 모두 아두이노의 GND에 연결한다. */
#define NEMA_DOOR_DIR 5 /* CW+ */
#define NEMA_DOOR_STEP 6 /* CLK+ */

#define NEMA_MAIN_DIR 9 /* CW+ */
#define NEMA_MAIN_STEP 8 /* CLK+ */

#define HC595_DATA 12 /* FND 구동용 쉬프트 레지스터 */
#define HC595_LATCH 11
#define HC595_CLOCK 10

#define LEDF1U 22 /* 각 층의 호출버튼 LED 핀번호 정의 */
#define LEDF2U 23
#define LEDF2D 24
#define LEDF3U 25
#define LEDF3D 26
#define LEDF4D 27
#define LEDUP 32 /* 운전 방향 표시등 정의 */
#define LEDDN 33

#define IR_SNSR_1 34 /* 카 위치 감지 거리 센서 정의 */
#define IR_SNSR_2 35
#define IR_SNSR_3 37
#define IR_SNSR_4 36

#define BTN1U 40 /* 층별 호출 버튼 정의 */
#define BTN2U 41
#define BTN2D 42
#define BTN3U 43
#define BTN3D 44
#define BTN4D 45
#define BTN1 46 /* 카 목적지 버튼 정의 */
#define BTN2 47
#define BTN3 48
#define BTN4 49

#define BTN_LOCK 50 /* 잠금버튼 */
#define LIGHT_EMERGENCY 51 /* 잠금 표시등 */
#define LOCK_OUT 52 /* 시스템 잠금 출력 */
#define LOCK_IN 53 /* 시스템 잠금 입력 */

typedef unsigned char BYTE; /* 바이트는 부호 없는 char */
typedef BYTE BOOL; /* 부울 자료형의 형식을 바이트로 지정 */

/* 전역변수는 언제든 값이 조작될 수 있으므로(특히 인터럽트) volatile을 선언해 최적화에서 제외함 */
volatile BOOL arrDest[] = { FALSE, FALSE, FALSE, FALSE }; /* 목적지 입력 상태 저장용 배열 */
volatile BOOL arrCall[] = { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE }; 
/* 호출 버튼 상태 저장용 배열, F1U, F2U, F3U, F2D, F3D, F4D 순 */
volatile BOOL isMoving = FALSE; /* 움직이는가? */
volatile BYTE carStat = STOP; /* 카 운전 상태(방향) */
volatile BYTE currFloor = 1; /* 현재 위치 */
volatile BOOL doorStat = CLOSE; /* 문 상태 변수(필요 없을 시 제거) */
volatile BYTE originalDir = STOP; /* 직전 이동 방향 기록용 변수 */

const BYTE fndDigits[] = { 0x03, 0x5F, 0x25, 0x0D, 0x58 }; /* FND 숫자 0~4, WCN1-0080GU-A51에 맞춤(공통 애노드 타입) */
const int nemaMainSpd = 30; /* 10~1024, 낮을 수록 빠름 */
const int nemaMainSlowSpd = 70; /* 모터 감속 후 속도 */
const int nemaDoorSpd = 40; /* 문 모터 속도 */
const int nemaDoorSteps = 200; /* 문 모터는 한 바퀴보다 살짝 덜 돌아가게 설정함 */

HCMotor nemaMain; /* HCMotor 인스턴스 생성 */
HCMotor nemaDoor;

BYTE chkArrs(void);
void getFloor(void);
void motorDrv(BYTE drvMode);
void doorDrv(BOOL op);
BOOL chkDest(void);
BYTE chkUpDn(void);
void preciseMotorCtrl(BYTE floor);
void fndDrv(BYTE floorNum);
void reset(void);

/* 인터럽트 함수 */
void isrBtnCall(void) { /* 호출 버튼 인터럽트 처리 */
    /* int analogVal = analogRead(BTN_CALL); */
    if (digitalRead(BTN1U)) {
        arrCall[0] = TRUE;
        digitalWrite(LEDF1U, HIGH);
    }
    if (digitalRead(BTN2U)) {
        arrCall[1] = TRUE;
        digitalWrite(LEDF2U, HIGH);
    }
    if (digitalRead(BTN2D)) {
        arrCall[3] = TRUE;
        digitalWrite(LEDF2D, HIGH);
    }
    if (digitalRead(BTN3U)) {
        arrCall[2] = TRUE;
        digitalWrite(LEDF3U, HIGH);
    }
    if (digitalRead(BTN3D)) {
        arrCall[4] = TRUE;
        digitalWrite(LEDF3D, HIGH);
    }
    if (digitalRead(BTN4D)) {
        arrCall[5] = TRUE;
        digitalWrite(LEDF4D, HIGH);
    }
}

void isrBtnDest(void) { /* 목적층 버튼 인터럽트 처리 */
    /* int analogVal = analogRead(BTN_DEST); */
    if (digitalRead(BTN1)) {
        arrDest[0] = TRUE;
    }
    if (digitalRead(BTN2)) {
        arrDest[1] = TRUE;
    }
    if (digitalRead(BTN3)) {
        arrDest[2] = TRUE;
    }
    if (digitalRead(BTN4)) {
        arrDest[3] = TRUE;
    }
}

void setup() { /* 시동할 때 설정 */
    Serial.begin(9600);
    Serial.println("Serial Comm initialized");
    /* 핀 배열에 맞게 pinout 정의 */
    pinMode(BTN_CALL_INTR, INPUT);
    pinMode(BTN_DEST_INTR, INPUT);
    pinMode(HC595_DATA, OUTPUT);
    pinMode(HC595_CLOCK, OUTPUT);
    pinMode(HC595_LATCH, OUTPUT);
    pinMode(LEDF1U, OUTPUT);
    pinMode(LEDF2U, OUTPUT);
    pinMode(LEDF2D, OUTPUT);
    pinMode(LEDF3U, OUTPUT);
    pinMode(LEDF3D, OUTPUT);
    pinMode(LEDF4D, OUTPUT);
    pinMode(LEDUP, OUTPUT);
    pinMode(LEDDN, OUTPUT);
    pinMode(BTN1U, INPUT);
    pinMode(BTN2U, INPUT);
    pinMode(BTN2D, INPUT);
    pinMode(BTN3U, INPUT);
    pinMode(BTN3D, INPUT);
    pinMode(BTN4D, INPUT);
    pinMode(BTN1, INPUT);
    pinMode(BTN2, INPUT);
    pinMode(BTN3, INPUT);
    pinMode(BTN4, INPUT);
    pinMode(IR_SNSR_1, INPUT);
    pinMode(IR_SNSR_2, INPUT);
    pinMode(IR_SNSR_3, INPUT);
    pinMode(IR_SNSR_4, INPUT);
    pinMode(BTN_LOCK, INPUT);
    pinMode(LIGHT_EMERGENCY, OUTPUT);
    pinMode(LOCK_OUT, OUTPUT);
    pinMode(LOCK_IN, INPUT);
    attachInterrupt(0, isrBtnCall, RISING); /* 인터럽트 실행 */
    attachInterrupt(1, isrBtnDest, RISING);
    nemaMain.Init(); /* 초기화 */
    nemaMain.attach(0, STEPPER, NEMA_MAIN_STEP, NEMA_MAIN_DIR); /* 모터 0를 스텝모터로 설정 */
    nemaMain.Steps(0, CONTINUOUS); /* 연속 구동모드로 설정 */
    nemaMain.DutyCycle(0, 0);
    nemaDoor.Init();
    nemaDoor.attach(1, STEPPER, NEMA_DOOR_STEP, NEMA_DOOR_DIR); /* 모터 1을 스텝모터로 설정 */
    nemaDoor.Steps(1, nemaDoorSteps); /* 구동모드를 nemaDoorSteps에 정의된 스텝각만큼 돌리게 설정 */
    nemaDoor.DutyCycle(1, 0);
    getFloor(); /* 층수 구하기 */
    Serial.println("Setup complete");
    
    /* 등화류 초기화 */
    digitalWrite(LIGHT_EMERGENCY, HIGH);
    digitalWrite(LEDUP, HIGH);
    digitalWrite(LEDDN, HIGH);

    digitalWrite(LOCK_OUT, HIGH); /* 잠금 핀이 걸리면 작동을 막음 */
    if (digitalRead(LOCK_IN)) {
            digitalWrite(LIGHT_EMERGENCY, LOW); /* 잠금 사실을 램프를 켜서 알림 */
            digitalWrite(LEDUP, LOW);
            digitalWrite(LEDDN, LOW);
            fndDrv(0); /* 0층을 표시함 */
        while (digitalRead(LOCK_IN)) {
            delay(2000);
        }
        digitalWrite(LIGHT_EMERGENCY, HIGH);
        digitalWrite(LEDUP, HIGH);
        digitalWrite(LEDDN, HIGH);
        getfloor(); /* 진짜 층수를 표시함 */
    }
    digitalWrite(LOCK_OUT, LOW);
}

void loop(void) {  /* 주 함수 */
    BYTE updn = 0;
    BOOL isDest = FALSE;
    getFloor(); /* 층수 구함 */
    if (digitalRead(BTN_LOCK) && carStat == STOP) { /* 멈춘 상태에서 잠금 버튼 입력 */
        Serial.println("[loop] lock btn input");
        nemaMain.DutyCycle(0, 0);
        digitalWrite(LIGHT_EMERGENCY, LOW);
        Serial.println("[loop] In infinite loop(locked)");
        detachInterrupt(0); /* 버튼 잠금 */
        detachInterrupt(1);
        while(digitalRead(BTN_LOCK)) /* 버튼 릴리스 전까지 무한 빈 루프 */
            (void) "aoeu";
        digitalWrite(LIGHT_EMERGENCY, HIGH); /* 버튼 릴리스 뒤 소등 및 모터 재구동 */
        attachInterrupt(0, isrBtnCall, RISING); /* 버튼 잠금 해제 */
        attachInterrupt(1, isrBtnDest, RISING);
        Serial.println("[loop] Infinite loop(lock) ended");
        return;
    }
    
    if (isMoving) { /* 움직이고 있으면 */
        Serial.println("[loop] isMoving is TRUE");
        getFloor();
        if (chkDest()) goto arrival; /* 목적지에 도착한 경우 arrival 레이블 아래의 코드를 실행*/
        return; /* 목적지가 아닌 경우 함수 재시작 */

        arrival: /* 목적지 도착 후 제어 */
        Serial.println("[loop] Arrival");
        delay(500); /* 문 열기 전 1/2초 딜레이 */
        getFloor(); /* 목적지 층수를 구하여 표시 */
        switch (currFloor) { /* 층수에 맞게 호출 램프 조작 */
            case 1:
            digitalWrite(LEDF1U, LOW);
            break;

            case 2:
            if (carStat == UP) digitalWrite(LEDF2U, LOW);
            else if (carStat == DN) digitalWrite(LEDF2D, LOW);
            break;

            case 3:
            if (carStat == UP) digitalWrite(LEDF3U, LOW);
            else if (carStat == DN) digitalWrite(LEDF3D, LOW);
            break;

            case 4:
            digitalWrite(LEDF4D, LOW);
            break;
        }
        carStat = STOP; /* 카 상태값을 멈춤으로  */

        if (chkUpDn() == STOP) { /* 움직일 필요가 없으면 */
            Serial.println("[loop] (Arrival) updn is STOP");
            /* 호출 및 목적지 배열값을 조작하고 램프를 조작함 */
            if ((arrDest[0] || arrCall[0]) && currFloor == 1) {
                arrDest[0] = FALSE;
                arrCall[0] = FALSE;
                digitalWrite(LEDF1U, LOW);
            } 
            if ((arrDest[1] || arrCall[1] || arrCall[3]) && currFloor == 2) {
                arrDest[1] = FALSE;
                arrCall[1] = FALSE;
                arrCall[3] = FALSE;
                digitalWrite(LEDF2U, LOW);
                digitalWrite(LEDF2D, LOW);
            } 
            if ((arrDest[2] || arrCall[2] || arrCall[4]) && currFloor == 3) {
                arrDest[2] = FALSE;
                arrCall[2] = FALSE;
                arrCall[4] = FALSE;
                digitalWrite(LEDF3U, LOW);
                digitalWrite(LEDF3D, LOW);
            } 
            if ((arrDest[3] || arrCall[5]) && currFloor == 4) {
                arrDest[3] = FALSE;
                arrCall[5] = FALSE;
                digitalWrite(LEDF4D, LOW);
            }
            originalDir = STOP; /* 원래의 운전 방향 변수 초기화 */
            carStat = STOP; /* 카 운전상태를 정지로 바꿈 */
        }
        
        driveDoor: /* 문 구동 명령 */
        Serial.println("[loop] Driving door...");
        if (doorStat == CLOSE) { 
            Serial.println("[loop] Opening door...");
            doorDrv(OPEN);
            delay(5000);
            doorDrv(CLOSE);
            delay(2000);
            Serial.println("[loop] Closed door");
        }
        return;

    } else { /* 움직이고 있지 않으면 */
        for (int i = 0; i < 4; i++) { /* 움직여야 하는가?, 목적지 입력 확인 */
            if (arrDest[i]) goto start;
        }
        for (int i = 0; i < 6; i++) { /* 움직여야 하는가?, 호출 입력 확인 */
            if (arrCall[i]) goto start;
        }
        Serial.println("[loop] Staying");
        originalDir = STOP; /* 대기 */
        digitalWrite(LEDUP, HIGH);
        digitalWrite(LEDDN, HIGH);
        return;
        
        start: /* 시동 */
        Serial.println("[loop] Starting");
        updn = chkUpDn();
        if (updn == UP) { /* 상승시 */
            Serial.println("[loop] updn is UP");
            digitalWrite(LEDUP, LOW);
            digitalWrite(LEDDN, HIGH);
            motorDrv(UP_ACCEL); /* 모터 가속 */
            motorDrv(UP); /* 정속 운전 */
        } else if (updn == DN) { /* 하강시 */
            Serial.println("[loop] updn is DN");
            digitalWrite(LEDUP, HIGH);
            digitalWrite(LEDDN, LOW);
            motorDrv(DN_ACCEL);
            motorDrv(DN);
        } else if (updn == STOP) { /* 대기 */
            Serial.println("[loop] updn is STOP");
            /* 배열값 및 램프 조작 */
            if ((arrDest[0] || arrCall[0]) && currFloor == 1) {
                arrDest[0] = FALSE;
                arrCall[0] = FALSE;
                digitalWrite(LEDF1U, LOW);
            } 
            if ((arrDest[1] || arrCall[1] || arrCall[3]) && currFloor == 2) {
                arrDest[1] = FALSE;
                arrCall[1] = FALSE;
                arrCall[3] = FALSE;
                digitalWrite(LEDF2U, LOW);
                digitalWrite(LEDF2D, LOW);
            } 
            if ((arrDest[2] || arrCall[2] || arrCall[4]) && currFloor == 3) {
                arrDest[2] = FALSE;
                arrCall[2] = FALSE;
                arrCall[4] = FALSE;
                digitalWrite(LEDF3U, LOW);
                digitalWrite(LEDF3D, LOW);
            } 
            if ((arrDest[3] || arrCall[5]) && currFloor == 4) {
                arrDest[3] = FALSE;
                arrCall[5] = FALSE;
                digitalWrite(LEDF4D, LOW);
            }
            originalDir = STOP;
            carStat = STOP;
            goto driveDoor; /* 문 구동 */
        }
    }
}

BYTE chkArrs(BYTE floor, BYTE dir) { /* 배열을 참조하여 계속 올라갈지 아닐지를 알아내는 함수 */
    //floor--; /* 배열 참조를 위해 floor 값 감산 */
    if (dir == UP) { /* 상승 */
        for (int i = floor; i < 3; i++) { /* 호출 확인 */
            if (arrCall[i] && arrCall[floor - 1] == FALSE) return GO;
        }
        if (arrCall[5] && arrCall[floor - 1] == FALSE) return GO; /* 마지막 층 */
        for (int i = floor; i < 4; i++) { /* 목적지 확인 */
            if (arrDest[i] && arrCall[floor - 1] == FALSE) return GO;
        }
        return STOP; /* 정지 명령 */
    } else if (dir == DN) { /* 하강 */
        for (int i = (floor + 2); i >= 3; i--) { /* 호출 확인 */
            if (i == 5) continue;
            if (arrCall[i] && arrCall[floor + 1] == FALSE) return GO;
        }
        if (arrCall[0] && arrCall[floor + 1] == FALSE) return GO; /* 마지막 층 */
        for (int i = 0; i < floor; i++) { /* 목적지 확인 */
            if (arrDest[i] && arrCall[floor + 1] == FALSE) return GO;
        }
        return STOP; /* ᅟ정지 명령 */
    }
}

void getFloor(void) { /* 층수 구하고 FND로 표시하는 함수 */
    Serial.println("[getFloor] Execution");
    if (!digitalRead(IR_SNSR_1)) {
        Serial.println("[getFloor] Floor is 1");
        currFloor = 1;
        fndDrv(1);
    } 
    if (!digitalRead(IR_SNSR_2)) {
        Serial.println("[getFloor] Floor is 2");
        currFloor = 2;
        fndDrv(2);
    } 
    if (!digitalRead(IR_SNSR_3)) {
        Serial.println("[getFloor] Floor is 3");
        currFloor = 3;
        fndDrv(3);
    } 
    if (!digitalRead(IR_SNSR_4)) {
        Serial.println("[getFloor] Floor is 4");
        currFloor = 4;
        fndDrv(4);
    }
    /* 센서 고장 시의 운전은 구현되지 않았음 */
}

void motorDrv(BYTE drvMode) { /* 모터 구동 */
    Serial.println("[motorDrv] Execution");
    int spdTmp = 0;
    switch(drvMode) {
        case STOP: /* 정지 */
        Serial.println("[motorDrv] drvMode is STOP");
        nemaMain.DutyCycle(0, 0);
        isMoving = FALSE;
        break;

        case UP: /* 상승(정속) */
        Serial.println("[motorDrv] drvMode is UP");
        nemaMain.Direction(0, REVERSE);
        nemaMain.DutyCycle(0, nemaMainSpd);
        carStat = UP;
        originalDir = UP;
        isMoving = TRUE;
        break;

        case DN: /* 하강(정속) */
        Serial.println("[motorDrv] drvMode is DN");
        nemaMain.Direction(0, FORWARD);
        nemaMain.DutyCycle(0, nemaMainSpd);
        carStat = DN;
        originalDir = DN;
        isMoving = TRUE;
        break;

        /* 목적층/호출위치에 가까워졌을 때 속도를 줄이고 정밀하게 운전 */
        case UP_SLOW: /* 상승(감속) */
        Serial.println("[motorDrv] drvMode is UP_SLOW");
        spdTmp = nemaMainSpd; /* 속도값(듀티사이클) 임시 변수를 nemaMainSpd(정속 운전 속도)로 설정 */
        nemaMain.Direction(0, REVERSE);
        while (spdTmp != nemaMainSlowSpd) { /* nemaMainSlowSpd(감속 후 운전 속도)가 될 때까지 7ms에 1 듀티사이클씩 감속 */
            nemaMain.DutyCycle(0, spdTmp++);
            delay(7);
        }
        break;

        case UP_ACCEL: /* 상승(가속) */
        Serial.println("[motorDrv] drvMode is UP_ACCEL");
        spdTmp = 150; /* 첫 듀티사이클을 150으로 설정 */
        nemaMain.Direction(0, REVERSE);
        while (spdTmp != nemaMainSpd) { /* nemaMainSpd(정속 운전 속도)가 될 때까지 5ms에 1 듀티사이클씩 가속 */
            nemaMain.DutyCycle(0, spdTmp--);
            delay(5);
        }        
        break;

        case DN_SLOW: /* 하강(감속) */
        Serial.println("[motorDrv] drvMode is DN_SLOW");
        spdTmp = nemaMainSpd;
        nemaMain.Direction(0, FORWARD);
        while (spdTmp != nemaMainSlowSpd) {
            nemaMain.DutyCycle(0, spdTmp++);
            delay(7);
        }        
        break;

        case DN_ACCEL: /* 하강(가속) */
        Serial.println("[motorDrv] drvMode is DN_ACCEL");
        spdTmp = 150;
        nemaMain.Direction(0, FORWARD);
        while (spdTmp != nemaMainSpd) {
            nemaMain.DutyCycle(0, spdTmp--);
            delay(5);
        }        
        break;
    }
}

void doorDrv(BOOL op) { /* 문 구동용 함수 */
    Serial.println("[doorDrv] Execution");
    if (doorStat == OPEN && op == CLOSE) {
        Serial.println("[doorDrv] Closing");
        /* 문을 닫는다  */
        nemaDoor.Direction(1, FORWARD);
        nemaDoor.Steps(1, nemaDoorSteps);
        nemaDoor.DutyCycle(1, nemaDoorSpd);
        doorStat = CLOSE;
        delay(1000);
        nemaDoor.DutyCycle(1, 0);
        return;
    } else if (doorStat == CLOSE && op == OPEN) {
        Serial.println("[doorDrv] Opening");
        /* 문을 연다 */
        nemaDoor.Direction(1, REVERSE);
        nemaDoor.Steps(1, nemaDoorSteps);
        nemaDoor.DutyCycle(1, nemaDoorSpd);
        doorStat = OPEN;
        delay(1000);
        nemaDoor.DutyCycle(1, 0);
        return;
    }
    Serial.println("[doorDrv] ?Problem occured(doorStat does not match with op)"); /* 오류 */
    if (doorStat == CLOSE && op == CLOSE) return;
    else if (doorStat == OPEN && op == OPEN) return;
}

BOOL chkDest(void) { /* 현재 층수가 호출된/목적지 층수인지를 확인한다. */
    Serial.println("[chkDest] Execution");
    if (digitalRead(IR_SNSR_1) == 0 && (arrDest[0] == TRUE || arrCall[0] == TRUE)) {
        Serial.println("[chkDest] SNSR 1 INPUT");
        preciseMotorCtrl(1); /* 모터 정밀 제어 */
        arrCall[0] = FALSE;
        arrDest[0] = FALSE;
        originalDir = STOP;
        return TRUE;
    } else if (digitalRead(IR_SNSR_2) == 0 && (arrDest[1] == TRUE || arrCall[1] == TRUE || arrCall[3] == TRUE)) {
        Serial.println("[chkDest] SNSR 2 INPUT");
        if (chkArrs(2, carStat) == GO && arrDest[1] == FALSE) return FALSE;
        preciseMotorCtrl(2);
        if (originalDir == UP) arrCall[1] = FALSE;
        else if (originalDir == DN) arrCall[3] = FALSE;
        arrDest[1] = FALSE;
        return TRUE;
    } else if (digitalRead(IR_SNSR_3) == 0 && (arrDest[2] == TRUE || arrCall[2] == TRUE || arrCall[4] == TRUE)) {
        Serial.println("[chkDest] SNSR 3 INPUT");
        if (chkArrs(3, carStat) == GO && arrDest[2] == FALSE) return FALSE;
        preciseMotorCtrl(3);
        if (originalDir == UP) arrCall[2] = FALSE;
        else if (originalDir == DN) arrCall[4] = FALSE;
        arrDest[2] = FALSE;
        return TRUE;
    } else if (digitalRead(IR_SNSR_4) == 0 && (arrDest[3] == TRUE || arrCall[5] == TRUE)) {
        Serial.println("[chkDest] SNSR 4 INPUT");
        preciseMotorCtrl(4);
        arrCall[5] = FALSE;
        arrDest[3] = FALSE;
        originalDir = STOP;
        return TRUE;
    } else if (digitalRead(IR_SNSR_1) == 0 && carStat == DN) { /* 뭔가가 꼬였을 때 */
        Serial.println("[chkDest] ?EMERGENCY STOP(1F). HALTING SYSTEM.");
        preciseMotorCtrl(1);
        goto EMER;
    } else if (digitalRead(IR_SNSR_4) == 0 && carStat == UP) {
        Serial.println("[chkDest] ?EMERGENCY STOP(4F). HALTING SYSTEM.");
        preciseMotorCtrl(4);
        goto EMER;
    } 
    Serial.println("[chkDest] NO SNSR INPUT");
    return FALSE;

    EMER: /* 꼬였을 때의 잠금 동작(감지 기능이 완벽하지는 않음) */
    getFloor();
    digitalWrite(LIGHT_EMERGENCY, LOW);
    digitalWrite(LEDUP, LOW);
    digitalWrite(LEDDN, LOW);
    Serial.println("?EMERGENCY, SYSTEM HALTED. PRESS LOCK BTN TO RESET");
    while (!digitalRead(BTN_LOCK)) {
        delay(5000);
    }
    Serial.println("?RELEASE LOCK BTN TO RESTART SYSTEM");
    while (digitalRead(BTN_LOCK)) {
        delay(5000);
    }
    reset();
    return FALSE;
}

BYTE chkUpDn(void) { /* 상승/하강 결정 함수 */
    Serial.println("[chkUpDn] Execution");
    /* 상승? */
    if (carStat != DN && originalDir != DN) {
        for (int i = currFloor; i < 4; i++) {
            if (arrDest[i] && i + 1 > currFloor) {
                Serial.println("[chkUpDn] UP");
                return UP; 
            }
        }
        /* 위의 for문에서 처리가 안됐을 때 지정된 조건을 만족하면 UP을 반환해주는 코드(버그 픽스) */
        if ((arrCall[1] || arrCall[2] || arrCall[3] || arrCall[4] || arrCall[5]) && currFloor == 1) return UP;
        if ((arrCall[2] || arrCall[4] || arrCall[5]) && currFloor == 2) return UP;
        if ((arrCall[5]) && currFloor == 3) return UP;
    }
    /* 하강? */
    if (carStat != UP && originalDir != UP) {
        for (int i = currFloor; i >= 0; i--) {
            if (arrDest[i] && i + 1 < currFloor) {
                Serial.println("[chkUpDn] DN");
                return DN;
            }
        }
        if ((arrCall[0] || arrCall[1] || arrCall[2] || arrCall[3] || arrCall[4]) && currFloor == 4) return DN;
        if ((arrCall[0] || arrCall[1] || arrCall[3]) && currFloor == 3) return DN;
        if ((arrCall[0]) && currFloor == 2) return DN;
    }
    /* 이동할 필요 없음 */
    return STOP;
}

void preciseMotorCtrl(BYTE floor) { /* 모터 정밀제어 함수 */
    Serial.println("[preciseMotorCtrl] Execution");
    if (carStat == UP) motorDrv(UP_SLOW);
    else if (carStat == DN) motorDrv(DN_SLOW);

    Serial.println("[preciseMotorCtrl] Motor slowed down");
    switch(floor) { /* 센서 입력이 들어올 때까지 무한 빈 루프 */
        case 1:
        while (digitalRead(IR_SNSR_1)) (void) "aoeu";
        break;

        case 2:
        while (digitalRead(IR_SNSR_2)) (void) "aoeu";
        break;

        case 3:
        while (digitalRead(IR_SNSR_3)) (void) "aoeu";
        break;

        case 4:
        while (digitalRead(IR_SNSR_4)) (void) "aoeu";
        break;
    }
    Serial.println("[preciseMotorCtrl] Stopping motor");
    delay(250); /* 센서 입력 위치 보정용 딜레이, 모터 감속 후 속도가 달라지면 이 값도 바꿔야 함 */
    motorDrv(STOP);
    return;
}

void fndDrv(BYTE floorNum) { /* 7seg 구동(74HC595) */
    Serial.println("[fndDrv] Execution");
    digitalWrite(HC595_LATCH, LOW);
    shiftOut(HC595_DATA, HC595_CLOCK, LSBFIRST, fndDigits[floorNum]);
    digitalWrite(HC595_LATCH, HIGH);
} 

void reset(void) { /* 초기화 함수 */
    isMoving = FALSE;
    carStat = STOP;
    doorStat = CLOSE;
    originalDir = STOP;
    for (int i = 0; i < 4; i++)
        arrDest[i] = FALSE;
    for (int i = 0; i < 6; i++)
        arrCall[i] = FALSE;
    digitalWrite(LEDF1U, LOW);
    digitalWrite(LEDF2U, LOW);
    digitalWrite(LEDF2D, LOW);
    digitalWrite(LEDF3U, LOW);
    digitalWrite(LEDF3D, LOW);
    digitalWrite(LEDF4D, LOW);
    digitalWrite(LEDUP, HIGH);
    digitalWrite(LEDDN, HIGH);
    digitalWrite(LIGHT_EMERGENCY, HIGH);
}
