# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct C:\unswworks\workspace\project_plat\platform.tcl
# 
# OR launch xsct and run below command.
# source C:\unswworks\workspace\project_plat\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {project_plat}\
-hw {C:\unswworks\COMP3601\COMP3601_Project_Audio_tuner\v2.0.xsa}\
-proc {psu_cortexa53_0} -os {standalone} -arch {64-bit} -fsbl-target {psu_cortexa53_0} -out {C:/unswworks/workspace}

platform write
platform generate -domains 
platform active {project_plat}
platform generate
catch {platform remove try2}
bsp reload
bsp setlib -name xilffs -ver 5.2
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains 
platform clean
platform generate
bsp setlib -name xilffs -ver 5.2
bsp write
bsp reload
catch {bsp regenerate}
platform generate -domains standalone_domain 
bsp write
platform clean
bsp reload
platform generate
