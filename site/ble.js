const bleStatus = document.getElementById("ble-status");
const connectBtn = document.getElementById("connect-btn");

let bluetoothDevice = null;

function setBleStatus(connected) {
  if (connected) {
    bleStatus.textContent = "BLE: CONNECTED";
  } else {
    bleStatus.textContent = "BLE: DISCONNECTED";
  }
}

connectBtn.addEventListener("click", async () => {

  if (!navigator.bluetooth) {
    bleStatus.textContent = "BLE: UNSUPPORTED";
    alert("Web Bluetooth not supported in this browser");
    return;
  }

  try {
    bluetoothDevice = await navigator.bluetooth.requestDevice({
      acceptAllDevices: true
    });

    await bluetoothDevice.gatt.connect();

    setBleStatus(true);

    bluetoothDevice.addEventListener("gattserverdisconnected", () => {
      setBleStatus(false);
    });

  } catch (error) {
    console.log(error);
    setBleStatus(false);
  }
});

setBleStatus(false);