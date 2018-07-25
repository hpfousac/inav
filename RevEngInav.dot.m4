dnl
dnl m4 RevEngInav.dot.m4 > RevEngInav.dot && dot -Tpng RevEngInav.dot > RevEngInav.png
dnl
define(FUNC,`$1 [label="$1",color=blue,shape=box];')
define(SFUNC,`$1 [label="(static)\n$1",color=blue,shape=box];')

define(VAR,`$1 [label="$1",color=green,shape=note];')
define(SVAR,`$1 [label="(static)\n$1",color=green,shape=note];')

define(IO,`$1 [label="$1",color=black,shape=box];')

define(`CALL', `ifelse($#, 1, ,$#, 2, "$1" -> "$2" [label=call`,'color=red];, `"$1" -> "$2" [label=call`,'color=red]; CALL(`$1', shift(shift($@)))')')
define(`ICALL', `ifelse($#, 1, ,$#, 2, "$1" -> "$2" [label=icall`,'color=orangered];, `"$1" -> "$2" [label=icall`,'color=orangered]; ICALL(`$1', shift(shift($@)))')')
define(`IN', `ifelse($#, 1, ,$#, 2, "$1" -> "$2" [label=in`,'color=red];, `"$1" -> "$2" [label=in`,'color=red]; IN(`$1', shift(shift($@)))')')

define(STORE,`$1 -> $2 [label=store,color=yellow3]')
define(LOAD,`$2 -> $1 [label=load,color=yellow3]')


digraph INAV {
	label="iNAV FC"

	subgraph cluster_cli_c {
		label="File: cli.c";

		FUNC(cliProcess)

		FUNC(cliPpmInfo)
		FUNC(cliPwmInfo)

		ICALL(cliProcess, cliPpmInfo, cliPwmInfo)
	}

	subgraph cluster_fc_init_c {
		label="File: fc_init.c";

		FUNC(init)
	}

	subgraph cluster_fc_msp_c {
		label="File: fc_msp.c";

		FUNC(mspFcProcessCommand)
	}

	subgraph cluster_fc_tasks_c {
		label="File: fc_tasks.c";

		FUNC(taskHandleSerial)
	}

	subgraph cluster_main_c {
		label="File: main.c";

		FUNC(main)
	}

	subgraph cluster_msp_serial_c {
		label="File: msp_serial.c";

		FUNC(mspSerialAllocatePorts)
		FUNC(mspSerialProcess)
	}

	subgraph cluster_pwm_c {
		label="File: pwm.c";

		FUNC(rxPwmInit)
	}

	subgraph cluster_rx_pwm_c {
		label="File: rx_pwm.c";

		SVAR(captures)

		FUNC(ppmInConfig)
		FUNC(pwmICConfig)
		FUNC(ppmInit)

		FUNC(isPWMDataBeingReceived)
		FUNC(isPPMDataBeingReceived)

		FUNC(ppmRead)
		FUNC(pwmRead)

		LOAD(ppmRead, captures)
		LOAD(pwmRead, captures)

		FUNC(pwmEdgeCallback)
		FUNC(pwmOverflowCallback)
	}

	subgraph cluster_serial_c {
		label="File: serial.c";

		FUNC(openSerialPort)
	}

	subgraph cluster_serial_uart_c {
		label="File: serial_uart.c";

		FUNC(uartOpen)
		FUNC(uartSetBaudRate)
		FUNC(uartSetMode)
		FUNC(uartTotalRxBytesWaiting)
		FUNC(uartTotalTxBytesFree)
		FUNC(isUartTransmitBufferEmpty)
		FUNC(uartRead)
		FUNC(uartWrite)
		FUNC(uartStartTxDMA)
	}

	CALL(main, init)
	CALL(openSerialPort,uartOpen)
	CALL(mspSerialAllocatePorts,openSerialPort)
	CALL(taskHandleSerial,mspFcProcessCommand,cliProcess)
	CALL(cliPpmInfo, ppmRead)
	CALL(cliPwmInfo, pwmRead)

}
