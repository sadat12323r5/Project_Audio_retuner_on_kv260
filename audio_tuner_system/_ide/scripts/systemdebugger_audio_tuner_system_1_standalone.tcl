# Usage with Vitis IDE:
# In Vitis IDE create a Single Application Debug launch configuration,
# change the debug type to 'Attach to running target' and provide this 
# tcl script in 'Execute Script' option.
# Path of this script: /home/benh/Desktop/vitis_workspace/audio_tuner_system/_ide/scripts/systemdebugger_audio_tuner_system_1_standalone.tcl
# 
# 
# Usage with xsct:
# To debug using xsct, launch xsct and run below command
# source /home/benh/Desktop/vitis_workspace/audio_tuner_system/_ide/scripts/systemdebugger_audio_tuner_system_1_standalone.tcl
# 
connect -url tcp:127.0.0.1:3121
source /tools/Xilinx/Vitis/2024.1/scripts/vitis/util/zynqmp_utils.tcl
targets -set -nocase -filter {name =~"APU*"}
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Xilinx X-MLCC-01 XFL15ZDME5RSA" && level==0 && jtag_device_ctx=="jsn-X-MLCC-01-XFL15ZDME5RSA-04724093-0"}
fpga -file /home/benh/Desktop/vitis_workspace/audio_tuner/_ide/bitstream/speaker_2_wrapper.bit
targets -set -nocase -filter {name =~"APU*"}
loadhw -hw /home/benh/Desktop/vitis_workspace/kv260_lab3_platform/export/kv260_lab3_platform/hw/speaker_2_wrapper.xsa -mem-ranges [list {0x80000000 0xbfffffff} {0x400000000 0x5ffffffff} {0x1000000000 0x7fffffffff}] -regs
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*"}
set mode [expr [mrd -value 0xFF5E0200] & 0xf]
targets -set -nocase -filter {name =~ "*A53*#0"}
rst -processor
dow /home/benh/Desktop/vitis_workspace/kv260_lab3_platform/export/kv260_lab3_platform/sw/kv260_lab3_platform/boot/fsbl.elf
set bp_8_14_fsbl_bp [bpadd -addr &XFsbl_Exit]
con -block -timeout 60
bpremove $bp_8_14_fsbl_bp
targets -set -nocase -filter {name =~ "*A53*#0"}
rst -processor
dow /home/benh/Desktop/vitis_workspace/audio_tuner/Debug/audio_tuner.elf
configparams force-mem-access 0
bpadd -addr &main
