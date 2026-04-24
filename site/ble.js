const bleStatus = document.getElementById("ble-status");
const lastCommand = document.getElementById("last-command");

const connectBtn = document.getElementById("connect-btn");
const openBtn = document.getElementById("open-btn");
const closeBtn = document.getElementById("close-btn");
const stopBtn = document.getElementById("stop-btn");
const setupBtn = document.getElementById("setup-btn");

const SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const CHARACTERISTIC_UUID_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";

let bluetoothDevice = null;
let gattServer = null;
let rxCharacteristic = null;

function setBleStatus(text) {
  bleStatus.textContent = text;
}

function setLastCommand(text) {
  lastCommand.textContent = `Last command: ${text}`;
}

function onDisconnected() {
  rxCharacteristic = null;
  gattServer = null;
  setBleStatus("BLE: DISCONNECTED");
}

async function connectBLE() {
  if (!navigator.bluetooth) {
    setBleStatus("BLE: UNSUPPORTED");
    alert("Web Bluetooth not supported in this browser");
    return;
  }

  try {
    setBleStatus("BLE: REQUESTING DEVICE");

    bluetoothDevice = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
      optionalServices: [SERVICE_UUID]
    });

    bluetoothDevice.addEventListener("gattserverdisconnected", onDisconnected);

    setBleStatus("BLE: CONNECTING");
    gattServer = await bluetoothDevice.gatt.connect();

    const service = await gattServer.getPrimaryService(SERVICE_UUID);
    rxCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID_RX);

    setBleStatus(`BLE: CONNECTED (${bluetoothDevice.name || "ESP32"})`);
  } catch (error) {
    console.error(error);
    setBleStatus("BLE: DISCONNECTED");
  }
}

async function sendCommand(command) {
  if (!rxCharacteristic) {
    alert("Connect to the ESP32 first.");
    return;
  }

  try {
    const data = new TextEncoder().encode(command);
    await rxCharacteristic.writeValue(data);
    console.log("Sent:", command);
    setLastCommand(command);
  } catch (error) {
    console.error(error);
    alert("Failed to send command.");
  }
}

connectBtn.addEventListener("click", connectBLE);
openBtn.addEventListener("click", () => sendCommand("OPEN"));
closeBtn.addEventListener("click", () => sendCommand("CLOSE"));
stopBtn.addEventListener("click", () => sendCommand("STOP"));
setupBtn.addEventListener("click", () => sendCommand("WHEEL SETUP"));

setBleStatus("BLE: DISCONNECTED");
setLastCommand("none");