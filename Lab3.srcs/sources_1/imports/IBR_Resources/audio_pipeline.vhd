library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.aud_param.all;

entity audio_pipeline is
    generic(
        PCM_PRECISION        : integer := 18;
        PCM_WIDTH            : integer := 24;
        DATA_WIDTH           : integer := 32;
        FIFO_DEPTH           : integer := 12;
        TRANSFER_LEN         : integer := 5;
        C_S00_AXI_DATA_WIDTH : integer := 32;
        C_S00_AXI_ADDR_WIDTH : integer := 5
    );
    port(
        clk : in std_logic;
        rst : in std_logic;

        --------------------------------------------------
        -- I2S
        --------------------------------------------------
        i2s_bclk : out std_logic;
        i2s_lrcl : out std_logic;
        i2s_dout : in  std_logic;

        --------------------------------------------------
        -- AXI4-Stream
        --------------------------------------------------
        axis_tdata  : out std_logic_vector(DATA_WIDTH-1 downto 0);
        axis_tvalid : out std_logic;
        axis_tready : in  std_logic;
        axis_tlast  : out std_logic;
        
        --------------------------------------------------
        -- Control interface (AXI4-Lite)
        --------------------------------------------------
        s00_axi_aclk    : in  std_logic;
        s00_axi_aresetn : in  std_logic;
        s00_axi_awaddr  : in  std_logic_vector(C_S00_AXI_ADDR_WIDTH-1 downto 0);
        s00_axi_awprot  : in  std_logic_vector(2 downto 0);
        s00_axi_awvalid : in  std_logic;
        s00_axi_awready : out std_logic;
        s00_axi_wdata   : in  std_logic_vector(C_S00_AXI_DATA_WIDTH-1 downto 0);
        s00_axi_wstrb   : in  std_logic_vector((C_S00_AXI_DATA_WIDTH/8)-1 downto 0);
        s00_axi_wvalid  : in  std_logic;
        s00_axi_wready  : out std_logic;
        s00_axi_bresp   : out std_logic_vector(1 downto 0);
        s00_axi_bvalid  : out std_logic;
        s00_axi_bready  : in  std_logic;
        s00_axi_araddr  : in  std_logic_vector(C_S00_AXI_ADDR_WIDTH-1 downto 0);
        s00_axi_arprot  : in  std_logic_vector(2 downto 0);
        s00_axi_arvalid : in  std_logic;
        s00_axi_arready : out std_logic;
        s00_axi_rdata   : out std_logic_vector(C_S00_AXI_DATA_WIDTH-1 downto 0);
        s00_axi_rresp   : out std_logic_vector(1 downto 0);
        s00_axi_rvalid  : out std_logic;
        s00_axi_rready  : in  std_logic
    );
end audio_pipeline;

architecture Behavioural of audio_pipeline is
    --------------------------------------------------
    -- FIFO
    --------------------------------------------------
    signal sig_fifo_rst    : std_logic;
    signal sig_fifo_wr     : std_logic;
    signal sig_fifo_rd     : std_logic;
    signal sig_fifo_full   : std_logic;
    signal sig_fifo_empty  : std_logic;
    signal sig_fifo_data_w : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal sig_fifo_data_r : std_logic_vector(DATA_WIDTH-1 downto 0);

    --------------------------------------------------
    -- AXI4-Stream
    --------------------------------------------------
    signal sig_axis_tdata  : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal sig_axis_tvalid : std_logic;
    signal sig_axis_tready : std_logic;
    signal sig_axis_tlast  : std_logic;

    --------------------------------------------------
    -- Control interface (AXI4-Lite)
    --------------------------------------------------
    signal sig_control_reg : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal sig_status_reg  : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal sig_gain_reg    : std_logic_vector(DATA_WIDTH-1 downto 0);
    
    --------------------------------------------------
    -- DSP chain signals
    --------------------------------------------------
    signal fir_sample : std_logic_vector(PCM_WIDTH-1 downto 0);
    signal env_sample : std_logic_vector(PCM_WIDTH-1 downto 0);
    signal dyn_sample : std_logic_vector(PCM_WIDTH-1 downto 0);

    signal rst_hi : std_logic;
