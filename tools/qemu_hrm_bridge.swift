#!/usr/bin/env swift
// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

import CoreBluetooth
import Darwin
import Foundation
import Network

setbuf(stdout, nil)

private let qemuHeader: [UInt8] = [0xfe, 0xed]
private let qemuFooter: [UInt8] = [0xbe, 0xef]
private let qemuProtocolBLEHrmBridge: UInt16 = 12
private let bridgeVersion: UInt8 = 1

private enum BridgeMessage: UInt8 {
  case hostReady = 1
  case serviceState = 2
  case measurement = 3
  case subscription = 4
  case hostSample = 5
}

private struct Config {
  var host = "127.0.0.1"
  var port: UInt16 = 12344
  var localName = "Pebble QEMU HRM"
  var syntheticBPM: UInt16? = 80
  var sourceFilter: String?
}

private func usage(exitCode: Int32 = 2) -> Never {
  print("""
  Usage: tools/qemu_hrm_bridge.swift [--host HOST] [--port PORT] [--name NAME]
                                    [--bpm BPM] [--no-synthetic] [--source FILTER]

  Connects to the QEMU pebble-tool serial port and advertises a standard BLE Heart Rate Service
  from this Mac. Garmin/head units connect to the Mac; HRM state and samples are relayed to QEMU.

  --source FILTER connects to a real BLE HRM source whose name or UUID contains FILTER.
  Use "--source any" to connect to the first discovered BLE HRM. When --source is set,
  synthetic HR is disabled unless --bpm is also provided.
  """)
  exit(exitCode)
}

private func parseConfig() -> Config {
  var config = Config()
  var didConfigureSynthetic = false
  var args = Array(CommandLine.arguments.dropFirst())
  while !args.isEmpty {
    let arg = args.removeFirst()
    switch arg {
    case "--host":
      guard let value = args.first else { usage() }
      config.host = value
      args.removeFirst()
    case "--port":
      guard let value = args.first, let port = UInt16(value) else { usage() }
      config.port = port
      args.removeFirst()
    case "--name":
      guard let value = args.first else { usage() }
      config.localName = value
      args.removeFirst()
    case "--bpm":
      guard let value = args.first, let bpm = UInt16(value), bpm > 0 else { usage() }
      config.syntheticBPM = bpm
      didConfigureSynthetic = true
      args.removeFirst()
    case "--no-synthetic":
      config.syntheticBPM = nil
      didConfigureSynthetic = true
    case "--source":
      guard let value = args.first, !value.isEmpty else { usage() }
      config.sourceFilter = value.lowercased() == "any" ? "" : value
      if !didConfigureSynthetic {
        config.syntheticBPM = nil
      }
      args.removeFirst()
    case "--help", "-h":
      usage(exitCode: 0)
    default:
      print("Unknown argument: \(arg)")
      usage()
    }
  }
  return config
}

private func uint16BE(_ bytes: [UInt8], _ offset: Int) -> UInt16 {
  return (UInt16(bytes[offset]) << 8) | UInt16(bytes[offset + 1])
}

private final class QemuSerialBridge {
  private let host: NWEndpoint.Host
  private let port: NWEndpoint.Port
  private let connection: NWConnection
  private var receiveBuffer: [UInt8] = []
  private let queue = DispatchQueue(label: "qemu-hrm-serial")

  var onServiceState: ((Bool) -> Void)?
  var onMeasurement: ((UInt16, Bool) -> Void)?
  private(set) var serviceEnabled = false

  init(host: String, port: UInt16) {
    self.host = NWEndpoint.Host(host)
    self.port = NWEndpoint.Port(rawValue: port)!
    self.connection = NWConnection(host: self.host, port: self.port, using: .tcp)
  }

  func start() {
    connection.stateUpdateHandler = { [weak self] state in
      switch state {
      case .ready:
        print("Connected to QEMU serial bridge at \(self?.host.debugDescription ?? "?"):\(self?.port.rawValue ?? 0)")
        self?.sendHostReady()
      case .failed(let error):
        print("QEMU serial bridge failed: \(error)")
        exit(1)
      case .cancelled:
        print("QEMU serial bridge disconnected")
        exit(0)
      default:
        break
      }
    }
    connection.start(queue: queue)
    receive()
  }

  func sendSubscription(_ isSubscribed: Bool) {
    sendBridgePayload([
      BridgeMessage.subscription.rawValue,
      bridgeVersion,
      isSubscribed ? 1 : 0,
    ])
  }

