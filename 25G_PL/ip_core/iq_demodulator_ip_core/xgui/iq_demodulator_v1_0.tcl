# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  ipgui::add_param $IPINST -name "ACC_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "ADC_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "AXI_ADDR_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "DDS_LATENCY" -parent ${Page_0}
  ipgui::add_param $IPINST -name "LO_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "MAX_DDS_LATENCY" -parent ${Page_0}
  ipgui::add_param $IPINST -name "PHASE_WIDTH" -parent ${Page_0}


}

proc update_PARAM_VALUE.ACC_WIDTH { PARAM_VALUE.ACC_WIDTH } {
	# Procedure called to update ACC_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.ACC_WIDTH { PARAM_VALUE.ACC_WIDTH } {
	# Procedure called to validate ACC_WIDTH
	return true
}

proc update_PARAM_VALUE.ADC_WIDTH { PARAM_VALUE.ADC_WIDTH } {
	# Procedure called to update ADC_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.ADC_WIDTH { PARAM_VALUE.ADC_WIDTH } {
	# Procedure called to validate ADC_WIDTH
	return true
}

proc update_PARAM_VALUE.AXI_ADDR_WIDTH { PARAM_VALUE.AXI_ADDR_WIDTH } {
	# Procedure called to update AXI_ADDR_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.AXI_ADDR_WIDTH { PARAM_VALUE.AXI_ADDR_WIDTH } {
	# Procedure called to validate AXI_ADDR_WIDTH
	return true
}

proc update_PARAM_VALUE.DDS_LATENCY { PARAM_VALUE.DDS_LATENCY } {
	# Procedure called to update DDS_LATENCY when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.DDS_LATENCY { PARAM_VALUE.DDS_LATENCY } {
	# Procedure called to validate DDS_LATENCY
	return true
}

proc update_PARAM_VALUE.LO_WIDTH { PARAM_VALUE.LO_WIDTH } {
	# Procedure called to update LO_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.LO_WIDTH { PARAM_VALUE.LO_WIDTH } {
	# Procedure called to validate LO_WIDTH
	return true
}

proc update_PARAM_VALUE.MAX_DDS_LATENCY { PARAM_VALUE.MAX_DDS_LATENCY } {
	# Procedure called to update MAX_DDS_LATENCY when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.MAX_DDS_LATENCY { PARAM_VALUE.MAX_DDS_LATENCY } {
	# Procedure called to validate MAX_DDS_LATENCY
	return true
}

proc update_PARAM_VALUE.PHASE_WIDTH { PARAM_VALUE.PHASE_WIDTH } {
	# Procedure called to update PHASE_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.PHASE_WIDTH { PARAM_VALUE.PHASE_WIDTH } {
	# Procedure called to validate PHASE_WIDTH
	return true
}


proc update_MODELPARAM_VALUE.ADC_WIDTH { MODELPARAM_VALUE.ADC_WIDTH PARAM_VALUE.ADC_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.ADC_WIDTH}] ${MODELPARAM_VALUE.ADC_WIDTH}
}

proc update_MODELPARAM_VALUE.LO_WIDTH { MODELPARAM_VALUE.LO_WIDTH PARAM_VALUE.LO_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.LO_WIDTH}] ${MODELPARAM_VALUE.LO_WIDTH}
}

proc update_MODELPARAM_VALUE.PHASE_WIDTH { MODELPARAM_VALUE.PHASE_WIDTH PARAM_VALUE.PHASE_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.PHASE_WIDTH}] ${MODELPARAM_VALUE.PHASE_WIDTH}
}

proc update_MODELPARAM_VALUE.ACC_WIDTH { MODELPARAM_VALUE.ACC_WIDTH PARAM_VALUE.ACC_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.ACC_WIDTH}] ${MODELPARAM_VALUE.ACC_WIDTH}
}

proc update_MODELPARAM_VALUE.DDS_LATENCY { MODELPARAM_VALUE.DDS_LATENCY PARAM_VALUE.DDS_LATENCY } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.DDS_LATENCY}] ${MODELPARAM_VALUE.DDS_LATENCY}
}

proc update_MODELPARAM_VALUE.MAX_DDS_LATENCY { MODELPARAM_VALUE.MAX_DDS_LATENCY PARAM_VALUE.MAX_DDS_LATENCY } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.MAX_DDS_LATENCY}] ${MODELPARAM_VALUE.MAX_DDS_LATENCY}
}

proc update_MODELPARAM_VALUE.AXI_ADDR_WIDTH { MODELPARAM_VALUE.AXI_ADDR_WIDTH PARAM_VALUE.AXI_ADDR_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.AXI_ADDR_WIDTH}] ${MODELPARAM_VALUE.AXI_ADDR_WIDTH}
}

