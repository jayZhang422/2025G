# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct E:\7020_Project\25G\25G_PS\Identification_platform\platform.tcl
# 
# OR launch xsct and run below command.
# source E:\7020_Project\25G\25G_PS\Identification_platform\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {Identification_platform}\
-hw {E:\7020_Project\25G\25G_PL\top.xsa}\
-proc {ps7_cortexa9_0} -os {freertos10_xilinx} -fsbl-target {psu_cortexa53_0} -out {E:/7020_Project/25G/25G_PS}

platform write
platform generate -domains 
platform active {Identification_platform}
platform active {Identification_platform}
platform active {Identification_platform}
platform config -updatehw {D:/DianSai/2023H_25/2025G_calvin/25G_PL/top.xsa}
platform config -updatehw {D:/DianSai/2023H_25/2025G_calvin/25G_PL/top.xsa}