begin

    sig_status_reg <= x"0ca7cafe";

    --------------------------------------------------
    -- Control bus
    --------------------------------------------------
    inst_ctrl_bus : ctrl_bus
        generic map (
            C_S_AXI_DATA_WIDTH => C_S00_AXI_DATA_WIDTH,
            C_S_AXI_ADDR_WIDTH => C_S00_AXI_ADDR_WIDTH
        )
        port map (
            cb_control_reg  => sig_control_reg,
            cb_status_reg   => sig_status_reg,
            cb_gain_reg     => sig_gain_reg,

            S_AXI_ACLK      => s00_axi_aclk,
            S_AXI_ARESETN   => s00_axi_aresetn,
            S_AXI_AWADDR    => s00_axi_awaddr,
            S_AXI_AWPROT    => s00_axi_awprot,
            S_AXI_AWVALID   => s00_axi_awvalid,
            S_AXI_AWREADY   => s00_axi_awready,
            S_AXI_WDATA     => s00_axi_wdata,
            S_AXI_WSTRB     => s00_axi_wstrb,
            S_AXI_WVALID    => s00_axi_wvalid,
            S_AXI_WREADY    => s00_axi_wready,
            S_AXI_BRESP     => s00_axi_bresp,
            S_AXI_BVALID    => s00_axi_bvalid,
            S_AXI_BREADY    => s00_axi_bready,
            S_AXI_ARADDR    => s00_axi_araddr,
            S_AXI_ARPROT    => s00_axi_arprot,
            S_AXI_ARVALID   => s00_axi_arvalid,
            S_AXI_ARREADY   => s00_axi_arready,
            S_AXI_RDATA     => s00_axi_rdata,
            S_AXI_RRESP     => s00_axi_rresp,
            S_AXI_RVALID    => s00_axi_rvalid,
            S_AXI_RREADY    => s00_axi_rready
        );

    -- sig_fifo_rst <= not rst; -- AXIS reset is active low
    sig_fifo_rst <= '0';

    --------------------------------------------------
    -- I2S Master
    --------------------------------------------------
    inst_i2s_master : i2s_master
        generic map (
            DATA_WIDTH    => DATA_WIDTH,
            PCM_PRECISION => PCM_PRECISION
        )
        port map (
            clk        => clk,

            i2s_lrcl   => i2s_lrcl,
            i2s_dout   => i2s_dout,
            i2s_bclk   => i2s_bclk,

            fifo_din   => sig_fifo_data_w,
            fifo_w_stb => sig_fifo_wr,
            fifo_full  => sig_fifo_full
        );

    --------------------------------------------------
    -- FIFO
    --------------------------------------------------
    inst_fifo : fifo 
        generic map (
            data_width => DATA_WIDTH,
            fifo_depth => FIFO_DEPTH
        )
        port map (
            clkw  => clk,
            clkr  => clk,
            rst   => sig_fifo_rst,

            wr    => sig_fifo_wr,
            din   => sig_fifo_data_w,
            full  => sig_fifo_full,

            rd    => sig_fifo_rd,
            dout  => sig_fifo_data_r,
            empty => sig_fifo_empty
        );
    
    --------------------------------------------------
    -- 4-tap FIR Low-Pass (on captured audio)
    --------------------------------------------------
    rst_hi <= not rst;  -- local inverted reset

    fir_inst : entity work.fir4_lowpass
        generic map (
            SAMPLE_WIDTH => PCM_WIDTH
        )
        port map (
            clk     => clk,
            rst     => rst_hi,  -- fir4_lowpass expects active-high reset
            x_in    => sig_fifo_data_r(PCM_WIDTH-1 downto 0),
            x_valid => sig_fifo_rd,  -- shift taps when a new FIFO word is read
            y_out   => fir_sample,
            y_valid => open          -- we use existing axis_tvalid logic
        );

    --------------------------------------------------
    -- Envelope follower (side-chain)
    --------------------------------------------------
    env_inst : entity work.envelope_follower
        generic map (
            SAMPLE_WIDTH  => PCM_WIDTH,
            SHIFT_ATTACK  => 3,
            SHIFT_RELEASE => 6
        )
        port map (
            clk       => clk,
            rst       => rst_hi,
            x_in      => fir_sample,
            x_valid   => sig_fifo_rd,
            env_out   => env_sample,
            env_valid => open
        );

    --------------------------------------------------
    -- Dynamics core (uses FIR audio + envelope)
    --------------------------------------------------
    dyn_inst : entity work.dynamics_core
        generic map (
            SAMPLE_WIDTH => PCM_WIDTH,

            -- tweak these later if needed
            GATE_LEVEL   => 2**15,
            BOOST_LEVEL  => 2**18,
            COMP_LEVEL   => 2**20,
            BOOST_SHIFT  => 1,
            COMP_SHIFT   => 1
        )
        port map (
            clk     => clk,
            rst     => rst_hi,
            x_in    => fir_sample,
            env_in  => env_sample,
            x_valid => sig_fifo_rd,
            y_out   => dyn_sample,
            y_valid => open
        );
    
    --------------------------------------------------
    -- FIFO to AXIS
    --------------------------------------------------
    -- FIFO read
    sig_fifo_rd    <= (not sig_fifo_empty) and sig_axis_tvalid and axis_tready;
    sig_axis_tvalid <= '1' when sig_fifo_empty = '0' else '0';
    axis_tvalid    <= sig_axis_tvalid;
    
    -- TLAST
    process (clk)
        variable v_cnt : integer := 0;
    begin
        if (rst = '0') then
            v_cnt := 0;
        elsif rising_edge(clk) then
            if ((sig_axis_tvalid and axis_tready) = '1') then
                v_cnt := v_cnt + 1;
                
                if (v_cnt = 256) then
                    axis_tlast <= '1';
                    v_cnt := 0;
                else
                    axis_tlast <= '0';
                end if;
            end if;
        end if;
    end process;
    -- axis_tlast <= '1';

    -- TDATA: drive processed (dynamics) audio into lower PCM_WIDTH bits
    -- upper bits zero-padded
    axis_tdata(PCM_WIDTH - 1 downto 0) <= dyn_sample;

end Behavioural;

