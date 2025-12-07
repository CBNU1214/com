#include "types.h"
#include "memory_map.h"
#include "ascii.h"
#include "uart.h"

#define BUF_LEN 128

// [수정] 8x8 이미지 크기 정의
#define IMG_WIDTH 8
#define IMG_HEIGHT 8
#define TOTAL_PIXELS (IMG_WIDTH * IMG_HEIGHT) // 64 픽셀

typedef void (*entry_t)(void);

// 타이머 함수 (기존 유지)
int __attribute__ ((noinline)) timer_count(uint32_t num)
{
    uint32_t counter = CYCLE_COUNTER;
    COUNTER_RST = 1;
    uint32_t temp;
    for(int i = 0 ; i < num ; i ++) temp = CYCLE_COUNTER;
    counter -= temp;
    return counter;
}

int main(void)
{
    // [1] 필터 커널 설정 (Identity Filter 예시: 중앙만 1)
    // 0 0 0
    // 0 1 0
    // 0 0 0
    uint32_t kernel[9] = {0, 0, 0, 0, 1, 0, 0, 0, 0};

    int i;
    volatile uint32_t val_in, val_out;
    volatile uint32_t counter1, counter2;
    int8_t buffer[BUF_LEN];

    // [2] MMIO 주소 포인터 정의
    volatile uint32_t* addr_din    = (volatile uint32_t*)0x80010000; // Pixel Input (Addr 0)
    volatile uint32_t* addr_dout   = (volatile uint32_t*)0x80010004; // Result Output (Addr 1)
    volatile uint32_t* addr_clear  = (volatile uint32_t*)0x80010008; // Clear (Addr 2)
    volatile uint32_t* addr_weight = (volatile uint32_t*)0x8001000C; // Weights Start (Addr 3)

    COUNTER_RST = 1;
    counter1 = CYCLE_COUNTER;

    // 가속기 초기화
    *addr_clear = 1;

    // [3] 가중치 설정 (Addr 3 ~ 11에 쓰기)
    uwrite_int8s("Setting Weights...\r\n");
    for(i=0; i<9; i++) {
        *(addr_weight + i) = kernel[i];
    }

    // [수정] 메시지를 8x8로 변경
    uwrite_int8s("Starting 8x8 2D Convolution...\r\n");

    // [4] 이미지 스트리밍 루프
    for(i = 0; i < TOTAL_PIXELS + 10; i++)
    {
        // 테스트 데이터 생성: 0, 1, 2... 255 반복 패턴
        val_in = i % 256; 
        
        // 데이터 입력 (쓰기)
        *addr_din = val_in;
        
        // 결과 읽기 (읽기)
        val_out = *addr_dout;

        // [수정] 조건문(if) 제거: 모든 픽셀(0~63)의 결과를 전부 출력
        // 64개밖에 안 되므로 Tera Term에서 한눈에 전체 흐름(지연 포함) 확인 가능
        uwrite_int8s("idx: ");
        uwrite_int8s(uint32_to_ascii_hex(i, buffer, BUF_LEN));
        uwrite_int8s(" | In: ");
        uwrite_int8s(uint32_to_ascii_hex(val_in, buffer, BUF_LEN));
        uwrite_int8s(" -> Out: ");
        uwrite_int8s(uint32_to_ascii_hex(val_out, buffer, BUF_LEN));
        uwrite_int8s("\r\n");
    }

    counter2 = CYCLE_COUNTER;

    uwrite_int8s("Computing time : ");
    uwrite_int8s(uint32_to_ascii_hex(counter2-counter1,buffer,BUF_LEN));
    uwrite_int8s("\r\n");
    
    uwrite_int8s("Program Finished.\r\n");

    // [5] 프로그램 종료 (무한 루프로 정지)
    while(1); 
    
    return 0;
}
