// Filename: onedim_filter_acc.v
module onedim_filter_acc (
    input  wire          HCLK,
    input  wire          HRESETn,
    input  wire          HSEL,
    input  wire [31:0]   HADDR,
    input  wire          HWRITE,
    input  wire [31:0]   HWDATA,
    input  wire          HREADY,     // ★ 수정: HREADY 입력 포트 추가
    output wire [31:0]  HRDATA,
    output wire         HREADYOUT
);

    // ------------------------------------------------------
    // AHB-Lite Signals
    // ------------------------------------------------------
    reg [31:0] haddr_reg;
    reg        hwrite_reg;
    reg        hsel_reg;
    
    // 이 가속기는 단일 사이클에 응답 가능하므로 항상 Ready
    assign HREADYOUT = 1'b1; 

    always @(posedge HCLK or negedge HRESETn) begin
        if (!HRESETn) begin
            haddr_reg  <= 32'd0;
            hwrite_reg <= 1'b0;
            hsel_reg   <= 1'b0;
        end else if (HREADY) begin // ★ 수정: HREADY가 High일 때만 래치
            haddr_reg  <= HADDR;
            hwrite_reg <= HWRITE;
            hsel_reg   <= HSEL;
        end
    end

    // ------------------------------------------------------
    // Register Map Definitions (동일)
    // ------------------------------------------------------
    reg [31:0] reg_ctrl;
    reg [31:0] reg_xn;    
    reg [31:0] reg_c0;
    reg [31:0] reg_c1;
    reg [31:0] reg_c2;
    reg [31:0] reg_yn;    

    // Internal History Buffers
    reg [31:0] reg_xn_1;  
    reg [31:0] reg_xn_2;  

    wire start_signal = reg_ctrl[0];
    wire clear_signal = reg_ctrl[1];

    // ------------------------------------------------------
    // Read Logic (동일)
    // ------------------------------------------------------
    reg [31:0] rdata_comb;

    always @(*) begin
        case (haddr_reg[7:0]) 
            8'h00: rdata_comb = reg_ctrl;
            8'h04: rdata_comb = reg_xn;
            8'h08: rdata_comb = reg_c0;
            8'h0C: rdata_comb = reg_c1;
            8'h10: rdata_comb = reg_c2;
            8'h14: rdata_comb = reg_yn;
            default: rdata_comb = 32'd0;
        endcase
    end

    assign HRDATA = rdata_comb;

    // ------------------------------------------------------
    // Write & Core Logic (동일)
    // ------------------------------------------------------
    // AHB Write operation check: we need to check hsel_reg and hwrite_reg 
    // which were latched when HREADY was high.
    // The write data (HWDATA) is for the current data phase.

    always @(posedge HCLK or negedge HRESETn) begin
        if (!HRESETn) begin
            reg_ctrl <= 32'd0;
            reg_xn   <= 32'd0;
            reg_c0   <= 32'd0;
            reg_c1   <= 32'd0;
            reg_c2   <= 32'd0;
            reg_yn   <= 32'd0;
            reg_xn_1 <= 32'd0;
            reg_xn_2 <= 32'd0;
        end else begin
            // 1. AHB Write Operation (hsel_reg & hwrite_reg 기반)
            if (hsel_reg && hwrite_reg) begin
                case (haddr_reg[7:0])
                    8'h00: reg_ctrl <= HWDATA;
                    8'h04: reg_xn   <= HWDATA;
                    8'h08: reg_c0   <= HWDATA;
                    8'h0C: reg_c1   <= HWDATA;
                    8'h10: reg_c2   <= HWDATA;
                    // 0x14 is Read Only (Result)
                endcase
            end

            // 2. Clear Logic (Software Reset for Accelerator)
            if (clear_signal) begin
                reg_xn_1 <= 32'd0;
                reg_xn_2 <= 32'd0;
                reg_yn   <= 32'd0;
                reg_ctrl[1] <= 1'b0; // Auto-clear the clear bit
            end
            
            // 3. FIR Filter Computation Logic
            else if (start_signal) begin
                // y[n] = c0*x[n] + c1*x[n-1] + c2*x[n-2]
                reg_yn <= (reg_c0 * reg_xn) + (reg_c1 * reg_xn_1) + (reg_c2 * reg_xn_2);
                
                // Shift History
                reg_xn_2 <= reg_xn_1;
                reg_xn_1 <= reg_xn;

                // Auto-clear the start bit after operation
                reg_ctrl[0] <= 1'b0; 
            end
        end
    end

endmodule