  func sendHostSample(bpm: UInt16, isOnWrist: Bool) {
    sendBridgePayload([
      BridgeMessage.hostSample.rawValue,
      bridgeVersion,
      UInt8((bpm >> 8) & 0xff),
      UInt8(bpm & 0xff),
      isOnWrist ? 1 : 0,
    ])
  }

  private func sendHostReady() {
    sendBridgePayload([
      BridgeMessage.hostReady.rawValue,
      bridgeVersion,
    ])
  }

  private func sendBridgePayload(_ payload: [UInt8]) {
    var frame: [UInt8] = []
    frame.append(contentsOf: qemuHeader)
    frame.append(UInt8((qemuProtocolBLEHrmBridge >> 8) & 0xff))
    frame.append(UInt8(qemuProtocolBLEHrmBridge & 0xff))
    frame.append(UInt8((payload.count >> 8) & 0xff))
    frame.append(UInt8(payload.count & 0xff))
    frame.append(contentsOf: payload)
    frame.append(contentsOf: qemuFooter)

    connection.send(content: Data(frame), completion: .contentProcessed { error in
      if let error {
        print("Failed to send QEMU HRM bridge frame: \(error)")
      }
    })
  }

  private func receive() {
    connection.receive(minimumIncompleteLength: 1, maximumLength: 4096) { [weak self] data, _, isComplete, error in
      guard let self else { return }
      if let data, !data.isEmpty {
        self.receiveBuffer.append(contentsOf: data)
        self.processFrames()
      }
      if let error {
        print("QEMU serial receive failed: \(error)")
        exit(1)
      }
      if isComplete {
        print("QEMU serial connection closed")
        exit(0)
      }
      self.receive()
    }
  }

  private func processFrames() {
    while receiveBuffer.count >= 8 {
      if receiveBuffer[0] != qemuHeader[0] || receiveBuffer[1] != qemuHeader[1] {
        if let headerIndex = receiveBuffer.indices.dropFirst().first(where: {
          receiveBuffer[$0] == qemuHeader[0] &&
            $0 + 1 < receiveBuffer.count &&
            receiveBuffer[$0 + 1] == qemuHeader[1]
        }) {
          receiveBuffer.removeFirst(headerIndex)
        } else {
          receiveBuffer.removeAll(keepingCapacity: true)
          return
        }
      }

      let proto = uint16BE(receiveBuffer, 2)
      let length = Int(uint16BE(receiveBuffer, 4))
      let totalLength = 6 + length + 2
      if receiveBuffer.count < totalLength {
        return
      }

      guard receiveBuffer[6 + length] == qemuFooter[0],
            receiveBuffer[6 + length + 1] == qemuFooter[1] else {
        receiveBuffer.removeFirst(2)
        continue
      }

      let payload = Array(receiveBuffer[6..<(6 + length)])
      receiveBuffer.removeFirst(totalLength)

      if proto == qemuProtocolBLEHrmBridge {
        handleBridgePayload(payload)
      }
    }
  }

  private func handleBridgePayload(_ payload: [UInt8]) {
    guard payload.count >= 2, payload[1] == bridgeVersion else {
      return
    }

    guard let message = BridgeMessage(rawValue: payload[0]) else {
      return
    }

    switch message {
    case .serviceState:
      guard payload.count == 3 else { return }
      serviceEnabled = payload[2] != 0
      DispatchQueue.main.async {
        self.onServiceState?(self.serviceEnabled)
      }
    case .measurement:
      guard payload.count == 5 else { return }
      let bpm = uint16BE(payload, 2)
      let isOnWrist = payload[4] != 0
      DispatchQueue.main.async {
        self.onMeasurement?(bpm, isOnWrist)
      }
    default:
      break
    }
  }
}

private final class HrmPeripheral: NSObject, CBPeripheralManagerDelegate {
  private let localName: String
  private let bridge: QemuSerialBridge
  private var peripheral: CBPeripheralManager!
  private var measurementCharacteristic: CBMutableCharacteristic!
  private var bodyLocationCharacteristic: CBMutableCharacteristic!
  private var subscribedCentralCount = 0
  private var isServiceEnabled = false
  private var isAdvertisingRequested = false
  private var pendingMeasurement: Data?

  private let serviceUUID = CBUUID(string: "180D")
  private let measurementUUID = CBUUID(string: "2A37")
  private let bodyLocationUUID = CBUUID(string: "2A38")

  init(localName: String, bridge: QemuSerialBridge) {
    self.localName = localName
    self.bridge = bridge
    super.init()
    self.peripheral = CBPeripheralManager(delegate: self, queue: DispatchQueue.main)
  }

