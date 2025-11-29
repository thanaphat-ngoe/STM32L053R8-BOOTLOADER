import { useState } from "react";
import "../src/App.css";

import FileSelector from "../src/components/FileSelector";

type ALStateMachine = 
	"AL_STATE_Sync" | 
	"AL_STATE_UpdateReq" | 
	"AL_STATE_Firmware_Update" | 
	"AL_STATE_Done";

async function readBytes(port: SerialPort, byteCount: number): Promise<Uint8Array> {
  	const reader = port.readable.getReader();
  	const buffer = new Uint8Array(byteCount);
  	let offset = 0;

  	while (offset < byteCount) {
    	const { value, done } = await reader.read();
    	if (done) break;

		if (value) {
			buffer.set(value.slice(0, byteCount - offset), offset);
			offset += value.length;
		}
  	}

  	reader.releaseLock();
  	return buffer;
}

function toHexString(bytes: Uint8Array) {
  	return Array
		.from(bytes)
		.map((b) => b.toString(16)
		.toUpperCase()
		.padStart(2, "0"))
    	.join(" ");
}

const ACK = new Uint8Array([
	0x00, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0x31,
]);

function App() {
  	const [port, setPort] = useState<SerialPort | null>(null);
	const [stateMachine, setStateMachine] = useState<ALStateMachine>("AL_STATE_Sync");

	const filters = [
		{ usbVendorId: 0x0483, usbProductId: 0x3748 }, // ST-LINK/V2
		{ usbVendorId: 0x0483, usbProductId: 0x374B }, // ST-LINK/V2.1
		{ usbVendorId: 0x0483, usbProductId: 0x374e }, // ST-LINK/V3
	];

  	const handleSelectPort = async () => {
		setStateMachine("AL_STATE_Sync");
		try {
			if (!("serial" in navigator)) {
				alert("Web Serial API is not supported in this browser.");
				return;
			}
			const selectedPort = await navigator.serial.requestPort({ filters });
			await selectedPort.open({ baudRate: 115200 });
			const { usbProductId, usbVendorId } = selectedPort.getInfo();
			setPort(selectedPort);
			console.log("Port selected:", selectedPort);
			console.log("USB Vendor ID: 0x" + usbVendorId.toString(16));
			console.log("USB Product ID: 0x" + usbProductId.toString(16));
			const writer = selectedPort.writable.getWriter();

			// Sync Sequence
			const syncSeq = new Uint8Array([0x01, 0x02, 0x03, 0x04]);
			await writer.write(syncSeq);
			let data = await readBytes(selectedPort, 35);
			console.log("Value: " + toHexString(data));
			
			// BL_AL_MESSAGE_FW_UPDATE_REQ
			const BL_AL_MESSAGE_FW_UPDATE_REQ = new Uint8Array([
				0x01, 0x00, 0x31, 0xff, 0xff, 0xff, 0xff, 0xff,
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
				0xff, 0xff, 0x6C,
			]);
			await writer.write(BL_AL_MESSAGE_FW_UPDATE_REQ);
			data = await readBytes(selectedPort, 105);
			console.log("Value: " + toHexString(data));

			// BL_AL_MESSAGE_DEVICE_ID_RES
			const BL_AL_MESSAGE_DEVICE_ID_RES = new Uint8Array([
				0x02, 0x00, 0x3f, 0x01, 0xff, 0xff, 0xff, 0xff,
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
				0xff, 0xff, 0xe2,
			]);
			await writer.write(BL_AL_MESSAGE_DEVICE_ID_RES);
			data = await readBytes(selectedPort, 70);
			console.log("Value: " + toHexString(data));

			// BL_AL_MESSAGE_FW_LENGTH_RES
			const BL_AL_MESSAGE_FW_LENGTH_RES = new Uint8Array([
				0x05, 0x00, 0x45, 0x78, 0x08, 0x00, 0x00, 0xff,
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
				0xff, 0xff, 0xdd,
			]);
			await writer.write(BL_AL_MESSAGE_FW_LENGTH_RES);
			data = await readBytes(selectedPort, 70);
			console.log("Value: " + toHexString(data));
			writer.releaseLock();
			setStateMachine("AL_STATE_Firmware_Update");
		} catch (err) {
			setStateMachine("AL_STATE_Sync")
			console.error("Serial port error:", err);
		}
  	};

  	return (
    	<div>
			<div>Serial Port</div>
			<button onClick={handleSelectPort}>Select Serial Port</button>
			{port && <div>Serial port selected!</div>}
			{stateMachine == "AL_STATE_Firmware_Update" && <FileSelector 
				port={port}
				stateMachine={stateMachine}
      			setStateMachine={setStateMachine}
			/>}
			{stateMachine == "AL_STATE_Done" && <div>Firmware Updated!</div>}
    	</div>
  	);
}

export default App;
