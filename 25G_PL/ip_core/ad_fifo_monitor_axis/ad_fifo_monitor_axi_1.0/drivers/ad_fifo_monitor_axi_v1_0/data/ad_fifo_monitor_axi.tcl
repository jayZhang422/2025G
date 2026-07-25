

proc generate {drv_handle} {
	xdefine_include_file $drv_handle "xparameters.h" "ad_fifo_monitor_axi" "NUM_INSTANCES" "DEVICE_ID"  "C_ad_fifo_monitor_axi_BASEADDR" "C_ad_fifo_monitor_axi_HIGHADDR"
}
