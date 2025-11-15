# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: C:\unswworks\workspace\audio_tuner_system\_ide\scripts\debugger_audio_tuner-default.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source C:\unswworks\workspace\audio_tuner_system\_ide\scripts\debugger_audio_tuner-default.tcl
# 
connect -url tcp:127.0.0.1:3121
source D:/Vivado/Vitis/2024.1/scripts/vitis/util/zynqmp_utils.tcl
targets -set -nocase -filter {name =~"APU*"}
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Xilinx X-MLCC-01 XFL14OY1OJLXA" && level==0 && jtag_device_ctx=="jsn-X-MLCC-01-XFL14OY1OJLXA-04724093-0"}
fpga -file C:/unswworks/workspace/audio_tuner/_ide/bitstream/v2.0.bit
targets -set -nocase -filter {name =~"APU*"}
loadhw -hw C:/unswworks/workspace/project_plat/export/project_plat/hw/v2.0.xsa -mem-ranges [list {0x80000000 0xbfffffff} {0x400000000 0x5ffffffff} {0x1000000000 0x7fffffffff}] -regs
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*"}
set mode [expr [mrd -value 0xFF5E0200] & 0xf]
targets -set -nocase -filter {name =~ "*A53*#0"}
rst -processor
dow C:/unswworks/workspace/project_plat/export/project_plat/sw/project_plat/boot/fsbl.elf
set bp_25_13_fsbl_bp [bpadd -addr &XFsbl_Exit]
con -block -timeout 60
bpremove $bp_25_13_fsbl_bp
targets -set -nocase -filter {name =~ "*A53*#0"}
rst -processor
dow C:/unswworks/workspace/audio_tuner/Debug/audio_tuner.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "*A53*#0"}
con
