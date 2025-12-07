module simple_acc (
    input wire          clk,
    input wire          rst,
    input wire [3:0]    addr,
    input wire          en,
    input wire          we,
    input wire [31:0]   din,    // Data input (Pixel or Coefficients)
    output wire [31:0]  dout    // Data output (Convolution Result)
);

    // [수정] 이미지 너비를 8로 설정 (8x8 이미지 처리용)
    // 이 파라미터에 따라 내부 시프트 레지스터(Line Buffer) 크기가 자동으로 결정됩니다.
    parameter IMG_WIDTH = 8;
    
    // 3x3 필터를 위해 필요한 버퍼 크기: 2줄 + 3픽셀
    localparam SR_DEPTH = (2 * IMG_WIDTH) + 3;

    // [1] 레지스터 선언
    reg [31:0] weights [0:8];            // 3x3 가중치 (W0 ~ W8)
    reg [31:0] shift_reg [0:SR_DEPTH-1]; // 라인 버퍼 (Shift Register)
    reg [31:0] reg_Y;                    // 연산 결과 레지스터

    // 제어 신호 디코딩
    wire data_in     = en & we & (addr == 4'd0); // 픽셀 입력 (Addr 0)
    wire result_read = en & (addr == 4'd1);      // 결과 읽기 (Addr 1)
    wire clear_acc   = en & we & (addr == 4'd2); // 초기화 (Addr 2)
    
    // 가중치 쓰기 (Addr 3 ~ 11)
    wire weight_we   = en & we & (addr >= 4'd3) & (addr <= 4'd11);
    wire [3:0] weight_idx = addr - 4'd3;

    // 읽기 동기화 파이프라인 레지스터
    wire result_read_ff;
    
    integer i;

    // [2] 3x3 윈도우 픽셀 추출
    // shift_reg 구조상 0번 인덱스가 가장 최신(우하단) 데이터입니다.
    wire [31:0] p0, p1, p2, p3, p4, p5, p6, p7, p8;
    
    // Top Row (2줄 전 데이터)
    assign p0 = shift_reg[2*IMG_WIDTH + 2]; 
    assign p1 = shift_reg[2*IMG_WIDTH + 1];
    assign p2 = shift_reg[2*IMG_WIDTH + 0];
    
    // Mid Row (1줄 전 데이터)
    assign p3 = shift_reg[IMG_WIDTH + 2];   
    assign p4 = shift_reg[IMG_WIDTH + 1];
    assign p5 = shift_reg[IMG_WIDTH + 0];
    
    // Bot Row (현재 줄 데이터)
    assign p6 = shift_reg[2];               
    assign p7 = shift_reg[1];
    assign p8 = shift_reg[0];               

    // [3] 컨볼루션 연산 (MAC: Multiply-Accumulate)
    // Overflow 방지를 위해 실제로는 더 큰 비트 폭이 필요할 수 있으나, 예제 단순화를 위해 32비트 사용
    wire [31:0] conv_sum;
    assign conv_sum = (weights[0] * p0) + (weights[1] * p1) + (weights[2] * p2) +
                      (weights[3] * p3) + (weights[4] * p4) + (weights[5] * p5) +
                      (weights[6] * p6) + (weights[7] * p7) + (weights[8] * p8);

    // [4] 순차 논리 (Clocked Logic)
    always @(posedge clk, posedge rst) begin
        if (rst) begin
            reg_Y <= 0;
            for (i=0; i<SR_DEPTH; i=i+1) shift_reg[i] <= 0;
            for (i=0; i<9; i=i+1) weights[i] <= 0;
        end
        else if (data_in) begin
            // 1. 시프트 레지스터 업데이트 (새 픽셀을 밀어 넣음)
            shift_reg[0] <= din;
            for (i=1; i<SR_DEPTH; i=i+1) begin
                shift_reg[i] <= shift_reg[i-1];
            end
            
            // 2. 연산 결과 저장 (다음 읽기 사이클에 출력됨)
            reg_Y <= conv_sum;
        end
        else if (clear_acc) begin
            // Clear 명령 시 데이터 버퍼와 결과만 초기화 (가중치는 유지)
            reg_Y <= 0;
            for (i=0; i<SR_DEPTH; i=i+1) shift_reg[i] <= 0;
        end
        else if (weight_we) begin
            // 가중치 설정
            weights[weight_idx] <= din;
        end
    end

    // [5] 결과 출력
    // 읽기 신호를 한 클럭 지연시켜 동기화 (PipeReg 모듈 사용 가정)
    PipeReg #(1) FF_LOAD_Y (.CLK(clk), .RST(1'b0), .EN(en), .D(result_read), .Q(result_read_ff));
    
    assign dout = (result_read_ff) ? reg_Y : 32'd0;

endmodule
