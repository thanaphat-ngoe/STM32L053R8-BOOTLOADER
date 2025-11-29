import "../../src/components/FileSelector.css"
import { useState, type ChangeEvent } from "react";

type Props = {
    port: any;
    stateMachine: any;
    setStateMachine: any;
};

function toHexString(bytes: Uint8Array) {
  	return Array
		.from(bytes)
		.map((b) => b.toString(16)
		.toUpperCase()
		.padStart(2, "0"))
    	.join(" ");
}

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

function crc8(data: Uint8Array, length: number): number {
    let crc = 0;

    for (let i = 0; i < length; i++) {
        crc ^= data[i];

        for (let j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = ((crc << 1) ^ 0x07) & 0xFF;
            } else {
                crc = (crc << 1) & 0xFF;
            }
        }
    }

    return crc & 0xFF;
}


function FileUploader({ port, stateMachine, setStateMachine }: Props) {
    const [file, setFile] = useState<File | null>(null);
    const [bytes, setBytes] = useState<Uint8Array | null>(null);

    async function handleFileChange(e: ChangeEvent<HTMLInputElement>) {
        if (e.target.files && e.target.files[0]) {
            const selectedFile = e.target.files[0];
            setFile(selectedFile);
            const arrayBuffer = await selectedFile.arrayBuffer(); // Read file as ArrayBuffer
            const byteArray = new Uint8Array(arrayBuffer); // Convert to Uint8Array (raw bytes)
            setBytes(byteArray);
        }
    }

    async function handleFileUpload() {
        if (!bytes) {
            console.error("No file loaded yet!");
            return;
        }
        
        if (bytes.length % 4 != 0) {
            return;
        }
        
        let byte_sent: number = 0;
        let segment_to_sent: Uint8Array = new Uint8Array(35);
        for (let i: number = 0; i < 35; i++) {
            segment_to_sent[i] = 0xff;
        }

        const writer = port.writable.getWriter();

        try {
            while (byte_sent < bytes.length) {
                if ((bytes.length - byte_sent) > 32) {
                    segment_to_sent[0] = 0x20;
                    segment_to_sent[1] = 0x00;
                    for (let i: number = 2; i < 34; i++) {
                        segment_to_sent[i] = bytes[byte_sent]
                        byte_sent += 1;
                    }
                    segment_to_sent[34] = crc8(segment_to_sent, 34);
                    await writer.write(segment_to_sent);
                    const data = await readBytes(port, 70);
			        console.log("Value: " + toHexString(data));
                } else {
                    let segment_data_padding_start: number = 0;
                    segment_to_sent[0] = (bytes.length - byte_sent) & 0xff;
                    segment_to_sent[1] = 0x00;
                    for (let i: number = 2; i < ((bytes.length - byte_sent) + 2); i++) {
                        segment_to_sent[i] = bytes[byte_sent];
                        byte_sent += 1;
                        segment_data_padding_start = i + 1;
                    }
                    for (let i: number = segment_data_padding_start; i < 34; i++) {
                        segment_to_sent[i] = 0xff;
                    }
                    segment_to_sent[34] = crc8(segment_to_sent, 34);
                    await writer.write(segment_to_sent);
                    const data = await readBytes(port, 70);
			        console.log("Value: " + toHexString(data));
                    setStateMachine("AL_STATE_Done");
                }
            }
        } finally {
            writer.releaseLock();
        }
        
    }

    return (
        <div className="space-y-4">
            <input type="file" onChange={handleFileChange} />
            {file && (
                <div>
                    <p>File name: {file.name}</p>
                    <p>Size: {(file.size)} Byte</p>
                    <p>Type: {file.type}</p>
                </div>
            )}
            {file 
                && stateMachine == "AL_STATE_Firmware_Update" 
                && <button onClick={handleFileUpload}>Upload</button>}
        </div>
    );
}

export default FileUploader;
