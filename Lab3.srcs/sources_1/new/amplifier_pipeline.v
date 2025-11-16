library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.aud_param.all;

entity amplifier_pipeline is
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
        clk  : in  std_logic;  -- audio / AXIS clock
        rst  : in  std_logic;  -- active-low reset (same convention as before)

        --------------------------------------------------
        -- AXI4-Stream INPUT from DMA (MM2S)
        --------------------------------------------------
        axis_tdata   : in  std_logic_vector(DATA_WIDTH-1 downto 0);
        axis_tvalid  : in  std_logic;
        axis_tready  : out std_logic;
        axis_tlast   : in  std_logic;    -- currently unused, but keep for AXIS compliance

        --------------------------------------------------
        -- I2S to speaker (OUTPUTS)
        --------------------------------------------------
        i2s_bclk     : out std_logic;
        i2s_lrcl     : out std_logic;
        i2s_dout     : out std_logic;    -- serial data going TO codec / amp

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
end amplifier_pipeline;

architecture Behavioural of amplifier_pipeline is

    --------------------------------------------------
    -- FIFO for SPEAKER path: AXIS -> FIFO -> I2S TX
    --------------------------------------------------
    signal sig_fifo_rst        : std_logic;
    signal sig_fifo_wr         : std_logic;
    signal sig_fifo_rd         : std_logic;
    signal sig_fifo_full       : std_logic;
    signal sig_fifo_empty      : std_logic;
    signal sig_fifo_data_w     : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal sig_fifo_data_r     : std_logic_vector(DATA_WIDTH-1 downto 0);

    --------------------------------------------------
    -- Control interface (AXI4-Lite)
    --------------------------------------------------
    signal sig_control_reg     : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal sig_status_reg      : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal sig_gain_reg        : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal sig_speaker_enable  : std_logic := '1';  -- simple enable bit (default ON)

    -- Internal tready (can be gated by enable)
    signal axis_tready_int     : std_logic;

begin

    --------------------------------------------------------------------
    -- Status register constant
    --------------------------------------------------------------------
    sig_status_reg <= x"0CA7CAFE";

    --------------------------------------------------------------------
    -- AXI-Lite control bus
    --------------------------------------------------------------------
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

    -- Optional: use bit 1 of control_reg as speaker enable
    process (sig_control_reg)
    begin
        sig_speaker_enable <= sig_control_reg(1);  -- 1 = enable, 0 = mute/stop
    end process;

    --------------------------------------------------------------------
    -- FIFO reset (active high)
    -- rst is active-low, so invert it. If board reset is asserted (rst='0'),
    -- this goes '1' and clears the FIFO.
    --------------------------------------------------------------------
    sig_fifo_rst <= not rst;

    --------------------------------------------------------------------
    -- AXIS SLAVE → FIFO write side (speaker path)
    --------------------------------------------------------------------
    -- Ready when FIFO not full and speaker enabled
    axis_tready_int <= (not sig_fifo_full) and sig_speaker_enable;
    axis_tready     <= axis_tready_int;

    -- Write to FIFO on valid & ready
    sig_fifo_wr     <= axis_tvalid and axis_tready_int;
    sig_fifo_data_w <= axis_tdata;

    --------------------------------------------------------------------
    -- FIFO instance (shared clock, simple depth)
    --------------------------------------------------------------------
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

    --------------------------------------------------------------------
    -- I2S Transmitter: pulls from FIFO and drives speaker pins
    --------------------------------------------------------------------
    inst_i2s_transmitter : entity work.i2s_transmitter
    generic map (
        DATA_WIDTH    => DATA_WIDTH,
        PCM_PRECISION => PCM_PRECISION,
        BCLK_HALF     => 16            -- adjust if you need exact fs
    )
    port map (
        clk        => clk,

        i2s_lrcl   => i2s_lrcl,
        i2s_din    => i2s_dout,
        i2s_bclk   => i2s_bclk,

        fifo_data  => sig_fifo_data_r,
        fifo_r_stb => sig_fifo_rd,
        fifo_empty => sig_fifo_empty
    );

end Behavioural;