  func setServiceEnabled(_ enabled: Bool) {
    isServiceEnabled = enabled
    if enabled {
      startAdvertisingIfReady()
    } else {
      peripheral.stopAdvertising()
      isAdvertisingRequested = false
      subscribedCentralCount = 0
      bridge.sendSubscription(false)
      print("HRM service disabled by QEMU")
    }
  }

  func notifyMeasurement(bpm: UInt16, isOnWrist: Bool) {
    guard isServiceEnabled, subscribedCentralCount > 0 else {
      return
    }

    var flags: UInt8 = isOnWrist ? 0x06 : 0x04
    var payload: [UInt8] = []
    if bpm > UInt8.max {
      flags |= 0x01
      payload = [flags, UInt8(bpm & 0xff), UInt8((bpm >> 8) & 0xff)]
    } else {
      payload = [flags, UInt8(bpm)]
    }

    let data = Data(payload)
    if !peripheral.updateValue(data, for: measurementCharacteristic, onSubscribedCentrals: nil) {
      pendingMeasurement = data
    }
  }

  func peripheralManagerDidUpdateState(_ peripheral: CBPeripheralManager) {
    switch peripheral.state {
    case .poweredOn:
      setupService()
      startAdvertisingIfReady()
    case .poweredOff:
      print("Bluetooth is powered off")
    case .unauthorized:
      print("Bluetooth permission is not authorized for this process")
    case .unsupported:
      print("This Mac does not support BLE peripheral mode")
    default:
      print("Bluetooth state: \(peripheral.state.rawValue)")
    }
  }

  func peripheralManager(_ peripheral: CBPeripheralManager, didAdd service: CBService, error: Error?) {
    if let error {
      print("Failed to add HRM service: \(error)")
    } else {
      startAdvertisingIfReady()
    }
  }

  func peripheralManagerDidStartAdvertising(_ peripheral: CBPeripheralManager, error: Error?) {
    if let error {
      isAdvertisingRequested = peripheral.isAdvertising
      print("Failed to advertise HRM service: \(error)")
    } else {
      isAdvertisingRequested = true
      print("Advertising BLE Heart Rate Service as \"\(localName)\"")
    }
  }

  func peripheralManager(_ peripheral: CBPeripheralManager,
                         central: CBCentral,
                         didSubscribeTo characteristic: CBCharacteristic) {
    guard characteristic.uuid == measurementUUID else {
      return
    }
    subscribedCentralCount += 1
    bridge.sendSubscription(true)
    print("BLE central subscribed to HR measurements")
  }

  func peripheralManager(_ peripheral: CBPeripheralManager,
                         central: CBCentral,
                         didUnsubscribeFrom characteristic: CBCharacteristic) {
    guard characteristic.uuid == measurementUUID else {
      return
    }
    subscribedCentralCount = max(0, subscribedCentralCount - 1)
    if subscribedCentralCount == 0 {
      bridge.sendSubscription(false)
    }
    print("BLE central unsubscribed from HR measurements")
  }

  func peripheralManager(_ peripheral: CBPeripheralManager, didReceiveRead request: CBATTRequest) {
    if request.characteristic.uuid == bodyLocationUUID {
      request.value = Data([0x02])
      peripheral.respond(to: request, withResult: .success)
    } else {
      peripheral.respond(to: request, withResult: .requestNotSupported)
    }
  }

  func peripheralManagerIsReady(toUpdateSubscribers peripheral: CBPeripheralManager) {
    guard let pendingMeasurement else {
      return
    }
    if peripheral.updateValue(pendingMeasurement, for: measurementCharacteristic, onSubscribedCentrals: nil) {
      self.pendingMeasurement = nil
    }
  }

  private func setupService() {
    peripheral.removeAllServices()

    measurementCharacteristic = CBMutableCharacteristic(
      type: measurementUUID,
      properties: [.notify],
      value: nil,
      permissions: []
    )
    bodyLocationCharacteristic = CBMutableCharacteristic(
      type: bodyLocationUUID,
      properties: [.read],
      value: nil,
      permissions: [.readable]
    )

    let service = CBMutableService(type: serviceUUID, primary: true)
    service.characteristics = [measurementCharacteristic, bodyLocationCharacteristic]
    peripheral.add(service)
  }

