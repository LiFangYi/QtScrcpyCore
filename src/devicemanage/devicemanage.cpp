#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include "devicemanage.h"

#include <QCryptographicHash>

#include "device.h"
#include "demuxer.h"

namespace qsc {

#define DM_MAX_DEVICES_NUM 1000

IDeviceManage& IDeviceManage::getInstance() {
    static DeviceManage dm;
    return dm;
}

DeviceManage::DeviceManage() {
    Demuxer::init();
}

DeviceManage::~DeviceManage() {
    Demuxer::deInit();
}

QPointer<IDevice> DeviceManage::getDevice(const QString &serial)
{
    if (!m_devices.contains(serial)) {
        return QPointer<IDevice>();
    }
    return m_devices[serial];
}

bool DeviceManage::connectDevice(qsc::DeviceParams params)
{
    if (params.serial.trimmed().isEmpty()) {
        return false;
    }
    if (m_devices.contains(params.serial)) {
        return false;
    }
    if (DM_MAX_DEVICES_NUM < m_devices.size()) {
        qInfo("over the maximum number of connections");
        return false;
    }

    quint16 port = 0;
    if (params.useReverse) {
         port = getFreePort(params.serial);
        if (0 == port) {
            qInfo("no port available, automatically switch to forward");
            params.useReverse = false;
        } else {
            params.localPort = port;
            qInfo("free port %d", port);
        }
    }

    IDevice *device = new Device(params);
    connect(device, &Device::deviceConnected, this, &DeviceManage::onDeviceConnected);
    connect(device, &Device::deviceDisconnected, this, &DeviceManage::onDeviceDisconnected);
    if (!device->connectDevice()) {
        delete device;
        return false;
    }
    m_devices[params.serial] = device;
    return true;
}

bool DeviceManage::disconnectDevice(const QString &serial)
{
    bool ret = false;
    if (!serial.isEmpty() && m_devices.contains(serial)) {
        auto it = m_devices.find(serial);
        if (it->data()) {
            delete it->data();
            ret = true;
        }
    }
    return ret;
}

void DeviceManage::disconnectAllDevice()
{
    QMapIterator<QString, QPointer<IDevice>> i(m_devices);
    while (i.hasNext()) {
        i.next();
        if (i.value()) {
            delete i.value();
        }
    }
}

void DeviceManage::onDeviceConnected(bool success, const QString &serial, const QString &deviceName, const QSize &size)
{
    emit deviceConnected(success, serial, deviceName, size);
    if (!success) {
        removeDevice(serial);
    }
}

void DeviceManage::onDeviceDisconnected(QString serial)
{
    emit deviceDisconnected(serial);
    removeDevice(serial);
}

quint16 DeviceManage::getFreePort(const QString &serial) const
{
    QByteArray hash = QCryptographicHash::hash(serial.toUtf8(), QCryptographicHash::Sha256);
    const quint16 port = (static_cast<quint8>(hash[0]) << 8) | static_cast<quint8>(hash[1]);
    const quint16 highPort = m_localPortStart + (port % (65535 - m_localPortStart + 1));
    return highPort;
}

void DeviceManage::removeDevice(const QString &serial)
{
    if (!serial.isEmpty() && m_devices.contains(serial)) {
        m_devices[serial]->deleteLater();
        m_devices.remove(serial);
    }
}

}
