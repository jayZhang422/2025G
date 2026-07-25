COMPILER=
ARCHIVER=
CP=cp
COMPILER_FLAGS=
EXTRA_COMPILER_FLAGS=
LIB=libxil.a

RELEASEDIR=../../../lib
INCLUDEDIR=../../../include
INCLUDES=-I./. -I${INCLUDEDIR}

INCLUDEFILES=ad_fifo_monitor_axi.h
LIBSOURCES=ad_fifo_monitor_axi.c ad_fifo_monitor_axi_selftest.c
OUTS=ad_fifo_monitor_axi.o ad_fifo_monitor_axi_selftest.o

libs:
	echo "Compiling ad_fifo_monitor_axi..."
	$(COMPILER) $(COMPILER_FLAGS) $(EXTRA_COMPILER_FLAGS) $(INCLUDES) $(LIBSOURCES)
	$(ARCHIVER) -r ${RELEASEDIR}/${LIB} ${OUTS}
	make clean

include:
	${CP} $(INCLUDEFILES) $(INCLUDEDIR)

clean:
	rm -rf ${OUTS}
