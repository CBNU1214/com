#include "types.h"
#include "memory_map.h"
#include "ascii.h"
#include "uart.h"

#define BUF_LEN 128
#define IMG_WIDTH 8
#define IMG_HEIGHT 8
#define TOTAL_PIXELS (IMG_WIDTH * IMG_HEIGHT)

// [추가] 스위치 주소 정의 (FPGA_TOP에서 12번 인덱스로 매핑: 0x30)
#define ADDR_SWITCHES ((volatile uint32_t*)0x80010030)

// 타이머 함수
int __attribute__ ((noinline)) timer_count(uint32_t num) {
    uint32_t counter = CYCLE_COUNTER;
    COUNTER_RST = 1;
    uint32_t temp;
    for(int i = 0 ; i < num ; i ++) temp = CYCLE_COUNTER;
    counter -= temp;
    return counter;
}

int main(void)
{
    // [1] 필터 커널 정의
    // Identity Filter (항등 필터)
    uint32_t kernel_identity[9] = {
        0, 0, 0,
        0, 1, 0,
        0, 0, 0
    };

    // Gaussian Filter (가우시안 필터 근사값)
    uint32_t kernel_gaussian[9] = {
        1, 2, 1,
        2, 4, 2,
        1, 2, 1
    };

    int i;
    volatile uint32_t val_in, val_out;
    volatile uint32_t counter1, counter2;
    int8_t buffer[BUF_LEN];

    // [2] MMIO 주소 포인터
    volatile uint32_t* addr_din    = (volatile uint32_t*)0x80010000;
    volatile uint32_t* addr_dout   = (volatile uint32_t*)0x80010004;
    volatile uint32_t* addr_clear  = (volatile uint32_t*)0x80010008;
    volatile uint32_t* addr_weight = (volatile uint32_t*)0x8001000C;
    volatile uint32_t* addr_sw     = ADDR_SWITCHES; // 스위치 주소

    uwrite_int8s("\r\n=== FPGA Accelerator Demo ===\r\n");
    uwrite_int8s("Switch 0 (0->1): Run Identity Filter\r\n");
    uwrite_int8s("Switch 1 (0->1): Run Gaussian Filter\r\n");

    uint32_t prev_sw = *addr_sw; // 초기 스위치 상태 저장
    uint32_t curr_sw;
    uint32_t *selected_kernel = 0;
    char *filter_name = "";

    // [3] 무한 루프로 스위치 감시
    while (1) {
        curr_sw = *addr_sw;

        // 스위치 0번이 0->1로 변했는지 확인 (Identity)
        if ((curr_sw & 1) && !(prev_sw & 1)) {
            selected_kernel = kernel_identity;
            filter_name = "Identity Filter";
        }
        // 스위치 1번이 0->1로 변했는지 확인 (Gaussian)
        else if ((curr_sw & 2) && !(prev_sw & 2)) {
            selected_kernel = kernel_gaussian;
            filter_name = "Gaussian Filter";
        }
        else {
            // 변화 없으면 상태 업데이트 후 계속 대기
            prev_sw = curr_sw;
            continue;
        }

        // [4] 선택된 필터로 연산 시작
        uwrite_int8s("\r\n[Selected]: ");
        uwrite_int8s(filter_name);
        uwrite_int8s("\r\nSetting Weights...\r\n");

        // 가속기 리셋
        *addr_clear = 1;

        // 가중치 설정
        for(i=0; i<9; i++) {
            *(addr_weight + i) = selected_kernel[i];
        }

        COUNTER_RST = 1;
        counter1 = CYCLE_COUNTER;

        // 이미지 스트리밍 및 출력
        for(i = 0; i < TOTAL_PIXELS + 10; i++)
        {
            val_in = i % 256; 
            *addr_din = val_in;     // 입력
            val_out = *addr_dout;   // 출력

            // 64개 픽셀 결과만 출력
            if (i < TOTAL_PIXELS) {
                uwrite_int8s("idx: ");
                uwrite_int8s(uint32_to_ascii_hex(i, buffer, BUF_LEN));
                uwrite_int8s(" | In: ");
                uwrite_int8s(uint32_to_ascii_hex(val_in, buffer, BUF_LEN));
                uwrite_int8s(" -> Out: ");
                uwrite_int8s(uint32_to_ascii_hex(val_out, buffer, BUF_LEN));
                uwrite_int8s("\r\n");
            }
        }

        counter2 = CYCLE_COUNTER;
        uwrite_int8s("Computing time : ");
        uwrite_int8s(uint32_to_ascii_hex(counter2-counter1, buffer, BUF_LEN));
        uwrite_int8s("\r\nDone. Waiting for switch toggle...\r\n");

        // 현재 상태 업데이트
        prev_sw = curr_sw;
    }

    return 0;
}
