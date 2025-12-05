#include "types.h"
#include "memory_map.h"
#include "ascii.h"
#include "uart.h"

#define BUF_LEN 128

typedef void (*entry_t)(void);

// 타이머 함수 (기존 유지)
int __attribute__ ((noinline)) timer_count(uint32_t num)
{
    uint32_t counter;
    counter = CYCLE_COUNTER;
    COUNTER_RST = 1;
    uint32_t temp;
    for(int i = 0 ; i < num ; i ++){
        temp = CYCLE_COUNTER;
    }
    counter -= temp;
    return counter;
}

int main(void)
{
    // 1. 테스트 입력 데이터 및 필터 계수 설정
    // 입력 신호 x[n]
    uint32_t data_in[10] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100}; 
    // 필터 계수 (가중치)
    uint32_t coef[3] = {1, 2, 1}; // 예: Moving Average와 유사한 가중치
    
    // 결과 저장용 배열
    uint32_t hw_result[10];
    uint32_t sw_result[10];

    uint32_t i;
    int8_t buffer[BUF_LEN];

    // 2. 가속기 레지스터 포인터 매핑 (설계한 Memory Map에 맞춤)
    // Base Address: 0x80010000
    volatile uint32_t* addr_ctrl   = (volatile uint32_t*)0x80010000; // Control (Start/Clear)
    volatile uint32_t* addr_din    = (volatile uint32_t*)0x80010004; // Data Input
    volatile uint32_t* addr_coef0  = (volatile uint32_t*)0x80010008; // Coefficient 0
    volatile uint32_t* addr_coef1  = (volatile uint32_t*)0x8001000C; // Coefficient 1
    volatile uint32_t* addr_coef2  = (volatile uint32_t*)0x80010010; // Coefficient 2
    volatile uint32_t* addr_dout   = (volatile uint32_t*)0x80010014; // Result Output

    // 타이머 및 UART 초기화 메시지
    COUNTER_RST = 1;
    uwrite_int8s("\r\n=== 1D Filter Accelerator Test ===\r\n");

    // 3. 가속기 초기화 및 계수 설정
    *addr_ctrl = 2; // Bit 1: Clear/Reset (내부 레지스터 초기화)
    *addr_ctrl = 0; // Clear 해제

    // 계수 입력 (w0, w1, w2)
    *addr_coef0 = coef[0];
    *addr_coef1 = coef[1];
    *addr_coef2 = coef[2];

    uwrite_int8s("Coefficients set: ");
    uwrite_int8s(uint32_to_ascii_hex(coef[0], buffer, BUF_LEN));
    uwrite_int8s(", ");
    uwrite_int8s(uint32_to_ascii_hex(coef[1], buffer, BUF_LEN));
    uwrite_int8s(", ");
    uwrite_int8s(uint32_to_ascii_hex(coef[2], buffer, BUF_LEN));
    uwrite_int8s("\r\n");

    // 4. 하드웨어 가속 실행 (Loop)
    uint32_t start_time = CYCLE_COUNTER;

    for(i = 0; i < 10; i++)
    {
        // (1) 데이터 입력
        *addr_din = data_in[i];

        // (2) 연산 시작 (Start Bit 0 -> High)
        // 하드웨어가 HREADYOUT으로 Wait State를 건다면, 여기서 연산 끝날 때까지 CPU가 대기함
        *addr_ctrl = 1; 
        
        // (3) Start 신호 Clear (필요 시) - Edge Trigger 방식이면 필요, Level이면 유지
        *addr_ctrl = 0; 

        // (4) 결과 읽기
        hw_result[i] = *addr_dout;
    }

    uint32_t end_time = CYCLE_COUNTER;

    // 5. 소프트웨어 검증 (Golden Model)
    // C언어로 똑같은 연산을 수행해서 결과가 맞는지 확인
    // y[n] = c0*x[n] + c1*x[n-1] + c2*x[n-2]
    // 경계 조건 처리: x[-1], x[-2]는 0으로 가정
    for(i = 0; i < 10; i++) {
        uint32_t x_n = data_in[i];
        uint32_t x_n_1 = (i >= 1) ? data_in[i-1] : 0;
        uint32_t x_n_2 = (i >= 2) ? data_in[i-2] : 0;

        sw_result[i] = (coef[0] * x_n) + (coef[1] * x_n_1) + (coef[2] * x_n_2);
    }

    // 6. 결과 비교 및 출력
    int error_cnt = 0;
    uwrite_int8s("\r\nIndex | Input | HW Result | SW Result | Check\r\n");
    uwrite_int8s("----------------------------------------------\r\n");

    for(i = 0; i < 10; i++) {
        uwrite_int8s("  ");
        uwrite_int8s(uint32_to_ascii_hex(i, buffer, BUF_LEN));
        uwrite_int8s("   |  ");
        uwrite_int8s(uint32_to_ascii_hex(data_in[i], buffer, BUF_LEN));
        uwrite_int8s("  |  ");
        uwrite_int8s(uint32_to_ascii_hex(hw_result[i], buffer, BUF_LEN));
        uwrite_int8s("   |  ");
        uwrite_int8s(uint32_to_ascii_hex(sw_result[i], buffer, BUF_LEN));
        uwrite_int8s("   |  ");

        if (hw_result[i] == sw_result[i]) {
            uwrite_int8s("OK\r\n");
        } else {
            uwrite_int8s("FAIL\r\n");
            error_cnt++;
        }
    }

    uwrite_int8s("----------------------------------------------\r\n");
    
    if (error_cnt == 0) {
        uwrite_int8s("Test Result: ALL PASS!\r\n");
    } else {
        uwrite_int8s("Test Result: FAIL found.\r\n");
    }

    uwrite_int8s("Total Cycles: ");
    uwrite_int8s(uint32_to_ascii_hex(end_time - start_time, buffer, BUF_LEN));
    uwrite_int8s("\r\n");

    // BIOS 복귀
    uint32_t bios = ascii_hex_to_uint32("40000000");
    entry_t start = (entry_t) (bios);
    start();
    return 0;
}
