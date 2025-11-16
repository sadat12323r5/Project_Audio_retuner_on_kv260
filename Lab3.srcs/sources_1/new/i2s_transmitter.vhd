----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 16.11.2025 17:04:58
-- Design Name: 
-- Module Name: i2s_transmitter - Behavioral
-- Project Name: 
-- Target Devices: 
-- Tool Versions: 
-- Description: 
-- 
-- Dependencies: 
-- 
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
-- 
----------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity i2s_transmitter is
    generic (
        DATA_WIDTH    : natural := 32;   -- bits per I2S sample word (per channel window)
        PCM_PRECISION : natural := 18;   -- not used here, kept for compatibility
        -- BCLK generator: clk / (2 * BCLK_HALF) ≈ desired BCLK
        -- e.g. for clk=100MHz, BCLK_HALF=16 -> ~3.125 MHz (close to 3.072 MHz @48k, 32b, 2ch)
        BCLK_HALF     : natural := 16
    );
    port (
        clk        : in  std_logic;

        -- I²S outputs
        i2s_lrcl   : out std_logic;      -- 0=Left, 1=Right (word select)
        i2s_din    : out std_logic;      -- serial data (MSB first)
        i2s_bclk   : out std_logic;      -- bit clock

        -- FIFO interface (from speaker FIFO)
        fifo_data  : in  std_logic_vector(DATA_WIDTH-1 downto 0);
        fifo_r_stb : out std_logic;      -- 1-cycle pulse to pop one word
        fifo_empty : in  std_logic
    );
end i2s_transmitter;

architecture Behavioral of i2s_transmitter is
    -- -------------------------
    -- BCLK generator (from clk)
    -- -------------------------
    signal bclk_cnt     : unsigned(15 downto 0) := (others => '0');
    signal bclk         : std_logic := '0';

    -- LRCLK: toggles every 32 BCLK rising edges (32 bits per channel)
    signal lr_cnt       : unsigned(5 downto 0) := (others => '0');   -- 0..31
    signal lrclk        : std_logic := '0';

    -- I²S shift control
    signal bit_idx      : unsigned(5 downto 0) := (others => '0');   -- 31..0
    signal msb_hold     : std_logic := '1';      -- 1-bit MSB delay after LR edge (I²S rule)
    signal sample_buf   : std_logic_vector(DATA_WIDTH-1 downto 0) := (others => '0');

    -- edge detect
    signal lrclk_d      : std_logic := '0';
begin
    -- Drive outputs
    i2s_bclk <= bclk;
    i2s_lrcl <= lrclk;

    ----------------------------------------------------------------
    -- Generate BCLK from fabric clk: toggle every BCLK_HALF cycles
    ----------------------------------------------------------------
    process (clk)
    begin
        if rising_edge(clk) then
            if bclk_cnt = to_unsigned(BCLK_HALF-1, bclk_cnt'length) then
                bclk_cnt <= (others => '0');
                bclk     <= not bclk;
            else
                bclk_cnt <= bclk_cnt + 1;
            end if;
        end if;
    end process;

    --------------------------------------------------------------
    -- LRCLK: toggle every 32 rising edges of BCLK
    -- (Don't clock logic on bclk directly in real silicon-use CE.
    --  Here we keep your style; it's acceptable for this small block.)
    --------------------------------------------------------------
    process (bclk)
    begin
        if rising_edge(bclk) then
            if lr_cnt = to_unsigned(31, lr_cnt'length) then
                lr_cnt <= (others => '0');
                lrclk  <= not lrclk;
            else
                lr_cnt <= lr_cnt + 1;
            end if;
        end if;
    end process;

    --------------------------------------------------------------
    -- I²S shifter with MSB delay and LR-aligned fetch
    --------------------------------------------------------------
    process (bclk)
    begin
        if rising_edge(bclk) then
            -- default
            fifo_r_stb <= '0';

            -- detect LRCLK edge (start of a new channel window)
            lrclk_d <= lrclk;
            if lrclk_d /= lrclk then
                -- New channel frame starts: load a fresh word and arm MSB delay
                bit_idx  <= to_unsigned(31, bit_idx'length);
                msb_hold <= '1';

                if fifo_empty = '0' then
                    sample_buf <= fifo_data;  -- grab next 32-bit word
                    fifo_r_stb <= '1';        -- one pop per channel frame
                else
                    -- no data -> send zeros this frame
                    sample_buf <= (others => '0');
                end if;

            else
                -- within a channel frame
                if msb_hold = '1' then
                    -- enforce the one-BCLK MSB delay after LRCLK toggle
                    msb_hold <= '0';
                else
                    -- shift through remaining bits
                    if bit_idx /= to_unsigned(0, bit_idx'length) then
                        bit_idx <= bit_idx - 1;
                    end if;
                end if;
            end if;

            -- Output current bit (MSB first)
            i2s_din <= sample_buf(to_integer(bit_idx));
        end if;
    end process;

end Behavioral;
