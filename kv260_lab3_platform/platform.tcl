# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct C:\Sadat_files\Current_Term\COMP3601\lab3\kv260_lab3_platform\platform.tcl
# 
# OR launch xsct and run below command.
# source C:\Sadat_files\Current_Term\COMP3601\lab3\kv260_lab3_platform\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {kv260_lab3_platform}\
-hw {C:\Sadat_files\Current_Term\COMP3601\Lab_3\Lab3\design_1_wrapper.xsa}\
-proc {psu_cortexa53_0} -os {standalone} -arch {64-bit} -fsbl-target {psu_cortexa53_0} -out {C:/Sadat_files/Current_Term/COMP3601/lab3}

platform write
platform generate -domains 
platform active {kv260_lab3_platform}
platform generate
platform generate
platform active {kv260_lab3_platform}
platform config -updatehw {C:/Sadat_files/Current_Term/COMP3601/Lab_3/Lab3/design_1_wrapper.xsa}
platform generate -domains 
bsp reload
bsp setlib -name xilffs -ver 5.2
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains standalone_domain 
platform config -updatehw {C:/Sadat_files/Current_Term/COMP3601/Lab_3/Lab3/design_1_wrapper.xsa}
platform generate -domains 
domain active {zynqmp_fsbl}
bsp reload
domain active {standalone_domain}
bsp reload
bsp reload
domain active {zynqmp_fsbl}
bsp reload
domain active {standalone_domain}
bsp write
platform active {kv260_lab3_platform}
platform active {kv260_lab3_platform}
platform config -updatehw {/home/benh/Desktop/comp3601_labs/COMP3601_Project_Audio_tuner/v2.0.xsa}
platform config -updatehw {/home/benh/Desktop/comp3601_labs/COMP3601_Project_Audio_tuner/v2.0.xsa}
platform clean
platform generate
platform clean
platform generate
platform active {kv260_lab3_platform}
platform config -updatehw {C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/speaker_wrapper.xsa}
platform generate -domains standalone_domain,zynqmp_fsbl,zynqmp_pmufw 
platform generate -domains standalone_domain,zynqmp_fsbl,zynqmp_pmufw 
platform active {kv260_lab3_platform}
platform config -updatehw {C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/speaker_2_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/speaker_2_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/speaker_3_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/speaker_4_wrapper.xsa}
platform generate -domains 
platform config -updatehw {C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/speaker_1_wrapper.xsa}
platform generate -domains 
