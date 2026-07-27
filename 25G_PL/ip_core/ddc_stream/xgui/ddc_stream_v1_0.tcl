# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  ipgui::add_param $IPINST -name "AXI_ADDR_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "BUILD_ID" -parent ${Page_0}
  ipgui::add_param $IPINST -name "DEFAULT_PINC" -parent ${Page_0}


}

proc update_PARAM_VALUE.AXI_ADDR_WIDTH { PARAM_VALUE.AXI_ADDR_WIDTH } {
	# Procedure called to update AXI_ADDR_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.AXI_ADDR_WIDTH { PARAM_VALUE.AXI_ADDR_WIDTH } {
	# Procedure called to validate AXI_ADDR_WIDTH
	return true
}

proc update_PARAM_VALUE.BUILD_ID { PARAM_VALUE.BUILD_ID } {
	# Procedure called to update BUILD_ID when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.BUILD_ID { PARAM_VALUE.BUILD_ID } {
	# Procedure called to validate BUILD_ID
	return true
}

proc update_PARAM_VALUE.DEFAULT_PINC { PARAM_VALUE.DEFAULT_PINC } {
	# Procedure called to update DEFAULT_PINC when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.DEFAULT_PINC { PARAM_VALUE.DEFAULT_PINC } {
	# Procedure called to validate DEFAULT_PINC
	return true
}


proc update_MODELPARAM_VALUE.AXI_ADDR_WIDTH { MODELPARAM_VALUE.AXI_ADDR_WIDTH PARAM_VALUE.AXI_ADDR_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.AXI_ADDR_WIDTH}] ${MODELPARAM_VALUE.AXI_ADDR_WIDTH}
}

proc update_MODELPARAM_VALUE.DEFAULT_PINC { MODELPARAM_VALUE.DEFAULT_PINC PARAM_VALUE.DEFAULT_PINC } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.DEFAULT_PINC}] ${MODELPARAM_VALUE.DEFAULT_PINC}
}

proc update_MODELPARAM_VALUE.BUILD_ID { MODELPARAM_VALUE.BUILD_ID PARAM_VALUE.BUILD_ID } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.BUILD_ID}] ${MODELPARAM_VALUE.BUILD_ID}
}

