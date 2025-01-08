from __future__ import print_function
import logging, os, sys
sys.path.append(os.path.join('opt', 'esp', 'esp-idf', 'tools'))
import pytest, esp_prov
from pytest_embedded import Dut

logging.basicConfig(level=logging.INFO)
esp_prov.config_throw_except = True

@pytest.mark.esp32
@pytest.mark.generic
@pytest.mark.parametrize('config', ['security1',], indirect=True)
@pytest.mark.xfail(reason='Runner unable to connect to target over Bluetooth', run=False)
def test_wifi_prov_mgr(dut: Dut) -> None:

    # Check if BT memory is released before provisioning starts
    dut.expect('wifi_prov_scheme_ble: BT memory released', timeout=60)

    # Parse BLE devname
    devname = dut.expect(b'Provisioning started with service name : CIVIC_(.{6})')[0].decode('utf-8').split(' : ')[1]
    logging.info('BLE Device Alias for DUT : {}'.format(devname))

    logging.info('Starting Provisioning')
    verbose = False
    protover = 'v1.1'
    ap_ssid = 'HedgeHome'
    ap_password = '8507487793'

    logging.info('Getting security')
    pop = 'sj$a8@;^0fcz'
    security = esp_prov.get_security(1, None, None, pop, verbose)
    if security is None:
        raise RuntimeError('Failed to get security')

    logging.info('Getting transport')
    transport = esp_prov.get_transport('ble', devname)
    if transport is None:
        raise RuntimeError('Failed to get transport')

    logging.info('Verifying protocol version')
    if not esp_prov.version_match(transport, protover):
        raise RuntimeError('Mismatch in protocol version')

    logging.info('Verifying scan list capability')
    if not esp_prov.has_capability(transport, 'wifi_scan'):
        raise RuntimeError('Capability not present')

    logging.info('Starting Session')
    if not esp_prov.establish_session(transport, security):
        raise RuntimeError('Failed to start session')

    logging.info('Sending Wifi credential to DUT')
    if not esp_prov.send_wifi_config(transport, security, ap_ssid, ap_password):
        raise RuntimeError('Failed to send Wi-Fi config')

    logging.info('Applying config')
    if not esp_prov.apply_wifi_config(transport, security):
        raise RuntimeError('Failed to send apply config')
    if not esp_prov.wait_wifi_connected(transport, security):
        raise RuntimeError('Provisioning failed')

    # Check if BTDM memory is released after provisioning finishes
    dut.expect('wifi_prov_scheme_ble: BTDM memory released', timeout=30)