  private func startAdvertisingIfReady() {
    guard isServiceEnabled,
          peripheral.state == .poweredOn,
          !peripheral.isAdvertising,
          !isAdvertisingRequested else {
      return
    }

    isAdvertisingRequested = true
    peripheral.startAdvertising([
      CBAdvertisementDataLocalNameKey: localName,
      CBAdvertisementDataServiceUUIDsKey: [serviceUUID],
    ])
  }
}

private final class SyntheticHeartRateSource {
  private let bpm: UInt16
  private let bridge: QemuSerialBridge
  private var timer: DispatchSourceTimer?

  init(bpm: UInt16, bridge: QemuSerialBridge) {
    self.bpm = bpm
    self.bridge = bridge
  }

  func start() {
    let timer = DispatchSource.makeTimerSource(queue: DispatchQueue.main)
    timer.schedule(deadline: .now() + 1, repeating: 1)
    timer.setEventHandler { [weak self] in
      guard let self, self.bridge.serviceEnabled else {
        return
      }
      self.bridge.sendHostSample(bpm: self.bpm, isOnWrist: true)
    }
    self.timer = timer
    timer.resume()
    print("Synthetic HR source enabled at \(bpm) BPM")
  }
}

private final class BluetoothHeartRateSource: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
  private let sourceFilter: String
  private let bridge: QemuSerialBridge
  private var central: CBCentralManager!
  private var shouldScan = false
  private var connectedPeripheral: CBPeripheral?
  private var seenPeripheralIdentifiers = Set<UUID>()
  private var lastLoggedBPM: UInt16?

  private let serviceUUID = CBUUID(string: "180D")
  private let measurementUUID = CBUUID(string: "2A37")

  init(sourceFilter: String, bridge: QemuSerialBridge) {
    self.sourceFilter = sourceFilter
    self.bridge = bridge
    super.init()
    self.central = CBCentralManager(delegate: self, queue: DispatchQueue.main)
  }

  func start() {
    shouldScan = true
    if sourceFilter.isEmpty {
      print("Real BLE HR source enabled: first discovered Heart Rate Service")
    } else {
      print("Real BLE HR source enabled: matching \"\(sourceFilter)\"")
    }
    scanIfReady()
  }

  func centralManagerDidUpdateState(_ central: CBCentralManager) {
    switch central.state {
    case .poweredOn:
      scanIfReady()
    case .poweredOff:
      print("Bluetooth is powered off")
    case .unauthorized:
      print("Bluetooth permission is not authorized for this process")
    case .unsupported:
      print("This Mac does not support BLE central mode")
    default:
      print("Bluetooth central state: \(central.state.rawValue)")
    }
  }

  func centralManager(_ central: CBCentralManager,
                      didDiscover peripheral: CBPeripheral,
                      advertisementData: [String: Any],
                      rssi RSSI: NSNumber) {
    let advertisedName = advertisementData[CBAdvertisementDataLocalNameKey] as? String
    if seenPeripheralIdentifiers.insert(peripheral.identifier).inserted {
      print("Discovered BLE HR source: \(displayName(for: peripheral, advertisedName: advertisedName))")
    }

    guard matches(peripheral: peripheral, advertisedName: advertisedName) else {
      return
    }

    central.stopScan()
    connectedPeripheral = peripheral
    peripheral.delegate = self
    print("Connecting to BLE HR source: \(displayName(for: peripheral, advertisedName: advertisedName))")
    central.connect(peripheral, options: nil)
  }

  func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
    print("Connected to BLE HR source: \(displayName(for: peripheral))")
    peripheral.discoverServices([serviceUUID])
  }

  func centralManager(_ central: CBCentralManager,
                      didFailToConnect peripheral: CBPeripheral,
                      error: Error?) {
    print("Failed to connect to BLE HR source \(displayName(for: peripheral)): \(errorDescription(error))")
    connectedPeripheral = nil
    scanIfReady()
  }

  func centralManager(_ central: CBCentralManager,
                      didDisconnectPeripheral peripheral: CBPeripheral,
                      error: Error?) {
    print("BLE HR source disconnected: \(displayName(for: peripheral)): \(errorDescription(error))")
    if connectedPeripheral?.identifier == peripheral.identifier {
      connectedPeripheral = nil
      lastLoggedBPM = nil
    }
    scanIfReady()
  }

  func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
    if let error {
      print("Failed to discover HR source services: \(error)")
      central.cancelPeripheralConnection(peripheral)
      return
    }

    guard let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }) else {
      print("BLE HR source does not expose Heart Rate Service")
      central.cancelPeripheralConnection(peripheral)
      return
    }

    peripheral.discoverCharacteristics([measurementUUID], for: service)
  }

  func peripheral(_ peripheral: CBPeripheral,
                  didDiscoverCharacteristicsFor service: CBService,
                  error: Error?) {
    if let error {
      print("Failed to discover HR source characteristics: \(error)")
      central.cancelPeripheralConnection(peripheral)
      return
    }

    guard let characteristic = service.characteristics?.first(where: { $0.uuid == measurementUUID }) else {
      print("BLE HR source does not expose Heart Rate Measurement")
      central.cancelPeripheralConnection(peripheral)
      return
    }

    peripheral.setNotifyValue(true, for: characteristic)
    print("Subscribed to real BLE HR measurements")
  }

  func peripheral(_ peripheral: CBPeripheral,
                  didUpdateNotificationStateFor characteristic: CBCharacteristic,
                  error: Error?) {
    if let error {
      print("Failed to update HR source notification state: \(error)")
      central.cancelPeripheralConnection(peripheral)
    }
  }

  func peripheral(_ peripheral: CBPeripheral,
                  didUpdateValueFor characteristic: CBCharacteristic,
                  error: Error?) {
    if let error {
      print("Failed to read HR source measurement: \(error)")
      return
    }

    guard characteristic.uuid == measurementUUID,
          let data = characteristic.value,
          let measurement = parseHeartRateMeasurement(data) else {
      return
    }

    if bridge.serviceEnabled {
      bridge.sendHostSample(bpm: measurement.bpm, isOnWrist: measurement.isOnWrist)
    }

    if lastLoggedBPM != measurement.bpm {
      lastLoggedBPM = measurement.bpm
      print("Real BLE HR sample: \(measurement.bpm) BPM")
    }
  }

  private func scanIfReady() {
    guard shouldScan, central.state == .poweredOn, connectedPeripheral == nil, !central.isScanning else {
      return
    }

    central.scanForPeripherals(withServices: [serviceUUID], options: [
      CBCentralManagerScanOptionAllowDuplicatesKey: false,
    ])
    print("Scanning for BLE Heart Rate sources")
  }

  private func matches(peripheral: CBPeripheral, advertisedName: String?) -> Bool {
    if sourceFilter.isEmpty {
      return true
    }

    let filter = sourceFilter.lowercased()
    let candidates = [
      peripheral.name,
      advertisedName,
      peripheral.identifier.uuidString,
    ].compactMap { $0?.lowercased() }

    return candidates.contains { $0.contains(filter) }
  }

  private func displayName(for peripheral: CBPeripheral, advertisedName: String? = nil) -> String {
    let name = peripheral.name ?? advertisedName ?? "Unnamed"
    return "\(name) [\(peripheral.identifier.uuidString)]"
  }

  private func errorDescription(_ error: Error?) -> String {
    guard let error else {
      return "no error"
    }
    return "\(error)"
  }

  private func parseHeartRateMeasurement(_ data: Data) -> (bpm: UInt16, isOnWrist: Bool)? {
    let bytes = [UInt8](data)
    guard let flags = bytes.first else {
      return nil
    }

    var index = 1
    let bpm: UInt16
    if flags & 0x01 != 0 {
      guard bytes.count >= index + 2 else {
        return nil
      }
      bpm = UInt16(bytes[index]) | (UInt16(bytes[index + 1]) << 8)
      index += 2
    } else {
      guard bytes.count >= index + 1 else {
        return nil
      }
      bpm = UInt16(bytes[index])
      index += 1
    }

    guard bpm > 0 else {
      return nil
    }

    let sensorContactSupported = flags & 0x04 != 0
    let sensorContactDetected = flags & 0x02 != 0
    return (bpm: bpm, isOnWrist: !sensorContactSupported || sensorContactDetected)
  }
}

private let config = parseConfig()
private let qemu = QemuSerialBridge(host: config.host, port: config.port)
private let peripheral = HrmPeripheral(localName: config.localName, bridge: qemu)
private let synthetic = config.syntheticBPM.map { SyntheticHeartRateSource(bpm: $0, bridge: qemu) }
private let realSource = config.sourceFilter.map { BluetoothHeartRateSource(sourceFilter: $0, bridge: qemu) }

qemu.onServiceState = { [peripheral] enabled in
  peripheral.setServiceEnabled(enabled)
}
qemu.onMeasurement = { [peripheral] bpm, isOnWrist in
  peripheral.notifyMeasurement(bpm: bpm, isOnWrist: isOnWrist)
}

qemu.start()
synthetic?.start()
realSource?.start()
RunLoop.main.run()
