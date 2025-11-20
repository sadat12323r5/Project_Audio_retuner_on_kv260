#
# This file was created by Xilinx Vitis tool.
#


#
# tools
#

RM = rm -f
RMDIR = rm -rf



#
# variables
#

SDCARD_GEN = sdcard_gen
GENERATE_BIF = ::scw::generate_bif

XPFM_PATH = C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/kv260_lab3_platform/export/kv260_lab3_platform/kv260_lab3_platform.xpfm
SYS_CONFIG = kv260_lab3_platform
DOMAINS = standalone_domain
BIF_PATH = C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/kv260_lab3_system/Debug/system.bif

BITSTREAM_PATH = C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/kv260_lab3/_ide/bitstream/4fir_wrapper.bit
SD_FILES = --elf C:/Sadat_files/Academics/Current_Term/COMP3601/Project/project_repo/COMP3601_Project_Audio_tuner/kv260_lab3/Debug/kv260_lab3.elf,psu_cortexa53_0

#
# primary build targets
#

package:
	-@echo Generating bif file for the system project
	generate_system_bif.bat 57245 ${XPFM_PATH} ${DOMAINS} ${BIF_PATH}
	${SDCARD_GEN} --xpfm ${XPFM_PATH} --sys_config ${SYS_CONFIG} --bif ${BIF_PATH} --bitstream ${BITSTREAM_PATH} ${SD_FILES}

clean:
	$(RMDIR) ./_sds
	${RMDIR} package
	${RM} system.bif

.PHONY: all package clean

all: package


