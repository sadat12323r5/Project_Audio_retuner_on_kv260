# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct /home/cse/Desktop/BB_Software/kv260_lab3_platform/platform.tcl
# 
# OR launch xsct and run below command.
# source /home/cse/Desktop/BB_Software/kv260_lab3_platform/platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {kv260_lab3_platform}\
-hw {/home/cse/Desktop/Project_Audio_retuner_on_kv260_BB/v2.0.xsa}\
-proc {psu_cortexa53_0} -os {standalone} -arch {64-bit} -fsbl-target {psu_cortexa53_0} -out {/home/cse/Desktop/BB_Software}

platform write
platform generate -domains 
platform active {kv260_lab3_platform}
platform generate
platform active {kv260_lab3_platform}
domain active {zynqmp_fsbl}
domain active {standalone_domain}
bsp reload
bsp reload
bsp setlib -name xilffs -ver 5.2
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains standalone_domain 
bsp setlib -name xilfpga -ver 6.6
bsp write
bsp reload
catch {bsp regenerate}
bsp setlib -name xilpm -ver 5.2
bsp write
bsp reload
catch {bsp regenerate}
bsp removelib -name xilfpga
bsp write
bsp reload
catch {bsp regenerate}
platform generate
bsp setlib -name xilfpga -ver 6.6
bsp write
bsp reload
catch {bsp regenerate}
bsp removelib -name xilfpga
bsp write
bsp reload
catch {bsp regenerate}
domain active {zynqmp_pmufw}
bsp reload
domain active {standalone_domain}
bsp setlib -name xilsecure -ver 5.3
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains standalone_domain 
bsp removelib -name xilsecure
bsp setlib -name libmetal -ver 2.7
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains standalone_domain 
bsp removelib -name libmetal
bsp setlib -name xilfpga -ver 6.6
bsp write
bsp reload
catch {bsp regenerate}
bsp removelib -name xilfpga
bsp write
bsp reload
catch {bsp regenerate}
bsp setlib -name xilflash -ver 4.11
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains standalone_domain 
bsp removelib -name xilflash
bsp setlib -name xilmailbox -ver 1.9
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains standalone_domain 
bsp removelib -name xilmailbox
bsp write
bsp reload
catch {bsp regenerate}
bsp reload
platform generate -domains standalone_domain 
platform generate
platform active {kv260_lab3_platform}
bsp reload
bsp reload
domain active {zynqmp_fsbl}
bsp reload
bsp reload
domain active {zynqmp_pmufw}
bsp reload
bsp reload
platform generate -domains 
platform generate
platform active {kv260_lab3_platform}
platform config -updatehw {/home/cse/Desktop/speaker_2_wrapper.xsa}
platform clean
platform generate
platform active {kv260_lab3_platform}
domain active {standalone_domain}
bsp reload
bsp reload
platform generate
platform active {kv260_lab3_platform}
platform generate -domains 
platform active {kv260_lab3_platform}
platform generate
platform config -updatehw {C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/speaker_2_wrapper.xsa}
platform generate -domains 
