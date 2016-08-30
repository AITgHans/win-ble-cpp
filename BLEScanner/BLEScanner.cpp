// BLEScanner.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <Windows.Foundation.h>
#include <Windows.Devices.Bluetooth.h>
#include <Windows.Devices.Bluetooth.Advertisement.h>
#include <Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <wrl/wrappers/corewrappers.h>
#include <wrl/client.h>
#include <wrl/event.h>

using namespace ABI::Windows::Devices;

#define VLOG(n) std::cout << std::endl

#pragma comment(lib,"runtimeobject")

void connectDevice(UINT64 bleAddr, GUID serviceUuuid) {
	HRESULT hr;
	Microsoft::WRL::ComPtr<Bluetooth::IBluetoothLEDeviceStatics> bluetoothLEDeviceStatics;
	hr = RoGetActivationFactory(
		Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Devices_Bluetooth_BluetoothLEDevice).Get(),
		__uuidof(Bluetooth::IBluetoothLEDeviceStatics),
		(void**)bluetoothLEDeviceStatics.GetAddressOf());
	VLOG(1) << "bluetoothLEDeviceStatics:" << hr;

	Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<Bluetooth::BluetoothLEDevice*>> operation;
	bluetoothLEDeviceStatics.Get()->FromBluetoothAddressAsync(bleAddr, operation.GetAddressOf());

	Microsoft::WRL::ComPtr<Bluetooth::IBluetoothLEDevice> device;
	do {
		hr = operation.Get()->GetResults(device.GetAddressOf());
	} while (hr != 0);

	Microsoft::WRL::ComPtr<Bluetooth::GenericAttributeProfile::IGattDeviceService> service;
	hr = device.Get()->GetGattService(serviceUuuid, service.GetAddressOf());
	VLOG(1) << "get_Service: " << hr;

	Microsoft::WRL::ComPtr<Bluetooth::GenericAttributeProfile::IGattCharacteristicStatics> gattCharacteristicStatics;
	hr = RoGetActivationFactory(
		Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Devices_Bluetooth_GenericAttributeProfile_GattCharacteristic).Get(),
		__uuidof(Bluetooth::GenericAttributeProfile::IGattCharacteristicStatics),
		(void**)gattCharacteristicStatics.GetAddressOf());

	GUID charUuid;
	VLOG(1) << "Convert: " << gattCharacteristicStatics.Get()->ConvertShortIdToUuid(0x5200, &charUuid);

	Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVectorView<Bluetooth::GenericAttributeProfile::GattCharacteristic*>> characteristics;
	hr = service.Get()->GetCharacteristics(charUuid, characteristics.GetAddressOf());
	VLOG(1) << "GetCharacterstics: " << hr;

	OLECHAR* bstrGuid;
	StringFromCLSID(charUuid, &bstrGuid);
	int charLen = WideCharToMultiByte(CP_ACP, 0, bstrGuid, wcslen(bstrGuid), NULL, 0, NULL, NULL);
	char* buffer = new char[charLen + 1];
	WideCharToMultiByte(CP_ACP, 0, bstrGuid, wcslen(bstrGuid), buffer, charLen, NULL, NULL);
	buffer[charLen] = 0;
	VLOG(1) << "Char UUID: " << buffer;

	unsigned int size;
	characteristics.Get()->get_Size(&size);
	VLOG(1) << "# Characteristics " << size;

	if (size > 0){
		Microsoft::WRL::ComPtr<Bluetooth::GenericAttributeProfile::IGattCharacteristic> characteristic;

		hr = characteristics.Get()->GetAt(0, characteristic.GetAddressOf());
		VLOG(1) << "Char 0: " << hr;

		Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<Bluetooth::GenericAttributeProfile::GattCommunicationStatus>> writeOperation;

		Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IDataWriter> writer;
		IInspectable *iWriterInsp;
		hr = RoActivateInstance(
			Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Storage_Streams_DataWriter).Get(),
			&iWriterInsp);
		VLOG(1) << "Construct Writer " << hr;
		hr = iWriterInsp->QueryInterface(__uuidof(ABI::Windows::Storage::Streams::IDataWriter), (void**)writer.GetAddressOf());
		VLOG(1) << "Query Interface is:" << hr;

		BYTE bytes[4] = { 90, 90, 90, 90 };
		VLOG(1) << "Write bytes:  " << writer.Get()->WriteBytes(4, bytes);

		Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IBuffer> buffer;
		VLOG(1) << "Get buffer:" << writer.Get()->DetachBuffer(buffer.GetAddressOf());

		VLOG(1) << "Write value async: " << characteristic.Get()->WriteValueAsync(buffer.Get(), writeOperation.GetAddressOf());
	}

	Bluetooth::BluetoothConnectionStatus status;
	hr = device.Get()->get_ConnectionStatus(&status);
	VLOG(1) << "get_ConnectionStatus: " << hr;
	VLOG(1) << "get_ConnectionStatus: -> " << status;
	for (;;);
}

int main()
{
	int a;

	Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);

	Microsoft::WRL::ComPtr<Bluetooth::Advertisement::IBluetoothLEAdvertisementWatcher> watcher;
	IInspectable *filterInsp;
	HRESULT hr = RoActivateInstance(
		Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementWatcher).Get(),
		&filterInsp);
	VLOG(1) << "HResult/filter is:" << hr;

	hr = filterInsp->QueryInterface(__uuidof(Bluetooth::Advertisement::IBluetoothLEAdvertisementWatcher), (void**)watcher.GetAddressOf());
	VLOG(1) << "Query Interface is:" << hr;

	EventRegistrationToken addedToken;
	auto callback = Microsoft::WRL::Callback<ABI::Windows::Foundation::ITypedEventHandler<Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher*, Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs*>>(
		[](Bluetooth::Advertisement::IBluetoothLEAdvertisementWatcher* watcher, Bluetooth::Advertisement::IBluetoothLEAdvertisementReceivedEventArgs* arg) -> HRESULT
	{
		UINT64 addr;
		
		arg->get_BluetoothAddress(&addr);
		VLOG(1) << "******ADVERTISEMENT RESULT!" << addr;

		Microsoft::WRL::ComPtr<Bluetooth::Advertisement::IBluetoothLEAdvertisement> adv;
		arg->get_Advertisement(adv.GetAddressOf());

		HSTRING localName;
		adv.Get()->get_LocalName(&localName);
		if (localName) {
			UINT32 len;
			PCWSTR pwzLocalName = WindowsGetStringRawBuffer(localName, &len);
			int charLen = WideCharToMultiByte(CP_ACP, 0, pwzLocalName, len, NULL, 0, NULL, NULL);
			char* buffer = new char[charLen + 1];
			WideCharToMultiByte(CP_ACP, 0, pwzLocalName, len, buffer, charLen, NULL, NULL);
			buffer[charLen] = 0;
			VLOG(1) << "******Local name:" << buffer;
		} else {
			VLOG(1) << "******no local name";
		}

		Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IVector<GUID>> serviceUuids;
		adv.Get()->get_ServiceUuids(serviceUuids.GetAddressOf());
		
		if (serviceUuids) {
			unsigned int size = 0;
			serviceUuids.Get()->get_Size(&size);
			VLOG(1) << "Count of services: " << size;
			for (int i = 0; i < size; i++) {
				GUID guid;
				serviceUuids.Get()->GetAt(i, &guid);

				OLECHAR* bstrGuid;
				StringFromCLSID(guid, &bstrGuid);
				int charLen = WideCharToMultiByte(CP_ACP, 0, bstrGuid, wcslen(bstrGuid), NULL, 0, NULL, NULL);
				char* buffer = new char[charLen + 1];
				WideCharToMultiByte(CP_ACP, 0, bstrGuid, wcslen(bstrGuid), buffer, charLen, NULL, NULL);
				buffer[charLen] = 0;

				if (!strcmp(buffer, "{0000180A-0000-1000-8000-00805F9B34FB}")) {
					VLOG(1) << "Found device!!!!!!!!!!!!!!!!!!!";
					connectDevice(addr, guid);
				}

				VLOG(1) << "* Guid: " << buffer;

			}
		}

		return S_OK;

	});
	VLOG(1) << "Scanning mode:" <<  watcher->put_ScanningMode(Bluetooth::Advertisement::BluetoothLEScanningMode_Active);
	VLOG(1) << "Add recevied:" << watcher->add_Received(callback.Get(), &addedToken);
	VLOG(1) << "Start:" << watcher->Start();

	ABI::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcherStatus status = Bluetooth::Advertisement::BluetoothLEAdvertisementWatcherStatus_Created;
	VLOG(1) << "Get status:" << watcher->get_Status(&status);
	VLOG(1) << "Status is " << status;


	VLOG(1) << "Sleep.5s";
	Sleep(25000);
	VLOG(1) << "After-Sleep.5s";

	std::cin >> a;
    return 0;
}
